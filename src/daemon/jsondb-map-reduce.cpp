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

#include "jsondbbtreestorage.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"
#include "json.h"

#include "jsondb.h"
#include "jsondb-proxy.h"
#include "objecttable.h"

QT_BEGIN_NAMESPACE_JSONDB

void JsonDb::initMap(const QString &partition)
{
    if (gVerbose) qDebug() << "Initializing views on partition" << partition;
    JsonDbBtreeStorage *storage = findPartition(partition);
    {
        JsonDbObjectList mrdList = getObjects(JsonDbString::kTypeStr, QJsonValue(QLatin1String("Map")), partition).data;
        for (int i = 0; i < mrdList.size(); ++i) {
            JsonDbObject mrd = mrdList.at(i);
            createMapDefinition(mrd, false, partition);
            storage->addView(mrd.value("targetType").toString());
        }
    }
    {
        JsonDbObjectList mrdList = getObjects(JsonDbString::kTypeStr, QJsonValue(QLatin1String("Reduce")), partition).data;
        for (int i = 0; i < mrdList.size(); ++i) {
            JsonDbObject mrd = mrdList.at(i);
            createReduceDefinition(mrd, false, partition);
            storage->addView(mrd.value("targetType").toString());
        }
    }
}

void JsonDb::createMapDefinition(QJsonObject mapDefinition, bool firstTime, const QString &partition)
{
    QString targetType = mapDefinition.value("targetType").toString();
    QStringList sourceTypes;
    if (mapDefinition.contains("join"))
        sourceTypes = mapDefinition.value("join").toObject().keys();
    else if (mapDefinition.contains("sourceType")) // deprecated
        sourceTypes.append(mapDefinition.value("sourceType").toString());
    else
        sourceTypes = mapDefinition.value("map").toObject().keys();

    if (gVerbose)
        qDebug() << "createMapDefinition" << sourceTypes << targetType;

    JsonDbBtreeStorage *storage = findPartition(partition);
    storage->addView(targetType);
    storage->addIndexOnProperty("_sourceUuids.*", "string", targetType);
    storage->addIndexOnProperty("_mapUuid", "string", targetType);

    JsonDbMapDefinition *def = new JsonDbMapDefinition(this, mOwner, partition, mapDefinition, this);
    for (int i = 0; i < sourceTypes.size(); i++)
        mMapDefinitionsBySource.insert(sourceTypes[i], def);
    mMapDefinitionsByTarget.insert(targetType, def);

    if (firstTime && def->isActive()) {
        def->initScriptEngine();
        GetObjectsResult getObjectResponse = getObjects(JsonDbString::kTypeStr, sourceTypes[0]);
        if (!getObjectResponse.error.isNull()) {
            if (gVerbose)
                qDebug() << "createMapDefinition" << sourceTypes << targetType << getObjectResponse.error.toString();
            def->setError(getObjectResponse.error.toString());
        };
        JsonDbObjectList objects = getObjectResponse.data;
        for (int i = 0; i < objects.size(); i++)
            def->mapObject(objects.at(i));
    }
}

void JsonDb::removeMapDefinition(QJsonObject mapDefinition, const QString &partition)
{
    QString targetType = mapDefinition.value("targetType").toString();

    JsonDbMapDefinition *def = 0;
    foreach (JsonDbMapDefinition *d, mMapDefinitionsByTarget.values(targetType)) {
        if (d->uuid() == mapDefinition.value(JsonDbString::kUuidStr).toString()) {
            def = d;
            mMapDefinitionsByTarget.remove(targetType, def);
            const QStringList &sourceTypes = def->sourceTypes();
            for (int i = 0; i < sourceTypes.size(); i++)
                mMapDefinitionsBySource.remove(sourceTypes[i], def);

            break;
        }
    }

    // remove the output objects
    GetObjectsResult getObjectResponse = getObjects("_mapUuid",
                                                   mapDefinition.value(JsonDbString::kUuidStr), targetType);
    JsonDbObjectList objects = getObjectResponse.data;
    for (int i = 0; i < objects.size(); i++)
        removeViewObject(mOwner, objects.at(i), partition);
}

