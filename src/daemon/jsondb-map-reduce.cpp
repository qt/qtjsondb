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

#include "jsondb-trace.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QRegExp>
#include <QJSValue>
#include <QStringList>

#include <QtJsonDbQson/private/qson_p.h>
#include "qsonconversion.h"

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
        QsonMap getObjectResponse = getObjects(JsonDbString::kTypeStr, "Map", partition);
        QsonList mrdList = getObjectResponse.subList("result");
        for (int i = 0; i < mrdList.size(); ++i) {
            QsonMap mrd = mrdList.at<QsonMap>(i);
            createMapDefinition(mrd, false, partition);
            storage->addView(mrd.value<QString>("targetType"));
        }
    }
    {
        QsonMap getObjectResponse = getObjects(JsonDbString::kTypeStr, "Reduce", partition);
        QsonList mrdList = getObjectResponse.subList("result");
        for (int i = 0; i < mrdList.size(); ++i) {
            QsonMap mrd = mrdList.at<QsonMap>(i);
            createReduceDefinition(mrd, false, partition);
            storage->addView(mrd.value<QString>("targetType"));
        }
    }
}

void JsonDb::createMapDefinition(QsonMap mapDefinition, bool firstTime, const QString &partition)
{
    QString targetType = mapDefinition.valueString("targetType");
    QStringList sourceTypes;
    if (mapDefinition.contains("join"))
        sourceTypes = mapDefinition.subObject("join").toMap().keys();
    else if (mapDefinition.contains("sourceType")) // deprecated
        sourceTypes.append(mapDefinition.valueString("sourceType"));
    else
        sourceTypes = mapDefinition.subObject("map").toMap().keys();

    if (gVerbose)
        qDebug() << "createMapDefinition" << sourceTypes << targetType;

    JsonDbBtreeStorage *storage = findPartition(partition);
    storage->addView(targetType);
    storage->addIndex("_sourceUuids.*", "string", targetType);
    storage->addIndex("_mapUuid", "string", targetType);

    JsonDbMapDefinition *def = new JsonDbMapDefinition(this, mOwner, partition, mapDefinition, this);
    for (int i = 0; i < sourceTypes.size(); i++)
        mMapDefinitionsBySource.insert(sourceTypes[i], def);
    mMapDefinitionsByTarget.insert(targetType, def);

    if (firstTime && def->isActive()) {
        QsonMap getObjectResponse = getObjects(JsonDbString::kTypeStr, sourceTypes[0]);
        if (getObjectResponse.contains("error")) {
            if (gVerbose)
                qDebug() << "createMapDefinition" << sourceTypes << targetType << getObjectResponse.valueString("error");
            def->setError(getObjectResponse.valueString("error"));
        };
        QsonList objects = getObjectResponse.subList("result");
        for (int i = 0; i < objects.size(); i++)
            def->mapObject(objects.objectAt(i));
    }
}

void JsonDb::removeMapDefinition(QsonMap mapDefinition, const QString &partition)
{
    QString targetType = mapDefinition.valueString("targetType");

    JsonDbMapDefinition *def = 0;
    foreach (JsonDbMapDefinition *d, mMapDefinitionsByTarget.values(targetType)) {
        if (d->uuid() == mapDefinition.valueString(JsonDbString::kUuidStr)) {
            def = d;
            mMapDefinitionsByTarget.remove(targetType, def);
            const QStringList &sourceTypes = def->sourceTypes();
            for (int i = 0; i < sourceTypes.size(); i++)
                mMapDefinitionsBySource.remove(sourceTypes[i], def);

            break;
        }
    }

    // remove the output objects
    QsonMap getObjectResponse = getObjects("_mapUuid",
                                          mapDefinition.valueString(JsonDbString::kUuidStr), targetType);
    QsonList objects = getObjectResponse.subList("result");
    for (int i = 0; i < objects.size(); i++)
      removeViewObject(mOwner, objects.objectAt(i), partition);
}

