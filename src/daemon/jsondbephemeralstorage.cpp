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

#include "jsondbephemeralstorage.h"

#include "jsondbquery.h"
#include "jsondb-error.h"
#include "jsondb-response.h"
#include "jsondb-strings.h"

QT_BEGIN_NAMESPACE_JSONDB

JsonDbEphemeralStorage::JsonDbEphemeralStorage(QObject *parent)
    : QObject(parent)
{
}

bool JsonDbEphemeralStorage::get(const QUuid &uuid, QsonMap *result) const
{
    ObjectMap::const_iterator it = mObjects.find(uuid);
    if (it == mObjects.end())
        return false;
    if (result)
        *result = it.value();
    return true;
}

QsonMap JsonDbEphemeralStorage::create(QsonMap &object)
{
    if (!object.isDocument()) {
        object.generateUuid();
        object.computeVersion();
    }

    QsonMap resultmap;

    QUuid uuid = object.uuid();
    if (mObjects.contains(uuid)) {
        return JsonDbResponse::makeErrorResponse(resultmap, JsonDbError::DatabaseError,
                                         QLatin1String("Already have an object with uuid") + uuid.toString());
    }
    mObjects.insert(uuid, object);

    resultmap.insert(JsonDbString::kUuidStr, uuid.toString());
    resultmap.insert(JsonDbString::kVersionStr, object.valueString(JsonDbString::kVersionStr));
    resultmap.insert(JsonDbString::kCountStr, 1);

    return JsonDbResponse::makeResponse(resultmap);
}

QsonMap JsonDbEphemeralStorage::update(QsonMap &object)
{
    QsonMap resultmap;

    QUuid uuid = object.uuid();
    mObjects.insert(uuid, object);

    resultmap.insert(JsonDbString::kUuidStr, uuid.toString());
    resultmap.insert(JsonDbString::kVersionStr, object.valueString(JsonDbString::kVersionStr));
    resultmap.insert(JsonDbString::kCountStr, 1);

    return JsonDbResponse::makeResponse(resultmap);
}

QsonMap JsonDbEphemeralStorage::remove(const QsonMap &object)
{
    QUuid uuid = object.uuid();

    mObjects.remove(uuid);

    QsonMap item;
    item.insert(JsonDbString::kUuidStr, uuid.toString());

    QsonList data;
    data.append(item);

    QsonMap resultmap;
    resultmap.insert(JsonDbString::kCountStr, 1);
    resultmap.insert(JsonDbString::kDataStr, data);
    resultmap.insert(JsonDbString::kErrorStr, QsonObject::NullValue);

    return JsonDbResponse::makeResponse(resultmap);
}

QsonMap JsonDbEphemeralStorage::query(const JsonDbQuery &query, int limit, int offset) const
{
    if (!query.orderTerms.isEmpty())
        return JsonDbResponse::makeErrorResponse(JsonDbError::InvalidMessage,
                                                 QLatin1String("Cannot query with order term on ephemeral objects"));
    if (limit != -1 || offset != 0)
        return JsonDbResponse::makeErrorResponse(JsonDbError::InvalidMessage,
                                                 QLatin1String("Cannot query with limit or offset on ephemeral objects"));

    QsonList results;
    ObjectMap::const_iterator it, e;
    for (it = mObjects.begin(), e = mObjects.end(); it != e; ++it) {
        const QsonMap &object = it.value();
        if (query.match(object, 0, 0))
            results.append(object);
    }

    QsonList sortKeys;
    sortKeys.append(QLatin1String("_uuid"));
    QsonMap map;
    map.insert(JsonDbString::kLengthStr, results.size());
    map.insert(JsonDbString::kOffsetStr, offset);
    map.insert(JsonDbString::kDataStr, results);
    map.insert(QLatin1String("state"), 0);
    map.insert(QLatin1String("sortKeys"), sortKeys);
    return JsonDbResponse::makeResponse(map);
}

QT_END_NAMESPACE_JSONDB
