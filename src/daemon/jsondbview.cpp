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

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QElapsedTimer>

#include "jsondb-global.h"
#include "jsondb.h"
#include "jsondbpartition.h"
#include "jsondbobject.h"
#include "jsondbview.h"
#include "jsondbmapdefinition.h"
#include "jsondbobjecttable.h"
#include "jsondbreducedefinition.h"

QT_BEGIN_NAMESPACE_JSONDB

#ifndef QT_NO_DEBUG_OUTPUT
extern bool gDebug;
#define DBG() if (gDebug) qDebug() << Q_FUNC_INFO << __LINE__
#else
#define DBG() if (0) qDebug() << Q_FUNC_INFO
#endif

JsonDbView::JsonDbView(JsonDb *jsonDb, JsonDbPartition *partition, const QString &viewType, QObject *parent)
  : QObject(parent)
  , mJsonDb(jsonDb)
  , mPartition(partition)
  , mObjectTable(0)
  , mViewType(viewType)
{
    mObjectTable = new JsonDbObjectTable(mPartition);
}

JsonDbView::~JsonDbView()
{
    close();
    delete mObjectTable;
}

void JsonDbView::open()
{
    QFileInfo fi(mPartition->filename());
    QString dirName = fi.dir().path();
    QString baseName = fi.fileName();
    baseName.replace(".db", "");
    if (!mObjectTable->open(QString("%1/%2-%3-View.db")
                               .arg(dirName)
                               .arg(baseName)
                               .arg(mViewType),
                               QBtree::NoSync | QBtree::UseSyncMarker)) {
        qCritical() << "viewDb->open" << mObjectTable->errorMessage();
        return;
    }
}

void JsonDbView::close()
{
    if (mObjectTable)
        mObjectTable->close();
}

void JsonDbView::initViews(JsonDbPartition *partition, const QString &partitionName)
{
    if (gVerbose) qDebug() << "Initializing views on partition" << partitionName;
    {
        JsonDbObjectList mrdList = partition->getObjects(JsonDbString::kTypeStr, QJsonValue(QLatin1String("Map")),
                                                         partitionName).data;
        for (int i = 0; i < mrdList.size(); ++i) {
            JsonDbObject mrd = mrdList.at(i);
            JsonDbView *view = partition->addView(mrd.value("targetType").toString());
            view->createJsonDbMapDefinition(mrd, false);
        }
    }
    {
        JsonDbObjectList mrdList = partition->getObjects(JsonDbString::kTypeStr, QJsonValue(QLatin1String("Reduce")),
                                                         partitionName).data;
        for (int i = 0; i < mrdList.size(); ++i) {
            JsonDbObject mrd = mrdList.at(i);
            JsonDbView *view = partition->addView(mrd.value("targetType").toString());
            view->createJsonDbReduceDefinition(mrd, false);
        }
    }
}

void JsonDbView::createJsonDbMapDefinition(QJsonObject mapDefinition, bool firstTime)
{
    QString targetType = mapDefinition.value("targetType").toString();
    QStringList sourceTypes;
    bool isJoin = mapDefinition.contains("join");
    if (isJoin)
        sourceTypes = mapDefinition.value("join").toObject().keys();
    else if (mapDefinition.contains("sourceType")) // deprecated
        sourceTypes.append(mapDefinition.value("sourceType").toString());
    else
        sourceTypes = mapDefinition.value("map").toObject().keys();

    if (gVerbose)
        qDebug() << "createJsonDbMapDefinition" << sourceTypes << targetType;

    mObjectTable->addIndexOnProperty("_sourceUuids.*", "string", targetType);

    const JsonDbOwner *owner = mJsonDb->findOwner(mapDefinition.value(JsonDbString::kOwnerStr).toString());
    JsonDbMapDefinition *def = new JsonDbMapDefinition(mJsonDb, owner, mPartition, mapDefinition, this);
    for (int i = 0; i < sourceTypes.size(); i++)
        mJsonDbMapDefinitionsBySource.insert(sourceTypes[i], def);
    mJsonDbMapDefinitions.insert(def);

    if (firstTime && def->isActive()) {
        def->initScriptEngine();
        foreach (const QString &sourceType, sourceTypes) {
            GetObjectsResult getObjectResponse = mPartition->getObjects(JsonDbString::kTypeStr, sourceType);
            if (!getObjectResponse.error.isNull()) {
                if (gVerbose)
                    qDebug() << "createJsonDbMapDefinition" << sourceTypes << sourceType << targetType << getObjectResponse.error.toString();
                def->setError(getObjectResponse.error.toString());
                return;
            }
            JsonDbObjectList objects = getObjectResponse.data;
            for (int i = 0; i < objects.size(); i++) {
                JsonDbObject object(objects.at(i));
                if (isJoin)
                    def->unmapObject(object);
                def->mapObject(objects.at(i));
            }
        }
    }
    updateSourceTypesList();
}

