/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
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
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "jsondbobject.h"

#include <QStringList>
#include <QCryptographicHash>

#include <qjsondocument.h>

#include "jsondb-strings.h"

QT_ADDON_JSONDB_BEGIN_NAMESPACE

static QUuid generateUUIDv3(const QString &uri)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(QUuid(0x6ba7b810, 0x9dad, 0x11d1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8).toRfc4122());
    hash.addData(uri.toUtf8());
    QByteArray hashResult = hash.result();

    QUuid result = QUuid::fromRfc4122(hashResult);

    result.data3 &= 0x0FFF;
    result.data3 |= (3 << 12);
    result.data4[0] &= 0x3F;
    result.data4[0] |= 0x80;

    return result;
}

JsonDbObject::JsonDbObject()
{
}

JsonDbObject::JsonDbObject(const QJsonObject &object)
    : QJsonObject(object)
{
}

JsonDbObject::~JsonDbObject()
{
}

QByteArray JsonDbObject::toBinaryData() const
{
    return QJsonDocument(*this).toBinaryData();
}

QUuid JsonDbObject::uuid() const
{
    return QUuid(value(JsonDbString::kUuidStr).toString());
}

QString JsonDbObject::version() const
{
    return value(JsonDbString::kVersionStr).toString();
}

QString JsonDbObject::type() const
{
    return value(JsonDbString::kTypeStr).toString();
}

void JsonDbObject::generateUuid()
{
    QLatin1String idStr("_id");
    if (contains(idStr)) {
        QByteArray rfc4122 = generateUUIDv3(value(idStr).toString()).toRfc4122();
        QUuid uuid(QUuid::fromRfc4122((rfc4122)));
        insert(JsonDbString::kUuidStr, uuid.toString());
    } else {
        QUuid uuid(QUuid::createUuid());
        insert(JsonDbString::kUuidStr, uuid.toString());
    }
}

void JsonDbObject::computeVersion()
{
    // TODO improve me

    Q_ASSERT(!isEmpty());
    QJsonObject content(*this); // content without special properties (_uuid and _version)
    content.remove(JsonDbString::kUuidStr);
    content.remove(JsonDbString::kVersionStr);
    QJsonDocument doc(content);
    QByteArray hash = QCryptographicHash::hash(doc.toBinaryData(), QCryptographicHash::Md5).toHex();
    insert(JsonDbString::kVersionStr, QString::fromLatin1(hash.constData(), hash.size()));
}

QT_ADDON_JSONDB_END_NAMESPACE