void JsonDb::createReduceDefinition(QJsonObject reduceDefinition, bool firstTime, const QString &partition)
{
    QString targetType = reduceDefinition.value("targetType").toString();
    QString sourceType = reduceDefinition.value("sourceType").toString();

    if (gDebug)
        qDebug() << "createReduceDefinition" << targetType << sourceType;

    JsonDbBtreeStorage *storage = findPartition(partition);
    storage->addView(targetType);

    JsonDbReduceDefinition *def = new JsonDbReduceDefinition(this, mOwner, partition, reduceDefinition, this);
    mReduceDefinitionsBySource.insert(sourceType, def);
    mReduceDefinitionsByTarget.insert(targetType, def);

    storage->addIndexOnProperty("_sourceUuids.*", "string", targetType);
    storage->addIndexOnProperty(def->sourceKeyName(), "string", sourceType);
    storage->addIndexOnProperty(def->targetKeyName(), "string", targetType);
    storage->addIndexOnProperty("_reduceUuid", "string", targetType);

    if (firstTime && def->isActive()) {
        def->initScriptEngine();
        GetObjectsResult getObjectResponse = getObjects(JsonDbString::kTypeStr, sourceType);
        if (!getObjectResponse.error.isNull()) {
            if (gVerbose)
                qDebug() << "createReduceDefinition" << targetType << getObjectResponse.error.toString();
            def->setError(getObjectResponse.error.toString());
        }
        JsonDbObjectList objects = getObjectResponse.data;
        for (int i = 0; i < objects.size(); i++) {
            def->updateObject(QJsonObject(), objects.at(i));
        }
    }
}

void JsonDb::removeReduceDefinition(QJsonObject reduceDefinition, const QString &partition)
{
    QString targetType = reduceDefinition.value("targetType").toString();
    QString sourceType = reduceDefinition.value("sourceType").toString();

    if (gVerbose)
        qDebug() << "removeReduceDefinition" << sourceType <<  targetType;

    JsonDbReduceDefinition *def = 0;
    foreach (JsonDbReduceDefinition *d, mReduceDefinitionsBySource.values(sourceType)) {
        if (d->uuid() == reduceDefinition.value(JsonDbString::kUuidStr).toString()) {
            def = d;
            mReduceDefinitionsBySource.remove(def->sourceType(), def);
            mReduceDefinitionsByTarget.remove(def->targetType(), def);
            break;
        }
    }
    // remove the output objects
    GetObjectsResult getObjectResponse = getObjects("_reduceUuid", reduceDefinition.value(JsonDbString::kUuidStr), targetType);
    JsonDbObjectList objects = getObjectResponse.data;
    for (int i = 0; i < objects.size(); i++)
        removeViewObject(mOwner, objects.at(i), partition);
    //TODO: actually remove the table
}

void JsonDb::updateView(const QString &viewType, const QString &partitionName)
{
    if (viewType.isEmpty())
        return;
    QElapsedTimer timer;
    if (gPerformanceLog)
        timer.start();
    JsonDbBtreeStorage *partition = findPartition(partitionName);
    ObjectTable *objectTable = partition->mainObjectTable();
    ObjectTable *targetTable = partition->findObjectTable(viewType);
    if ((objectTable == targetTable)
        || (objectTable->stateNumber() == targetTable->stateNumber()))
        return;

    updateMap(viewType, partitionName);
    updateReduce(viewType, partitionName);
    if (gPerformanceLog)
        qDebug() << "updateView" << viewType << timer.elapsed() << "ms";
}