void JsonDbView::removeJsonDbMapDefinition(QJsonObject mapDefinition)
{
    QString targetType = mapDefinition.value("targetType").toString();

    JsonDbMapDefinition *def = 0;
    foreach (JsonDbMapDefinition *d, mJsonDbMapDefinitions) {
        if (d->uuid() == mapDefinition.value(JsonDbString::kUuidStr).toString()) {
            def = d;
            mJsonDbMapDefinitions.remove(def);
            const QStringList &sourceTypes = def->sourceTypes();
            for (int i = 0; i < sourceTypes.size(); i++)
                mJsonDbMapDefinitionsBySource.remove(sourceTypes[i], def);

            break;
        }
    }

    // remove the output objects
    GetObjectsResult getObjectResponse = mObjectTable->getObjects("_sourceUuids.*",
                                                                  mapDefinition.value(JsonDbString::kUuidStr), targetType);
    JsonDbObjectList objects = getObjectResponse.data;
    for (int i = 0; i < objects.size(); i++)
        mJsonDb->removeViewObject(def->owner(), objects.at(i), mPartition->name());
    updateSourceTypesList();
}

void JsonDbView::createJsonDbReduceDefinition(QJsonObject reduceDefinition, bool firstTime)
{
    QString targetType = reduceDefinition.value("targetType").toString();
    QString sourceType = reduceDefinition.value("sourceType").toString();

    if (gDebug)
        qDebug() << "createJsonDbReduceDefinition" << targetType << sourceType;

    const JsonDbOwner *owner = mJsonDb->findOwner(reduceDefinition.value(JsonDbString::kOwnerStr).toString());
    JsonDbReduceDefinition *def = new JsonDbReduceDefinition(mJsonDb, owner, mPartition, reduceDefinition, this);
    mJsonDbReduceDefinitionsBySource.insert(sourceType, def);
    mJsonDbReduceDefinitions.insert(def);

    // TODO: this index should not be automatic
    mObjectTable->addIndexOnProperty(def->sourceKeyName(), "string", sourceType);
    // TODO: this index should not be automatic
    mObjectTable->addIndexOnProperty(def->targetKeyName(), "string", targetType);
    mObjectTable->addIndexOnProperty("_reduceUuid", "string", targetType);

    if (firstTime && def->isActive()) {
        def->initScriptEngine();
        GetObjectsResult getObjectResponse = mPartition->getObjects(JsonDbString::kTypeStr, sourceType);
        if (!getObjectResponse.error.isNull()) {
            if (gVerbose)
                qDebug() << "createJsonDbReduceDefinition" << targetType << getObjectResponse.error.toString();
            def->setError(getObjectResponse.error.toString());
        }
        JsonDbObjectList objects = getObjectResponse.data;
        for (int i = 0; i < objects.size(); i++) {
            def->updateObject(QJsonObject(), objects.at(i));
        }
    }
    updateSourceTypesList();
}

void JsonDbView::removeJsonDbReduceDefinition(QJsonObject reduceDefinition)
{
    QString targetType = reduceDefinition.value("targetType").toString();
    QString sourceType = reduceDefinition.value("sourceType").toString();

    if (gVerbose)
        qDebug() << "removeJsonDbReduceDefinition" << sourceType <<  targetType;

    JsonDbReduceDefinition *def = 0;
    foreach (JsonDbReduceDefinition *d, mJsonDbReduceDefinitionsBySource.values(sourceType)) {
        if (d->uuid() == reduceDefinition.value(JsonDbString::kUuidStr).toString()) {
            def = d;
            mJsonDbReduceDefinitionsBySource.remove(def->sourceType(), def);
            mJsonDbReduceDefinitions.remove(def);
            break;
        }
    }
    // remove the output objects
    GetObjectsResult getObjectResponse = mObjectTable->getObjects("_reduceUuid", reduceDefinition.value(JsonDbString::kUuidStr), targetType);
    JsonDbObjectList objects = getObjectResponse.data;
    for (int i = 0; i < objects.size(); i++)
        mJsonDb->removeViewObject(def->owner(), objects.at(i), mPartition->name());
    //TODO: actually remove the table
    updateSourceTypesList();
}

