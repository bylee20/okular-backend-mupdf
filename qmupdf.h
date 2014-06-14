/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef QMUPDF_H
#define QMUPDF_H

#include <QtCore/QList>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>

class QImage;
class QStringList;

namespace QMuPDF {

class DocumentPrivate;
class Page;
class PagePrivate;
class TextBox;
class TextBoxPrivate;
class Outline;
class OutlinePrivate;

class Document {
public:
    enum PageMode {
        UseNone,
        UseOutlines,
        UseThumbs,
        FullScreen,
        UseOC,
        UseAttachments
    };
    Document();
    ~Document();
    bool load(const QString &fileName);
    void close();
    bool isLocked() const;
    bool unlock(const QByteArray &password);
    int pageCount() const;
    Page* page(int page) const;
    QList<QByteArray> infoKeys() const;
    QString infoKey(const QByteArray &key) const;
    Outline* outline() const;
    float pdfVersion() const;
    PageMode pageMode() const;
private:
    Q_DISABLE_COPY(Document)
    DocumentPrivate* d;
};

class Page {
    friend class Document;
public:
    ~Page();
    int number() const;
    QSizeF size() const;
    qreal duration() const;
    QImage render(qreal width, qreal height) const;
    QList<TextBox *> textBoxes() const;
private:
    Page();
    Q_DISABLE_COPY(Page)
    PagePrivate* d;
};

class TextBox {
    friend class Page;
public:
    ~TextBox();
    QRect rect() const;
    QChar text() const;
    bool isAtEndOfLine() const;
private:
    TextBox();
    Q_DISABLE_COPY(TextBox)
    TextBoxPrivate* d;
};

class Outline {
    friend class Document;
    friend class DocumentPrivate;
public:
    ~Outline();
    QString title() const;
    bool isOpen() const;
    QList<Outline *> children() const;
private:
    Outline();
    Q_DISABLE_COPY(Outline)
    OutlinePrivate* d;
};

}

#endif
