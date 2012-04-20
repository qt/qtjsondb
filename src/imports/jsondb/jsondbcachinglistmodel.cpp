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

//#define JSONDB_LISTMODEL_DEBUG
//#define JSONDB_LISTMODEL_BENCHMARK

#include "jsondbcachinglistmodel.h"
#include "jsondbcachinglistmodel_p.h"
#include "plugin.h"

#include <QJSEngine>
#include <QJSValueIterator>
#include <QDebug>
#ifdef JSONDB_LISTMODEL_BENCHMARK
#include <QElapsedTimer>
#endif

/*!
  \internal
  \class JsonDbCachingListModel
*/

QT_BEGIN_NAMESPACE_JSONDB

JsonDbCachingListModelPrivate::JsonDbCachingListModelPrivate(JsonDbCachingListModel *q)
    : q_ptr(q)
    , componentComplete(false)
    , resetModel(true)
    , cacheSize(-1)
    , state(JsonDbCachingListModel::None)
    , errorCode(0)
    , lastQueriedIndex(-1)
{
    setCacheParams(INT_MAX/10);
}

void JsonDbCachingListModelPrivate::init()
{
}

void JsonDbCachingListModelPrivate::setCacheParams(int maxItems)
{
    objectCache.setPageSize(maxItems);
    chunkSize = objectCache.chunkSize();
    lowWaterMark = objectCache.chunkSize()/4;
}

JsonDbCachingListModelPrivate::~JsonDbCachingListModelPrivate()
{
    clearNotifications();
    while (!keyRequests.isEmpty()) {
        delete keyRequests[0];
        keyRequests.removeFirst();
    }
    while (!indexRequests.isEmpty()) {
        delete indexRequests[0];
        indexRequests.removeFirst();
    }
    while (!valueRequests.isEmpty()) {
        delete valueRequests[0];
        valueRequests.removeFirst();
    }

}

// insert item notification handler
// + add items, for chunked read
void JsonDbCachingListModelPrivate::addItem(const QJsonObject &item, int partitionIndex)
{
    Q_Q(JsonDbCachingListModel);
    const QString &uuid = item.value(QLatin1String("_uuid")).toString();
    // ignore duplicates.
    if (objectSortValues.contains(uuid))
        return;

    QVariantList vl;
    vl.append(uuid);
    vl.append(item.value(QLatin1String("_indexValue")).toVariant());
    SortingKey key(partitionIndex, vl, QList<bool>() << ascendingOrder, partitionIndexDetails[partitionIndex].spec);
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    QMap<SortingKey, QString>::const_iterator end = objectUuids.constEnd();
    QMap<SortingKey, QString>::const_iterator i = objectUuids.upperBound(key);
    int index = iterator_position(begin, end, i);
    if (index <= lastQueriedIndex)
        lastQueriedIndex++;

    q->beginInsertRows(parent, index, index);
    objectUuids.insert(key, uuid);
    objectCache.insert(index, uuid, item, objectUuids);
    partitionObjectUuids[partitionIndex].insert(key, uuid);
    objectSortValues.insert(uuid, key);
    q->endInsertRows();
    emit q->rowCountChanged(objectSortValues.count());
}


// deleteitem notification handler
void JsonDbCachingListModelPrivate::deleteItem(const QJsonObject &item, int partitionIndex)
{
    Q_Q(JsonDbCachingListModel);
    QString uuid = item.value(QLatin1String("_uuid")).toString();
    QMap<QString, SortingKey>::const_iterator keyIndex =  objectSortValues.constFind(uuid);
    if (keyIndex != objectSortValues.constEnd()) {
        SortingKey key = keyIndex.value();
        QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
        QMap<SortingKey, QString>::const_iterator end = objectUuids.constEnd();
        QMap<SortingKey, QString>::const_iterator i = objectUuids.constFind(key);
        if (i != end) {
            int index = iterator_position(begin, end, i);
            q->beginRemoveRows(parent, index, index);
            objectCache.remove(index, uuid);
            partitionObjectUuids[partitionIndex].remove(key);
            objectUuids.remove(key);
            objectSortValues.remove(uuid);
            if (index == lastQueriedIndex)
                lastQueriedIndex = -1;
            else if (index < lastQueriedIndex)
                lastQueriedIndex--;
            q->endRemoveRows();
            emit q->rowCountChanged(objectUuids.count());
        }
    }
}

// updateitem notification handler
void JsonDbCachingListModelPrivate::updateItem(const QJsonObject &item, int partitionIndex)
{
    Q_Q(JsonDbCachingListModel);
    QString uuid = item.value(QLatin1String("_uuid")).toString();
    QMap<QString, SortingKey>::const_iterator keyIndex = objectSortValues.constFind(uuid);
    if (keyIndex != objectSortValues.constEnd()) {
        SortingKey key = keyIndex.value();
        QVariantList vl;
        vl.append(uuid);
        vl.append(item.value(QLatin1String("_indexValue")).toVariant());
        SortingKey newKey(partitionIndex, vl, QList<bool>() << ascendingOrder, partitionIndexDetails[partitionIndex].spec);
        QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
        QMap<SortingKey, QString>::const_iterator end = objectUuids.constEnd();
        QMap<SortingKey, QString>::const_iterator oldPos = objectUuids.constFind(key);
        int oldIndex = iterator_position(begin, end, oldPos);
        if (oldIndex == lastQueriedIndex) // Cached object has changed
            lastQueriedIndex = -1;
        // keys are same, modify the object
        if (key == newKey) {
            objectCache.update(uuid, item);
            QModelIndex modelIndex = q->createIndex(oldIndex, 0);
            emit q->dataChanged(modelIndex, modelIndex);
            return;
        }
        // keys are different
        QMap<SortingKey, QString>::const_iterator newPos = objectUuids.upperBound(newKey);
        int newIndex = iterator_position(begin, end, newPos);
        if ((newIndex != oldIndex) && (newIndex != oldIndex+1)) {
            if (oldIndex < lastQueriedIndex && newIndex > lastQueriedIndex)
                lastQueriedIndex--;
            else if (oldIndex > lastQueriedIndex && newIndex <= lastQueriedIndex)
                lastQueriedIndex++;
            q->beginMoveRows(parent, oldIndex, oldIndex, parent, newIndex);
            objectUuids.remove(key);
            partitionObjectUuids[partitionIndex].remove(key);
            objectCache.remove(oldIndex, uuid);

            objectUuids.insert(newKey, uuid);
            partitionObjectUuids[partitionIndex].insert(newKey, uuid);

            objectSortValues.remove(uuid);
            objectSortValues.insert(uuid, newKey);

            // recompute the new position
            newPos = objectUuids.constFind(newKey);
            begin = objectUuids.constBegin();
            end = objectUuids.constEnd();
            newIndex = iterator_position(begin, end, newPos);
            objectCache.insert(newIndex, uuid, item, objectUuids);
            q->endMoveRows();
            // send data changed and return
            QModelIndex modelIndex = q->createIndex(newIndex, 0);
            emit q->dataChanged(modelIndex, modelIndex);
        } else {
            // same position, update the object
            objectCache.update(uuid, item);
            objectUuids.remove(key);
            objectUuids.insert(newKey, uuid);
            partitionObjectUuids[partitionIndex].remove(key);
            partitionObjectUuids[partitionIndex].insert(newKey, uuid);
            objectSortValues.remove(uuid);
            objectSortValues.insert(uuid, newKey);
            newPos = objectUuids.constFind(newKey);
            begin = objectUuids.constBegin();
            end = objectUuids.constEnd();
            newIndex = iterator_position(begin, end, newPos);
            QModelIndex modelIndex = q->createIndex(newIndex, 0);
            emit q->dataChanged(modelIndex, modelIndex);
        }
    } else {
        addItem(item, partitionIndex);
    }
}