void JsonDbView::updateSourceTypesList()
{
    QSet<QString> sourceTypes;
    foreach (const JsonDbMapDefinition *d, mJsonDbMapDefinitions) {
        foreach (const QString sourceType, d->sourceTypes())
            sourceTypes.insert(sourceType);
    }
    foreach (const JsonDbReduceDefinition *d, mJsonDbReduceDefinitions)
        sourceTypes.insert(d->sourceType());
    mSourceTypes = sourceTypes.toList();
}

void JsonDbView::updateView()
{
    QElapsedTimer timer;
    if (gPerformanceLog)
        timer.start();
    //qDebug() << endl << "updateView" << mViewType << "{" << endl;
    updateMap();
    updateReduce();
    //qDebug() << endl << "}" << "updateView" << mViewType << endl;
    if (gPerformanceLog)
        qDebug() << "updateView" << mViewType << timer.elapsed() << "ms";
}

void JsonDbView::updateMap()
{
    JsonDbObjectTable *targetTable = mObjectTable;
    quint32 targetStateNumber = qMax(1u, targetTable->stateNumber());
    if (gVerbose) qDebug() << "JsonDb::updateMap" << mViewType << targetStateNumber << "targetStateNumber" << targetStateNumber;

    QHash<QString,JsonDbMapDefinition *> mapDefinitions;
    QSet<JsonDbObjectTable *>                  sourceTables;
    QMultiMap<JsonDbObjectTable *,QString>     objectTableSourceType;
    QMap<QString, QJsonObject> addedJsonDbMapDefinitions;   // uuid -> added definition
    QMap<QString, QJsonObject> removedJsonDbMapDefinitions; // uuid -> removed definition
    quint32 endStateNumber =
        findUpdatedMapJsonDbReduceDefinitions(JsonDbString::kMapTypeStr,
                                        mViewType, targetStateNumber, addedJsonDbMapDefinitions, removedJsonDbMapDefinitions);
    for (QSet<JsonDbMapDefinition*>::const_iterator it = mJsonDbMapDefinitions.begin();
         it != mJsonDbMapDefinitions.end();
         ++it) {
        JsonDbMapDefinition *def = *it;
        const QStringList &sourceTypes = def->sourceTypes();
        for (int i = 0; i < sourceTypes.size(); i++) {
            const QString &sourceType = sourceTypes[i];
            JsonDbObjectTable *sourceTable = def->sourceTable(sourceType);
            sourceTables.insert(sourceTable);
            objectTableSourceType.insert(sourceTable, sourceType);
            mapDefinitions.insert(sourceType, def);
        }
    }
    if (sourceTables.isEmpty() && addedJsonDbMapDefinitions.isEmpty() && removedJsonDbMapDefinitions.isEmpty()) {
        return;
    }

    for (QMap<JsonDbObjectTable *,QString>::const_iterator it = objectTableSourceType.begin();
         it != objectTableSourceType.end();
         ++it) {
        QString sourceType = it.value();
        // make sure the source is updated
        mPartition->updateView(sourceType);

        if (!endStateNumber) {
            JsonDbObjectTable *sourceTable = it.key();
            quint32 sourceStateNumber = sourceTable->stateNumber();
            endStateNumber = sourceStateNumber;
        }
    }

    // this transaction private to targetTable
    targetTable->begin();

    for (QMap<QString,QJsonObject>::const_iterator it = removedJsonDbMapDefinitions.begin();
         it != removedJsonDbMapDefinitions.end();
         ++it) {
        QJsonObject def = it.value();
        removeJsonDbMapDefinition(def);
    }
    for (QMap<QString,QJsonObject>::const_iterator it = addedJsonDbMapDefinitions.begin();
         it != addedJsonDbMapDefinitions.end();
         ++it) {
        QJsonObject def = it.value();
        createJsonDbMapDefinition(def, true);
    }

    for (QSet<JsonDbObjectTable *>::const_iterator it = sourceTables.begin();
         it != sourceTables.end();
         ++it) {
        JsonDbObjectTable *sourceTable = *it;
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
    Q_UNUSED(ok);
    Q_ASSERT(ok);
}

void JsonDbView::updateReduce()
{
    //Q_ASSERT(mJsonDbReduceDefinitions.contains(mViewType));
    JsonDbPartition *partition = mPartition;
    JsonDbObjectTable *targetTable = partition->findObjectTable(mViewType);
    quint32 targetStateNumber = qMax(1u, targetTable->stateNumber());
    if (gVerbose) qDebug() << "JsonDb::updateReduce" << mViewType << targetStateNumber << "{";
    QHash<QString,JsonDbReduceDefinition *> reduceDefinitions;
    QSet<JsonDbObjectTable *>                  sourceTables;
    QMultiMap<JsonDbObjectTable *,QString>     objectTableSourceType;
    QMap<QString, QJsonObject> addedJsonDbReduceDefinitions;   // uuid -> added definition
    QMap<QString, QJsonObject> removedJsonDbReduceDefinitions; // uuid -> removed definition
    quint32 endStateNumber =
        findUpdatedMapJsonDbReduceDefinitions(JsonDbString::kReduceTypeStr,
                                        mViewType, targetStateNumber, addedJsonDbReduceDefinitions, removedJsonDbReduceDefinitions);
    for (QSet<JsonDbReduceDefinition*>::const_iterator it = mJsonDbReduceDefinitions.begin();
         it != mJsonDbReduceDefinitions.end();
         ++it) {
        JsonDbReduceDefinition *def = *it;
        if (addedJsonDbReduceDefinitions.contains(def->uuid()) || removedJsonDbReduceDefinitions.contains(def->uuid()))
            continue;
        const QString &sourceType = def->sourceType();
        JsonDbObjectTable *sourceTable = partition->findObjectTable(sourceType);

        sourceTables.insert(sourceTable);
        objectTableSourceType.insert(sourceTable, sourceType);
        reduceDefinitions.insert(sourceType, def);
    }
    if (sourceTables.isEmpty() && addedJsonDbReduceDefinitions.isEmpty() && removedJsonDbReduceDefinitions.isEmpty()) {
        if (gVerbose) qDebug() << "}" << mViewType;
        return;
    }

    for (QMap<JsonDbObjectTable*,QString>::const_iterator it = objectTableSourceType.begin();
         it != objectTableSourceType.end();
         ++it) {
        QString sourceType = it.value();
        // make sure the source is updated
        mPartition->updateView(sourceType);

        if (!endStateNumber) {
            JsonDbObjectTable *sourceTable = it.key();
            quint32 sourceStateNumber = sourceTable->stateNumber();
            endStateNumber = sourceStateNumber;
        }
    }

    // transaction private to this objecttable
    targetTable->begin();

    for (QMap<QString,QJsonObject>::const_iterator it = removedJsonDbReduceDefinitions.begin();
         it != removedJsonDbReduceDefinitions.end();
         ++it) {
        QJsonObject def = it.value();
        removeJsonDbReduceDefinition(def);
    }
    for (QMap<QString,QJsonObject>::const_iterator it = addedJsonDbReduceDefinitions.begin();
         it != addedJsonDbReduceDefinitions.end();
         ++it) {
        QJsonObject def = it.value();
        createJsonDbReduceDefinition(def, true);
    }

    for (QSet<JsonDbObjectTable*>::const_iterator it = sourceTables.begin();
         it != sourceTables.end();
         ++it) {
        JsonDbObjectTable *sourceTable = *it;
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
    Q_UNUSED(committed);
    Q_ASSERT(committed);
    if (gVerbose) qDebug() << "}" << mViewType;
}

quint32 JsonDbView::findUpdatedMapJsonDbReduceDefinitions(const QString &definitionType,
                                                    const QString &viewType, quint32 targetStateNumber,
                                                    QMap<QString,QJsonObject> &addedDefinitions, QMap<QString,QJsonObject> &removedDefinitions) const
{
    JsonDbObjectTable *objectTable = mPartition->findObjectTable(definitionType);
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

void JsonDbView::reduceMemoryUsage()
{
    mObjectTable->flushCaches();

    for (QSet<JsonDbMapDefinition*>::iterator it = mJsonDbMapDefinitions.begin();
         it != mJsonDbMapDefinitions.end();
         ++it)
        (*it)->releaseScriptEngine();
    for (QSet<JsonDbReduceDefinition*>::iterator it = mJsonDbReduceDefinitions.begin();
         it != mJsonDbReduceDefinitions.end();
         ++it)
        (*it)->releaseScriptEngine();
}

QT_END_NAMESPACE_JSONDB
