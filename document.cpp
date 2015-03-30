/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "document.hpp"
#include "page.hpp"
#include <QtCore/QFile>
#include <cstring>
extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

namespace QMuPDF {

QRectF convert_fz_rect(const fz_rect &rect, const QSizeF &dpi);

struct Document::Data {
    Data()
        : ctx(fz_new_context(NULL, NULL, FZ_STORE_DEFAULT))
        , mdoc(0), stream(0), pageCount(0), info(0)
        , pageMode(Document::UseNone), locked(false) { }

    fz_context *ctx;
    fz_document *mdoc;
    fz_stream *stream;
    int pageCount;
    pdf_obj *info;
    PageMode pageMode;
    bool locked;

    pdf_document *pdf() const { return reinterpret_cast<pdf_document*>(mdoc); }
    pdf_obj *dict(const char *key) const
        { return pdf_dict_gets(pdf_trailer(pdf()), key); }
    void loadInfoDict() { if (!info) info = dict("Info"); }
    bool load()
    {
        pdf_obj *root = dict("Root");
        if (!root)
            return false;

        pageCount = fz_count_pages(mdoc);
        pdf_obj *obj = pdf_dict_gets(root, "PageMode");
        if (obj && pdf_is_name(obj)) {
            const char* mode = pdf_to_name(obj);
            if (!std::strcmp(mode, "UseNone"))
                pageMode = Document::UseNone;
            else if (!std::strcmp(mode, "UseOutlines"))
                pageMode = Document::UseOutlines;
            else if (!std::strcmp(mode, "UseThumbs"))
                pageMode = Document::UseThumbs;
            else if (!std::strcmp(mode, "FullScreen"))
                pageMode = Document::FullScreen;
            else if (!std::strcmp(mode, "UseOC"))
                pageMode = Document::UseOC;
            else if (!std::strcmp(mode, "UseAttachments"))
                pageMode = Document::UseAttachments;
        }
        return true;
    }
    void convertOutline(fz_outline *out, Outline *item)
    {
        for (; out; out = out->next) {
            Outline *child = new Outline(out);
            item->appendChild(child);
            convertOutline(out->down, child);
        }
    }
};

Document::Document()
    : d(new Data)
{
    fz_register_document_handler(d->ctx, &pdf_document_handler);
}

Document::~Document()
{
    close();
    fz_free_context(d->ctx);
    delete d;
}

bool Document::load(const QString &fileName)
{
    QByteArray fileData = QFile::encodeName(fileName);
    d->stream = fz_open_file(d->ctx, fileData.constData());
    if (!d->stream)
        return false;
    char *oldlocale = std::setlocale(LC_NUMERIC, "C");
    d->mdoc = fz_open_document_with_stream(d->ctx, "pdf", d->stream);
    if (oldlocale)
        std::setlocale(LC_NUMERIC, oldlocale);
    if (!d->mdoc)
        return false;

    d->locked = fz_needs_password(d->mdoc);

    if (!d->locked) {
        if (!d->load())
            return false;
    }

    return true;
}

void Document::close()
{
    if (!d->mdoc)
        return;

    fz_close_document(d->mdoc);
    d->mdoc = 0;
    fz_close(d->stream);
    d->stream = 0;
    d->pageCount = 0;
    d->info = 0;
    d->pageMode = UseNone;
    d->locked = false;
}

bool Document::isLocked() const
{
    return d->locked;
}

bool Document::unlock(const QByteArray &password)
{
    if (!d->locked)
        return false;

    QByteArray a = password;
    if (!fz_authenticate_password(d->mdoc, a.data()))
        return false;

    d->locked = false;
    if (!d->load())
        return false;

    return true;
}

int Document::pageCount() const
{
    return d->pageCount;
}

Page* Document::page(int pageno) const
{
    if (d->mdoc && 0 <= pageno && pageno < d->pageCount)
        return Page::make(d->mdoc, d->ctx, pageno);
    return 0;
}

QList<QByteArray> Document::infoKeys() const
{
    QList<QByteArray> keys;
    if (!d->mdoc)
        return keys;

    d->loadInfoDict();

    if (!d->info)
        return keys;

    const int dictSize = pdf_dict_len(d->info);
    for (int i = 0; i < dictSize; ++i) {
        pdf_obj *obj = pdf_dict_get_key(d->info, i);
        if (pdf_is_name(obj))
            keys.append(QByteArray(pdf_to_name(obj)));
    }
    return keys;
}

QString Document::infoKey(const QByteArray &key) const
{
    if (!d->mdoc)
        return QString();

    d->loadInfoDict();

    if (!d->info)
        return QString();

    pdf_obj *obj = pdf_dict_gets(d->info, key.constData());
    if (obj) {
        obj = pdf_resolve_indirect(obj);
        char *value = pdf_to_utf8(d->pdf(), obj);
        if (value) {
            const QString res = QString::fromUtf8(value);
            fz_free(d->ctx, value);
            return res;
        }
    }
    return QString();
}

Outline* Document::outline() const
{
    fz_outline *out = fz_load_outline(d->mdoc);
    if (!out)
        return 0;

    Outline *item = new Outline;
    d->convertOutline(out, item);

    fz_free_outline(d->ctx, out);

    return item;
}

float Document::pdfVersion() const
{
    if (!d->mdoc)
        return 0.0f;
    char buf[64];
    if (fz_meta(d->mdoc, FZ_META_FORMAT_INFO, buf, sizeof(buf)) == FZ_META_OK) {
        int major, minor;
        if (sscanf(buf, "PDF %d.%d", &major, &minor) == 2)
            return float(major + minor / 10.0);
    }
    return 0.0f;
}

Document::PageMode Document::pageMode() const
{
    return d->pageMode;
}

/******************************************************************************/

Outline::Outline(const fz_outline *out)
{
    if (out->title)
        m_title = QString::fromUtf8(out->title);
    m_link = LinkDest::create(&out->dest);
}

Outline::~Outline()
{
    qDeleteAll(m_children);
    delete m_link;
}

LinkDest *LinkDest::create(const fz_link_dest *dest)
{
    if (!dest)
        return 0;
    switch (dest->kind) {
    case FZ_LINK_GOTO:
        return new GotoDest(dest);
    case FZ_LINK_GOTOR:
        return new ExternalDest(dest);
    case FZ_LINK_LAUNCH:
        return new LaunchDest(dest);
    case FZ_LINK_NAMED:
        return new NamedDest(dest);
    case FZ_LINK_URI:
        return new UrlDest(dest);
    default:
        return 0;
    }
}

}