int findIndexOf(const JsonDbModelIndexType::const_iterator &begin, const SortingKey &key, int low, int high)
{
    if (high < low)
        return -1;
    int mid = (low + high) / 2;
    JsonDbModelIndexType::const_iterator midItr =  begin + mid;
    if (midItr.key() == key) // ==
        return mid;
    else if (midItr.key() < key) // <
        return findIndexOf(begin, key, mid+1, high);
    else // >
        return findIndexOf(begin, key, low, mid-1);
}

void JsonDbCachingListModelPrivate::createObjectRequests(int startIndex, int maxItems)
{
    Q_Q(JsonDbCachingListModel);

#ifdef JSONDB_LISTMODEL_DEBUG
    qDebug()<<Q_FUNC_INFO<<startIndex<<maxItems<<objectUuids.count();
#endif
    Q_ASSERT(startIndex>=0);

    if (startIndex >= objectUuids.size())
        return;
    if ((startIndex + maxItems) > objectUuids.size())
        maxItems = objectUuids.size() - startIndex;

    if (state == JsonDbCachingListModel::Querying &&
            currentCacheRequest.index == startIndex &&
            currentCacheRequest.count == maxItems) {
        // we are fetching the same set, skip this
#ifdef JSONDB_LISTMODEL_DEBUG
        qDebug()<<"Skip this request";
#endif
        return;
    }
    currentCacheRequest.index = startIndex;
    currentCacheRequest.count = maxItems;

#ifdef JSONDB_LISTMODEL_DEBUG
    qDebug()<<"startIndex"<<startIndex<<" maxItems "<<maxItems;
#endif
    JsonDbModelIndexNSize *indexNSizes = new JsonDbModelIndexNSize[partitionObjects.count()] ;
    JsonDbModelIndexType::const_iterator itr = objectUuids.constBegin()+startIndex;
    for (int i = startIndex; i < startIndex+maxItems; i++, itr++) {
        const SortingKey &key = itr.key();
        int index = key.partitionIndex();
        if (indexNSizes[index].index == -1) {
            indexNSizes[index].index = findIndexOf(partitionObjectUuids[index].constBegin(),
                                                   key, 0, partitionObjectUuids[index].count()-1);
            Q_ASSERT(indexNSizes[index].index != -1);
        }
        indexNSizes[index].count++;
    }
    for (int i = 0; i < partitionObjects.count(); i++) {
        RequestInfo &r = partitionObjectDetails[i];
        if (indexNSizes[i].count) {
            if (state != JsonDbCachingListModel::Querying) {
                state = JsonDbCachingListModel::Querying;
                emit q->stateChanged(state);
            }

            r.lastOffset = indexNSizes[i].index;
            r.lastSize = -1;
            r.requestCount = indexNSizes[i].count;
            QJsonDbReadRequest *request = valueRequests[i]->newRequest(i);
            request->setQuery(query+sortOrder);
            request->setProperty("queryOffset", indexNSizes[i].index);
            request->setQueryLimit(qMin(r.requestCount, chunkSize));
            request->setPartition(partitionObjects[i]->name());
            JsonDatabase::sharedConnection().send(request);
#ifdef JSONDB_LISTMODEL_DEBUG
            qDebug()<<"Query"<<query+sortOrder<<partitionObjects[i]->name();
            qDebug()<<"Request "<<request->property("requestId") <<
                      "Total Count "<<r.requestCount <<
                      "Offset"<<r.lastOffset<<
                      "Count "<<qMin(r.requestCount,chunkSize);
#endif
        } else {
            r.lastSize = 0;
            r.requestCount = 0;
        }
    }
    delete [] indexNSizes;
}

void JsonDbCachingListModelPrivate::verifyIndexSpec(const QList<QJsonObject> &items, int partitionIndex)
{
    Q_Q(JsonDbCachingListModel);
    SortIndexSpec &indexSpec = partitionIndexDetails[partitionIndex].spec;
    bool validIndex = false;
    if (items.count()) {
        for (int i = 0; i < items.length() && !validIndex; i++) {
            QJsonObject spec = items[i];
            indexSpec.propertyName = QLatin1String("_indexValue");
            QString propertyType = spec.value(QLatin1String("propertyType")).toString();
            indexSpec.name = spec.value(QLatin1String("name")).toString();
            if (indexSpec.name.isEmpty())
                indexSpec.name = spec.value(QLatin1String("propertyName")).toString();
            indexSpec.caseSensitive = true;
            if (!indexName.isEmpty()) {
                if (indexSpec.name == indexName) {
                    if (!propertyType.compare(QLatin1String("string"), Qt::CaseInsensitive)) {
                        indexSpec.type = SortIndexSpec::String;
                        validIndex = true;
                    } else if (!propertyType.compare(QLatin1String("number"), Qt::CaseInsensitive)) {
                        indexSpec.type = SortIndexSpec::Number;
                        validIndex = true;
                    } else if (!propertyType.compare(QLatin1String("UUID"), Qt::CaseInsensitive)) {
                        indexSpec.type = SortIndexSpec::UUID;
                        indexSpec.caseSensitive = false;
                        validIndex = true;
                    }
                }
            }
        }
    }
    if (!validIndex) {
        qWarning() << "Error JsonDbCachingListModel requires a supported Index for "<<indexName;
        reset();
        state = JsonDbCachingListModel::Error;
        emit q->stateChanged(state);
    } else {
        partitionIndexDetails[partitionIndex].valid = true;
        //Check if all index specs are supported.
        bool checkedAll = true;
        for (int i = 0; i < partitionIndexDetails.count(); i++) {
            if (!partitionIndexDetails[i].valid) {
                checkedAll = false;
                break;
            }
        }
        if (checkedAll) {
            //Start fetching the keys.
            setQueryForSortKeys();
            for (int i = 0; i < partitionKeyRequestDetails.count(); i++) {
                fetchPartitionKeys(i);
            }
        }
    }
}

