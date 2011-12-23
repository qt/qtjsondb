/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "jsondb-object.h"

#include <QCryptographicHash>

Q_DECLARE_METATYPE(QUuid)

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

/*!
    \class JsonDbObject

    \brief The JsonDbObject class provides convenience api for constructing
    object that should be persisted to QtJsonDb.

    Objects that are persisted to Qt JsonDb are uniquely identified by
    \l{http://en.wikipedia.org/wiki/Universally_unique_identifier}{UUID} which
    value is stored inside an object in a special property \c{_uuid}.  JsonDbObject
    provides convenience api for generating uuid for a given object.

    There are several "versions" (variations) of UUID described in the
    specification, and for Qt JsonDb the two important ones are the following:

    \list
    \o version 3 constructs uuid from a given string (usually, uri)
    \o version 4 generates a random uuid
    \endlist

    Uuid version 3 makes a deterministic uuid from a given string, that can be
    reproduce later on, which is a very useful feature allowing to make a
    unique object identifier from a user-given string. For example if one saves
    file meta-data into Qt JsonDb, it might be convienient to deterministically
    generate uuid from a given file path. In Qt JsonDb this is achieved by
    putting the raw string data into a special \c{_id} property and constructing
    uuid from the object using JsonDbObject::uuidFromObject() call.

    \code
        #include <jsondb-object.h>

        QVariantMap object;
        object.insert(QLatin1String("name"), QLatin1String("Tor"));
        object.insert(QLatin1String("foo"), 42);
        object.insert(QLatin1String("_uuid"), JsonDbObject::uuidFromObject(object));

        JsonDbClient client;
        client.create(object);
    \endcode

    \code
        #include <jsondb-object.h>

        QMap<QUuid, JsonDbObject> objectsMap;
        for (int i = 0; i < 10; ++i) {
            JsonDbObject object;
            object.insert(QLatin1String("name"), QLatin1String("Tor"));
            object.insert(QLatin1String("foo"), 42);
            QUuid objectid = JsonDbObject::uuidFromObject(object);
            object.setUuid(objectid);
            objectsMap.insert(objectid, object);
        }
        JsonDbClient client;
        client.create(objectsMap.values());
    \endcode
*/

/*!
    Constructs an empty object.
*/
JsonDbObject::JsonDbObject()
{ }

/*!
    Constructs object from the given \a other QVariantMap.
*/
JsonDbObject::JsonDbObject(const QVariantMap &other)
    : QVariantMap(other)
{ }

/*!
    Returns uuid of an object, if present.

    This is the same as retrieving a value of the \c _uuid element.

    \sa setUuid()
*/
QUuid JsonDbObject::uuid() const
{
    QVariant v = value(QLatin1String("_uuid"));
    if (v.canConvert<QUuid>())
        return v.value<QUuid>();
    return QUuid(v.toString());
}

/*!
    Inserts the given \a uuid into the map.

    This is the same as calling \c {insert(QLatin1String("_uuid"), uuid)}.

    \sa uuid(), uuidFromObject()
*/
void JsonDbObject::setUuid(const QUuid &uuid)
{
    insert(QLatin1String("_uuid"), QVariant::fromValue(uuid));
}

/*!
    Returns a new uuid that can be used to identificate given \a object.

    Note that the returned uuid might be unique on every invocation on the same
    object, if the \a object doesn't have the \c{_id} property and there is no
    schema.
*/
QUuid JsonDbObject::uuidFromObject(const QVariantMap &object)
{
    QString idvalue = object.value(QLatin1String("_id")).toString();
    if (idvalue.isNull())
        return QUuid::createUuid();
    return generateUUIDv3(idvalue);
}

QT_ADDON_JSONDB_END_NAMESPACE