void JsonDb::createReduceDefinition(QsonMap reduceDefinition, bool firstTime, const QString &partition)
{
    QString targetType = reduceDefinition.valueString("targetType");
    QString sourceType = reduceDefinition.valueString("sourceType");

    if (gDebug)
        qDebug() << "createReduceDefinition" << targetType << sourceType;

    JsonDbBtreeStorage *storage = findPartition(partition);
    storage->addView(targetType);

    JsonDbReduceDefinition *def = new JsonDbReduceDefinition(this, mOwner, partition, reduceDefinition, this);
    mReduceDefinitionsBySource.insert(sourceType, def);
    mReduceDefinitionsByTarget.insert(targetType, def);

    storage->addIndex("_sourceUuids.*", "string", targetType);
    storage->addIndex(def->sourceKeyName(), "string", sourceType);
    storage->addIndex(def->targetKeyName(), "string", targetType);
    storage->addIndex("_reduceUuid", "string", targetType);

    if (firstTime && def->isActive()) {
        QsonMap getObjectResponse = getObjects(JsonDbString::kTypeStr, sourceType);
        if (getObjectResponse.contains("error")) {
            if (gVerbose)
                qDebug() << "createReduceDefinition" << targetType << getObjectResponse.valueString("error");
            def->setError(getObjectResponse.valueString("error"));
        }
        QsonList objects = getObjectResponse.subList("result");
        for (int i = 0; i < objects.size(); i++)
            def->updateObject(QsonMap(), objects.objectAt(i));
    }
}

void JsonDb::removeReduceDefinition(QsonMap reduceDefinition,const QString &partition)
{
    QString targetType = reduceDefinition.valueString("targetType");
    QString sourceType = reduceDefinition.valueString("sourceType");

    if (gVerbose)
        qDebug() << "removeReduceDefinition" << sourceType <<  targetType;

    JsonDbReduceDefinition *def = 0;
    foreach (JsonDbReduceDefinition *d, mReduceDefinitionsBySource.values(sourceType)) {
        if (d->uuid() == reduceDefinition.valueString(JsonDbString::kUuidStr)) {
            def = d;
            mReduceDefinitionsBySource.remove(def->sourceType(), def);
            mReduceDefinitionsByTarget.remove(def->targetType(), def);
            break;
        }
    }
    // remove the output objects
    QsonMap getObjectResponse = getObjects("_reduceUuid", reduceDefinition.valueString(JsonDbString::kUuidStr), targetType);
    QsonList objects = getObjectResponse.subList("result");
    for (int i = 0; i < objects.size(); i++)
      removeViewObject(mOwner, objects.objectAt(i), partition);
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
    QMap<QString, QsonMap> addedMapDefinitions;   // uuid -> added definition
    QMap<QString, QsonMap> removedMapDefinitions; // uuid -> removed definition
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

    for (QMap<QString,QsonMap>::const_iterator it = removedMapDefinitions.begin();
         it != removedMapDefinitions.end();
         ++it) {
        QsonMap def = it.value();
        removeMapDefinition(def, partitionName);
    }
    for (QMap<QString,QsonMap>::const_iterator it = addedMapDefinitions.begin();
         it != addedMapDefinitions.end();
         ++it) {
        QsonMap def = it.value();
        createMapDefinition(def, true, partitionName);
    }

    for (QSet<ObjectTable*>::const_iterator it = sourceTables.begin();
         it != sourceTables.end();
         ++it) {
        ObjectTable *sourceTable = *it;
        QStringList sourceTypes = objectTableSourceType.values(sourceTable);
        QsonMap changes = sourceTable->changesSince(targetStateNumber).subObject("result");
        quint32 count = changes.valueInt("count", 0);
        QsonList changeList = changes.subList("changes");
        for (quint32 i = 0; i < count; i++) {
            QsonMap change = changeList.objectAt(i).toMap();
            QsonMap before = change.subObject("before").toMap();
            QsonMap after = change.subObject("after").toMap();
            if (!before.isEmpty()) {
                QString sourceType = before.valueString(JsonDbString::kTypeStr);
                if (sourceTypes.contains(sourceType)) {
                    mapDefinitions.value(sourceType)->unmapObject(before);
                }
            }
            if (!after.isEmpty()) {
                QString sourceType = after.valueString(JsonDbString::kTypeStr);
                if (sourceTypes.contains(sourceType)) {
                    Q_ASSERT(mapDefinitions.value(sourceType));
                    mapDefinitions.value(sourceType)->mapObject(after);
                }
            }
        }
    }

    targetTable->commit(endStateNumber);
    mViewsUpdating.remove(viewType);
}

