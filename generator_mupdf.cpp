/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "generator_mupdf.h"

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

static Okular::TextPage* buildTextPage(const QList<QMuPDF::TextBox *> &boxes, qreal width, qreal height)
{
    Okular::TextPage *ktp = new Okular::TextPage();

    Q_FOREACH (QMuPDF::TextBox *box, boxes) {
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

static void recurseCreateTOC(QDomDocument &maindoc, QMuPDF::Outline *outline, QDomNode &parentDestination)
{
    foreach (QMuPDF::Outline *child, outline->children()) {
        QDomElement newel = maindoc.createElement(child->title());
        parentDestination.appendChild(newel);

        if (child->isOpen()) {
            newel.setAttribute("Open", "true");
        }

        recurseCreateTOC(maindoc, child, newel);
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
    aboutData.addAuthor(ki18n("Pino Toscano"), KLocalizedString(), "pino@kde.org");
    return aboutData;
}

OKULAR_EXPORT_PLUGIN(MuPDFGenerator, createAboutData())

MuPDFGenerator::MuPDFGenerator(QObject *parent, const QVariantList &args)
    : Generator(parent, args)
    , m_docInfo(0)
    , m_docSyn(0)
{
    setFeature(Threaded);
    setFeature(TextExtraction);
}

MuPDFGenerator::~MuPDFGenerator()
{
}

bool MuPDFGenerator::loadDocument(const QString &filePath, QVector<Okular::Page *> &pagesVector)
{
    if (!m_pdfdoc.load(filePath)) {
        return false;
    }
    return init(pagesVector, filePath.section('/', -1, -1));
}

bool MuPDFGenerator::doCloseDocument()
{
    userMutex()->lock();
    m_pdfdoc.close();
    userMutex()->unlock();
    delete m_docInfo;
    m_docInfo = 0;
    delete m_docSyn;
    m_docSyn = 0;

    return true;
}

bool MuPDFGenerator::init(QVector<Okular::Page *> &pagesVector, const QString &walletKey)
{
    // if the file didn't open correctly it might be encrypted, so ask for a pass
    bool firstInput = true;
    bool triedWallet = false;
    KWallet::Wallet *wallet = 0;
    bool keep = true;
    while (m_pdfdoc.isLocked()) {
        QString password;

        // 1.A. try to retrieve the first password from the kde wallet system
        if (!triedWallet && !walletKey.isNull()) {
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
                if (!wallet->readPassword(walletKey, retrievedPass)) {
                    password = retrievedPass;
                }
            }
            triedWallet = true;
        }

        // 1.B. if not retrieved, ask the password using the kde password dialog
        if (password.isNull()) {
            QString prompt;
            if (firstInput) {
                prompt = i18n("Please insert the password to read the document:");
            } else {
                prompt = i18n("Incorrect password. Try again:");
            }
            firstInput = false;

            // if the user presses cancel, abort opening
            KPasswordDialog dlg(document()->widget(), wallet ? KPasswordDialog::ShowKeepPassword : KPasswordDialog::KPasswordDialogFlags());
            dlg.setCaption(i18n("Document Password"));
            dlg.setPrompt(prompt);
            if (!dlg.exec()) {
                break;
            }
            password = dlg.password();
            if (wallet) {
                keep = dlg.keepPassword();
            }
        }

        // 2. reopen the document using the password
        m_pdfdoc.unlock(password.toLatin1());

        // 3. if the password is correct and the user chose to remember it, store it to the wallet
        if (!m_pdfdoc.isLocked() && wallet && /*safety check*/ wallet->isOpen() && keep) {
            wallet->writePassword(walletKey, password);
        }
    }
    if (m_pdfdoc.isLocked()) {
        m_pdfdoc.close();
        return false;
    }

    loadPages(pagesVector);

    return true;
}

void MuPDFGenerator::loadPages(QVector<Okular::Page *> &pagesVector)
{
    pagesVector.resize(m_pdfdoc.pageCount());

    for (int i = 0; i < pagesVector.count(); ++i) {
        QMuPDF::Page *page = m_pdfdoc.page(i);
        const QSizeF size = page->size();
        Okular::Rotation rot = Okular::Rotation0;

        Okular::Page* newpage = new Okular::Page(i, size.width(), size.height(), rot);
        newpage->setDuration(page->duration());
        pagesVector[i] = newpage;
        delete page;
    }
}

const Okular::DocumentInfo* MuPDFGenerator::generateDocumentInfo()
{
    if (m_docInfo) {
        return m_docInfo;
    }

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
    m_docInfo->set(Okular::DocumentInfo::Pages, QString::number(m_pdfdoc.pageCount()));

    userMutex()->unlock();

    return m_docInfo;
}

const Okular::DocumentSynopsis* MuPDFGenerator::generateDocumentSynopsis()
{
    if (m_docSyn) {
        return m_docSyn;
    }

    userMutex()->lock();
    QMuPDF::Outline* outline = m_pdfdoc.outline();
    userMutex()->unlock();
    if (!outline) {
        return NULL;
    }

    m_docSyn = new Okular::DocumentSynopsis();
    recurseCreateTOC(*m_docSyn, outline, *m_docSyn);
    delete outline;

    return m_docSyn;
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

Okular::TextPage* MuPDFGenerator::textPage(Okular::Page *page)
{
    userMutex()->lock();
    QMuPDF::Page *mp = m_pdfdoc.page(page->number());
    QList<QMuPDF::TextBox *> boxes = mp->textBoxes();
    const QSizeF s = mp->size();
    userMutex()->unlock();
    delete mp;

    Okular::TextPage *tp = buildTextPage(boxes, s.width(), s.height());
    qDeleteAll(boxes);
    return tp;
}

QVariant MuPDFGenerator::metaData(const QString &key, const QVariant &option) const
{
    Q_UNUSED(option)
    if (key == QLatin1String("DocumentTitle"))
    {
        userMutex()->lock();
        const QString title = m_pdfdoc.infoKey("Title");
        userMutex()->unlock();
        return title;
    } else if (key == QLatin1String("StartFullScreen")) {
        if (m_pdfdoc.pageMode() == QMuPDF::Document::FullScreen) {
            return true;
        }
    } else if (key == QLatin1String("OpenTOC")) {
        if (m_pdfdoc.pageMode() == QMuPDF::Document::UseOutlines) {
            return true;
        }
    }
    return QVariant();
}

#include "generator_mupdf.moc"
