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
#include "jsondbpartition_p.h"
#include "jsondbstrings.h"
#include "jsondberrors.h"

#include "jsondbproxy.h"
#include "jsondbobjecttable.h"
#include "jsondbmapdefinition.h"
#include "jsondbsettings.h"
#include "jsondbscriptengine.h"
#include "jsondbview.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

JsonDbMapDefinition::JsonDbMapDefinition(const JsonDbOwner *owner, JsonDbPartition *partition, QJsonObject definition, QObject *parent) :
    QObject(parent)
    , mPartition(partition)
    , mOwner(owner)
    , mDefinition(definition)
    , mUuid(definition.value(JsonDbString::kUuidStr).toString())
    , mTargetType(definition.value(QStringLiteral("targetType")).toString())
    , mTargetTable(mPartition->findObjectTable(mTargetType))
{
    mMapId = mUuid;
    mMapId.replace(QRegExp(QStringLiteral("[-{}]")), QStringLiteral("$"));
    QJsonObject sourceFunctions(mDefinition.contains(QStringLiteral("join"))
                                ? mDefinition.value(QStringLiteral("join")).toObject()
                                : mDefinition.value(QStringLiteral("map")).toObject());
    mSourceTypes = sourceFunctions.keys();
    for (int i = 0; i < mSourceTypes.size(); i++) {
        const QString &sourceType = mSourceTypes[i];
        mSourceTables[sourceType] = mPartition->findObjectTable(sourceType);
    }
    if (mDefinition.contains(QStringLiteral("targetKeyName")))
        mTargetKeyName = mDefinition.value(QStringLiteral("targetKeyName")).toString();
}

void JsonDbMapDefinition::definitionCreated()
{
    initScriptEngine();
    initIndexes();

    foreach (const QString &sourceType, mSourceTypes) {
        GetObjectsResult getObjectResponse = mPartition->d_func()->getObjects(JsonDbString::kTypeStr, sourceType);
        if (!getObjectResponse.error.isNull()) {
            if (jsondbSettings->verbose())
                qDebug() << "createMapDefinition" << mSourceTypes << sourceType << mTargetType << getObjectResponse.error.toString();
            setError(getObjectResponse.error.toString());
            return;
        }
        JsonDbObjectList objects = getObjectResponse.data;
        bool isJoin = mDefinition.contains(JsonDbString::kJoinStr);
        for (int i = 0; i < objects.size(); i++) {
            JsonDbObject object(objects.at(i));
            if (isJoin)
                unmapObject(object);
            updateObject(JsonDbObject(), objects.at(i));
        }
    }
}

void JsonDbMapDefinition::definitionRemoved(JsonDbPartition *partition, JsonDbObjectTable *table, const QString targetType, const QString &definitionUuid)
{
    // remove the output objects
    GetObjectsResult getObjectResponse = table->getObjects(JsonDbString::kSourceUuidsDotStarStr,
                                                           definitionUuid,
                                                           targetType);
    JsonDbObjectList objects = getObjectResponse.data;
    for (int i = 0; i < objects.size(); i++) {
        JsonDbObject o = objects[i];
        o.markDeleted();
        partition->updateObject(partition->defaultOwner(), o, JsonDbPartition::ViewObject);
    }
}

void JsonDbMapDefinition::initScriptEngine()
{
    if (mScriptEngine)
        return;

    mScriptEngine = JsonDbScriptEngine::scriptEngine();
    JsonDbJoinProxy *joinProxy = new JsonDbJoinProxy(mOwner, mPartition, mScriptEngine);
    connect(joinProxy, SIGNAL(lookupRequested(QJSValue,QJSValue)),
            this, SLOT(lookupRequested(QJSValue,QJSValue)));
    connect(joinProxy, SIGNAL(viewObjectEmitted(QJSValue)),
            this, SLOT(viewObjectEmitted(QJSValue)));

    QString message;
    bool compiled = compileMapFunctions(mScriptEngine, mDefinition, joinProxy, mMapFunctions, message);
    if (!compiled)
      setError(message);
}

