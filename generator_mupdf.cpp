/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "generator_mupdf.hpp"
#include "page.hpp"
#include <qimage.h>
#include <qmutex.h>

#include <kaboutdata.h>
#include <kdebug.h>
#include <kglobal.h>
#include <klocale.h>
#include <kpassworddialog.h>
#include <kwallet.h>

#include <okular/core/page.h>
#include <okular/core/textpage.h>

static const int MuPDFDebug = 4716;

static Okular::TextPage *buildTextPage(const QVector<QMuPDF::TextBox*> &boxes,
                                       qreal width, qreal height)
{
    Okular::TextPage *ktp = new Okular::TextPage();
    for (int i = 0; i < boxes.size(); ++i) {
        QMuPDF::TextBox *box = boxes.at(i);
        const QChar c = box->text();
        const QRectF charBBox = box->rect();
        QString text(c);
        if (box->isAtEndOfLine()) {
            text.append('\n');
        }
        ktp->append(text, new Okular::NormalizedRect(
                        charBBox.left() / width, charBBox.top() / height,
                        charBBox.right() / width, charBBox.bottom() / height));
    }
    return ktp;
}

static void recurseCreateTOC(QDomDocument &mainDoc, QMuPDF::Outline *outline,
                             QDomNode &parentDestination)
{
    foreach (QMuPDF::Outline *child, outline->children()) {
        QDomElement newel = mainDoc.createElement(child->title());
        parentDestination.appendChild(newel);

        if (child->isOpen()) {
            newel.setAttribute("Open", "true");
        }

        recurseCreateTOC(mainDoc, child, newel);
    }
}

static KAboutData createAboutData()
{
    KAboutData aboutData(
        "okular_mupdf",
        "okular_mupdf",
        ki18n("MuPDF Backend"),
        "0.1",
        ki18n("A PDF backend based on the MuPDF library"),
        KAboutData::License_GPL,
        ki18n("© 2008 Pino Toscano")
    );
    aboutData.addAuthor(ki18n("Pino Toscano"),
                        KLocalizedString(), "pino@kde.org");
    return aboutData;
}

OKULAR_EXPORT_PLUGIN(MuPDFGenerator, createAboutData())

MuPDFGenerator::MuPDFGenerator(QObject *parent, const QVariantList &args)
    : Generator(parent, args)
    , m_docInfo(0)
    , m_docSyn(0)
    , synctex_scanner(0)
{
    setFeature(Threaded);
    setFeature(TextExtraction);
}

MuPDFGenerator::~MuPDFGenerator()
{
}

#if OKULAR_IS_VERSION(0, 20, 0)
Okular::Document::OpenResult MuPDFGenerator::loadDocumentWithPassword(
    const QString &fileName, QVector<Okular::Page*> &pages,
    const QString &password)
{
    if (!m_pdfdoc.load(fileName))
        return Okular::Document::OpenError;
    if (m_pdfdoc.isLocked()) {
        m_pdfdoc.unlock(password.toLatin1());
        if (m_pdfdoc.isLocked()) {
            m_pdfdoc.close();
            return Okular::Document::OpenNeedsPassword;
        }
    }
    Q_ASSERT(!m_pdfdoc.isLocked());
    
    if (!m_pdfdoc.isLocked())
    {
        loadPages(pages);
        // no need to check for the existence of a synctex file, no parser will 
        // be created if none exists
        initSynctexParser(fileName);
        return Okular::Document::OpenSuccess;
    }
    else return Okular::Document::OpenError;
}
#else
bool MuPDFGenerator::loadDocument(const QString &filePath,
                                  QVector<Okular::Page*> &pages)
{
    if (!m_pdfdoc.load(filePath))
        return false;
    bool success = init(pages, filePath.section('/', -1, -1));
    if (success)
    {
        // no need to check for the existence of a synctex file, no parser will 
        // be created if none exists
        initSynctexParser(filePath);
    }
    return success;
}
    
