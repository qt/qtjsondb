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

#include "jsondbpartition.h"
#include "jsondbobject.h"
#include "jsondbview.h"
#include "jsondbmapdefinition.h"
#include "jsondbobjecttable.h"
#include "jsondbreducedefinition.h"
#include "jsondbsettings.h"
#include "jsondbscriptengine.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

JsonDbView::JsonDbView(JsonDbPartition *partition, const QString &viewType, QObject *parent) :
    QObject(parent)
  , mPartition(partition)
  , mViewObjectTable(0)
  , mMainObjectTable(mPartition->mainObjectTable())
  , mViewStateNumber(0)
  , mViewType(viewType)
  , mUpdating(false)
{
    mViewObjectTable = new JsonDbObjectTable(mPartition);
}

JsonDbView::~JsonDbView()
{
    close();
    delete mViewObjectTable;
}

void JsonDbView::open()
{
    QFileInfo fi(mPartition->filename());
    QString dirName = fi.dir().path();
    QString baseName = fi.fileName();
    baseName.replace(QStringLiteral(".db"), QStringLiteral(""));
    if (!mViewObjectTable->open(QString::fromLatin1("%1/%2-%3-View.db")
                               .arg(dirName)
                               .arg(baseName)
                               .arg(mViewType))) {
        qCritical() << "viewDb->open" << mViewObjectTable->errorMessage();
        return;
    }
}

void JsonDbView::close()
{
    if (mViewObjectTable)
        mViewObjectTable->close();
}

void JsonDbView::initViews(JsonDbPartition *partition)
{
    if (jsondbSettings->verbose())
        qDebug() << "Initializing views on partition" << partition->name();
    {
        JsonDbObjectList mrdList = partition->getObjects(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr).data;

        for (int i = 0; i < mrdList.size(); ++i) {
            JsonDbObject mrd = mrdList.at(i);
            JsonDbView *view = partition->addView(mrd.value(QStringLiteral("targetType")).toString());
            view->createMapDefinition(mrd);
        }
    }
    {
        JsonDbObjectList mrdList = partition->getObjects(JsonDbString::kTypeStr, JsonDbString::kReduceTypeStr).data;

        for (int i = 0; i < mrdList.size(); ++i) {
            JsonDbObject mrd = mrdList.at(i);
            JsonDbView *view = partition->addView(mrd.value(QStringLiteral("targetType")).toString());
            view->createReduceDefinition(mrd);
        }
    }
}

void JsonDbView::createDefinition(JsonDbPartition *partition, QJsonObject definition)
{
    QString definitionType = definition.value(JsonDbString::kTypeStr).toString();
    QString targetType = definition.value(QStringLiteral("targetType")).toString();
    JsonDbView *view = partition->findView(targetType);
     if (!view)
        return;
    if (jsondbSettings->verbose())
        qDebug() << "createDefinition" << targetType;
    if (definitionType == JsonDbString::kMapTypeStr)
        view->createMapDefinition(definition);
    else
        view->createReduceDefinition(definition);
}
void JsonDbView::removeDefinition(JsonDbPartition *partition, QJsonObject definition)
{
    QString definitionType = definition.value(JsonDbString::kTypeStr).toString();
    QString targetType = definition.value(QStringLiteral("targetType")).toString();
    JsonDbView *view = partition->findView(targetType);
     if (!view)
        return;
    if (jsondbSettings->verbose())
        qDebug() << "removeDefinition" << targetType;

    if (definitionType == JsonDbString::kMapTypeStr)
        view->removeMapDefinition(definition);
    else
        view->removeReduceDefinition(definition);
}


