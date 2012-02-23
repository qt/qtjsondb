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

#include "jsondbephemeralpartition.h"

#include "jsondb.h"
#include "jsondbobject.h"
#include "jsondbquery.h"
#include "jsondbresponse.h"
#include "jsondb-error.h"
#include "jsondb-strings.h"

#include <qjsonobject.h>

QT_BEGIN_NAMESPACE_JSONDB

JsonDbEphemeralPartition::JsonDbEphemeralPartition(QObject *parent)
    : QObject(parent)
{
}

bool JsonDbEphemeralPartition::get(const QUuid &uuid, JsonDbObject *result) const
{
    ObjectMap::const_iterator it = mObjects.find(uuid);
    if (it == mObjects.end())
        return false;
    if (result)
        *result = it.value();
    return true;
}

QJsonObject JsonDbEphemeralPartition::create(JsonDbObject &object)
{
    if (!object.contains(JsonDbString::kUuidStr)) {
        object.generateUuid();
        object.computeVersion();
    }

    QJsonObject resultmap;

    QUuid uuid = object.uuid();
    if (mObjects.contains(uuid)) {
        return JsonDbResponse::makeErrorResponse(resultmap, JsonDbError::DatabaseError,
                                         QLatin1String("Already have an object with uuid") + uuid.toString());
    }
    mObjects.insert(uuid, object);

    resultmap.insert(JsonDbString::kUuidStr, uuid.toString());
    resultmap.insert(JsonDbString::kVersionStr, object.value(JsonDbString::kVersionStr).toString());
    resultmap.insert(JsonDbString::kCountStr, 1);

    return JsonDbResponse::makeResponse(resultmap);
}

QJsonObject JsonDbEphemeralPartition::update(JsonDbObject &object)
{
    QJsonObject resultmap;

    QUuid uuid = object.uuid();
    mObjects.insert(uuid, object);

    resultmap.insert(JsonDbString::kUuidStr, uuid.toString());
    resultmap.insert(JsonDbString::kVersionStr, object.value(JsonDbString::kVersionStr));
    resultmap.insert(JsonDbString::kCountStr, 1);

    return JsonDbResponse::makeResponse(resultmap);
}

QJsonObject JsonDbEphemeralPartition::remove(const JsonDbObject &object)
{
    QUuid uuid = object.uuid();

    mObjects.remove(uuid);

    QJsonObject item;
    item.insert(JsonDbString::kUuidStr, uuid.toString());

    QJsonArray data;
    data.append(item);

    QJsonObject resultmap;
    resultmap.insert(JsonDbString::kCountStr, 1);
    resultmap.insert(JsonDbString::kDataStr, data);
    resultmap.insert(JsonDbString::kErrorStr, QJsonValue());

    return JsonDbResponse::makeResponse(resultmap);
}

JsonDbQueryResult JsonDbEphemeralPartition::query(const JsonDbQuery *query, int limit, int offset) const
{
    if (!query->orderTerms.isEmpty())
        return JsonDbQueryResult::makeErrorResponse(JsonDbError::InvalidMessage,
                                                      QLatin1String("Cannot query with order term on ephemeral objects"));
    if (limit != -1 || offset != 0)
        return JsonDbQueryResult::makeErrorResponse(JsonDbError::InvalidMessage,
                                                    QLatin1String("Cannot query with limit or offset on ephemeral objects"));

    JsonDbObjectList results;
    ObjectMap::const_iterator it, e;
    for (it = mObjects.begin(), e = mObjects.end(); it != e; ++it) {
        QJsonObject object = it.value();
        if (query->match(object, 0, 0))
            results.append(object);
    }

    QJsonArray sortKeys;
    sortKeys.append(QLatin1String("_uuid"));
    JsonDbQueryResult result;
    result.length = results.size();
    result.offset = offset;
    result.data = results;
    result.state = 0;
    result.sortKeys = sortKeys;
    return result;
}

QT_END_NAMESPACE_JSONDB