bool MuPDFGenerator::init(QVector<Okular::Page *> &pages, const QString &wkey)
{
    // if the file didn't open correctly it might be encrypted, so ask for a pass
    bool firstInput = true;
    bool triedWallet = false;
    KWallet::Wallet *wallet = 0;
    bool keep = true;
    while (m_pdfdoc.isLocked()) {
        QString password;

        // 1.A. try to retrieve the first password from the kde wallet system
        if (!triedWallet && !wkey.isNull()) {
            QString walletName = KWallet::Wallet::NetworkWallet();
            WId parentwid = 0;
            if (document() && document()->widget()) {
                parentwid = document()->widget()->effectiveWinId();
            }
            wallet = KWallet::Wallet::openWallet(walletName, parentwid);
            if (wallet) {
                // use the Okular folder (and create if missing)
                if (!wallet->hasFolder("Okular")) {
                    wallet->createFolder("Okular");
                }
                wallet->setFolder("Okular");

                // look for the pass in that folder
                QString retrievedPass;
                if (!wallet->readPassword(wkey, retrievedPass)) {
                    password = retrievedPass;
                }
            }
            triedWallet = true;
        }

        // 1.B. if not retrieved, ask the password using the kde password dialog
        if (password.isNull()) {
            QString prompt;
            prompt = !firstInput ? i18n("Incorrect password. Try again:")
                   : i18n("Please insert the password to read the document:");
            firstInput = false;

            // if the user presses cancel, abort opening
            KPasswordDialog dlg(document()->widget(),
                                wallet ? KPasswordDialog::ShowKeepPassword
                                       : KPasswordDialog::KPasswordDialogFlags());
            dlg.setCaption(i18n("Document Password"));
            dlg.setPrompt(prompt);
            if (!dlg.exec())
                break;
            password = dlg.password();
            if (wallet)
                keep = dlg.keepPassword();
        }

        // 2. reopen the document using the password
        m_pdfdoc.unlock(password.toLatin1());

        // 3. if the password is correct and the user chose to remember it, store it to the wallet
        if (!m_pdfdoc.isLocked() && wallet
                && /*safety check*/ wallet->isOpen() && keep)
            wallet->writePassword(wkey, password);
    }
    if (m_pdfdoc.isLocked()) {
        m_pdfdoc.close();
        return false;
    }

    loadPages(pages);

    return true;
}
#endif

bool MuPDFGenerator::doCloseDocument()
{
    userMutex()->lock();
    m_pdfdoc.close();
    userMutex()->unlock();
    delete m_docInfo;
    m_docInfo = 0;
    delete m_docSyn;
    m_docSyn = 0;
    
    if ( synctex_scanner )
    {
        synctex_scanner_free( synctex_scanner );
        synctex_scanner = 0;
    }
    
    return true;
}

void MuPDFGenerator::loadPages(QVector<Okular::Page *> &pages)
{
    pages.resize(m_pdfdoc.pageCount());

    for (int i = 0; i < pages.count(); ++i) {
        QMuPDF::Page *page = m_pdfdoc.page(i);
        const QSizeF s = page->size();
        const Okular::Rotation rot = Okular::Rotation0;
        Okular::Page* new_ = new Okular::Page(i, s.width(), s.height(), rot);
        new_->setDuration(page->duration());
        pages[i] = new_;
        delete page;
    }
}

void MuPDFGenerator::initSynctexParser ( const QString& filePath )
{
    synctex_scanner = synctex_scanner_new_with_output_file( QFile::encodeName( 
    filePath ), 0, 1);
}

const Okular::DocumentInfo* MuPDFGenerator::generateDocumentInfo()
{
    if (m_docInfo)
        return m_docInfo;

    m_docInfo = new Okular::DocumentInfo();
    userMutex()->lock();

    m_docInfo->set(Okular::DocumentInfo::MimeType, "application/pdf");
    m_docInfo->set(Okular::DocumentInfo::Title, m_pdfdoc.infoKey("Title"));
    m_docInfo->set(Okular::DocumentInfo::Subject, m_pdfdoc.infoKey("Subject"));
    m_docInfo->set(Okular::DocumentInfo::Author, m_pdfdoc.infoKey("Author"));
    m_docInfo->set(Okular::DocumentInfo::Keywords, m_pdfdoc.infoKey("Keywords"));
    m_docInfo->set(Okular::DocumentInfo::Creator, m_pdfdoc.infoKey("Creator"));
    m_docInfo->set(Okular::DocumentInfo::Producer, m_pdfdoc.infoKey("Producer"));
    m_docInfo->set("format", i18nc("PDF v. <version>", "PDF v. %1",
                                   m_pdfdoc.pdfVersion()), i18n("Format"));
    m_docInfo->set(Okular::DocumentInfo::Pages,
                   QString::number(m_pdfdoc.pageCount()));

    userMutex()->unlock();

    return m_docInfo;
}

const Okular::DocumentSynopsis* MuPDFGenerator::generateDocumentSynopsis()
{
    if (m_docSyn)
        return m_docSyn;

    userMutex()->lock();
    QMuPDF::Outline* outline = m_pdfdoc.outline();
    userMutex()->unlock();
    if (!outline)
        return 0;

    m_docSyn = new Okular::DocumentSynopsis();
    recurseCreateTOC(*m_docSyn, outline, *m_docSyn);
    delete outline;

    return m_docSyn;
}