void JsonDbCachingListModelPrivate::fillKeys(const QList<QJsonObject> &items, int partitionIndex)
{
    RequestInfo &r = partitionKeyRequestDetails[partitionIndex];
    r.lastSize = items.size();
    for (int i = 0; i < r.lastSize; i++) {
        const QJsonObject &item = items.at(i);
        QString uuidStr = item.value(QLatin1String("_uuid")).toString();
        QByteArray uuid = QUuid(uuidStr).toRfc4122();
        SortingKey key(partitionIndex, uuid, item.value(QLatin1String("_indexValue")).toVariant(), ascendingOrder,  partitionIndexDetails[partitionIndex].spec);
        objectUuids.insert(key, uuidStr);
        partitionObjectUuids[partitionIndex].insert(key, uuidStr);
        objectSortValues.insert(uuidStr, key);

    }
    // Check if requests from different partitions returned
    // all the results
    bool allRequestsFinished = true;
    for (int i = 0; i < partitionKeyRequestDetails.count(); i++) {
        if (partitionKeyRequestDetails[i].lastSize >= chunkSize || partitionKeyRequestDetails[i].lastSize == -1) {
            allRequestsFinished = false;
            break;
        }
    }
    if (allRequestsFinished) {
        // retrieve the first chunk of data
#ifdef JSONDB_LISTMODEL_DEBUG
        qDebug()<<"All Keys Received "<<objectUuids.count();
#endif
        if (!objectUuids.count()) {
            for (int i = 0; i<partitionObjectDetails.count(); i++) {
                fillData(QList<QJsonObject>(), i);
            }
            return;
        }
        createObjectRequests(0, qMin(objectCache.maxItems(), objectUuids.count()));
    } else if (r.lastSize >= chunkSize){
        // more items, fetch next chunk of keys
        fetchNextKeyChunk(partitionIndex);
    }
}

void JsonDbCachingListModelPrivate::emitDataChanged(int from, int to)
{
    Q_Q(JsonDbCachingListModel);
    QModelIndex modelIndexFrom = q->createIndex(from, 0);
    QModelIndex modelIndexTo = q->createIndex(to, 0);
    emit q->dataChanged(modelIndexFrom, modelIndexTo);
}

void JsonDbCachingListModelPrivate::fillData(const QList<QJsonObject> &items, int partitionIndex)
{
    Q_Q(JsonDbCachingListModel);
    RequestInfo &r = partitionObjectDetails[partitionIndex];
    r.lastSize = items.size();
    r.requestCount -= r.lastSize;
    r.lastOffset += r.lastSize;

    for (int i = 0; i < r.lastSize; i++) {
        const QJsonObject &item = items.at(i);
        const QString &uuid = item.value(QLatin1String("_uuid")).toString();
        tmpObjects.insert(uuid, item);
    }

    // Check if requests from different partitions returned
    // all the results
    bool allRequestsFinished = true;
    for (int i = 0; i < partitionObjectDetails.count(); i++) {
        if (partitionObjectDetails[i].lastSize >= chunkSize || partitionObjectDetails[i].lastSize == -1) {
            allRequestsFinished = false;
            break;
        }
    }
    if (allRequestsFinished) {
#ifdef JSONDB_LISTMODEL_DEBUG
        qDebug()<<"Finished Req For:"<<currentCacheRequest.index<<currentCacheRequest.count;
        qDebug()<<"Finished Req recieved count: "<<tmpObjects.count()<<" Total Items:"<<objectUuids.count();
#endif
        objectCache.addObjects(currentCacheRequest.index, objectUuids, tmpObjects);
        tmpObjects.clear();
        JsonDbModelIndexNSize req = currentCacheRequest;
        currentCacheRequest.clear();
        // send the update for missed items
        QList<int> pendingCacheMiss;
        int changedFrom = -1, changedTo = -2;
        for (int i = 0; i < cacheMiss.size(); i++) {
            if (cacheMiss[i] >= req.index &&
                cacheMiss[i] < req.index + req.count) {
                if (changedFrom >= 0 && cacheMiss[i] != changedTo+1) {
                    emitDataChanged(changedFrom, changedTo);
                    changedFrom = -1;
                }
                if (changedFrom < 0) changedFrom = cacheMiss[i];
                changedTo = cacheMiss[i];
            } else {
                pendingCacheMiss.append(cacheMiss[i]);
            }
        }
        if (changedFrom >= 0)
            emitDataChanged(changedFrom, changedTo);
        cacheMiss.clear();
        cacheMiss = pendingCacheMiss;
        // call the get callback
        QMap<int, QJSValue> pendingGetCallbacks;
        QMapIterator<int, QJSValue> itr(getCallbacks);
        while (itr.hasNext()) {
            itr.next();
            int index = itr.key();
            if (index >= req.index && index < req.index + req.count) {
                callGetCallback(index, itr.value());
            } else {
                pendingGetCallbacks.insert(index, itr.value());
            }
        }
        getCallbacks.clear();
        getCallbacks = pendingGetCallbacks;
        if (resetModel) {
            q->beginResetModel();
            q->endResetModel();
            emit q->rowCountChanged(objectUuids.count());
            resetModel = false;
        }
        // retrieved all elements
        state = JsonDbCachingListModel::Ready;
        emit q->stateChanged(state);
        if (!pendingNotifications.isEmpty()) {
            foreach (NotificationItem pending, pendingNotifications)
                sendNotification(pending.partitionIndex, pending.item, pending.action);
            pendingNotifications.clear();
        }
        if (requestQueue.count()) {
            QPair<int, int> req = requestQueue.takeFirst();
            createObjectRequests(req.first, req.second);
        }
    } else if (r.lastSize >= chunkSize){
        // more items, fetch next chunk
        fetchNextChunk(partitionIndex);
    }
}