void JsonDbView::createMapDefinition(QJsonObject mapDefinition)
{
    QString targetType = mapDefinition.value(QStringLiteral("targetType")).toString();
    QString uuid = mapDefinition.value(JsonDbString::kUuidStr).toString();
    if (jsondbSettings->verbose())
        qDebug() << "createMapDefinition" << uuid << targetType << "{";

    JsonDbOwner *owner = new JsonDbOwner(this);
    owner->setAllowAll(true);
    JsonDbMapDefinition *def = new JsonDbMapDefinition(owner, mPartition, mapDefinition, this);
    def->initIndexes();

    QStringList sourceTypes = def->sourceTypes();
    for (int i = 0; i < sourceTypes.size(); i++) {
        const QString sourceType = sourceTypes[i];
        mMapDefinitionsBySource.insert(sourceType, def);
    }
    mMapDefinitions.insert(def->uuid(), def);
    updateSourceTypesList();
    if (jsondbSettings->verbose())
        qDebug() << "createMapDefinition" << uuid << targetType << "}";
}

void JsonDbView::removeMapDefinition(QJsonObject mapDefinition)
{
    QString targetType = mapDefinition.value(QStringLiteral("targetType")).toString();
    QString uuid = mapDefinition.value(JsonDbString::kUuidStr).toString();
    if (jsondbSettings->verbose())
        qDebug() << "removeMapDefinition" << uuid << targetType << "{";
    foreach (JsonDbMapDefinition *d, mMapDefinitions) {
        if (d->uuid() == uuid) {
            JsonDbMapDefinition *def = d;
            mMapDefinitions.remove(def->uuid());
            const QStringList &sourceTypes = def->sourceTypes();
            for (int i = 0; i < sourceTypes.size(); i++)
                mMapDefinitionsBySource.remove(sourceTypes[i]);
            break;
        }
    }

    updateSourceTypesList();
    if (jsondbSettings->verbose())
        qDebug() << "removeMapDefinition" << uuid << targetType << "}";
}

void JsonDbView::createReduceDefinition(QJsonObject reduceDefinition)
{
    QString targetType = reduceDefinition.value(QStringLiteral("targetType")).toString();
    QString sourceType = reduceDefinition.value(QStringLiteral("sourceType")).toString();
    if (jsondbSettings->debug())
        qDebug() << "createReduceDefinition" << sourceType << targetType << sourceType << "{";

    JsonDbOwner *owner = new JsonDbOwner(this);
    owner->setAllowAll(true);
    JsonDbReduceDefinition *def = new JsonDbReduceDefinition(owner, mPartition, reduceDefinition, this);
    def->initIndexes();
    mReduceDefinitionsBySource.insert(sourceType, def);
    mReduceDefinitions.insert(def->uuid(), def);

    updateSourceTypesList();
    if (jsondbSettings->verbose())
        qDebug() << "createReduceDefinition" << sourceType << targetType << "}";
}

void JsonDbView::removeReduceDefinition(QJsonObject reduceDefinition)
{
    QString targetType = reduceDefinition.value(QStringLiteral("targetType")).toString();
    QString sourceType = reduceDefinition.value(QStringLiteral("sourceType")).toString();
    QString uuid = reduceDefinition.value(JsonDbString::kUuidStr).toString();

    if (jsondbSettings->verbose())
        qDebug() << "removeReduceDefinition" << sourceType <<  targetType << "{";

    foreach (JsonDbReduceDefinition *d, mReduceDefinitionsBySource.values(sourceType)) {
        if (d->uuid() == uuid) {
            JsonDbReduceDefinition *def = d;
            mReduceDefinitionsBySource.remove(def->sourceType());
            mReduceDefinitions.remove(def->uuid());
            break;
        }
    }

    updateSourceTypesList();
    //TODO: actually remove the table
    if (jsondbSettings->verbose())
        qDebug() << "removeReduceDefinition" << sourceType <<  targetType << "}";
}

void JsonDbView::updateSourceTypesList()
{
    QSet<QString> sourceTypes;
    foreach (const JsonDbMapDefinition *d, mMapDefinitions) {
        foreach (const QString sourceType, d->sourceTypes())
            sourceTypes.insert(sourceType);
    }
    foreach (const JsonDbReduceDefinition *d, mReduceDefinitions)
        sourceTypes.insert(d->sourceType());

    ObjectTableSourceTypeMap objectTableSourceTypeMap;
    for (QSet<QString>::const_iterator it = sourceTypes.begin();
         it != sourceTypes.end();
         ++it) {
        QString sourceType = *it;
        JsonDbObjectTable *objectTable = mPartition->findObjectTable(sourceType);
        objectTableSourceTypeMap[objectTable].insert(sourceType);
    }
    mSourceTypes = sourceTypes.toList();
    mObjectTableSourceTypeMap = objectTableSourceTypeMap;
}

