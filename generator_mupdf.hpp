/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef GENERATOR_MUPDF_H
#define GENERATOR_MUPDF_H

#include "synctex/synctex_parser.h"

#include <okular/core/document.h>
#include <okular/core/generator.h>
#include <okular/core/sourcereference.h>
#include <okular/core/version.h>
#include <qfile.h>

#include "document.hpp"

class MuPDFGenerator : public Okular::Generator {
    Q_OBJECT
public:
    MuPDFGenerator(QObject *parent, const QVariantList &args);
    virtual ~MuPDFGenerator();
#if OKULAR_IS_VERSION(0, 20, 0)
    Okular::Document::OpenResult loadDocumentWithPassword(
        const QString &fileName, QVector<Okular::Page *> &pages,
        const QString &password);
#else
    bool loadDocument(const QString &fileName, QVector<Okular::Page*> &pages);
#endif
    const Okular::DocumentInfo *generateDocumentInfo();
    const Okular::DocumentSynopsis *generateDocumentSynopsis();
    QVariant metaData(const QString &key, const QVariant &option) const;
protected:
    bool doCloseDocument();
    QImage image(Okular::PixmapRequest *page);
    Okular::TextPage* textPage(Okular::Page *page);
protected slots:
    const Okular::SourceReference * dynamicSourceReference( int pageNr, double 
          absX, double absY );
    
private:
    bool init(QVector<Okular::Page*> &pages, const QString &walletKey);
    void loadPages(QVector<Okular::Page*> &pages);
    void initSynctexParser( const QString& filePath );
    void fillViewportFromSourceReference( Okular::DocumentViewport & viewport, 
         const QString & reference ) const;
    QMuPDF::Document m_pdfdoc;
    Okular::DocumentInfo *m_docInfo;
    Okular::DocumentSynopsis *m_docSyn;
    
    synctex_scanner_t synctex_scanner;
};

#endif