bool JsonDbMapDefinition::compileMapFunctions(QJSEngine *scriptEngine, QJsonObject definition, JsonDbJoinProxy *joinProxy, QMap<QString,QJSValue> &mapFunctions, QString &message)
{
    bool status = true;
    QJsonObject sourceFunctions(definition.contains(QStringLiteral("join"))
                                ? definition.value(QStringLiteral("join")).toObject()
                                : definition.value(QStringLiteral("map")).toObject());
    QJSValue svJoinProxyValue = joinProxy ? scriptEngine->newQObject(joinProxy) : QJSValue(QJSValue::UndefinedValue);
    for (QJsonObject::const_iterator it = sourceFunctions.begin(); it != sourceFunctions.end(); ++it) {
        const QString &sourceType = it.key();
        const QString &script = it.value().toString();
        QString jsonDbBinding;
        if (definition.contains(QStringLiteral("join")))
            // only joins can use lookup()
            jsonDbBinding = QStringLiteral("{ emit: proxy.create, lookup: proxy.lookup, createUuidFromString: proxy.createUuidFromString}");
        else
            jsonDbBinding = QStringLiteral("{ emit: proxy.create, createUuidFromString: proxy.createUuidFromString }");

        // first, package it as a function that takes a jsondb proxy and returns the map function
        QJSValue moduleFunction =
            scriptEngine->evaluate(QString::fromLatin1("(function (proxy) { var jsondb=%3; map_%1 = (%2); return map_%1; })")
                                   .arg(QString(sourceType).replace(QLatin1Char('.'), QLatin1Char('_')))
                                   .arg(script)
                                   .arg(jsonDbBinding));
        if (moduleFunction.isError() || !moduleFunction.isCallable()) {
            message = QString::fromLatin1("Unable to parse map function: %1").arg(moduleFunction.toString());
            status = false;
        }

        // now pass it the jsondb proxy to get the map function
        QJSValueList args;
        args << svJoinProxyValue;
        QJSValue mapFunction = moduleFunction.call(args);
        if (moduleFunction.isError() || !moduleFunction.isCallable()) {
            message = QString::fromLatin1("Unable to evaluate map function: %1").arg(moduleFunction.toString());
            status = false;
        }
        mapFunctions[sourceType] = mapFunction;
    }
    return status;
}

void JsonDbMapDefinition::releaseScriptEngine()
{
    mMapFunctions.clear();
}


void JsonDbMapDefinition::initIndexes()
{
    mTargetTable->addIndexOnProperty(JsonDbString::kSourceUuidsDotStarStr, QLatin1String("string"), mTargetType);
}

void JsonDbMapDefinition::updateObject(const JsonDbObject &beforeObject, const JsonDbObject &afterObject, JsonDbUpdateList *changeList)
{
    initScriptEngine();
    QHash<QString, JsonDbObject> unmappedObjects;
    mEmittedObjects.clear();

    if (!beforeObject.isEmpty()) {
        QJsonValue uuid = beforeObject.value(JsonDbString::kUuidStr);
        GetObjectsResult getObjectResponse = mTargetTable->getObjects(QStringLiteral("_sourceUuids.*"), uuid, mTargetType);
        foreach (const JsonDbObject &unmappedObject, getObjectResponse.data) {
            QString uuid = unmappedObject.value(JsonDbString::kUuidStr).toString();
            unmappedObjects[uuid] = unmappedObject;
        }
    }

    if (!afterObject.isDeleted()) {
        if (jsondbSettings->verbose())
            qDebug() << "Mapping object" << afterObject;
        mapObject(afterObject);
    }

    JsonDbObjectList objectsToUpdate;
    for (QHash<QString, JsonDbObject>::const_iterator it = unmappedObjects.begin();
         it != unmappedObjects.end();
         ++it) {
        JsonDbObject unmappedObject = it.value();
        QString uuid = unmappedObject.value(JsonDbString::kUuidStr).toString();
        if (mEmittedObjects.contains(uuid)) {
            JsonDbObject emittedObject(mEmittedObjects.value(uuid));
            emittedObject.insert(JsonDbString::kVersionStr, unmappedObject.value(JsonDbString::kVersionStr));
            emittedObject.insert(JsonDbString::kOwnerStr, unmappedObject.value(JsonDbString::kOwnerStr));
            if (emittedObject == it.value())
                // skip duplicates
                continue;
            else
                // update changed view objects
                objectsToUpdate.append(emittedObject);

            mEmittedObjects.remove(uuid);
        } else {
            // remove unmatched objects
            unmappedObject.markDeleted();
            if (jsondbSettings->verbose())
                qDebug() << "Unmapping object" << unmappedObject;
            objectsToUpdate.append(unmappedObject);
        }
    }

    for (QHash<QString, JsonDbObject>::const_iterator it = mEmittedObjects.begin();
         it != mEmittedObjects.end();
         ++it)
        objectsToUpdate.append(JsonDbObject(it.value()));

    JsonDbWriteResult res = mPartition->updateObjects(mOwner, objectsToUpdate, JsonDbPartition::ViewObject, changeList);
    if (res.code != JsonDbError::NoError)
        setError(QString::fromLatin1("Error creating view object: %1").arg(res.message));
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
    const QString &sourceType = object.value(JsonDbString::kTypeStr).toString();

    QJSValue sv = mScriptEngine->toScriptValue(static_cast<QJsonObject>(object));
    QString uuid = object.value(JsonDbString::kUuidStr).toString();
    mSourceUuids.clear();
    mSourceUuids.append(mUuid); // depends on the map definition object
    mSourceUuids.append(uuid);  // depends on the source object
    QJSValue mapped;

    QJSValueList mapArgs;
    mapArgs << sv;
    mapped = mapFunction(sourceType).call(mapArgs);

    if (mapped.isError())
        setError(QString::fromLatin1("Error executing map function: %1").arg(mapped.toString()));
}

