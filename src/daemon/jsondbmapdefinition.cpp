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

#include <QDebug>
#include <QElapsedTimer>
#include <QRegExp>
#include <QJSValue>
#include <QJSValueIterator>
#include <QStringList>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "jsondbpartition.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"
#include "json.h"

#include "jsondb.h"
#include "jsondbproxy.h"
#include "jsondbobjecttable.h"
#include "jsondbmapdefinition.h"
#include "jsondbsettings.h"
#include "jsondbview.h"

QT_BEGIN_NAMESPACE_JSONDB

JsonDbMapDefinition::JsonDbMapDefinition(JsonDb *jsonDb, const JsonDbOwner *owner, JsonDbPartition *partition, QJsonObject definition, QObject *parent)
    : QObject(parent)
    , mJsonDb(jsonDb)
    , mPartition(partition)
    , mOwner(owner)
    , mDefinition(definition)
    , mScriptEngine(0)
    , mUuid(definition.value(JsonDbString::kUuidStr).toString())
    , mTargetType(definition.value("targetType").toString())
    , mTargetTable(mPartition->findObjectTable(mTargetType))
{
    if (!mDefinition.contains("sourceType")) {
        QJsonObject sourceFunctions(mDefinition.contains("join")
                                   ? mDefinition.value("join").toObject()
                                   : mDefinition.value("map").toObject());
        mSourceTypes = sourceFunctions.keys();
        for (int i = 0; i < mSourceTypes.size(); i++) {
            const QString &sourceType = mSourceTypes[i];
            mSourceTables[sourceType] = mJsonDb->findPartition(mPartition->name())->findObjectTable(sourceType);
        }
    } else {
        // TODO: remove this case
        const QString sourceType = mDefinition.value("sourceType").toString();
        mSourceTables[sourceType] = mJsonDb->findPartition(mPartition->name())->findObjectTable(sourceType);
        mSourceTypes.append(sourceType);
    }
}


void JsonDbMapDefinition::initScriptEngine()
{
    if (mScriptEngine)
        return;

    mScriptEngine = new QJSEngine(this);
    QJSValue globalObject = mScriptEngine->globalObject();
    globalObject.setProperty("console", mScriptEngine->newQObject(new Console()));

    if (!mDefinition.contains("sourceType")) {
        QJsonObject sourceFunctions(mDefinition.contains("join")
                                   ? mDefinition.value("join").toObject()
                                   : mDefinition.value("map").toObject());
        for (int i = 0; i < mSourceTypes.size(); i++) {
            const QString &sourceType = mSourceTypes[i];
            const QString &script = sourceFunctions.value(sourceType).toString();
            QJSValue mapFunction =
                mScriptEngine->evaluate(QString("var map_%1 = (%2); map_%1;").arg(QString(sourceType).replace(".", "_")).arg(script));
            if (mapFunction.isError() || !mapFunction.isCallable())
                setError( "Unable to parse map function: " + mapFunction.toString());
            mMapFunctions[sourceType] = mapFunction;
        }

        mJoinProxy = new JsonDbJoinProxy(mOwner, mJsonDb, this);
        connect(mJoinProxy, SIGNAL(lookupRequested(QJSValue,QJSValue)),
                this, SLOT(lookupRequested(QJSValue,QJSValue)));
        connect(mJoinProxy, SIGNAL(viewObjectEmitted(QJSValue)),
                this, SLOT(viewObjectEmitted(QJSValue)));
        globalObject.setProperty("_jsondb", mScriptEngine->newQObject(mJoinProxy));
        // we use this snippet of javascript so that we can bind "jsondb.emit"
        // even though "emit" is a Qt keyword
        if (mDefinition.contains("join"))
            // only joins can use lookup()
            mScriptEngine->evaluate("var jsondb = { emit: _jsondb.create, lookup: _jsondb.lookup };");
        else
            mScriptEngine->evaluate("var jsondb = { emit: _jsondb.create };");

    } else {
        const QString sourceType = mDefinition.value("sourceType").toString();
        const QString &script = mDefinition.value("map").toString();
        QJSValue mapFunction =
            mScriptEngine->evaluate(QString("var map_%1 = (%2); map_%1;").arg(QString(sourceType).replace(".", "_")).arg(script));
        if (mapFunction.isError() || !mapFunction.isCallable())
            setError( "Unable to parse map function: " + mapFunction.toString());
        mMapFunctions[sourceType] = mapFunction;

        mMapProxy = new JsonDbMapProxy(mOwner, mJsonDb, this);
        connect(mMapProxy, SIGNAL(lookupRequested(QJSValue,QJSValue)),
                this, SLOT(lookupRequested(QJSValue,QJSValue)));
        connect(mMapProxy, SIGNAL(viewObjectEmitted(QJSValue)),
                this, SLOT(viewObjectEmitted(QJSValue)));
        globalObject.setProperty("jsondb", mScriptEngine->newQObject(mMapProxy));
        qWarning() << "Old-style Map from sourceType" << sourceType << " to targetType" << mDefinition.value("targetType");
    }
}