void JsonDb::updateMap(const QString &viewType, const QString &partitionName)
{
    JsonDbBtreeStorage *partition = findPartition(partitionName);
    ObjectTable *targetTable = partition->findObjectTable(viewType);
    quint32 targetStateNumber = qMax(1u, targetTable->stateNumber());
    if (gVerbose) qDebug() << "JsonDb::updateMap" << viewType << targetStateNumber << (mViewsUpdating.contains(viewType) ? "already updating" : "") << "targetStateNumber" << targetStateNumber;
    if (mViewsUpdating.contains(viewType))
        return;
    mViewsUpdating.insert(viewType);

    QHash<QString,JsonDbMapDefinition*> mapDefinitions;
    QSet<ObjectTable*>                  sourceTables;
    QMultiMap<ObjectTable*,QString>     objectTableSourceType;
    QMap<QString, QJsonObject> addedMapDefinitions;   // uuid -> added definition
    QMap<QString, QJsonObject> removedMapDefinitions; // uuid -> removed definition
    quint32 endStateNumber =
        findUpdatedMapReduceDefinitions(partition, JsonDbString::kMapTypeStr,
                                        viewType, targetStateNumber, addedMapDefinitions, removedMapDefinitions);
    for (QMap<QString,JsonDbMapDefinition*>::const_iterator it = mMapDefinitionsByTarget.find(viewType);
         (it != mMapDefinitionsByTarget.end() && it.key() == viewType);
         ++it) {
        JsonDbMapDefinition *def = it.value();
        const QStringList &sourceTypes = def->sourceTypes();
        for (int i = 0; i < sourceTypes.size(); i++) {
            const QString &sourceType = sourceTypes[i];
            ObjectTable *sourceTable = def->sourceTable(sourceType);
            sourceTables.insert(sourceTable);
            objectTableSourceType.insert(sourceTable, sourceType);
            mapDefinitions.insert(sourceType, def);
        }
    }
    if (sourceTables.isEmpty() && addedMapDefinitions.isEmpty() && removedMapDefinitions.isEmpty()) {
        mViewsUpdating.remove(viewType);
        return;
    }

    for (QMap<ObjectTable*,QString>::const_iterator it = objectTableSourceType.begin();
         it != objectTableSourceType.end();
         ++it) {
        QString sourceType = it.value();
        // make sure the source is updated
        updateView(sourceType);

        if (!endStateNumber) {
            ObjectTable *sourceTable = it.key();
            quint32 sourceStateNumber = sourceTable->stateNumber();
            endStateNumber = sourceStateNumber;
        }
    }

    // this transaction private to targetTable
    targetTable->begin();

    for (QMap<QString,QJsonObject>::const_iterator it = removedMapDefinitions.begin();
         it != removedMapDefinitions.end();
         ++it) {
        QJsonObject def = it.value();
        removeMapDefinition(def, partitionName);
    }
    for (QMap<QString,QJsonObject>::const_iterator it = addedMapDefinitions.begin();
         it != addedMapDefinitions.end();
         ++it) {
        QJsonObject def = it.value();
        createMapDefinition(def, true, partitionName);
    }

    for (QSet<ObjectTable*>::const_iterator it = sourceTables.begin();
         it != sourceTables.end();
         ++it) {
        ObjectTable *sourceTable = *it;
        QStringList sourceTypes = objectTableSourceType.values(sourceTable);
        QJsonObject changesSince(sourceTable->changesSince(targetStateNumber));
        QJsonObject changes(changesSince.value("result").toObject());
        QJsonArray changeList(changes.value("changes").toArray());
        quint32 count = changeList.size();
        for (quint32 i = 0; i < count; i++) {
            QJsonObject change = changeList.at(i).toObject();
            QJsonValue before = change.value("before");
            QJsonValue after = change.value("after");
            if (!before.isUndefined()) {
                QJsonObject beforeObject = before.toObject();
                QString sourceType = beforeObject.value(JsonDbString::kTypeStr).toString();
                if (sourceTypes.contains(sourceType)) {
                    mapDefinitions.value(sourceType)->unmapObject(beforeObject);
                }
            }
            if (after.type() == QJsonValue::Object) {
                QJsonObject afterObject = after.toObject();
                if (!afterObject.isEmpty()
                    && !afterObject.contains(JsonDbString::kDeletedStr)
                    && !afterObject.value(JsonDbString::kDeletedStr).toBool()) {
                    QString sourceType = afterObject.value(JsonDbString::kTypeStr).toString();
                    if (sourceTypes.contains(sourceType)) {
                        Q_ASSERT(mapDefinitions.value(sourceType));
                        mapDefinitions.value(sourceType)->mapObject(afterObject);
                    }
                }
            }
        }
    }

    bool ok = targetTable->commit(endStateNumber);
    Q_ASSERT(ok);
    mViewsUpdating.remove(viewType);
}