//Clears all the state information.
void JsonDbCachingListModelPrivate::reset()
{
    Q_Q(JsonDbCachingListModel);
    lastQueriedIndex = -1;
    q->beginResetModel();
    clearNotifications();
    for (int i = 0; i < partitionObjectDetails.count(); i++) {
        partitionObjectDetails[i].clear();
    }
    for (int i = 0; i < partitionKeyRequestDetails.count(); i++) {
        partitionKeyRequestDetails[i].clear();
    }
    for (int i = 0; i < partitionIndexDetails.count(); i++) {
        partitionIndexDetails[i].clear();
    }
    for (int i = 0; i < partitionObjectUuids.count(); i++) {
        partitionObjectUuids[i].clear();
    }

    objectCache.clear();
    objectUuids.clear();
    objectSortValues.clear();
    currentCacheRequest.clear();
    cacheMiss.clear();
    getCallbacks.clear();
    requestQueue.clear();
    q->endResetModel();
    emit q->rowCountChanged(0);
    state = JsonDbCachingListModel::None;
    emit q->stateChanged(state);
}

bool JsonDbCachingListModelPrivate::checkForDefaultIndexTypes(int index)
{
    Q_Q(JsonDbCachingListModel);
    bool defaultType = false;
    if (!indexName.compare(QLatin1String("_uuid")) || !indexName.compare(QLatin1String("_type"))) {
        defaultType = true;
        QMetaObject::invokeMethod(q, "_q_verifyDefaultIndexType", Qt::QueuedConnection,
                                  QGenericReturnArgument(),
                                  Q_ARG(int, index));
    }
    return defaultType;
}

void JsonDbCachingListModelPrivate::fetchIndexSpec(int index)
{
    Q_Q(JsonDbCachingListModel);
    if (index >= partitionObjects.count())
        return;
    if (checkForDefaultIndexTypes(index))
        return;
    if (state != JsonDbCachingListModel::Querying) {
        state = JsonDbCachingListModel::Querying;
        emit q->stateChanged(state);
    }
    QPointer<JsonDbPartition> p = partitionObjects[index];
    if (p) {
        QJsonDbReadRequest *request = indexRequests[index]->newRequest(index);
        request->setQuery(queryForIndexSpec);
        request->setPartition(p->name());
        JsonDatabase::sharedConnection().send(request);
    }
}

void JsonDbCachingListModelPrivate::fetchPartitionKeys(int index)
{
    Q_Q(JsonDbCachingListModel);
    if (index >= partitionObjects.count())
        return;

    if (state != JsonDbCachingListModel::Querying) {
        state = JsonDbCachingListModel::Querying;
        emit q->stateChanged(state);
    }
    RequestInfo &r = partitionKeyRequestDetails[index];
    QPointer<JsonDbPartition> p = partitionObjects[index];
    if (p) {
        r.lastSize = -1;
        r.lastOffset = 0;
        QJsonDbReadRequest *request = keyRequests[index]->newRequest(index);
        request->setQuery(queryForSortKeys);
        request->setQueryLimit(chunkSize);
        request->setPartition(p->name());
        JsonDatabase::sharedConnection().send(request);
    }
}

void JsonDbCachingListModelPrivate::initializeModel(bool reset)
{
    resetModel = reset;
    if (resetModel) {
        objectCache.clear();
        objectUuids.clear();
        objectSortValues.clear();
        for (int i = 0; i < partitionObjectUuids.count(); i++) {
            partitionObjectUuids[i].clear();
        }
        for (int i = 0; i < partitionIndexDetails.count(); i++) {
            partitionIndexDetails[i].clear();
        }
    }
    for (int i = 0; i < partitionObjects.count(); i++) {
        fetchIndexSpec(i);
    }
}

void JsonDbCachingListModelPrivate::fetchModel(bool reset)
{
    parseSortOrder();
    initializeModel(reset);
}

void JsonDbCachingListModelPrivate::fetchNextKeyChunk(int partitionIndex)
{
    RequestInfo &r = partitionKeyRequestDetails[partitionIndex];
    r.lastOffset += chunkSize;
    QJsonDbReadRequest *request = keyRequests[partitionIndex]->newRequest(partitionIndex);
    request->setQuery(queryForSortKeys);
    request->setProperty("queryOffset", r.lastOffset);
    request->setQueryLimit(chunkSize);
    request->setPartition(partitionObjects[partitionIndex]->name());
    JsonDatabase::sharedConnection().send(request);

}

void JsonDbCachingListModelPrivate::fetchNextChunk(int partitionIndex)
{
    RequestInfo &r = partitionObjectDetails[partitionIndex];
    QJsonDbReadRequest *request = valueRequests[partitionIndex]->newRequest(partitionIndex);
    request->setQuery(query+sortOrder);
    request->setProperty("queryOffset", r.lastOffset);
    request->setQueryLimit(qMin(r.requestCount, chunkSize));
    request->setPartition(partitionObjects[partitionIndex]->name());
    JsonDatabase::sharedConnection().send(request);

}

void JsonDbCachingListModelPrivate::prefetchNearbyPages(int index)
{
    int pos = objectCache.findPrefetchIndex(index, lowWaterMark);
    if (pos != -1 && index <= objectUuids.count()) {
        createObjectRequests(pos, objectCache.findChunkSize(pos));
    }
}
void JsonDbCachingListModelPrivate::addIndexToQueue(int index)
{
    int maxItems = 0;
    int start = objectCache.findIndexNSize(index, maxItems);
    QPair<int, int> req;
    foreach (req, requestQueue) {
        if (start == req.first && maxItems == req.second) {
#ifdef JSONDB_LISTMODEL_DEBUG
            qDebug()<<"Allready in Queue "<<start<<maxItems;
#endif
            return;
        }
    }
    requestQueue.append(QPair<int, int>(start,maxItems));
}

void JsonDbCachingListModelPrivate::requestPageContaining(int index)
{
#ifdef JSONDB_LISTMODEL_DEBUG
    qDebug()<<Q_FUNC_INFO<<index;
#endif
    if (state == JsonDbCachingListModel::Querying) {
        if (index >= currentCacheRequest.index &&
                index < currentCacheRequest.index+currentCacheRequest.count) {
            // Check if we are querying for this range already
            if (!cacheMiss.contains(index))
                cacheMiss.append(index);
            return;
        } else {
#ifdef JSONDB_LISTMODEL_DEBUG
            qDebug()<<"Add new Request to Queue" << index <<" currentCacheRequest = " <<
                      currentCacheRequest.index << ", " << currentCacheRequest.count;
#endif
            addIndexToQueue(index);
            return;
        }
    }
    int maxItems = 0;
    int start = objectCache.findIndexNSize(index, maxItems);
    createObjectRequests(start, maxItems);

}

