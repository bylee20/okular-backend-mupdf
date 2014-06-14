/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "qmupdf.h"
#include "qmupdf_p.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtGui/QImage>

#include <clocale>
#include <cstring>

namespace
{

QImage convert_fz_pixmap(fz_context *ctxt, fz_pixmap *image)
{
    const int w = fz_pixmap_width(ctxt, image);
    const int h = fz_pixmap_height(ctxt, image);
    QImage img(w, h, QImage::Format_ARGB32);
    unsigned char *data = fz_pixmap_samples(ctxt, image);
    unsigned int *imgdata = (unsigned int *)img.bits();
    for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
            *imgdata = qRgba(data[0], data[1], data[2], data[3]);
            data = data + 4;
            imgdata++;
        }
    }
    return img;
}

QRectF convert_fz_rect(const fz_rect &rect)
{
    return QRectF(QPointF(rect.x0, rect.y0), QPointF(rect.x1, rect.y1));
}

}

using namespace QMuPDF;

DocumentPrivate::DocumentPrivate()
    : ctxt(fz_new_context(NULL, NULL, FZ_STORE_DEFAULT))
    , xref(0)
    , stream(0)
    , pageCount(0)
    , info(0)
    , pageMode(Document::UseNone)
    , locked(false)
{
}

bool DocumentPrivate::load()
{
    pdf_obj *obj = 0;
    pdf_obj *root;

    root = pdf_dict_gets(xref->trailer, "Root");
    if (!root) {
        return false;
    }

    pageCount = pdf_count_pages(xref);

    obj = pdf_dict_gets(root, "PageMode");
    if (obj && pdf_is_name(obj)) {
        const char* mode = pdf_to_name(obj);
        if (!std::strcmp(mode, "UseNone")) {
            pageMode = Document::UseNone;
        } else if (!std::strcmp(mode, "UseOutlines")) {
            pageMode = Document::UseOutlines;
        } else if (!std::strcmp(mode, "UseThumbs")) {
            pageMode = Document::UseThumbs;
        } else if (!std::strcmp(mode, "FullScreen")) {
            pageMode = Document::FullScreen;
        } else if (!std::strcmp(mode, "UseOC")) {
            pageMode = Document::UseOC;
        } else if (!std::strcmp(mode, "UseAttachments")) {
            pageMode = Document::UseAttachments;
        }
    }

    return true;
}

void DocumentPrivate::loadInfoDict()
{
    if (info) {
        return;
    }

    info = pdf_dict_gets(xref->trailer, "Info");
}

void DocumentPrivate::convertOutline(fz_outline *out, Outline *item)
{
    for (; out; out = out->next) {
        Outline *child = new Outline();
        if (out->title) {
            child->d->title = QString::fromUtf8(out->title);
        }
        item->d->children.append(child);
        convertOutline(out->down, child);
    }
}


PagePrivate::PagePrivate()
    : pageNum(-1), doc(0), page(0)
{
}


TextBoxPrivate::TextBoxPrivate()
    : atEOL(false)
{
}


OutlinePrivate::OutlinePrivate()
    : open(false)
{
}


Document::Document()
    : d(new DocumentPrivate)
{
}

Document::~Document()
{
    close();

    fz_free_context(d->ctxt);

    delete d;
}

bool Document::load(const QString &fileName)
{
    QByteArray fileData = QFile::encodeName(fileName);
    d->stream = fz_open_file(d->ctxt, fileData.constData());
    if (!d->stream) {
        return false;
    }
    char *oldlocale = std::setlocale(LC_NUMERIC, "C");
    d->xref = pdf_open_document_with_stream(d->ctxt, d->stream);
    if (oldlocale) {
        std::setlocale(LC_NUMERIC, oldlocale);
    }
    if (!d->xref) {
        return false;
    }

    d->locked = pdf_needs_password(d->xref);

    if (!d->locked) {
        if (!d->load()) {
            return false;
        }
    }

    return true;
}

void Document::close()
{
    if (!d->xref) {
        return;
    }

    pdf_close_document(d->xref);
    d->xref = 0;
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
    if (!d->locked) {
        return false;
    }

    QByteArray a = password;
    if (!pdf_authenticate_password(d->xref, a.data())) {
        return false;
    }

    d->locked = false;
    if (!d->load()) {
        return false;
    }

    return true;
}

int Document::pageCount() const
{
    return d->pageCount;
}

Page* Document::page(int pageno) const
{
    if (!d->xref || pageno < 0 || pageno >= d->pageCount) {
        return 0;
    }

    pdf_page *page = pdf_load_page(d->xref, pageno);
    if (!page) {
        return 0;
    }

    Page *p = new Page();
    p->d->pageNum = pageno;
    p->d->doc = d;
    p->d->page = page;
    return p;
}

QList<QByteArray> Document::infoKeys() const
{
    QList<QByteArray> keys;
    if (!d->xref) {
        return keys;
    }

    d->loadInfoDict();

    if (!d->info) {
        return keys;
    }

    const int dictSize = pdf_dict_len(d->info);
    for (int i = 0; i < dictSize; ++i) {
        pdf_obj *obj = pdf_dict_get_key(d->info, i);
        if (pdf_is_name(obj)) {
            keys.append(QByteArray(pdf_to_name(obj)));
        }
    }
    return keys;
}