void JsonDbMapDefinition::releaseScriptEngine()
{
    mMapFunctions.clear();
    delete mScriptEngine;
    mScriptEngine = 0;
}

QJSValue JsonDbMapDefinition::mapFunction(const QString &sourceType) const
{
    if (mMapFunctions.contains(sourceType))
        return mMapFunctions[sourceType];
    else
        return QJSValue();
}

void JsonDbMapDefinition::mapObject(JsonDbObject object)
{
    initScriptEngine();
    const QString &sourceType = object.value(JsonDbString::kTypeStr).toString();

    QJSValue sv = JsonDb::toJSValue(object, mScriptEngine);
    QString uuid = object.value(JsonDbString::kUuidStr).toString();
    mSourceUuids.clear();
    mSourceUuids.append(mUuid); // depends on the map definition object
    mSourceUuids.append(uuid);  // depends on the source object
    QJSValue mapped;

    QJSValueList mapArgs;
    mapArgs << sv;
    mapped = mapFunction(sourceType).call(mapArgs);

    if (mapped.isError())
        setError("Error executing map function: " + mapped.toString());
}

void JsonDbMapDefinition::unmapObject(const JsonDbObject &object)
{
    Q_ASSERT(object.value(JsonDbString::kUuidStr).type() == QJsonValue::String);
    initScriptEngine();
    QJsonValue uuid = object.value(JsonDbString::kUuidStr);
    GetObjectsResult getObjectResponse = mTargetTable->getObjects("_sourceUuids.*", uuid, mTargetType);
    JsonDbObjectList dependentObjects = getObjectResponse.data;

    for (int i = 0; i < dependentObjects.size(); i++) {
        JsonDbObject dependentObject = dependentObjects.at(i);
        if (dependentObject.value(JsonDbString::kTypeStr).toString() != mTargetType)
            continue;
        mJsonDb->removeViewObject(mOwner, dependentObject, mPartition->name());
    }
}

void JsonDbMapDefinition::lookupRequested(const QJSValue &query, const QJSValue &context)
{
    QString objectType = query.property("objectType").toString();
    // compatibility for old style maps
    if (mDefinition.value("map").isObject()) {
        if (objectType.isEmpty()) {
            setError("No objectType provided to jsondb.lookup");
            return;
        }
        if (!mSourceTypes.contains(objectType)) {
            setError(QString("lookup requested for type %1 not in source types: %2")
                     .arg(objectType)
                     .arg(mSourceTypes.join(", ")));
            return;
        }
    }
    QString findKey = query.property("index").toString();
    QJSValue findValue = query.property("value");
    GetObjectsResult getObjectResponse =
        mPartition->getObjects(findKey, JsonDb::fromJSValue(findValue), objectType, false);
    if (!getObjectResponse.error.isNull()) {
        if (jsondbSettings->verbose())
            qDebug() << "lookupRequested" << mSourceTypes << mTargetType
                     << getObjectResponse.error.toString();
        setError(getObjectResponse.error.toString());
    }
    JsonDbObjectList objectList = getObjectResponse.data;
    for (int i = 0; i < objectList.size(); ++i) {
        JsonDbObject object = objectList.at(i);
        const QString uuid = object.value(JsonDbString::kUuidStr).toString();
        if (mSourceUuids.contains(uuid)) {
            if (jsondbSettings->verbose())
                qDebug() << "Lookup cycle detected" << "key" << findKey << JsonDb::fromJSValue(findValue) << "matching object" << uuid << "source uuids" << mSourceUuids;
            continue;
        }
        mSourceUuids.append(uuid);
        QJSValueList mapArgs;
        QJSValue sv = JsonDb::toJSValue(object, mScriptEngine);

        mapArgs << sv << context;
        QJSValue mapped = mMapFunctions[objectType].call(mapArgs);

        if (mapped.isError())
            setError("Error executing map function during lookup: " + mapped.toString());

        mSourceUuids.removeLast();
    }
}