void JsonDbView::updateView(quint32 desiredStateNumber, JsonDbUpdateList *resultingChanges)
{
    QElapsedTimer timer;
    if (jsondbSettings->performanceLog())
        timer.start();
    if (jsondbSettings->verbose())
        qDebug() << "updateView" << mViewType << "{";
    // current state of the main object table of the partition
    quint32 partitionStateNumber = mMainObjectTable->stateNumber();
    quint32 viewStateNumber = (mViewStateNumber ? mViewStateNumber : mViewObjectTable->stateNumber());

    // if the view is up to date, then return
    if ((desiredStateNumber && viewStateNumber >= desiredStateNumber)
        || viewStateNumber == partitionStateNumber) {
        if (jsondbSettings->verbose())
            qDebug() << "updateView" << mViewType << "}";
        return;
    }
    if (mUpdating) {
        if (jsondbSettings->verbose())
            qDebug() << "Update already in progess"
                     << "updateView" << mViewType << "}";
        return;
    }
    mUpdating = true;

    // update the source types in case they are views
    for (JsonDbView::ObjectTableSourceTypeMap::const_iterator it = mObjectTableSourceTypeMap.begin();
         it != mObjectTableSourceTypeMap.end();
         ++it) {
        const QSet<QString> &sourceTypes = it.value();
        QString sourceType = *sourceTypes.begin();
        // update the source tables (as indicated by one of the source types)
        mPartition->updateView(sourceType);
    }

    // The Uuids of the definitions processed for the first time by processUpdatedDefinitions.
    QSet<QString> processedDefinitionUuids;

    // find any Map or Reduce definitions that have been updated from
    // states viewStateNumber to partitionStateNumber and process them.
    bool inTransaction =
      processUpdatedDefinitions(mViewType, viewStateNumber, processedDefinitionUuids);
    // Do not process them again during this update of the view.

    if (jsondbSettings->verbose())
        qDebug() << "processedDefinitionUuids" << processedDefinitionUuids;
    // if there were any updated definitions, then it would have
    // already begun the transaction on the view's mObjectTable.
    if (!inTransaction)
      mViewObjectTable->begin();
    inTransaction = true;

    // now process the changes on each of the source tables
    for (ObjectTableSourceTypeMap::const_iterator it = mObjectTableSourceTypeMap.begin();
         it != mObjectTableSourceTypeMap.end();
         ++it) {
        JsonDbObjectTable *sourceTable = it.key();
        const QSet<QString> &sourceTypes = it.value();
        if (sourceTable->stateNumber() == viewStateNumber)
            // up-to-date with respect this source table
            continue;

        QList<JsonDbUpdate> changeList;
        sourceTable->changesSince(viewStateNumber, sourceTypes, &changeList, JsonDbObjectTable::SplitTypeChanges);
        updateViewOnChanges(changeList, processedDefinitionUuids, resultingChanges);
    }
    mViewStateNumber = partitionStateNumber;
    mUpdating = false;
    if (inTransaction)
        mViewObjectTable->commit(partitionStateNumber);
    if (jsondbSettings->verbose())
        qDebug() << "}" << "updateView" << mViewType << partitionStateNumber;
    if (jsondbSettings->performanceLog())
        qDebug() << "updateView" << "stateNumber" << mViewStateNumber << mViewType << timer.elapsed() << "ms";

    emit updated(mViewType);
}

