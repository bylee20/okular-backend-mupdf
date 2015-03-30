/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef QMUPDF_DOCUMENT_HPP
#define QMUPDF_DOCUMENT_HPP

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtAlgorithms>
#include <okular/core/document.h>
extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

namespace QMuPDF {

class Page;                             class Outline;

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
    Page *page(int page) const;
    QList<QByteArray> infoKeys() const;
    QString infoKey(const QByteArray &key) const;
    Outline *outline() const;
    float pdfVersion() const;
    PageMode pageMode() const;
private:
    Q_DISABLE_COPY(Document)
    struct Data;
    Data *d;
};

class LinkDest;

class Outline {
public:
    Outline(const fz_outline *fz);
    Outline(): m_open(false), m_link(0) { }
    ~Outline();
    QString title() const { return m_title; }
    bool isOpen() const { return m_open; }
    QVector<Outline*> children() const { return m_children; }
    void appendChild(Outline *child) { m_children.push_back(child); }
    LinkDest *link() const { return m_link; }
private:
    Q_DISABLE_COPY(Outline)
    QString m_title;
    QVector<Outline*> m_children;
    bool m_open : 1;
    LinkDest *m_link;
};

class LinkDest {
public:
    enum Type {
        None = FZ_LINK_NONE,
        Goto = FZ_LINK_GOTO,
        Url = FZ_LINK_URI,
        Launch = FZ_LINK_LAUNCH,
        Named = FZ_LINK_NAMED,
        External = FZ_LINK_GOTOR
    };
    LinkDest(Type type): m_type(type) { }
    virtual ~LinkDest() { }
    Type type() const { return m_type; }
    static LinkDest *create(const fz_link_dest *dest);
protected:
    Type m_type;
};

static QPointF f2z(const fz_point &fz) { return QPointF(fz.x, fz.y); }

class GotoDest : public LinkDest {
public:
    GotoDest(const fz_link_dest *dest)
        : LinkDest(Goto)
    {
        m_page = dest->ld.gotor.page;
        m_rect = QRectF(f2z(dest->ld.gotor.lt), f2z(dest->ld.gotor.rb));
    }
    int page() const { return m_page; }
    QRectF rect(const QSizeF &dpi) const {
        return QRectF(m_rect.topLeft() * dpi.width() / 72.0,
                      m_rect.bottomRight() * dpi.height() / 72.0);
    }
private:
    int m_page;
    QRectF m_rect;
};

class ExternalDest : public LinkDest {
public:
    ExternalDest(const fz_link_dest *dest)
        : LinkDest(External)
    {
        if (dest->ld.gotor.page == -1)
            m_dest = QByteArray(dest->ld.gotor.dest);
        else
            m_dest = dest->ld.gotor.page;
        m_fileName = QString::fromUtf8(dest->ld.gotor.file_spec);
        m_window = dest->ld.gotor.new_window;
    }
    QString fileName() const { return m_fileName; }
    QVariant destination() const { return m_dest; }
    bool newWindow() const { return m_window; }
private:
    QString m_fileName;
    QVariant m_dest;
    bool m_window;
};

class UrlDest : public LinkDest {
public:
    UrlDest(const fz_link_dest *dest)
        : LinkDest(Url)
    {
        m_address = QString::fromUtf8(dest->ld.uri.uri);
        m_map = dest->ld.uri.is_map;
    }
    QString address() const { return m_address; }
    bool isMap() const { return m_map; }
private:
    QString m_address;
    bool m_map;
};

class LaunchDest : public LinkDest {
public:
    LaunchDest(const fz_link_dest *dest)
        : LinkDest(Launch)
    {
        m_fileName = QString::fromUtf8(dest->ld.launch.file_spec);
        m_window = dest->ld.launch.new_window;
        m_url = dest->ld.launch.is_uri;
    }
    bool isUrl() const { return m_url; }
    bool newWindow() const { return m_window; }
    QString fileName() const { return m_fileName; }
private:
    QString m_fileName;
    bool m_window, m_url;
};

class NamedDest : public LinkDest {
public:
    NamedDest(const fz_link_dest *dest)
        : LinkDest(Named)
    {
        m_name = QString::fromUtf8(dest->ld.named.named);
    }
    QString name() const { return m_name; }
private:
    QString m_name;
};

}

#endif