quint32 JsonDb::findUpdatedMapReduceDefinitions(JsonDbBtreeStorage *partition, const QString &definitionType,
                                                const QString &viewType, quint32 targetStateNumber,
                                                QMap<QString,QsonMap> &addedDefinitions, QMap<QString,QsonMap> &removedDefinitions) const
{
    ObjectTable *objectTable = partition->findObjectTable(definitionType);
    quint32 stateNumber = objectTable->stateNumber();
    if (stateNumber == targetStateNumber)
        return stateNumber;
    QSet<QString> limitTypes;
    limitTypes << definitionType;
    QsonMap changes = objectTable->changesSince(targetStateNumber, limitTypes).subObject("result");
    quint32 count = changes.valueInt("count", 0);
    QsonList changeList = changes.subList("changes");
    for (quint32 i = 0; i < count; i++) {
        QsonMap change = changeList.objectAt(i).toMap();
        if (change.contains("before")) {
            QsonMap before = change.subObject("before").toMap();
            if ((before.valueString(JsonDbString::kTypeStr) == definitionType)
                && (before.valueString("targetType") == viewType))
                removedDefinitions.insert(before.valueString(JsonDbString::kUuidStr), before);
        }
        if (change.contains("after")) {
            QsonMap after = change.subObject("after").toMap();
            if ((after.valueString(JsonDbString::kTypeStr) == definitionType)
                && (after.valueString("targetType") == viewType))
                addedDefinitions.insert(after.valueString(JsonDbString::kUuidStr), after);
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
    if (gVerbose) qDebug() << "JsonDb::updateReduce" << viewType << targetStateNumber << (mViewsUpdating.contains(viewType) ? "already updating" : "");
    if (mViewsUpdating.contains(viewType))
        return;
    mViewsUpdating.insert(viewType);
    QHash<QString,JsonDbReduceDefinition*> reduceDefinitions;
    QSet<ObjectTable*>                  sourceTables;
    QMultiMap<ObjectTable*,QString>     objectTableSourceType;
    QMap<QString, QsonMap> addedReduceDefinitions;   // uuid -> added definition
    QMap<QString, QsonMap> removedReduceDefinitions; // uuid -> removed definition
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

    for (QMap<QString,QsonMap>::const_iterator it = removedReduceDefinitions.begin();
         it != removedReduceDefinitions.end();
         ++it) {
        QsonMap def = it.value();
        removeReduceDefinition(def, partitionName);
    }
    for (QMap<QString,QsonMap>::const_iterator it = addedReduceDefinitions.begin();
         it != addedReduceDefinitions.end();
         ++it) {
        QsonMap def = it.value();
        createReduceDefinition(def, true, partitionName);
    }

    for (QSet<ObjectTable*>::const_iterator it = sourceTables.begin();
         it != sourceTables.end();
         ++it) {
        ObjectTable *sourceTable = *it;
        QStringList sourceTypes = objectTableSourceType.values(sourceTable);
        QsonMap changes = sourceTable->changesSince(targetStateNumber).subObject("result");
        QsonList changeList = changes.subList("changes");
        quint32 count = changeList.size();
        for (quint32 i = 0; i < count; i++) {
            QsonMap change = changeList.objectAt(i).toMap();
            QsonMap before = change.subObject("before").toMap();
            QsonMap after = change.subObject("after").toMap();
            QString beforeType = before.valueString(JsonDbString::kTypeStr);
            QString afterType = after.valueString(JsonDbString::kTypeStr);
            if (sourceTypes.contains(beforeType))
                reduceDefinitions.value(beforeType)->updateObject(before, after);
            else if (sourceTypes.contains(afterType))
                reduceDefinitions.value(afterType)->updateObject(before, after);
        }
    }

    targetTable->commit(endStateNumber);
    mViewsUpdating.remove(viewType);
}

JsonDbMapDefinition::JsonDbMapDefinition(JsonDb *jsonDb, JsonDbOwner *owner, const QString &partition, QsonMap definition, QObject *parent)
    : QObject(parent)
    , mJsonDb(jsonDb)
    , mPartition(partition)
    , mOwner(owner)
    , mDefinition(definition)
    , mScriptEngine(new QJSEngine(this))
    , mUuid(definition.valueString(JsonDbString::kUuidStr))
    , mTargetType(definition.valueString("targetType"))
    , mTargetTable(jsonDb->findPartition(partition)->findObjectTable(mTargetType))
{
    QJSValue globalObject = mScriptEngine->globalObject();
    globalObject.setProperty("console", mScriptEngine->newQObject(new Console()));

    if (!mDefinition.contains("sourceType")) {
        QsonMap sourceFunctions(mDefinition.contains("join")
                                ? mDefinition.subObject("join")
                                : mDefinition.subObject("map"));
        mSourceTypes = sourceFunctions.keys();
        for (int i = 0; i < mSourceTypes.size(); i++) {
            const QString &sourceType = mSourceTypes[i];
            const QString &script = sourceFunctions.valueString(sourceType);
            QJSValue mapFunction =
                mScriptEngine->evaluate(QString("var map_%1 = (%2); map_%1;").arg(QString(sourceType).replace(".", "_")).arg(script));
            if (mapFunction.isError() || !mapFunction.isFunction())
                setError( "Unable to parse map function: " + mapFunction.toString());
            mMapFunctions[sourceType] = mapFunction;

            mSourceTables[sourceType] = jsonDb->findPartition(partition)->findObjectTable(sourceType);
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
        const QString sourceType = mDefinition.valueString("sourceType");
        const QString &script = mDefinition.valueString("map");
        QJSValue mapFunction =
            mScriptEngine->evaluate(QString("var map_%1 = (%2); map_%1;").arg(QString(sourceType).replace(".", "_")).arg(script));
        if (mapFunction.isError() || !mapFunction.isFunction())
            setError( "Unable to parse map function: " + mapFunction.toString());
        mMapFunctions[sourceType] = mapFunction;

        mSourceTables[sourceType] = jsonDb->findPartition(partition)->findObjectTable(sourceType);
        mSourceTypes.append(sourceType);

        mMapProxy = new JsonDbMapProxy(mOwner, mJsonDb, this);
        connect(mMapProxy, SIGNAL(lookupRequested(QJSValue,QJSValue)),
                this, SLOT(lookupRequested(QJSValue,QJSValue)));
        connect(mMapProxy, SIGNAL(viewObjectEmitted(QJSValue)),
                this, SLOT(viewObjectEmitted(QJSValue)));
        globalObject.setProperty("jsondb", mScriptEngine->newQObject(mMapProxy));
        qWarning() << "Old-style Map from sourceType" << sourceType << " to targetType" << mDefinition.valueString("targetType");
    }
}

QJSValue JsonDbMapDefinition::mapFunction(const QString &sourceType) const
{
    if (mMapFunctions.contains(sourceType))
        return mMapFunctions[sourceType];
    else
        return QJSValue();
}

void JsonDbMapDefinition::mapObject(QsonMap object)
{
    QJSValue globalObject = mScriptEngine->globalObject();
    const QString &sourceType = object.valueString(JsonDbString::kTypeStr);

    QJSValue sv = qsonToJSValue(object, mScriptEngine);
    QString uuid = object.valueString(JsonDbString::kUuidStr);
    mSourceUuids.clear();
    mSourceUuids.append(uuid);
    QJSValue maped;

    QJSValueList mapArgs;
    mapArgs << sv;
    maped = mapFunction(sourceType).call(globalObject, mapArgs);

    if (maped.isError())
        setError("Error executing map function: " + maped.toString());
}

void JsonDbMapDefinition::unmapObject(const QsonMap &object)
{
    QString uuid = object.valueString(JsonDbString::kUuidStr);
    QsonMap getObjectResponse = mTargetTable->getObjects("_sourceUuids.*", uuid, mTargetType);
    QsonList dependentObjects = getObjectResponse.subList("result");

    for (int i = 0; i < dependentObjects.size(); i++) {
        QsonMap dependentObject = dependentObjects.objectAt(i).toMap();
        if (dependentObject.valueString(JsonDbString::kTypeStr) != mTargetType)
            continue;
        mJsonDb->removeViewObject(mOwner, dependentObject, mPartition);
    }
}

void JsonDbMapDefinition::lookupRequested(const QJSValue &query, const QJSValue &context)
{
    //qDebug() << "lookupRequested" << query.toVariant() << context.toVariant();
    QString objectType = query.property("objectType").toString();
    // compatibility for old style maps
    if (mDefinition.valueType("map") == QsonObject::MapType) {
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
    QJSValue findValue = query.property("value").toString();
    QsonMap getObjectResponse =
        mJsonDb->getObjects(findKey, findValue.toVariant(), objectType);
    if (getObjectResponse.contains("error")) {
        if (gVerbose)
            qDebug() << "lookupRequested" << mSourceTypes << mTargetType << getObjectResponse.valueString("error");
        setError(getObjectResponse.valueString("error"));
    }
    QsonList objectList = getObjectResponse.subList("result");
    for (int i = 0; i < objectList.size(); ++i) {
        QsonMap object = objectList.at<QsonMap>(i);
        const QString uuid = object.valueString(JsonDbString::kUuidStr);
        mSourceUuids.append(uuid);
        QJSValueList mapArgs;
        QJSValue sv = qsonToJSValue(object, mScriptEngine);

        mapArgs << sv << context;
        QJSValue globalObject = mScriptEngine->globalObject();
        QJSValue maped = mMapFunctions[objectType].call(globalObject, mapArgs);

        if (maped.isError())
            setError("Error executing map function during lookup: " + maped.toString());

        mSourceUuids.removeLast();
    }
}

void JsonDbMapDefinition::viewObjectEmitted(const QJSValue &value)
{
    QsonMap newItem(variantToQson(value.toVariant()));
    newItem.insert(JsonDbString::kTypeStr, mTargetType);
    QsonList sourceUuids;
    foreach (const QString &str, mSourceUuids)
        sourceUuids.append(str);
    newItem.insert("_sourceUuids", sourceUuids);
    newItem.insert("_mapUuid", mUuid);

    QsonMap res = mJsonDb->createViewObject(mOwner, newItem, mPartition);
    if (JsonDb::responseIsError(res))
        setError("Error executing map function during emitViewObject: " +
                 res.subObject(JsonDbString::kErrorStr).valueString(JsonDbString::kMessageStr));
}

bool JsonDbMapDefinition::isActive() const
{
    return mDefinition.isNull(JsonDbString::kActiveStr) || mDefinition.valueBool(JsonDbString::kActiveStr);
}

void JsonDbMapDefinition::setError(const QString &errorMsg)
{
    mDefinition.insert(JsonDbString::kActiveStr, false);
    mDefinition.insert(JsonDbString::kErrorStr, errorMsg);
    if (JsonDbBtreeStorage *storage = mJsonDb->findPartition(mPartition)) {
        WithTransaction transaction(storage, "JsonDbMapDefinition::setError");
        ObjectTable *objectTable = storage->findObjectTable(JsonDbString::kMapTypeStr);
        transaction.addObjectTable(objectTable);
        storage->updatePersistentObject(mDefinition);
        transaction.commit();
    }
}

JsonDbReduceDefinition::JsonDbReduceDefinition(JsonDb *jsonDb, JsonDbOwner *owner, const QString &partition,
                                               QsonMap definition, QObject *parent)
    : QObject(parent)
    , mJsonDb(jsonDb)
    , mOwner(owner)
    , mPartition(partition)
    , mDefinition(definition)
    , mScriptEngine(new QJSEngine(this))
    , mUuid(mDefinition.valueString(JsonDbString::kUuidStr))
    , mTargetType(mDefinition.valueString("targetType"))
    , mSourceType(mDefinition.valueString("sourceType"))
    , mTargetKeyName(mDefinition.contains("targetKeyName") ? mDefinition.valueString("targetKeyName") : QString("key"))
    , mTargetValueName(mDefinition.contains("targetValueName") ? mDefinition.valueString("targetValueName") : QString("value"))
    , mSourceKeyName(mDefinition.contains("sourceKeyName") ? mDefinition.valueString("sourceKeyName") : QString("key"))
{
    Q_ASSERT(!mDefinition.valueString("add").isEmpty());
    Q_ASSERT(!mDefinition.valueString("subtract").isEmpty());

    QJSValue globalObject = mScriptEngine->globalObject();
    globalObject.setProperty("console", mScriptEngine->newQObject(new Console()));

    QString script = mDefinition.valueString("add");
    mAddFunction = mScriptEngine->evaluate(QString("var %1 = (%2); %1;").arg("add").arg(script));

    if (mAddFunction.isError() || !mAddFunction.isFunction()) {
        setError("Unable to parse add function: " + mAddFunction.toString());
        return;
    }

    script = mDefinition.valueString("subtract");
    mSubtractFunction = mScriptEngine->evaluate(QString("var %1 = (%2); %1;").arg("subtract").arg(script));

    if (mSubtractFunction.isError() || !mSubtractFunction.isFunction())
        setError("Unable to parse subtract function: " + mSubtractFunction.toString());
}

void JsonDbReduceDefinition::updateObject(QsonMap before, QsonMap after)
{
    Q_ASSERT(mAddFunction.isFunction());

    QString beforeKeyValue = mSourceKeyName.contains(".") ? JsonDb::propertyLookup(before, mSourceKeyName).toString()
        : before.valueString(mSourceKeyName);
    QString afterKeyValue = mSourceKeyName.contains(".") ? JsonDb::propertyLookup(after, mSourceKeyName).toString()
        : after.valueString(mSourceKeyName);

    if (!after.isEmpty() && (beforeKeyValue != afterKeyValue)) {
        // do a subtract only on the before key
        //qDebug() << "beforeKeyValue" << beforeKeyValue << "afterKeyValue" << afterKeyValue;
        //qDebug() << "before" << before << endl << "after" << after << endl;
        if (!beforeKeyValue.isEmpty())
            updateObject(before, QsonMap());

        // and then continue here with the add with the after key
        before = QsonMap();
    }

    const QString keyValue(after.isEmpty() ? beforeKeyValue : afterKeyValue);
    if (keyValue.isEmpty())
        return;

    QsonMap getObjectResponse = mJsonDb->getObjects(mTargetKeyName, keyValue, mTargetType);
    if (getObjectResponse.contains("error")) {
        qDebug() << "JsonDbReduceDefinition::updateObject" << mTargetType << getObjectResponse.valueString("error");
        setError(getObjectResponse.valueString("error"));
    }
    QsonMap previousResult;
    QsonObject previousValue;

    QsonList previousResults = getObjectResponse.subList("result");
    for (int k = 0; k < previousResults.size(); ++k) {
        QsonMap previous = previousResults.at<QsonMap>(k);
        if (previous.valueString("_reduceUuid") == mUuid) {
            previousResult = previous;

            if (!previousResult.subObject(mTargetValueName).isEmpty())
                previousValue = previousResult.subObject(mTargetValueName);
            break;
        }
    }

    QsonMap value(previousResult);
    if (!before.isEmpty())
        value = subtractObject(keyValue, value, before);
    if (!after.isEmpty())
        value = addObject(keyValue, value, after);

    QsonMap res;
    if (!previousResult.valueString(JsonDbString::kUuidStr).isEmpty()) {
        if (value.isEmpty()) {
            res = mJsonDb->removeViewObject(mOwner, previousResult, mPartition);
        } else {
            value.insert(JsonDbString::kTypeStr, mTargetType);
            value.insert(JsonDbString::kUuidStr,
                         previousResult.valueString(JsonDbString::kUuidStr));
            value.insert(JsonDbString::kVersionStr,
                         previousResult.valueString(JsonDbString::kVersionStr));
            value.insert(mTargetKeyName, keyValue);
            value.insert("_reduceUuid", mUuid);
            res = mJsonDb->updateViewObject(mOwner, value, mPartition);
        }
    } else {
        value.insert(JsonDbString::kTypeStr, mTargetType);
        value.insert(mTargetKeyName, keyValue);
        value.insert("_reduceUuid", mUuid);
        res = mJsonDb->createViewObject(mOwner, value, mPartition);
    }

    if (JsonDb::responseIsError(res))
        setError("Error executing add function: " +
                 res.subObject(JsonDbString::kErrorStr).valueString(JsonDbString::kMessageStr));
}

QsonObject JsonDbReduceDefinition::addObject(const QString &keyValue, const QsonObject &previousValue, QsonMap object)
{
    QJSValue globalObject = mScriptEngine->globalObject();
    QJSValue svKeyValue = mScriptEngine->toScriptValue(keyValue);
    QJSValue svPreviousValue = mScriptEngine->toScriptValue(qsonToVariant(previousValue).toMap().value(mTargetValueName));
    QJSValue svObject = qsonToJSValue(object, mScriptEngine);

    QJSValueList reduceArgs;
    reduceArgs << svKeyValue << svPreviousValue << svObject;
    QJSValue reduced = mAddFunction.call(globalObject, reduceArgs);

    if (!reduced.isUndefined() && !reduced.isError()) {
        QVariantMap vReduced;
        vReduced.insert(mTargetValueName, reduced.toVariant());
        return variantToQson(vReduced);
    } else {

        if (reduced.isError())
            setError("Error executing add function: " + reduced.toString());

        return QsonObject();
    }
}

QsonObject JsonDbReduceDefinition::subtractObject(const QString &keyValue, const QsonObject &previousValue, QsonMap object)
{
    Q_ASSERT(mSubtractFunction.isFunction());

    QJSValue globalObject = mScriptEngine->globalObject();

    QJSValue svKeyValue = mScriptEngine->toScriptValue(keyValue);
    QJSValue svPreviousValue = mScriptEngine->toScriptValue(qsonToVariant(previousValue).toMap().value(mTargetValueName));
    QJSValue sv = qsonToJSValue(object, mScriptEngine);

    QJSValueList reduceArgs;
    reduceArgs << svKeyValue << svPreviousValue << sv;
    QJSValue reduced = mSubtractFunction.call(globalObject, reduceArgs);

    if (!reduced.isUndefined() && !reduced.isError()) {
        QVariantMap vReduced;
        vReduced.insert(mTargetValueName, reduced.toVariant());
        return variantToQson(vReduced);
    } else {
        if (reduced.isError())
            setError("Error executing subtract function: " + reduced.toString());
        return QsonObject();
    }
}

bool JsonDbReduceDefinition::isActive() const
{
    return mDefinition.isNull(JsonDbString::kActiveStr) || mDefinition.valueBool(JsonDbString::kActiveStr);
}

void JsonDbReduceDefinition::setError(const QString &errorMsg)
{
    mDefinition.insert(JsonDbString::kActiveStr, false);
    mDefinition.insert(JsonDbString::kErrorStr, errorMsg);
    if (JsonDbBtreeStorage *storage = mJsonDb->findPartition(mPartition)) {
        WithTransaction transaction(storage, "JsonDbReduceDefinition::setError");
        ObjectTable *objectTable = storage->findObjectTable(JsonDbString::kMapTypeStr);
        transaction.addObjectTable(objectTable);
        storage->updatePersistentObject(mDefinition);
        transaction.commit();
    }
}


QT_END_NAMESPACE_JSONDB
