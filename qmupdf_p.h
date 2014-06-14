/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef QMUPDF_P_H
#define QMUPDF_P_H

#include <QtCore/QList>
#include <QtCore/QRect>

extern "C"
{
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <mupdf/pdf/xref.h>
}

namespace QMuPDF {

class Outline;

class DocumentPrivate {
public:
    DocumentPrivate();

    bool load();
    void loadInfoDict();
    void convertOutline(fz_outline *out, Outline *item);

    fz_context *ctxt;
    fz_document *mdoc;
    fz_stream *stream;
    int pageCount;
    pdf_obj *info;
    int pageMode;
    bool locked : 1;
private:
    pdf_obj *trailer() const { return pdf_trailer(reinterpret_cast<pdf_document*>(mdoc)); }
};

class PagePrivate {
public:
    PagePrivate();
    int pageNum;
    DocumentPrivate *doc;
    fz_page *page;
};

class TextBoxPrivate {
public:
    TextBoxPrivate();
    QRect rect;
    QChar text;
    bool atEOL : 1;
};

class OutlinePrivate {
public:
    OutlinePrivate();

    QString title;
    QList<Outline *> children;
    bool open : 1;
};

}

#endif