void JsonDbMapDefinition::unmapObject(const JsonDbObject &object)
{
    Q_ASSERT(object.value(JsonDbString::kUuidStr).type() == QJsonValue::String);
    QJsonValue uuid = object.value(JsonDbString::kUuidStr);
    GetObjectsResult getObjectResponse = mTargetTable->getObjects(QStringLiteral("_sourceUuids.*"), uuid, mTargetType);
    JsonDbObjectList dependentObjects = getObjectResponse.data;

    for (int i = 0; i < dependentObjects.size(); i++) {
        JsonDbObject dependentObject = dependentObjects.at(i);
        if (dependentObject.value(JsonDbString::kTypeStr).toString() != mTargetType)
            continue;

        dependentObject.markDeleted();
        mPartition->updateObject(mOwner, dependentObject, JsonDbPartition::ViewObject);
    }
}

void JsonDbMapDefinition::lookupRequested(const QJSValue &query, const QJSValue &context)
{
    QString objectType = query.property(QStringLiteral("objectType")).toString();
    // compatibility for old style maps
    if (mDefinition.value(QStringLiteral("map")).isObject()) {
        if (objectType.isEmpty()) {
            setError(QStringLiteral("No objectType provided to jsondb.lookup"));
            return;
        }
        if (!mSourceTypes.contains(objectType)) {
            setError(QString::fromLatin1("lookup requested for type %1 not in source types: %2")
                     .arg(objectType)
                     .arg(mSourceTypes.join(QStringLiteral(", "))));
            return;
        }
    }
    QString findKey = query.property(QStringLiteral("index")).toString();
    QJSValue findValue = query.property(QStringLiteral("value"));
    GetObjectsResult getObjectResponse =
        mPartition->d_func()->getObjects(findKey, mScriptEngine->fromScriptValue<QJsonValue>(findValue), objectType, false);
    if (!getObjectResponse.error.isNull()) {
        if (jsondbSettings->verbose())
            qDebug() << "lookupRequested" << mSourceTypes << mTargetType
                     << getObjectResponse.error.toString();
        setError(getObjectResponse.error.toString());
    }
    JsonDbObjectList objectList = getObjectResponse.data;
    for (int i = 0; i < objectList.size(); ++i) {
        QJsonObject object = objectList.at(i);
        const QString uuid = object.value(JsonDbString::kUuidStr).toString();
        if (mSourceUuids.contains(uuid)) {
            if (jsondbSettings->verbose())
                qDebug() << "Lookup cycle detected" << "key" << findKey << mScriptEngine->fromScriptValue<QJsonValue>(findValue) << "matching object" << uuid << "source uuids" << mSourceUuids;
            continue;
        }
        mSourceUuids.append(uuid);
        QJSValueList mapArgs;
        QJSValue sv = mScriptEngine->toScriptValue(object);

        mapArgs << sv << context;
        QJSValue mapped = mMapFunctions[objectType].call(mapArgs);

        if (mapped.isError())
            setError(QString::fromLatin1("Error executing map function during lookup: %1").arg(mapped.toString()));

        mSourceUuids.removeOne(uuid);
    }
}

