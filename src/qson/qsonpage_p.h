/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSONPAGE_H
#define QSONPAGE_H

#include <QtJsonDbQson/qsonglobal.h>
#include <QtJsonDbQson/private/qsonversion_p.h>

#include <QList>
#include <QString>
#include <QSharedData>
#include <QUuid>

QT_BEGIN_NAMESPACE_JSONDB

typedef unsigned short qson_size;

class Q_ADDON_JSONDB_QSON_EXPORT QsonPage : public QSharedData
{
public:
    enum PageType {
        LIST_HEADER_PAGE = 'L',
        LIST_FOOTER_PAGE = 'l',

        DOCUMENT_HEADER_PAGE = 'D',
        DOCUMENT_FOOTER_PAGE = 'd',

        OBJECT_HEADER_PAGE = 'O',
        OBJECT_FOOTER_PAGE = 'o',

        META_HEADER_PAGE = 'M',
        META_FOOTER_PAGE = 'm',

        ARRAY_VALUE_PAGE = 'a',
        KEY_VALUE_PAGE = 'k',

        EMPTY_PAGE = 1,
        UNKNOWN_PAGE = 0
    };

    enum DataType {
        UNKNOWN_DATA = 0,
        NULL_TYPE = 0x10,
        FALSE_TYPE = 0x20,
        TRUE_TYPE = 0x21,
        DOUBLE_TYPE = 0x30,
        UINT_TYPE = 0x31,
        INT_TYPE = 0x32,
        STRING_TYPE = 0x40,
        KEY_TYPE = 0x41,
        VERSION_TYPE = 0x42,
        UUID_TYPE = 0x43
    };

    QsonPage();
    QsonPage(PageType type);
    QsonPage(const char *data, qson_size size);
    QsonPage(const QByteArray &page, quint32 pageOffset, qson_size pageSize);
    QsonPage(const QsonPage &copy);
    ~QsonPage();

    PageType type() const;
    int dataSize() const;
    const char *constData() const;
    char *data();

    bool writeKey(const QString &key);
    bool writeVersion(const QsonVersion &version);

    bool writeValue(); // null
    bool writeValue(const bool value);
    bool writeValue(const qint64 value);
    bool writeValue(const quint64 value);
    bool writeValue(const double value);
    bool writeValue(const QString &value);

    int readSize(int offset, bool expectKey) const;
    bool readNull(int offset) const;
    bool readBool(int offset) const;
    quint64 readUInt(int offset, quint64 fallback) const;
    qint64 readInt(int offset, qint64 fallback) const;
    double readDouble(int offset, double fallback) const;
    QString readString(int offset) const;
    QUuid readUuid(int offset) const;

    inline bool operator==(const QsonPage &other) const
    {
        if (mOffset != other.mOffset)
            return false;
        // can't test complete byte array, it might be shared
        return memcmp(constData(), other.constData(), mOffset) == 0;
    }
    inline bool operator!=(const QsonPage &other) const
    {
        return !operator==(other);
    }
    inline void resize(int size)
    {
        if (mPageOffset != 0) {
            if (size < mOffset) {
                mPage = QByteArray(constData(), size);
            } else {
                QByteArray newBuffer;
                newBuffer.reserve(size);
                newBuffer.append(constData(), mOffset);
                newBuffer.resize(size);
                mPage = newBuffer;
            }
            mPageOffset = 0;
        } else {
            mPage.resize(size);
        }
    }
private:
    qson_size mMaxSize;
    qson_size mOffset;
    QByteArray mPage;
    qint32 mPageOffset;

    inline qson_size stringSize(const QString &string) const;
    bool writeString(const QString &string);
    inline void updateOffset()
    {
        if (type() == QsonPage::EMPTY_PAGE || type() == QsonPage::UNKNOWN_PAGE)
            return;
        memcpy(data() + 2, &mOffset, 2);
    }

    friend class QsonObject;
    friend class QsonMap;
};

typedef QSharedDataPointer<QsonPage> QsonPagePtr;
typedef QList<QSharedDataPointer<QsonPage> > QsonContent;

QT_END_NAMESPACE_JSONDB

QT_BEGIN_NAMESPACE
template<> Q_DECLARE_TYPEINFO_BODY(QSharedDataPointer<QtAddOn::JsonDb::QsonPage>, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QSONPAGE_H
