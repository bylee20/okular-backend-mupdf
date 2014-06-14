/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef QMUPDF_PAGE_HPP
#define QMUPDF_PAGE_HPP

#include <QtCore/QString>
#include <QtCore/QRect>

class QImage;                           class QSizeF;
struct fz_document_s;                   struct fz_context_s;

namespace QMuPDF {

class TextBox;

class Page {
public:
    ~Page();
    int number() const;
    QSizeF size() const;
    qreal duration() const;
    QImage render(qreal width, qreal height) const;
    QVector<TextBox *> textBoxes() const;
    static Page *make(fz_document_s *doc, fz_context_s *ctx, int num);
private:
    Page();
    Q_DISABLE_COPY(Page)
    struct Data;
    Data *d;
};

class TextBox {
public:
    TextBox(QChar c, const QRect &bbox)
        : m_text(c), m_rect(bbox), m_end(false) { }
    QRect rect() const { return m_rect; }
    QChar text() const { return m_text; }
    bool isAtEndOfLine() const { return m_end; }
    void markAtEndOfLine() { m_end = true; }
private:
    QChar m_text;
    QRect m_rect;
    bool m_end : 1;
};

}

#endif