QString Document::infoKey(const QByteArray &key) const
{
    if (!d->xref) {
        return QString();
    }

    d->loadInfoDict();

    if (!d->info) {
        return QString();
    }

    pdf_obj *obj = pdf_dict_gets(d->info, key.constData());
    if (obj) {
        obj = pdf_resolve_indirect(obj);
        char *value = pdf_to_utf8(d->xref, obj);
        if (value) {
            const QString res = QString::fromUtf8(value);
            fz_free(d->ctxt, value);
            return res;
        }
    }
    return QString();
}

Outline* Document::outline() const
{
    fz_outline *out = pdf_load_outline(d->xref);
    if (!out) {
        return 0;
    }

    Outline *item = new Outline;
    d->convertOutline(out, item);

    fz_free_outline(d->ctxt, out);

    return item;
}

float Document::pdfVersion() const
{
    char buf[64];
    if (d->xref && pdf_meta(d->xref, FZ_META_FORMAT_INFO, buf, sizeof(buf)) == FZ_META_OK) {
        int major, minor;
        if (sscanf(buf, "PDF %d.%d", &major, &minor) == 2) {
            return float(major + minor / 10.0);
        }
    }
    return 0.0f;
}

Document::PageMode Document::pageMode() const
{
    return static_cast<Document::PageMode>(d->pageMode);
}


Page::Page()
    : d(new PagePrivate())
{
}

Page::~Page()
{
    pdf_free_page(d->doc->xref, d->page);

    delete d;
}

int Page::number() const
{
    return d->pageNum;
}

QSizeF Page::size() const
{
    return convert_fz_rect(pdf_bound_page(d->doc->xref, d->page)).size();
}

qreal Page::duration() const
{
    float val;
    (void)pdf_page_presentation(d->doc->xref, d->page, &val);
    return val < 0.1 ? -1 : val;
}

QImage Page::render(qreal width, qreal height) const
{
    const QSizeF s = size();

    fz_matrix ctm;
    ctm = fz_identity;
    ctm = fz_concat(ctm, fz_scale(width / s.width(), height / s.height()));

    fz_cookie cookie = { 0, 0, 0, 0 };
    fz_pixmap *image = fz_new_pixmap(d->doc->ctxt, fz_device_rgb, width, height);
    fz_clear_pixmap_with_value(d->doc->ctxt, image, 0xff);
    fz_device *device = fz_new_draw_device(d->doc->ctxt, image);
    pdf_run_page(d->doc->xref, d->page, device, ctm, &cookie);
    fz_free_device(device);

    QImage img;
    if (!cookie.errors) {
        img = convert_fz_pixmap(d->doc->ctxt, image);
    }

    fz_drop_pixmap(d->doc->ctxt, image);

    return img;
}

QList<TextBox *> Page::textBoxes() const
{
    fz_matrix ctm;
    ctm = fz_identity;
    fz_cookie cookie = { 0, 0, 0, 0 };
    fz_text_page *page = fz_new_text_page(d->doc->ctxt, pdf_bound_page(d->doc->xref, d->page));
    fz_text_sheet *sheet = fz_new_text_sheet(d->doc->ctxt);
    fz_device *device = fz_new_text_device(d->doc->ctxt, sheet, page);
    pdf_run_page(d->doc->xref, d->page, device, ctm, &cookie);
    fz_free_device(device);
    if (cookie.errors) {
        fz_free_text_page(d->doc->ctxt, page);
        fz_free_text_sheet(d->doc->ctxt, sheet);
        return QList<TextBox *>();
    }

    QList<TextBox *> boxes;

    for (int i_block = 0; i_block < page->len; ++i_block) {
        fz_text_block &block = page->blocks[i_block];
        for (int i_line = 0; i_line < block.len; ++i_line) {
            fz_text_line &line = block.lines[i_line];
            bool hastext = false;
            for (int i_span = 0; i_span < line.len; ++i_span) {
                fz_text_span &span = line.spans[i_span];
                for (int i_char = 0; i_char < span.len; ++i_char) {
                    fz_text_char &tc = span.text[i_char];
                    TextBox *box = new TextBox();
                    box->d->rect = convert_fz_rect(tc.bbox).toRect();
                    box->d->text = QChar(tc.c);
                    boxes.append(box);
                    hastext = true;
                }
            }
            if (hastext) {
                boxes.last()->d->atEOL = true;
            }
        }
    }

    fz_free_text_page(d->doc->ctxt, page);
    fz_free_text_sheet(d->doc->ctxt, sheet);

    return boxes;
}


TextBox::TextBox()
    : d(new TextBoxPrivate())
{
}

TextBox::~TextBox()
{
    delete d;
}

QRect TextBox::rect() const
{
    return d->rect;
}

QChar TextBox::text() const
{
    return d->text;
}

bool TextBox::isAtEndOfLine() const
{
    return d->atEOL;
}

Outline::Outline()
    : d(new OutlinePrivate())
{
}

Outline::~Outline()
{
    qDeleteAll(d->children);

    delete d;
}

QString Outline::title() const
{
    return d->title;
}

bool Outline::isOpen() const
{
    return d->open;
}

QList<Outline *> Outline::children() const
{
    return d->children;
}