quint32 JsonDb::findUpdatedMapReduceDefinitions(JsonDbBtreeStorage *partition, const QString &definitionType,
                                                const QString &viewType, quint32 targetStateNumber,
                                                QMap<QString,QJsonObject> &addedDefinitions, QMap<QString,QJsonObject> &removedDefinitions) const
{
    ObjectTable *objectTable = partition->findObjectTable(definitionType);
    quint32 stateNumber = objectTable->stateNumber();
    if (stateNumber == targetStateNumber)
        return stateNumber;
    QSet<QString> limitTypes;
    limitTypes << definitionType;
    QJsonObject changes = objectTable->changesSince(targetStateNumber, limitTypes).value("result").toObject();
    quint32 count = changes.value("count").toDouble();
    QJsonArray changeList = changes.value("changes").toArray();
    for (quint32 i = 0; i < count; i++) {
        QJsonObject change = changeList.at(i).toObject();
        if (change.contains("before")) {
            QJsonObject before = change.value("before").toObject();
            if ((before.value(JsonDbString::kTypeStr).toString() == definitionType)
                && (before.value("targetType").toString() == viewType))
                removedDefinitions.insert(before.value(JsonDbString::kUuidStr).toString(), before);
        }
        if (change.contains("after")) {
            QJsonObject after = change.value("after").toObject();
            if ((after.value(JsonDbString::kTypeStr).toString() == definitionType)
                && (after.value("targetType").toString() == viewType))
                addedDefinitions.insert(after.value(JsonDbString::kUuidStr).toString(), after);
        }
    }
    return stateNumber;
}

void JsonDb::updateReduce(const QString &viewType, const QString &partitionName)
{
    //Q_ASSERT(mReduceDefinitionsByTarget.contains(viewType));
    JsonDbBtreeStorage *partition = findPartition(partitionName);
    ObjectTable *targetTable = partition->findObjectTable(viewType);
    quint32 targetStateNumber = qMax(1u, targetTable->stateNumber());
    if (gVerbose) qDebug() << "JsonDb::updateReduce" << viewType << targetStateNumber << (mViewsUpdating.contains(viewType) ? "already updating" : "") << "{";
    if (mViewsUpdating.contains(viewType)) {
        if (gVerbose) qDebug() << "}" << viewType;
        return;
    }
    mViewsUpdating.insert(viewType);
    QHash<QString,JsonDbReduceDefinition*> reduceDefinitions;
    QSet<ObjectTable*>                  sourceTables;
    QMultiMap<ObjectTable*,QString>     objectTableSourceType;
    QMap<QString, QJsonObject> addedReduceDefinitions;   // uuid -> added definition
    QMap<QString, QJsonObject> removedReduceDefinitions; // uuid -> removed definition
    quint32 endStateNumber =
        findUpdatedMapReduceDefinitions(partition, JsonDbString::kReduceTypeStr,
                                        viewType, targetStateNumber, addedReduceDefinitions, removedReduceDefinitions);
    for (QMap<QString,JsonDbReduceDefinition*>::const_iterator it = mReduceDefinitionsByTarget.find(viewType);
         (it != mReduceDefinitionsByTarget.end() && it.key() == viewType);
         ++it) {
        JsonDbReduceDefinition *def = it.value();
        if (addedReduceDefinitions.contains(def->uuid()) || removedReduceDefinitions.contains(def->uuid()))
            continue;
        const QString &sourceType = def->sourceType();
        ObjectTable *sourceTable = partition->findObjectTable(sourceType);

        sourceTables.insert(sourceTable);
        objectTableSourceType.insert(sourceTable, sourceType);
        reduceDefinitions.insert(sourceType, def);
    }
    if (sourceTables.isEmpty() && addedReduceDefinitions.isEmpty() && removedReduceDefinitions.isEmpty()) {
        mViewsUpdating.remove(viewType);
        if (gVerbose) qDebug() << "}" << viewType;
        return;
    }

    for (QMap<ObjectTable*,QString>::const_iterator it = objectTableSourceType.begin();
         it != objectTableSourceType.end();
         ++it) {
        QString sourceType = it.value();
        // make sure the source is updated
        updateView(sourceType);

        if (!endStateNumber) {
            ObjectTable *sourceTable = it.key();
            quint32 sourceStateNumber = sourceTable->stateNumber();
            endStateNumber = sourceStateNumber;
        }
    }

    // transaction private to this objecttable
    targetTable->begin();

    for (QMap<QString,QJsonObject>::const_iterator it = removedReduceDefinitions.begin();
         it != removedReduceDefinitions.end();
         ++it) {
        QJsonObject def = it.value();
        removeReduceDefinition(def, partitionName);
    }
    for (QMap<QString,QJsonObject>::const_iterator it = addedReduceDefinitions.begin();
         it != addedReduceDefinitions.end();
         ++it) {
        QJsonObject def = it.value();
        createReduceDefinition(def, true, partitionName);
    }

    for (QSet<ObjectTable*>::const_iterator it = sourceTables.begin();
         it != sourceTables.end();
         ++it) {
        ObjectTable *sourceTable = *it;
        QStringList sourceTypes = objectTableSourceType.values(sourceTable);
        QJsonObject changes = sourceTable->changesSince(targetStateNumber).value("result").toObject();
        quint32 count = changes.value("count").toDouble();
        QJsonArray changeList = changes.value("changes").toArray();
        for (quint32 i = 0; i < count; i++) {
            QJsonObject change = changeList.at(i).toObject();
            QJsonObject before = change.value("before").toObject();
            QJsonObject after = change.value("after").toObject();
            QString beforeType = before.value(JsonDbString::kTypeStr).toString();
            QString afterType = after.value(JsonDbString::kTypeStr).toString();
            if (sourceTypes.contains(beforeType))
              reduceDefinitions.value(beforeType)->updateObject(before, after);
            else if (sourceTypes.contains(afterType))
                reduceDefinitions.value(afterType)->updateObject(before, after);
        }
    }

    bool committed = targetTable->commit(endStateNumber);
    Q_ASSERT(committed);
    mViewsUpdating.remove(viewType);
    if (gVerbose) qDebug() << "}" << viewType;
}

