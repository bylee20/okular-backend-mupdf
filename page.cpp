/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "page.hpp"
#include <QtGui/QImage>
extern "C" {
#include <mupdf/fitz.h>
}

namespace QMuPDF {

QRectF convert_fz_rect(const fz_rect &rect)
{
    return QRectF(QPointF(rect.x0, rect.y0), QPointF(rect.x1, rect.y1));
}

QImage convert_fz_pixmap(fz_context *ctx, fz_pixmap *image)
{
    const int w = fz_pixmap_width(ctx, image);
    const int h = fz_pixmap_height(ctx, image);
    QImage img(w, h, QImage::Format_ARGB32);
    unsigned char *data = fz_pixmap_samples(ctx, image);
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

struct Page::Data {
    Data(): pageNum(-1), doc(0), page(0) { }
    int pageNum;
    fz_document *doc;
    fz_context *ctx;
    fz_page *page;
};

Page::Page()
    : d(new Data)
{
}

Page::~Page()
{
    fz_free_page(d->doc, d->page);
    delete d;
}

Page *Page::make(fz_document_s *doc, fz_context_s *ctx, int num)
{
    Q_ASSERT(doc && ctx);
    fz_page *page = fz_load_page(doc, num);
    if (!page)
        return 0;
    Page *p = new Page();
    p->d->pageNum = num;
    p->d->doc = doc;
    p->d->ctx = ctx;
    p->d->page = page;
    return p;
}


int Page::number() const
{
    return d->pageNum;
}

QSizeF Page::size() const
{
    fz_rect rect;
    fz_bound_page(d->doc, d->page, &rect);
    return QSizeF(rect.x1 - rect.x0, rect.y1 - rect.y0);
}

qreal Page::duration() const
{
    float val;
    (void)fz_page_presentation(d->doc, d->page, &val);
    return val < 0.1 ? -1 : val;
}

QImage Page::render(qreal width, qreal height) const
{
    const QSizeF s = size();

    fz_matrix ctm;
    fz_scale(&ctm, width / s.width(), height / s.height());

    fz_cookie cookie = { 0, 0, 0, 0, 0, 0 };
    fz_colorspace *csp = fz_device_rgb(d->ctx);
    fz_pixmap *image = fz_new_pixmap(d->ctx, csp, width, height);
    fz_clear_pixmap_with_value(d->ctx, image, 0xff);
    fz_device *device = fz_new_draw_device(d->ctx, image);
    fz_run_page(d->doc, d->page, device, &ctm, &cookie);
    fz_free_device(device);

    QImage img;
    if (!cookie.errors)
        img = convert_fz_pixmap(d->ctx, image);
    fz_drop_pixmap(d->ctx, image);
    return img;
}

QVector<TextBox *> Page::textBoxes() const
{
    fz_cookie cookie = { 0, 0, 0, 0, 0, 0 };
    fz_text_page *page = fz_new_text_page(d->ctx);
    fz_text_sheet *sheet = fz_new_text_sheet(d->ctx);
    fz_device *device = fz_new_text_device(d->ctx, sheet, page);
    fz_run_page(d->doc, d->page, device, &fz_identity, &cookie);
    fz_free_device(device);
    if (cookie.errors) {
        fz_free_text_page(d->ctx, page);
        fz_free_text_sheet(d->ctx, sheet);
        return QVector<TextBox *>();
    }

    QVector<TextBox *> boxes;

    for (int i_block = 0; i_block < page->len; ++i_block) {
        if (page->blocks[i_block].type != FZ_PAGE_BLOCK_TEXT)
            continue;
        fz_text_block &block = *page->blocks[i_block].u.text;
        for (int i_line = 0; i_line < block.len; ++i_line) {
            fz_text_line &line = block.lines[i_line];
            bool hasText = false;
            for (fz_text_span *s = line.first_span; s; s = s->next) {
                fz_text_span &span = *s;
                for (int i_char = 0; i_char < span.len; ++i_char) {
                    fz_rect bbox; fz_text_char_bbox(&bbox, s, i_char);
                    const int text = span.text[i_char].c;
                    TextBox *box = new TextBox(text, convert_fz_rect(bbox));
                    boxes.append(box);
                    hasText = true;
                }
            }
            if (hasText)
                boxes.back()->markAtEndOfLine();
        }
    }

    fz_free_text_page(d->ctx, page);
    fz_free_text_sheet(d->ctx, sheet);

    return boxes;
}

}
