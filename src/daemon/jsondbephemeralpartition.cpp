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

#include "jsondbobject.h"
#include "jsondbquery.h"
#include "jsondberrors.h"
#include "jsondbstrings.h"

#include <qjsonobject.h>

QT_USE_NAMESPACE_JSONDB_PARTITION

JsonDbEphemeralPartition::JsonDbEphemeralPartition(const QString &name, QObject *parent) :
    QObject(parent)
  , mName(name)
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

JsonDbQueryResult JsonDbEphemeralPartition::queryObjects(const JsonDbOwner *owner, const JsonDbQuery *query, int limit, int offset)
{
    Q_UNUSED(owner);
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

JsonDbWriteResult JsonDbEphemeralPartition::updateObjects(const JsonDbOwner *owner, const JsonDbObjectList &objects, JsonDbPartition::WriteMode mode)
{
    Q_UNUSED(mode);
    JsonDbWriteResult result;
    QList<JsonDbUpdate> updated;

    foreach (const JsonDbObject &toUpdate, objects) {

        JsonDbObject object = toUpdate;

        JsonDbObject oldObject;
        if (mObjects.contains(object.uuid()))
            oldObject = mObjects[object.uuid()];

        JsonDbNotification::Action action = JsonDbNotification::Update;

        if (object.uuid().isNull())
            object.generateUuid();

        // FIXME: stale update rejection, access control
        if (object.value(JsonDbString::kDeletedStr).toBool()) {
            if (mObjects.contains(object.uuid())) {
                action = JsonDbNotification::Delete;
                mObjects.remove(object.uuid());
            } else {
                result.code =  JsonDbError::MissingObject;
                result.message = QLatin1String("Cannot remove non-existing object");
                return result;
            }
        } else {

            if (!mObjects.contains(object.uuid()))
                action = JsonDbNotification::Create;

            object.insert(JsonDbString::kOwnerStr, owner->ownerId());
            object.computeVersion();
            mObjects[object.uuid()] = object;
        }

        result.objectsWritten.append(object);
        updated.append(JsonDbUpdate(oldObject, object, action));
    }

    emit objectsUpdated(updated);
    return result;
}
