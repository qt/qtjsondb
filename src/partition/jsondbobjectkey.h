/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDB_OBJECT_KEY_H
#define JSONDB_OBJECT_KEY_H

#include <qbytearray.h>
#include <qendian.h>
#include <qdebug.h>
#include <quuid.h>

#include "jsondbpartitionglobal.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class Q_JSONDB_PARTITION_EXPORT ObjectKey
{
public:
    QUuid key;  // object uuid

    ObjectKey() {}
    ObjectKey(const QUuid &uuid) : key(uuid) {}
    ObjectKey(const QByteArray &uuid) : key(QUuid::fromRfc4122(uuid)) {}
    inline bool isNull() const { return key.isNull(); }

    inline QByteArray toByteArray() const
    {
        return key.toRfc4122();
    }
    inline bool isValid() const
    { return !key.isNull(); }
    inline bool operator==(const ObjectKey &rhs) const
    { return key == rhs.key; }

    inline bool operator < (const ObjectKey &rhs) const
    { return key < rhs.key; }
};

inline QDebug &operator<<(QDebug &qdb, const ObjectKey &objectKey)
{
    qdb << objectKey.key.toString();
    return qdb;
}

QT_END_NAMESPACE_JSONDB_PARTITION

QT_BEGIN_NAMESPACE

template <> inline void qToBigEndian(QT_PREPEND_NAMESPACE_JSONDB_PARTITION(ObjectKey) src, uchar *dest)
{
    //TODO: improve me
    QByteArray key = src.key.toRfc4122();
    memcpy(dest, key.constData(), key.size());
}
template <> inline QT_PREPEND_NAMESPACE_JSONDB_PARTITION(ObjectKey) qFromBigEndian(const uchar *src)
{
    QT_PREPEND_NAMESPACE_JSONDB_PARTITION(ObjectKey) key;
    key.key = QUuid::fromRfc4122(QByteArray::fromRawData((const char *)src, 16));
    return key;
}

QT_END_NAMESPACE

QT_END_HEADER

#endif // JSONDB_OBJECT_KEY_H