JsonDbMapDefinition::JsonDbMapDefinition(JsonDb *jsonDb, JsonDbOwner *owner, const QString &partition, QJsonObject definition, QObject *parent)
    : QObject(parent)
    , mJsonDb(jsonDb)
    , mPartition(partition)
    , mOwner(owner)
    , mDefinition(definition)
    , mScriptEngine(0)
    , mUuid(definition.value(JsonDbString::kUuidStr).toString())
    , mTargetType(definition.value("targetType").toString())
    , mTargetTable(jsonDb->findPartition(partition)->findObjectTable(mTargetType))
{
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
        mSourceTypes = sourceFunctions.keys();
        for (int i = 0; i < mSourceTypes.size(); i++) {
            const QString &sourceType = mSourceTypes[i];
            const QString &script = sourceFunctions.value(sourceType).toString();
            QJSValue mapFunction =
                mScriptEngine->evaluate(QString("var map_%1 = (%2); map_%1;").arg(QString(sourceType).replace(".", "_")).arg(script));
            if (mapFunction.isError() || !mapFunction.isCallable())
                setError( "Unable to parse map function: " + mapFunction.toString());
            mMapFunctions[sourceType] = mapFunction;

            mSourceTables[sourceType] = mJsonDb->findPartition(mPartition)->findObjectTable(sourceType);
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

        mSourceTables[sourceType] = mJsonDb->findPartition(mPartition)->findObjectTable(sourceType);
        mSourceTypes.append(sourceType);

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
    mSourceUuids.append(uuid);
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
        mJsonDb->removeViewObject(mOwner, dependentObject, mPartition);
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
        mJsonDb->getObjects(findKey, JsonDb::fromJSValue(findValue), objectType);
    if (!getObjectResponse.error.isNull()) {
        if (gVerbose)
            qDebug() << "lookupRequested" << mSourceTypes << mTargetType
                     << getObjectResponse.error.toString();
        setError(getObjectResponse.error.toString());
    }
    JsonDbObjectList objectList = getObjectResponse.data;
    for (int i = 0; i < objectList.size(); ++i) {
        JsonDbObject object = objectList.at(i);
        const QString uuid = object.value(JsonDbString::kUuidStr).toString();
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
    newItem.insert("_mapUuid", mUuid);

    QJsonObject res = mJsonDb->createViewObject(mOwner, newItem, mPartition);
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
    if (JsonDbBtreeStorage *storage = mJsonDb->findPartition(mPartition)) {
        WithTransaction transaction(storage, "JsonDbMapDefinition::setError");
        ObjectTable *objectTable = storage->findObjectTable(JsonDbString::kMapTypeStr);
        transaction.addObjectTable(objectTable);
        JsonDbObject doc(mDefinition);
        JsonDbObject _delrec;
        storage->getObject(mUuid, _delrec, JsonDbString::kMapTypeStr);
        storage->updatePersistentObject(_delrec, doc);
        transaction.commit();
    }
}

JsonDbReduceDefinition::JsonDbReduceDefinition(JsonDb *jsonDb, JsonDbOwner *owner, const QString &partition,
                                               QJsonObject definition, QObject *parent)
    : QObject(parent)
    , mJsonDb(jsonDb)
    , mOwner(owner)
    , mPartition(partition)
    , mDefinition(definition)
    , mScriptEngine(0)
    , mUuid(mDefinition.value(JsonDbString::kUuidStr).toString())
    , mTargetType(mDefinition.value("targetType").toString())
    , mSourceType(mDefinition.value("sourceType").toString())
    , mTargetKeyName(mDefinition.contains("targetKeyName") ? mDefinition.value("targetKeyName").toString() : QString("key"))
    , mTargetValueName(mDefinition.contains("targetValueName") ? mDefinition.value("targetValueName").toString() : QString("value"))
    , mSourceKeyName(mDefinition.contains("sourceKeyName") ? mDefinition.value("sourceKeyName").toString() : QString("key"))
    , mSourceKeyNameList(mSourceKeyName.split("."))
{
}

void JsonDbReduceDefinition::initScriptEngine()
{
    if (mScriptEngine)
        return;

    mScriptEngine = new QJSEngine(this);
    Q_ASSERT(!mDefinition.value("add").toString().isEmpty());
    Q_ASSERT(!mDefinition.value("subtract").toString().isEmpty());

    QJSValue globalObject = mScriptEngine->globalObject();
    globalObject.setProperty("console", mScriptEngine->newQObject(new Console()));

    QString script = mDefinition.value("add").toString();
    mAddFunction = mScriptEngine->evaluate(QString("var %1 = (%2); %1;").arg("add").arg(script));

    if (mAddFunction.isError() || !mAddFunction.isCallable()) {
        setError("Unable to parse add function: " + mAddFunction.toString());
        return;
    }

    script = mDefinition.value("subtract").toString();
    mSubtractFunction = mScriptEngine->evaluate(QString("var %1 = (%2); %1;").arg("subtract").arg(script));

    if (mSubtractFunction.isError() || !mSubtractFunction.isCallable())
        setError("Unable to parse subtract function: " + mSubtractFunction.toString());
}

void JsonDbReduceDefinition::releaseScriptEngine()
{
    mAddFunction = QJSValue();
    mSubtractFunction = QJSValue();
    delete mScriptEngine;
    mScriptEngine = 0;
}

void JsonDbReduceDefinition::updateObject(JsonDbObject before, JsonDbObject after)
{
    initScriptEngine();
    Q_ASSERT(mAddFunction.isCallable());

    QJsonValue beforeKeyValue = mSourceKeyName.contains(".")
        ? JsonDb::propertyLookup(before, mSourceKeyNameList)
        : before.value(mSourceKeyName);
    QJsonValue afterKeyValue = mSourceKeyName.contains(".")
        ? JsonDb::propertyLookup(after, mSourceKeyNameList)
        : after.value(mSourceKeyName);

    if (!after.isEmpty() && !before.isEmpty() && (beforeKeyValue != afterKeyValue)) {
        // do a subtract only on the before key
        if (!beforeKeyValue.isUndefined())
            updateObject(before, QJsonObject());

        // and then continue here with the add with the after key
        before = QJsonObject();
    }

    const QJsonValue keyValue(after.isEmpty() ? beforeKeyValue : afterKeyValue);
    if (keyValue.isUndefined())
        return;

    GetObjectsResult getObjectResponse = mJsonDb->getObjects(mTargetKeyName, keyValue, mTargetType);
    if (!getObjectResponse.error.isNull())
        setError(getObjectResponse.error.toString());

    JsonDbObject previousObject;
    QJsonValue previousValue(QJsonValue::Undefined);

    JsonDbObjectList previousResults = getObjectResponse.data;
    for (int k = 0; k < previousResults.size(); ++k) {
        JsonDbObject previous = previousResults.at(k);
        if (previous.value("_reduceUuid").toString() == mUuid) {
            previousObject = previous;
            previousValue = previousObject;
            break;
        }
    }

    QJsonValue value = previousValue;
    if (!before.isEmpty())
        value = subtractObject(keyValue, value, before);
    if (!after.isEmpty())
        value = addObject(keyValue, value, after);

    QJsonObject res;
    // if we had a previous object to reduce
    if (previousObject.contains(JsonDbString::kUuidStr)) {
        // and now the value is undefined
        if (value.isUndefined()) {
            // then remove it
            res = mJsonDb->removeViewObject(mOwner, previousObject, mPartition);
        } else {
            //otherwise update it
            JsonDbObject reduced(value.toObject());
            reduced.insert(JsonDbString::kTypeStr, mTargetType);
            reduced.insert(JsonDbString::kUuidStr,
                         previousObject.value(JsonDbString::kUuidStr));
            reduced.insert(JsonDbString::kVersionStr,
                         previousObject.value(JsonDbString::kVersionStr));
            reduced.insert(mTargetKeyName, keyValue);
            reduced.insert("_reduceUuid", mUuid);
            res = mJsonDb->updateViewObject(mOwner, reduced, mPartition);
        }
    } else {
        // otherwise create the new object
        JsonDbObject reduced(value.toObject());
        reduced.insert(JsonDbString::kTypeStr, mTargetType);
        reduced.insert(mTargetKeyName, keyValue);
        reduced.insert("_reduceUuid", mUuid);
        res = mJsonDb->createViewObject(mOwner, reduced, mPartition);
    }

    if (JsonDb::responseIsError(res))
        setError("Error executing add function: " +
                 res.value(JsonDbString::kErrorStr).toObject().value(JsonDbString::kMessageStr).toString());
}

QJsonValue JsonDbReduceDefinition::addObject(const QJsonValue &keyValue, const QJsonValue &previousValue, JsonDbObject object)
{
    initScriptEngine();
    QJSValue svKeyValue = mScriptEngine->toScriptValue(keyValue.toVariant());
    QJSValue svPreviousValue = mScriptEngine->toScriptValue(previousValue.toObject().value(mTargetValueName).toVariant());
    QJSValue svObject = JsonDb::toJSValue(object, mScriptEngine);

    QJSValueList reduceArgs;
    reduceArgs << svKeyValue << svPreviousValue << svObject;
    QJSValue reduced = mAddFunction.call(reduceArgs);

    if (!reduced.isUndefined() && !reduced.isError()) {
        QJsonObject jsonReduced;
        jsonReduced.insert(mTargetValueName, QJsonValue::fromVariant(reduced.toVariant()));
        return jsonReduced;
    } else {

        if (reduced.isError())
            setError("Error executing add function: " + reduced.toString());

        return QJsonValue(QJsonValue::Undefined);
    }
}

QJsonValue JsonDbReduceDefinition::subtractObject(const QJsonValue &keyValue, const QJsonValue &previousValue, JsonDbObject object)
{
    initScriptEngine();
    Q_ASSERT(mSubtractFunction.isCallable());

    QJSValue svKeyValue = mScriptEngine->toScriptValue(keyValue.toVariant());
    QJSValue svPreviousValue = mScriptEngine->toScriptValue(previousValue.toObject().value(mTargetValueName).toVariant());
    QJSValue sv = JsonDb::toJSValue(object, mScriptEngine);

    QJSValueList reduceArgs;
    reduceArgs << svKeyValue << svPreviousValue << sv;
    QJSValue reduced = mSubtractFunction.call(reduceArgs);

    if (!reduced.isUndefined() && !reduced.isError()) {
        QJsonObject jsonReduced;
        jsonReduced.insert(mTargetValueName, QJsonValue::fromVariant(reduced.toVariant()));
        return jsonReduced;
    } else {
        if (reduced.isError())
            setError("Error executing subtract function: " + reduced.toString());
        return QJsonValue(QJsonValue::Undefined);
    }
}

bool JsonDbReduceDefinition::isActive() const
{
    return !mDefinition.contains(JsonDbString::kActiveStr) || mDefinition.value(JsonDbString::kActiveStr).toBool();
}

void JsonDbReduceDefinition::setError(const QString &errorMsg)
{
    mDefinition.insert(JsonDbString::kActiveStr, false);
    mDefinition.insert(JsonDbString::kErrorStr, errorMsg);
    if (JsonDbBtreeStorage *storage = mJsonDb->findPartition(mPartition)) {
        WithTransaction transaction(storage, "JsonDbReduceDefinition::setError");
        ObjectTable *objectTable = storage->findObjectTable(JsonDbString::kReduceTypeStr);
        transaction.addObjectTable(objectTable);
        JsonDbObject doc(mDefinition);
        JsonDbObject _delrec;
        storage->getObject(mUuid, _delrec, JsonDbString::kReduceTypeStr);
        storage->updatePersistentObject(_delrec, doc);
        transaction.commit();
    }
}


QT_END_NAMESPACE_JSONDB