void JsonDbCachingListModelPrivate::clearNotification(int index)
{
    if (index >= partitionObjects.count())
        return;

    RequestInfo &r = partitionObjectDetails[index];
    if (r.watcher) {
        JsonDatabase::sharedConnection().removeWatcher(r.watcher);
    }
    r.clear();
}

void JsonDbCachingListModelPrivate::clearNotifications()
{
    for (int i = 0; i < partitionObjects.count(); i++)
        clearNotification(i);
}

void JsonDbCachingListModelPrivate::createOrUpdateNotification(int index)
{
    Q_Q(JsonDbCachingListModel);
    if (index >= partitionObjects.count())
        return;
    clearNotification(index);
    QJsonDbWatcher *watcher = new QJsonDbWatcher();
    watcher->setQuery(query+sortOrder);
    watcher->setWatchedActions(QJsonDbWatcher::Created | QJsonDbWatcher::Updated |QJsonDbWatcher::Removed);
    watcher->setPartition(partitionObjects[index]->name());
    QObject::connect(watcher, SIGNAL(notificationsAvailable(int)),
                     q, SLOT(_q_notificationsAvailable()));
    QObject::connect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
                     q, SLOT(_q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    JsonDatabase::sharedConnection().addWatcher(watcher);
    partitionObjectDetails[index].watcher = watcher;
}

void JsonDbCachingListModelPrivate::createOrUpdateNotifications()
{
    for (int i = 0; i < partitionObjects.count(); i++) {
        createOrUpdateNotification(i);
    }
}

void JsonDbCachingListModelPrivate::parseSortOrder()
{
    Q_Q(JsonDbCachingListModel);
    QRegExp orderMatch("\\[([/\\\\[\\]])[ ]*([^\\[\\]]+)[ ]*\\]");
    if (orderMatch.indexIn(sortOrder, 0) >= 0) {
        ascendingOrder = false;
        if (!orderMatch.cap(1).compare(QLatin1String("/")))
            ascendingOrder = true;
        indexName = orderMatch.cap(2);
    }
    if (!indexName.isEmpty()) {
        queryForIndexSpec = QString(QLatin1String("[?_type=\"Index\"][?name=\"%1\" | propertyName=\"%1\"]")).arg(indexName);
    } else {
        // Set default sort order (by _uuid)
        q->setSortOrder(QLatin1String("[/_uuid]"));
    }
}

void JsonDbCachingListModelPrivate::setQueryForSortKeys()
{
    // Query to retrieve the sortKeys
    // TODO remove the "[= {}]" from query
    queryForSortKeys = query + QLatin1String("[= { _uuid: _uuid");
    queryForSortKeys += QLatin1String("}]");
    queryForSortKeys += sortOrder;
}

int JsonDbCachingListModelPrivate::indexOfWatcher(QJsonDbWatcher *watcher)
{
    for (int i = 0; i < partitionObjectDetails.count(); i++) {
        if (watcher == partitionObjectDetails[i].watcher)
            return i;
    }
    return -1;
}

QJsonObject JsonDbCachingListModelPrivate::getJsonObject(int index)
{
    if (index == lastQueriedIndex)
        return lastQueriedObject;
    if (index < 0 || index >= objectUuids.size()) {
#ifdef JSONDB_LISTMODEL_DEBUG
        qDebug() << "getItem" << index << "size  " << objectUuids.size();
#endif
        return QJsonObject();
    }
    int page = objectCache.findPage(index);
    if (page == -1) {
        if (!cacheMiss.contains(index))
            cacheMiss.append(index);
        requestPageContaining(index);
        return QJsonObject();
    }

    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    const QString &uuid = (begin+index).value();
    if (!objectCache.hasValueAtPage(page, uuid)) {
        // The value is missing, refresh page
        int startIndex = 0; int count = 0;
        objectCache.dropPage(page, startIndex, count);
#ifdef JSONDB_LISTMODEL_DEBUG
        qDebug() << "getItem Refresh Page: "<<page<<"Start: "<<startIndex<< "Count:"<<count<< "State :"<<state;
#endif
        if (state == JsonDbCachingListModel::Ready) {
            // Start the request
            createObjectRequests(startIndex, count);
        } else {
            requestQueue.append(QPair<int, int>(startIndex, count));
        }
        if (!cacheMiss.contains(index))
            cacheMiss.append(index);
        return QJsonObject();
    }
    if (state == JsonDbCachingListModel::Ready) // Pre-fetch only, if in Ready state
        prefetchNearbyPages(index);
    QJsonObject ret = objectCache.valueAtPage(page, uuid);
    lastQueriedIndex = index;
    lastQueriedObject = ret;
    return ret;
}

QVariant JsonDbCachingListModelPrivate::getItem(int index)
{
    return QVariant(getJsonObject(index).toVariantMap());
}

QVariant JsonDbCachingListModelPrivate::getItem(int index, int role)
{
    QJsonObject obj = getJsonObject(index);
    return lookupJsonProperty(obj, properties[role]).toVariant();
}

void JsonDbCachingListModelPrivate::queueGetCallback(int index, const QJSValue &callback)
{
    if (index < 0 || index >= objectUuids.size())
        return;
    int page = objectCache.findPage(index);
    if (page == -1) {
        requestPageContaining(index);
        getCallbacks.insert(index, callback);
        return;
    }
    callGetCallback(index, callback);
}

void JsonDbCachingListModelPrivate::callGetCallback(int index, QJSValue callback)
{
    if (index < 0 || index >= objectUuids.size())
        return;
    int page = objectCache.findPage(index);
    if (page == -1) {
        return;
    }
    QVariant object = getItem(index);
    JsonDbPartition *partition = getItemPartition(index);
    QJSValue result = g_declEngine->newObject();
    result.setProperty(QLatin1String("object"), g_declEngine->toScriptValue(object));
    result.setProperty(QLatin1String("partition"), g_declEngine->newQObject(partition));
    if (callback.isCallable()) {
        QJSValueList args;
        args << QJSValue(index) << result;
        callback.call(args);
        getCallbacks.remove(index);
    }
}

JsonDbPartition* JsonDbCachingListModelPrivate::getItemPartition(int index)
{
    if (index < 0 || index >= objectUuids.size())
        return 0;
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    int partitionIndex = (begin+index).key().partitionIndex();
    if (partitionIndex <= partitionObjects.count())
        return partitionObjects[partitionIndex];
    return 0;
}

int JsonDbCachingListModelPrivate::indexOf(const QString &uuid) const
{
    if (!objectSortValues.contains(uuid))
        return -1;
    const SortingKey &key = objectSortValues.value(uuid);
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    QMap<SortingKey, QString>::const_iterator end = objectUuids.constEnd();
    QMap<SortingKey, QString>::const_iterator i = objectUuids.find(key);
    return iterator_position(begin, end, i);
}

void JsonDbCachingListModelPrivate::sendNotification(int partitionIndex, const QJsonObject &object, QJsonDbWatcher::Action action)
{
    if (action == QJsonDbWatcher::Created) {
       addItem(object, partitionIndex);
    } else if (action == QJsonDbWatcher::Removed)
        deleteItem(object, partitionIndex);
    else if (action == QJsonDbWatcher::Updated) {
        updateItem(object, partitionIndex);
    }
}

void JsonDbCachingListModelPrivate::_q_keyResponse(int index, const QList<QJsonObject> &v, const QString &sortKey)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_UNUSED(sortKey)
    fillKeys(v, index);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void JsonDbCachingListModelPrivate::_q_valueResponse(int index, const QList<QJsonObject> &v)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    fillData(v, index);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void JsonDbCachingListModelPrivate::_q_indexResponse(int index, const QList<QJsonObject> &v)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    verifyIndexSpec(v, index);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void JsonDbCachingListModelPrivate::_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString & message)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(JsonDbCachingListModel);
    qWarning() << QString("JsonDb error: %1 %2").arg(code).arg(message);
    int oldErrorCode = errorCode;
    errorCode = code;
    errorString = message;
    if (oldErrorCode != errorCode)
        emit q->errorChanged(q->error());
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void JsonDbCachingListModelPrivate::_q_notificationsAvailable()
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(JsonDbCachingListModel);
    QJsonDbWatcher *watcher = qobject_cast<QJsonDbWatcher *>(q->sender());
    int partitionIndex = indexOfWatcher(watcher);
    if (!watcher || partitionIndex == -1)
        return;
    QList<QJsonDbNotification> list = watcher->takeNotifications();
    for (int i = 0; i < list.count(); i++) {
        const QJsonDbNotification & notification = list[i];
        QJsonObject object = notification.object();
        QJsonDbWatcher::Action action = notification.action();
        if (state == JsonDbCachingListModel::Querying) {
            NotificationItem  pending;
            pending.partitionIndex = partitionIndex;
            pending.item = object;
            pending.action = action;
            pendingNotifications.append(pending);
        } else {
            foreach (NotificationItem pending, pendingNotifications)
                sendNotification(pending.partitionIndex, pending.item, pending.action);
            pendingNotifications.clear();
            sendNotification(partitionIndex, object, action);
        }
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void JsonDbCachingListModelPrivate::_q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(JsonDbCachingListModel);
    qWarning() << QString("JsonDbCachingListModel Notification error: %1 %2").arg(code).arg(message);
    int oldErrorCode = errorCode;
    errorCode = code;
    errorString = message;
    if (oldErrorCode != errorCode)
        emit q->errorChanged(q->error());
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void JsonDbCachingListModelPrivate::_q_verifyDefaultIndexType(int index)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    SortIndexSpec &indexSpec = partitionIndexDetails[index].spec;
    partitionIndexDetails[index].valid = true;
    if (!indexName.compare(QLatin1String("_uuid"))) {
        indexSpec.name = QLatin1String("_uuid");
        indexSpec.type = SortIndexSpec::UUID;
        indexSpec.caseSensitive = false;
    } else if (!indexName.compare(QLatin1String("_type"))) {
        indexSpec.name = QLatin1String("_type");
        indexSpec.type = SortIndexSpec::String;
        indexSpec.caseSensitive = true;
    }
    //Check if all index specs are supported.
    bool checkedAll = true;
    for (int i = 0; i < partitionIndexDetails.count(); i++) {
        if (!partitionIndexDetails[i].valid) {
            checkedAll = false;
            break;
        }
    }
    if (checkedAll) {
        //Start fetching the keys.
        setQueryForSortKeys();
        for (int i = 0; i < partitionKeyRequestDetails.count(); i++) {
            fetchPartitionKeys(i);
        }
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void JsonDbCachingListModelPrivate::appendPartition(JsonDbPartition *v)
{
    Q_Q(JsonDbCachingListModel);
    partitionObjects.append(QPointer<JsonDbPartition>(v));

    partitionObjectDetails.append(RequestInfo());
    ModelRequest *valueRequest = new ModelRequest();
    QObject::connect(valueRequest, SIGNAL(finished(int,QList<QJsonObject>,QString)),
                     q, SLOT(_q_valueResponse(int,QList<QJsonObject>)));
    QObject::connect(valueRequest, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     q, SLOT(_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    valueRequests.append(valueRequest);

    partitionKeyRequestDetails.append(RequestInfo());
    ModelRequest *keyRequest = new ModelRequest();
    QObject::connect(keyRequest, SIGNAL(finished(int,QList<QJsonObject>,QString)),
                     q, SLOT(_q_keyResponse(int,QList<QJsonObject>,QString)));
    QObject::connect(keyRequest, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     q, SLOT(_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    keyRequests.append(keyRequest);

    partitionObjectUuids.append(JsonDbModelIndexType());

    partitionIndexDetails.append(IndexInfo());
    ModelRequest *indexRequest = new ModelRequest();
    QObject::connect(indexRequest, SIGNAL(finished(int,QList<QJsonObject>,QString)),
                     q, SLOT(_q_indexResponse(int,QList<QJsonObject>)));
    QObject::connect(indexRequest, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     q, SLOT(_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    indexRequests.append(indexRequest);

    if (componentComplete && !query.isEmpty()) {
        parseSortOrder();
        createOrUpdateNotification(partitionObjects.count()-1);
        if (state == JsonDbCachingListModel::None)
            resetModel = true;
        fetchIndexSpec(partitionObjects.count()-1);
    }
}

void JsonDbCachingListModelPrivate::partitions_append(QQmlListProperty<JsonDbPartition> *p, JsonDbPartition *v)
{
    JsonDbCachingListModel *q = qobject_cast<JsonDbCachingListModel *>(p->object);
    JsonDbCachingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        pThis->appendPartition(v);
    }
}

int JsonDbCachingListModelPrivate::partitions_count(QQmlListProperty<JsonDbPartition> *p)
{
    JsonDbCachingListModel *q = qobject_cast<JsonDbCachingListModel *>(p->object);
    JsonDbCachingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        return pThis->partitionObjects.count();
    }
    return 0;
}

JsonDbPartition* JsonDbCachingListModelPrivate::partitions_at(QQmlListProperty<JsonDbPartition> *p, int idx)
{
    JsonDbCachingListModel *q = qobject_cast<JsonDbCachingListModel *>(p->object);
    JsonDbCachingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis && idx < pThis->partitionObjects.count()) {
        return pThis->partitionObjects.at(idx);
    }
    return 0;
}

void JsonDbCachingListModelPrivate::clearPartitions()
{
    partitionObjects.clear();
    partitionObjectDetails.clear();
    partitionKeyRequestDetails.clear();
    partitionObjectUuids.clear();
    partitionIndexDetails.clear();
    while (!keyRequests.isEmpty()) {
        delete keyRequests[0];
        keyRequests.removeFirst();
    }
    while (!indexRequests.isEmpty()) {
        delete indexRequests[0];
        indexRequests.removeFirst();
    }
    while (!valueRequests.isEmpty()) {
        delete valueRequests[0];
        valueRequests.removeFirst();
    }
    reset();
}

void JsonDbCachingListModelPrivate::partitions_clear(QQmlListProperty<JsonDbPartition> *p)
{
    JsonDbCachingListModel *q = qobject_cast<JsonDbCachingListModel *>(p->object);
    JsonDbCachingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        pThis->clearPartitions();
    }
}

/*!
    \qmlclass JsonDbCachingListModel
    \inqmlmodule QtJsonDb
    \since 1.x

    The JsonDbCachingListModel provides a read-only ListModel usable with views such as
    ListView or GridView displaying data items matching a query. The sorting is done using
    an index set on the JsonDb server. If it doesn't find a matching index for the sortkey,
    the model goes into Error state. Maximum number of items in the model cache can be set
    by cacheSize property.

    When an item is not present in the internal cache, the model can return an 'undefined'
    object from data() method. It will be queued for retrieval and the model will notify its
    presence using the dataChanged() signal.

    The model is initialized by retrieving the result in chunks. After receiving the first
    chunk, the model is reset with items from it. The state will be "Querying" during
    fetching data and will be changed to "Ready".

    \note This is still a work in progress, so expect minor changes.

    \code
    import QtJsonDb 1.0 as JsonDb

    JsonDb.JsonDbCachingListModel {
        id: contactsModel
        query: '[?_type="Contact"]'
        cacheSize: 100
        partitions: [JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
        sortOrder: "[/firstName]"
        roleNames: ["firstName", "lastName", "phoneNumber"]
    }
    ListView {
        model: contactsModel
        Row {
            spacing: 10
            Text {
                text: firstName + " " + lastName
            }
            Text {
                text: phoneNumber
            }
        }
    }
    \endcode
*/

JsonDbCachingListModel::JsonDbCachingListModel(QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(new JsonDbCachingListModelPrivate(this))
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(JsonDbCachingListModel);
    d->init();
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

JsonDbCachingListModel::~JsonDbCachingListModel()
{
}

void JsonDbCachingListModel::classBegin()
{
}

void JsonDbCachingListModel::componentComplete()
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(JsonDbCachingListModel);
    d->componentComplete = true;
    if (!d->query.isEmpty() && d->partitionObjects.count()) {
        d->createOrUpdateNotifications();
        d->fetchModel();
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

/*!
    \qmlproperty int QtJsonDb::JsonDbCachingListModel::rowCount
    The number of items in the model.
*/
int JsonDbCachingListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    Q_D(const JsonDbCachingListModel);
    return d->objectUuids.count();
}

QVariant JsonDbCachingListModel::data(const QModelIndex &modelIndex, int role) const
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    JsonDbCachingListModel *pThis = const_cast<JsonDbCachingListModel *>(this);
    QVariant ret = pThis->d_func()->getItem(modelIndex.row(), role);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
    return ret;
}

/*!
    \qmlproperty ListOrObject QtJsonDb::JsonDbCachingListModel::roleNames

    Controls which properties to expose from the objects matching the query.

    Setting \a roleNames to a list of strings causes the model to expose
    corresponding object values as roles to the delegate for each item viewed.

    \code
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"MyType\"]"
        partitions:[ JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
        roleNames: ['a', 'b']
    }
    ListView {
        model: listModel
        Text {
            text: a + ":" + b
        }
    \endcode

    Setting \a roleNames to a dictionary remaps properties in the object
    to the specified roles in the model.

    In the following example, role \a a would yield the value of
    property \a aLongName in the objects. Role \a liftedProperty would
    yield the value of \a o.nested.property for each matching object \a
    o in the database.

    \code
    function makeRoleNames() {
        return { 'a': 'aLongName', 'liftedProperty': 'nested.property' };
    }
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"MyType\"]"
        partitions: [JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
        roleNames: makeRoleNames()
    }
    ListView {
        model: listModel
        Text {
            text: a + " " + liftedProperty
        }
    }
    \endcode
*/

QVariant JsonDbCachingListModel::scriptableRoleNames() const
{
    Q_D(const JsonDbCachingListModel);
    return d->roleMap;
}

void JsonDbCachingListModel::setScriptableRoleNames(const QVariant &vroles)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(JsonDbCachingListModel);
    d->properties.clear();
    d->roleNames.clear();
    if (vroles.type() == QVariant::Map) {
        QVariantMap roles = vroles.toMap();
        d->roleMap = roles;
        int i = 0;
        for (QVariantMap::const_iterator it = roles.begin(); it != roles.end(); ++it) {
            d->roleNames.insert(i, it.key().toLatin1());
            d->properties.insert(i, removeArrayOperator(it.value().toString()).split('.'));
            i++;
        }
    } else {
        QVariantList roleList = vroles.toList();
        d->roleMap.clear();
        for (int i = 0; i < roleList.size(); i++) {
            QString role = roleList[i].toString();
            d->roleMap[role] = role;
            d->roleNames.insert(i, role.toLatin1());
            d->properties.insert(i, removeArrayOperator(role).split('.'));
        }
    }
    QAbstractItemModel::setRoleNames(d->roleNames);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

/*!
    \qmlproperty string QtJsonDb::JsonDbCachingListModel::query

    The query string in JsonQuery format used by the model to fetch
    items from the database. Setting an empty query clears all the elements

    In the following example, the JsonDbCachingListModel would contain all
    the objects with \a _type "CONTACT" from partition called "com.nokia.shared"

    \qml
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        partitions:[ JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
    }
    \endqml

*/
QString JsonDbCachingListModel::query() const
{
    Q_D(const JsonDbCachingListModel);
    return d->query;
}

void JsonDbCachingListModel::setQuery(const QString &newQuery)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(JsonDbCachingListModel);

    const QString oldQuery = d->query;
    if (oldQuery == newQuery)
        return;

    d->query = newQuery;
    if (rowCount() && d->query.isEmpty()) {
        d->reset();
    }

    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count())
        return;
    d->createOrUpdateNotifications();
    d->fetchModel();
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

/*!
    \qmlproperty int QtJsonDb::JsonDbCachingListModel::cacheSize
    Holds the maximum number of items cached by the model.

    \code
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        cacheSize: 100
        partitions: [JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
    }
    \endcode

*/

int JsonDbCachingListModel::cacheSize() const
{
    Q_D(const JsonDbCachingListModel);
    return d->cacheSize;
}

void JsonDbCachingListModel::setCacheSize(int newCacheSize)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(JsonDbCachingListModel);
    if (newCacheSize == d->cacheSize)
        return;

    d->cacheSize = newCacheSize;
    d->setCacheParams(d->cacheSize);
    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count())
        return;

    d->fetchModel();
#ifdef JSONDB_LISTMODEL_DEBUG
    d->objectCache.dumpCacheDetails();
#endif
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void JsonDbCachingListModel::partitionNameChanged(const QString &partitionName)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_UNUSED(partitionName);
    Q_D(JsonDbCachingListModel);

    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count())
        return;

    d->createOrUpdateNotifications();
    d->fetchModel();
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

/*!
    \qmlproperty list QtJsonDb::JsonDbCachingListModel::partitions
    Holds the list of partition objects for the model.
    \code
    JsonDb.JsonDbCachingListModel {
        id: contacts
        query: '[?_type="Contact"]'
        partitions :[nokiaPartition, nokiaPartition2]
        roleNames: ["firstName", "lastName", "_uuid", "_version"]
        sortOrder:"[/firstName]"
    }

    \endcode
*/


QQmlListProperty<JsonDbPartition> JsonDbCachingListModel::partitions()
{
    return QQmlListProperty<JsonDbPartition>(this, 0
                                                     , &JsonDbCachingListModelPrivate::partitions_append
                                                     , &JsonDbCachingListModelPrivate::partitions_count
                                                     , &JsonDbCachingListModelPrivate::partitions_at
                                                     , &JsonDbCachingListModelPrivate::partitions_clear);
}

/*!
    \qmlproperty string QtJsonDb::JsonDbCachingListModel::sortOrder

    The order used by the model to sort the items. Make sure that there
    is a matching Index in the database for this sortOrder. This has to be
    specified in the JsonQuery format.

    In the following example, the JsonDbCachingListModel would contain all
    the objects of type \a "CONTACT" sorted by their \a firstName field

    \qml
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        partitions: [ JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
        sortOrder: "[/firstName]"
    }
    \endqml

*/

QString JsonDbCachingListModel::sortOrder() const
{
    Q_D(const JsonDbCachingListModel);
    return d->sortOrder;
}

void JsonDbCachingListModel::setSortOrder(const QString &newSortOrder)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(JsonDbCachingListModel);

    const QString oldSortOrder = d->sortOrder;
    d->sortOrder = newSortOrder;
    if (oldSortOrder != newSortOrder) {
        d->parseSortOrder();
        if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count())
            return;
        d->createOrUpdateNotifications();
        d->fetchModel();
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

/*!
    \qmlproperty State QtJsonDb::JsonDbCachingListModel::state
    The current state of the model.
    \list
    \li State.None - The model is not initialized
    \li State.Querying - It is querying the results from server
    \li State.Ready - Results are ready
    \li State.Error - Cannot find a matching index on the server
    \endlist
*/

JsonDbCachingListModel::State JsonDbCachingListModel::state() const
{
    Q_D(const JsonDbCachingListModel);
    return d->state;
}

/*!
    \qmlmethod int QtJsonDb::JsonDbCachingListModel::indexOf(string uuid)

    Returns the index of the object with the \a uuid in the model. If the object is
    not found it returns -1
*/
int JsonDbCachingListModel::indexOf(const QString &uuid) const
{
    Q_D(const JsonDbCachingListModel);
    return d->indexOf(uuid);
}

/*!
    \qmlmethod  QtJsonDb::JsonDbCachingListModel::get(int index, function callback)

    Calls the callback with object at the specified \a index in the model. The result.object property
    contains the object in its raw form as returned by the query, the rolenames
    are not applied. The object.partition is the partition for the returned.
    If the index is out of range it returns an object with empty partition & object properties.

    \code
    function updateCallback(error, response) {
        if (error) {
            console.log("Update Error :"+JSON.stringify(error));
            return,
        }
        console.log("Response from Update");
        console.log("response.id = "+response.id +" count = "+response.items.length);
        for (var i = 0; i < response.items.length; i++) {
            console.log("_uuid = "+response.items[i]._uuid +" ._version = "+response.items[i]._version);
        }
    }
    function updateItemCallback(index, response) {
        if (response) {
            response.object.firstName = response.object.firstName+ "*";
            response.partition.update(response.object, updateCallback);
        }
    }
    onClicked: {
        contacts.get(listView.currentIndex, updateItemCallback);
    }
    \endcode
*/
void JsonDbCachingListModel::get(int index, const QJSValue &callback)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(JsonDbCachingListModel);
    d->queueGetCallback(index, callback);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

/*!
    \qmlmethod object QtJsonDb::JsonDbCachingListModel::getPartition(int index)

    Returns the partition object at the specified \a index in the model. If
    the index is out of range it returns an empty object.
*/

JsonDbPartition* JsonDbCachingListModel::getPartition(int index) const
{
    JsonDbCachingListModel *pThis = const_cast<JsonDbCachingListModel *>(this);
    return pThis->d_func()->getItemPartition(index);
}

/*!
    \qmlproperty object QtJsonDb::JsonDbCachingListModel::error
    \readonly

    This property holds the current error information for the object. It contains:
    \list
    \li error.code -  code for the current error.
    \li error.message - detailed explanation of the error
    \endlist
*/

QVariantMap JsonDbCachingListModel::error() const
{
    Q_D(const JsonDbCachingListModel);
    QVariantMap errorMap;
    errorMap.insert(QLatin1String("code"), d->errorCode);
    errorMap.insert(QLatin1String("message"), d->errorString);
    return errorMap;
}

#include "moc_jsondbcachinglistmodel.cpp"
QT_END_NAMESPACE_JSONDB