const Okular::SourceReference * MuPDFGenerator::dynamicSourceReference( int 
                                pageNr, double absX, double absY )
{
    if  ( !synctex_scanner )
        return 0;
    
    if (synctex_edit_query(synctex_scanner, pageNr + 1, absX * 96. / 
        dpi().width(), absY * 96. / dpi().height()) > 0)
    {
        synctex_node_t node;
        while ((node = synctex_next_result( synctex_scanner) ))
        {
            int line = synctex_node_line(node);
            int col = synctex_node_column(node);
            // column extraction does not seem to be implemented in synctex so 
            // far. set the SourceReference default value.
            if ( col == -1 )
            {
                col = 0;
            }
            const char *name = synctex_scanner_get_name( synctex_scanner, 
            synctex_node_tag( node ) );
            
            Okular::SourceReference * sourceRef = new Okular::SourceReference( 
            QFile::decodeName (name), line, col );
            return sourceRef;
        }
    }
    return 0;
}

QImage MuPDFGenerator::image(Okular::PixmapRequest *request)
{
    userMutex()->lock();
    QMuPDF::Page *page = m_pdfdoc.page(request->page()->number());
    QImage image = page->render(request->width(), request->height());
    userMutex()->unlock();
    delete page;
    return image;
}

void MuPDFGenerator::fillViewportFromSourceReference (Okular::DocumentViewport 
& viewport, const QString & reference ) const
{
    if ( !synctex_scanner )
        return;
    
    // The reference is of form "src:1111Filename", where "1111"
    // points to line number 1111 in the file "Filename".
    // Extract the file name and the numeral part from the reference string.
    // This will fail if Filename starts with a digit.
    QString name, lineString;
    // Remove "src:". Presence of substring has been checked before this
    // function is called.
    name = reference.mid( 4 );
    // split
    int nameLength = name.length();
    int i = 0;
    for( i = 0; i < nameLength; ++i )
    {
        if ( !name[i].isDigit() ) break;
    }
    lineString = name.left( i );
    name = name.mid( i );
    // Remove spaces.
    name = name.trimmed();
    lineString = lineString.trimmed();
    // Convert line to integer.
    bool ok;
    int line = lineString.toInt( &ok );
    if (!ok) line = -1;
    
    // Use column == -1 for now.
    if( synctex_display_query( synctex_scanner, QFile::encodeName(name), line, 
        -1 ) > 0 )
    {
        synctex_node_t node;
        // For now use the first hit. Could possibly be made smarter
        // in case there are multiple hits.
        while( ( node = synctex_next_result( synctex_scanner ) ) )
        {
            // TeX pages start at 1.
            viewport.pageNumber = synctex_node_page( node ) - 1;
            
            if ( !viewport.isValid() ) return;
            
            // TeX small points ...
            double px = (synctex_node_visible_h( node ) * dpi().width()) / 
            96;
            double py = (synctex_node_visible_v( node ) * dpi().height()) / 
            96;
            viewport.rePos.normalizedX = px / 
            document()->page(viewport.pageNumber)->width();
            viewport.rePos.normalizedY = ( py + 0.5 ) / 
            document()->page(viewport.pageNumber)->height();
            viewport.rePos.enabled = true;
            viewport.rePos.pos = Okular::DocumentViewport::Center;

            return;
        }
    }
}

Okular::TextPage* MuPDFGenerator::textPage(Okular::Page *page)
{
    userMutex()->lock();
    QMuPDF::Page *mp = m_pdfdoc.page(page->number());
    const QVector<QMuPDF::TextBox *> boxes = mp->textBoxes();
    const QSizeF s = mp->size();
    userMutex()->unlock();
    delete mp;

    Okular::TextPage *tp = buildTextPage(boxes, s.width(), s.height());
    qDeleteAll(boxes);
    return tp;
}

QVariant MuPDFGenerator::metaData(const QString &key,
                                  const QVariant &option) const
{
    Q_UNUSED(option)
    if ( key == "NamedViewport" && !option.toString().isEmpty() )
    {
        Okular::DocumentViewport viewport;
        QString optionString = option.toString();
        
        // if option starts with "src:" assume that we are handling a
        // source reference
        if ( optionString.startsWith( "src:", Qt::CaseInsensitive ) )
        {
            fillViewportFromSourceReference( viewport, optionString );
        }
        if ( viewport.pageNumber >= 0 )
            return viewport.toString();
    }
    else if (key == QLatin1String("DocumentTitle")) {
        userMutex()->lock();
        const QString title = m_pdfdoc.infoKey("Title");
        userMutex()->unlock();
        return title;
    } else if (key == QLatin1String("StartFullScreen")) {
        if (m_pdfdoc.pageMode() == QMuPDF::Document::FullScreen)
            return true;
    } else if (key == QLatin1String("OpenTOC")) {
        if (m_pdfdoc.pageMode() == QMuPDF::Document::UseOutlines)
            return true;
    }
    return QVariant();
}

#include "generator_mupdf.moc"