void JsonDbMapDefinition::viewObjectEmitted(const QJSValue &value)
{
    JsonDbObject newItem(mScriptEngine->fromScriptValue<QJsonObject>(value));
    newItem.insert(JsonDbString::kTypeStr, mTargetType);
    mSourceUuids.sort();
    QJsonArray sourceUuidArray;
    foreach (const QString &sourceUuid, mSourceUuids)
        sourceUuidArray.append(sourceUuid);
    newItem.insert(QStringLiteral("_sourceUuids"), sourceUuidArray);

    if (!newItem.contains(JsonDbString::kUuidStr)) {
        if (newItem.contains(QLatin1String("_id")))
            newItem.generateUuid();
        else {
            QString targetKeyString;
            if (!mTargetKeyName.isEmpty())
                targetKeyString = JsonDbObject(newItem).valueByPath(mTargetKeyName).toString();

            // colon separated sorted source uuids
            QString sourceUuidString = mSourceUuids.join(QStringLiteral(":"));
            QString identifier =
                QString::fromLatin1("%1:%2%3%4")
                .arg(mTargetType)
                .arg(sourceUuidString)
                .arg(targetKeyString.isEmpty() ? QStringLiteral("") : QStringLiteral(":"))
                .arg(targetKeyString);
            newItem.insert(JsonDbString::kUuidStr,
                           JsonDbObject::createUuidFromString(identifier).toString());
        }
    }

    QString uuid = newItem.value(JsonDbString::kUuidStr).toString();
    mEmittedObjects.insert(uuid, newItem);
}

bool JsonDbMapDefinition::isActive() const
{
    return !mDefinition.contains(JsonDbString::kActiveStr) || mDefinition.value(JsonDbString::kActiveStr).toBool();
}

void JsonDbMapDefinition::setError(const QString &errorMsg)
{
    mDefinition.insert(JsonDbString::kActiveStr, false);
    mDefinition.insert(JsonDbString::kErrorStr, errorMsg);
    if (jsondbSettings->verboseErrors())
        qDebug() << "Error in Map definition" << mTargetType << errorMsg;
    if (mPartition)
        mPartition->updateObject(mOwner, mDefinition, JsonDbPartition::Replace);
}

bool JsonDbMapDefinition::validateDefinition(const JsonDbObject &map, JsonDbPartition *partition, QString &message)
{
    message.clear();
    QString targetType = map.value(QStringLiteral("targetType")).toString();
    QString uuid = map.value(JsonDbString::kUuidStr).toString();
    JsonDbView *view = partition->findView(targetType);
    QStringList sourceTypes;

    if (targetType.isEmpty()) {
        message = QLatin1Literal("targetType property for Map not specified");
    } else if (map.contains(QLatin1Literal("sourceType"))) {
        message = QLatin1Literal("sourceType property for Map no longer supported");
    } else if (!view) {
        message = QLatin1Literal("targetType must be of a type that extends View");
    } else if (map.contains(QStringLiteral("join"))) {
        QJsonObject sourceFunctions = map.value(QStringLiteral("join")).toObject();

        if (sourceFunctions.isEmpty())
            message = QLatin1Literal("sourceTypes and functions for Map with join not specified");

        sourceTypes = sourceFunctions.keys();

        foreach (const QString &sourceType, sourceTypes) {
            if (sourceFunctions.value(sourceType).toString().isEmpty())
                message = QString::fromLatin1("join function for source type '%1' not specified for Map").arg(sourceType);
            if (view->mMapDefinitionsBySource.contains(sourceType)
                && view->mMapDefinitionsBySource.value(sourceType)->uuid() != uuid)
                message =  QString::fromLatin1("duplicate Map definition on source %1 and target %2")
                        .arg(sourceType).arg(targetType);
        }

        if (map.contains(QStringLiteral("map")))
            message = QLatin1Literal("Map 'join' and 'map' options are mutually exclusive");
    } else {
        QJsonValue mapValue = map.value(QStringLiteral("map"));
        if (!mapValue.isObject())
            message = QStringLiteral("sourceType property for Map not specified");
        else if (!mapValue.isString() && !mapValue.isObject())
            message = QStringLiteral("map function for Map not specified");

        if (mapValue.isObject()) {

            QJsonObject sourceFunctions = mapValue.toObject();

            if (sourceFunctions.isEmpty())
                message = QLatin1Literal("sourceTypes and functions for Map with map not specified");

            sourceTypes = sourceFunctions.keys();

            foreach (const QString &sourceType, sourceTypes) {
                if (sourceFunctions.value(sourceType).toString().isEmpty())
                    message = QString::fromLatin1("map function for source type '%1' not specified for Map").arg(sourceType);
                if (view->mMapDefinitionsBySource.contains(sourceType)
                    && view->mMapDefinitionsBySource.value(sourceType)->uuid() != uuid)
                    message = QString::fromLatin1("duplicate Map definition on source %1 and target %2")
                            .arg(sourceType).arg(targetType);
            }
        }
    }

    // check for parse errors
    if (message.isEmpty()) {
        QJSEngine *scriptEngine = JsonDbScriptEngine::scriptEngine();
        QMap<QString,QJSValue> mapFunctions;
        compileMapFunctions(scriptEngine, map, 0, mapFunctions, message);
    }

    return message.isEmpty();
}

#include "moc_jsondbmapdefinition.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