void JsonDbView::updateEagerView(const JsonDbUpdateList &objectsUpdated, JsonDbUpdateList *resultingChanges)
{
    quint32 partitionStateNumber = mMainObjectTable->stateNumber();
    quint32 viewStateNumber = mViewStateNumber;

    // make sure we can run this set of updates
    if (mViewStateNumber != (partitionStateNumber - 1)
        || viewDefinitionUpdated(objectsUpdated)) {
        // otherwise do a full update
        if (jsondbSettings->verbose())
            qDebug() << "updateEagerView" << mViewType << "full update"
                     << "viewStateNumber" << mViewStateNumber << "partitionStateNumber" << partitionStateNumber
                     << (viewDefinitionUpdated(objectsUpdated) ? "definition updated" : "");
        updateView(partitionStateNumber, resultingChanges);
        return;
    }

    QElapsedTimer timer;
    if (jsondbSettings->performanceLog())
        timer.start();
    if (jsondbSettings->verbose())
        qDebug() << "updateEagerView" << mViewType << "{";

    // begin transaction
    mViewObjectTable->begin();

    // then do the update
    QSet<QString> processedDefinitionUuids;
    updateViewOnChanges(objectsUpdated, processedDefinitionUuids, resultingChanges);

    // end transaction
    mViewObjectTable->commit(partitionStateNumber);
    mViewStateNumber = partitionStateNumber;

    if (jsondbSettings->verbose())
        qDebug() << "updateEagerView" << mViewType << viewStateNumber << "}";
    if (jsondbSettings->performanceLog())
        qDebug() << "updateEagerView" << "stateNumber" << mViewStateNumber << mViewType << timer.elapsed() << "ms";
}

// Updates the in-memory state numbers on the view so that we know it
// has seen all relevant updates from this transaction
void JsonDbView::updateViewStateNumber(quint32 partitionStateNumber)
{
    // make sure we've updated the view the first time
    if (!mViewStateNumber)
        return;

    // If the change is not zero or one it's an error
    if (jsondbSettings->verbose() || (mViewStateNumber != (partitionStateNumber - 1) && mViewStateNumber != partitionStateNumber))
        qCritical() << "updateViewStateNumber" << mViewType << "viewStateNumber" << mViewStateNumber << "partitionStateNumber" << partitionStateNumber;
    mViewStateNumber = partitionStateNumber;
}

bool JsonDbView::viewDefinitionUpdated(const JsonDbUpdateList &objectsUpdated) const
{
    foreach (const JsonDbUpdate &update, objectsUpdated) {
        QJsonObject beforeObject = update.oldObject;
        QJsonObject afterObject = update.newObject;
        QString beforeUuid = beforeObject.value(JsonDbString::kUuidStr).toString();
        QString afterUuid = afterObject.value(JsonDbString::kUuidStr).toString();

        if ((!beforeObject.isEmpty()
             && (mMapDefinitions.contains(beforeUuid) || mReduceDefinitions.contains(beforeUuid)))
            || (!afterObject.isEmpty()
                && (mMapDefinitions.contains(afterUuid) || mReduceDefinitions.contains(afterUuid))))
            return false;
    }
    return false;
}

void JsonDbView::updateViewOnChanges(const JsonDbUpdateList &objectsUpdated,
                                     QSet<QString> &processedDefinitionUuids,
                                     JsonDbUpdateList *changeList)
{
    foreach (const JsonDbUpdate &update, objectsUpdated) {
        QJsonObject beforeObject = update.oldObject;
        QJsonObject afterObject = update.newObject;
        QString beforeType = beforeObject.value(JsonDbString::kTypeStr).toString();
        QString afterType = afterObject.value(JsonDbString::kTypeStr).toString();

        if (mMapDefinitionsBySource.contains(beforeType)) {
            JsonDbMapDefinition *def = mMapDefinitionsBySource.value(beforeType);
            if (processedDefinitionUuids.contains(def->uuid()))
                continue;
            def->updateObject(beforeObject, afterObject, changeList);
        } else if (mMapDefinitionsBySource.contains(afterType)) {
            JsonDbMapDefinition *def = mMapDefinitionsBySource.value(afterType);
            if (processedDefinitionUuids.contains(def->uuid()))
                continue;
            def->updateObject(beforeObject, afterObject, changeList);
        }

        if (mReduceDefinitionsBySource.contains(beforeType)) {
            JsonDbReduceDefinition *def = mReduceDefinitionsBySource.value(beforeType);
            if (processedDefinitionUuids.contains(def->uuid()))
                continue;
            def->updateObject(beforeObject, afterObject, changeList);
        } else if (mReduceDefinitionsBySource.contains(afterType)) {
            JsonDbReduceDefinition *def = mReduceDefinitionsBySource.value(afterType);
            if (processedDefinitionUuids.contains(def->uuid()))
                continue;
            def->updateObject(beforeObject, afterObject, changeList);
        }
    }
}