void JsonDbMapDefinition::viewObjectEmitted(const QJSValue &value)
{
    JsonDbObject newItem(JsonDb::fromJSValue(value).toObject());
    newItem.insert(JsonDbString::kTypeStr, mTargetType);
    QJsonArray sourceUuids;
    foreach (const QString &str, mSourceUuids)
        sourceUuids.append(str);
    newItem.insert("_sourceUuids", sourceUuids);

    QJsonObject res = mJsonDb->createViewObject(mOwner, newItem, mPartition->name());
    if (JsonDb::responseIsError(res))
        setError("Error executing map function during emitViewObject: " +
                 res.value(JsonDbString::kErrorStr).toObject().value(JsonDbString::kMessageStr).toString());
}

bool JsonDbMapDefinition::isActive() const
{
    return !mDefinition.contains(JsonDbString::kActiveStr) || mDefinition.value(JsonDbString::kActiveStr).toBool();
}

void JsonDbMapDefinition::setError(const QString &errorMsg)
{
    mDefinition.insert(JsonDbString::kActiveStr, false);
    mDefinition.insert(JsonDbString::kErrorStr, errorMsg);
    if (JsonDbPartition *partition = mJsonDb->findPartition(mPartition->name())) {
        WithTransaction transaction(partition, "JsonDbMapDefinition::setError");
        JsonDbObjectTable *objectTable = partition->findObjectTable(JsonDbString::kMapTypeStr);
        transaction.addObjectTable(objectTable);
        JsonDbObject doc(mDefinition);
        JsonDbObject _delrec;
        partition->getObject(mUuid, _delrec, JsonDbString::kMapTypeStr);
        partition->updatePersistentObject(_delrec, doc);
        transaction.commit();
    }
}

bool JsonDbMapDefinition::validateDefinition(const JsonDbObject &map, JsonDbPartition *partition, QString &message)
{
    message.clear();
    QString targetType = map.value("targetType").toString();
    QString uuid = map.value(JsonDbString::kUuidStr).toString();
    JsonDbView *view = partition->findView(targetType);
    QStringList sourceTypes;

    if (targetType.isEmpty()) {
        message = QLatin1Literal("targetType property for Map not specified");
    } else if (!view) {
        message = QLatin1Literal("targetType must be of a type that extends View");
    } else if (map.contains("join")) {
        QJsonObject sourceFunctions = map.value("join").toObject();
        sourceTypes = sourceFunctions.keys();
        if (sourceFunctions.isEmpty())
            message = QLatin1Literal("sourceTypes and functions for Map with join not specified");

        foreach (const QString &sourceType, sourceTypes) {
            if (sourceFunctions.value(sourceType).toString().isEmpty())
                message = QString("join function for source type '%1' not specified for Map").arg(sourceType);
            if (view->mMapDefinitionsBySource.contains(sourceType)
                && view->mMapDefinitionsBySource.value(sourceType)->uuid() != uuid)
                message =
                  QString("duplicate Map definition on source %1 and target %2")
                    .arg(sourceType).arg(targetType);
        }

        if (map.contains("map"))
            message = QLatin1Literal("Map 'join' and 'map' options are mutually exclusive");
        else if (map.contains("sourceType"))
            message = QLatin1Literal("Map 'join' and 'sourceType' options are mutually exclusive");
    } else {
        QJsonValue mapValue = map.value("map");
        if (map.value("sourceType").toString().isEmpty() && !mapValue.isObject())
            message = QLatin1String("sourceType property for Map not specified");
        else if (!mapValue.isString() && !mapValue.isObject())
            message = QLatin1String("map function for Map not specified");
    }

    return message.isEmpty();
}

QT_END_NAMESPACE_JSONDB
