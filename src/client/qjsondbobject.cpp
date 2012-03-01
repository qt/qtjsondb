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

#include "qjsondbobject.h"
#include "qjsondbstrings_p.h"

#include <QCryptographicHash>

QT_BEGIN_NAMESPACE_JSONDB

struct Uuid
{
    uint    data1;
    ushort  data2;
    ushort  data3;
    uchar   data4[8];
};

static const Uuid JsonDbNamespace = {0x6ba7b810, 0x9dad, 0x11d1, { 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };

/*!
    \class QJsonDbObject
    \inmodule QtJsonDb

    \brief The QJsonDbObject class provides convenience api for constructing
    object that should be persisted to QtJsonDb.

    Objects that are persisted to Qt JsonDb are uniquely identified by
    \l{http://en.wikipedia.org/wiki/Universally_unique_identifier}{UUID} which
    value is stored inside an object in a special property \c{_uuid}.  QJsonDbObject
    provides convenience api for generating uuid for a given object.

    There are several "versions" (variations) of UUID described in the
    specification, and for Qt JsonDb the two important ones are the following:

    \list
    \li version 3 constructs uuid from a given string (usually, uri)
    \li version 4 generates a random uuid
    \endlist

    Uuid version 3 makes a deterministic uuid from a given string, that can be
    reproduce later on, which is a very useful feature allowing to make a
    unique object identifier from a user-given string. For example if one saves
    file meta-data into Qt JsonDb, it might be convienient to deterministically
    generate uuid from a given file path. In Qt JsonDb this is achieved by
    constructing uuid from the string using
    QJsonDbObject::createUuidFromString() call.

    Creating random uuid:

    \code
        #include <QtJsonDb/qjsondbobject.h>
        QT_USE_NAMESPACE_JSONDB

        QJsonDbObject object;
        object.setUuid(QJsonDbObject::createUuid());
        object.insert(QStringLiteral("name"), QLatin1String("Tor"));
        object.insert(QStringLiteral("foo"), 42);
    \endcode

    QJsonDbObject::createUuidFromString() function can be used to conveniently
    construct deterministic uuid for a given string identifier:

    \code
        #include <QtJsonDb/qjsondbobject.h>
        QT_USE_NAMESPACE_JSONDB

        QJsonDbObject object;
        // uuid from "Tor" is created and assigned to the object
        object.setUuid(QJsonDbObject::createUuidFromString(QStringLiteral("Tor")));
        object.insert(QStringLiteral("name"), QLatin1String("Tor"));
        object.insert(QStringLiteral("foo"), 42);
    \endcode
*/

/*!
    Constructs an empty object.
*/
QJsonDbObject::QJsonDbObject()
{ }

/*!
    Constructs object from the given \a other QJsonObject.
*/
QJsonDbObject::QJsonDbObject(const QJsonObject &other)
    : QJsonObject(other)
{ }

/*!
    Assigns \a other to this object.
*/
QJsonDbObject &QJsonDbObject::operator=(const QJsonObject &other)
{
    *static_cast<QJsonObject *>(this) = other;
    return *this;
}

/*!
    Returns uuid of an object, if present.

    This is the same as retrieving a value of the \c _uuid element.

    \sa setUuid()
*/
QUuid QJsonDbObject::uuid() const
{
    return QUuid(value(JsonDbStrings::Property::uuid()).toString());
}

/*!
    Inserts the given \a uuid into the map.

    This is the same as calling \c {insert(QStringLiteral("_uuid"), uuid.toString())}.

    \sa uuid(), createUuidFromString()
*/
void QJsonDbObject::setUuid(const QUuid &uuid)
{
    insert(JsonDbStrings::Property::uuid(), uuid.toString());
}

/*!
    Returns random uuid that can be used to identify an object.

    This is exactly the same as calling QUuid::createUuid()

    \sa createUuidFromString()
*/
QUuid QJsonDbObject::createUuid()
{
    return QUuid::createUuid();
}

/*!
    Returns deterministic uuid that can be used to identify given \a identifier.

    The uuid is generated using QtJsonDb UUID namespace on a value of the
    given \a identifier.

    \sa createUuid()
*/
QUuid QJsonDbObject::createUuidFromString(const QString &identifier)
{
    const QUuid ns(JsonDbNamespace.data1, JsonDbNamespace.data2, JsonDbNamespace.data3,
                   JsonDbNamespace.data4[0], JsonDbNamespace.data4[1], JsonDbNamespace.data4[2],
                   JsonDbNamespace.data4[3], JsonDbNamespace.data4[4], JsonDbNamespace.data4[5],
                   JsonDbNamespace.data4[6], JsonDbNamespace.data4[7]);
    return QUuid::createUuidV3(ns, identifier);
}

///*!
//    Inserts a new item with the key \a key and a value of \a value.

//    If there is already an item with the key \a key then that item's value
//    is replaced with \a value.

//    Returns an iterator pointing to the inserted item.

//    \sa QJsonObject::insert()
//*/
//QJsonObject::iterator QJsonDbObject::insert(const QString &key, const QUuid &value)
//{
//    return insert(key, QJsonValue(value.toString())); // ### TODO: make QJsonObject store raw uuid
//}

QT_END_NAMESPACE_JSONDB