bool JsonDbView::processUpdatedDefinitions(const QString &viewType, quint32 targetStateNumber,
                                           QSet<QString> &processedDefinitionUuids)
{
    bool inTransaction = false;
    quint32 stateNumber = mMainObjectTable->stateNumber();
    if (stateNumber == targetStateNumber)
        return inTransaction;
    QSet<QString> limitTypes;
    limitTypes << JsonDbString::kMapTypeStr << JsonDbString::kReduceTypeStr;
    QList<JsonDbUpdate> changeList;
    mMainObjectTable->changesSince(targetStateNumber, limitTypes, &changeList, JsonDbObjectTable::SplitTypeChanges);
    foreach (const JsonDbUpdate &change, changeList) {
        QString definitionUuid;
        JsonDbNotification::Action action = change.action;
        JsonDbObject before = change.oldObject;
        JsonDbObject after = change.newObject;
        QString beforeType = before.value(JsonDbString::kTypeStr).toString();
        QString afterType = after.value(JsonDbString::kTypeStr).toString();
        if (jsondbSettings->verbose())
            qDebug() << "definition change" << change;
        if (action != JsonDbNotification::Create) {
            if (limitTypes.contains(beforeType)
                && (before.value(QStringLiteral("targetType")).toString() == viewType)) {
                if (!inTransaction) {
                    mViewObjectTable->begin();
                    inTransaction = true;
                }
                definitionUuid = before.value(JsonDbString::kUuidStr).toString();
                QString definitionType = before.value(JsonDbString::kTypeStr).toString();
                QString targetType = before.value(QStringLiteral("targetType")).toString();
                if (definitionType == JsonDbString::kMapTypeStr)
                    JsonDbMapDefinition::definitionRemoved(mPartition, mViewObjectTable, targetType, definitionUuid);
                else
                    JsonDbReduceDefinition::definitionRemoved(mPartition, mViewObjectTable, targetType, definitionUuid);
            }
        }
        if (action != JsonDbNotification::Delete) {
            if ((limitTypes.contains(afterType))
                && (after.value(QStringLiteral("targetType")).toString() == viewType)) {
                if (!inTransaction) {
                    mViewObjectTable->begin();
                    inTransaction = true;
                }
                definitionUuid = after.value(JsonDbString::kUuidStr).toString();
                QString definitionType = after.value(JsonDbString::kTypeStr).toString();
                if (!after.contains(JsonDbString::kActiveStr) || after.value(JsonDbString::kActiveStr).toBool()) {
                    if (definitionType == JsonDbString::kMapTypeStr)
                        mMapDefinitions.value(definitionUuid)->definitionCreated();
                    else
                        mReduceDefinitions.value(definitionUuid)->definitionCreated();
                }
            }
        }
        if (!definitionUuid.isEmpty())
            processedDefinitionUuids.insert(definitionUuid);
    }
    return inTransaction;
}

void JsonDbView::reduceMemoryUsage()
{
    mViewObjectTable->flushCaches();

    for (QMap<QString,JsonDbMapDefinition*>::iterator it = mMapDefinitions.begin();
         it != mMapDefinitions.end();
         ++it)
        it.value()->releaseScriptEngine();
    for (QMap<QString,JsonDbReduceDefinition*>::iterator it = mReduceDefinitions.begin();
         it != mReduceDefinitions.end();
         ++it)
        it.value()->releaseScriptEngine();
}

bool JsonDbView::isActive() const
{
    foreach (JsonDbMapDefinition *mapDef, mMapDefinitions) {
        if (mapDef->isActive())
            return true;
    }

    foreach (JsonDbReduceDefinition *reduceDef, mReduceDefinitions) {
        if (reduceDef->isActive())
            return true;
    }

    return false;
}

#include "moc_jsondbview.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
