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

#include "qjsondbquerymodel_p_p.h"
#include "qjsondbconnection.h"
#include <QDebug>
#ifdef JSONDB_LISTMODEL_BENCHMARK
#include <QElapsedTimer>
#endif
#include <QJSEngine>

QT_BEGIN_NAMESPACE_JSONDB

QJsonDbQueryModelPrivate::QJsonDbQueryModelPrivate(QJsonDbQueryModel *q)
    : q_ptr(q)
    , componentComplete(false)
    , resetModel(true)
    , cacheSize(-1)
    , state(QJsonDbQueryModel::None)
    , errorCode(0)
    , lastQueriedIndex(-1)
    , isCallable(false)
{
    setCacheParams(INT_MAX/10);
}

void QJsonDbQueryModelPrivate::init(QJsonDbConnection *dbConnection)
{
    mConnection = dbConnection;
}

void QJsonDbQueryModelPrivate::setCacheParams(int maxItems)
{
    objectCache.setPageSize(maxItems);
    chunkSize = objectCache.chunkSize();
    lowWaterMark = objectCache.chunkSize()/4;
}

QJsonDbQueryModelPrivate::~QJsonDbQueryModelPrivate()
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
void QJsonDbQueryModelPrivate::addItem(const QJsonObject &newItem, int partitionIndex)
{
    Q_Q(QJsonDbQueryModel);
    QJsonObject item = newItem;
    if (isCallable)
        generateCustomData(item);
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
void QJsonDbQueryModelPrivate::deleteItem(const QJsonObject &item, int partitionIndex)
{
    Q_Q(QJsonDbQueryModel);
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
void QJsonDbQueryModelPrivate::updateItem(const QJsonObject &changedItem, int partitionIndex)
{
    Q_Q(QJsonDbQueryModel);
    QJsonObject item = changedItem;
    if (isCallable)
        generateCustomData(item);
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

inline void setQueryBindings(QJsonDbReadRequest *request, const QVariantMap &bindings)
{
    QVariantMap::ConstIterator i = bindings.constBegin();
    while (i != bindings.constEnd()) {
        request->bindValue(i.key(), QJsonValue::fromVariant(i.value()));
        ++i;
    }
}

void QJsonDbQueryModelPrivate::createObjectRequests(int startIndex, int maxItems)
{
    Q_Q(QJsonDbQueryModel);

#ifdef JSONDB_LISTMODEL_DEBUG
    qDebug()<<Q_FUNC_INFO<<startIndex<<maxItems<<objectUuids.count();
#endif
    Q_ASSERT(startIndex>=0);

    if (startIndex >= objectUuids.size())
        return;
    if ((startIndex + maxItems) > objectUuids.size())
        maxItems = objectUuids.size() - startIndex;

    if (state == QJsonDbQueryModel::Querying &&
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
            if (state != QJsonDbQueryModel::Querying) {
                state = QJsonDbQueryModel::Querying;
                emit q->stateChanged(state);
            }

            r.lastOffset = indexNSizes[i].index;
            r.lastSize = -1;
            r.requestCount = indexNSizes[i].count;
            QJsonDbReadRequest *request = valueRequests[i]->newRequest(i);
            request->setQuery(query+sortOrder);
            request->setProperty("queryOffset", indexNSizes[i].index);
            request->setQueryLimit(qMin(r.requestCount, chunkSize));
            request->setPartition(partitionObjects[i]);
            setQueryBindings(request, queryBindings);
            if (mConnection)
                mConnection->send(request);
#ifdef JSONDB_LISTMODEL_DEBUG
            qDebug()<<"Query"<<query+sortOrder<<partitionObjects[i];
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

void QJsonDbQueryModelPrivate::verifyIndexSpec(const QList<QJsonObject> &items, int partitionIndex)
{
    Q_Q(QJsonDbQueryModel);
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
                        if (spec.value(QLatin1String("caseSensitive")).isBool())
                            indexSpec.caseSensitive = spec.value(QLatin1String("caseSensitive")).toBool();
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
        qWarning() << "Supported Index for" << indexName << "not found, using _uuid";
        indexSpec.name = QLatin1String("_uuid");
        indexSpec.type = SortIndexSpec::UUID;
        indexSpec.caseSensitive = false;
    }
    partitionIndexDetails[partitionIndex].valid = true;
    //Check if all index specs are supported.
    bool checkedAll = true;
    for (int i = 0; i < partitionIndexDetails.count(); i++) {
        if (availablePartitions[i].state != PartitionStateOnline)
            continue;
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

void QJsonDbQueryModelPrivate::fillKeys(const QList<QJsonObject> &items, int partitionIndex)
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
        if (availablePartitions[i].state != PartitionStateOnline)
            continue;
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

void QJsonDbQueryModelPrivate::emitDataChanged(int from, int to)
{
    Q_Q(QJsonDbQueryModel);
    QModelIndex modelIndexFrom = q->createIndex(from, 0);
    QModelIndex modelIndexTo = q->createIndex(to, 0);
    emit q->dataChanged(modelIndexFrom, modelIndexTo);
}

void QJsonDbQueryModelPrivate::generateCustomData(QJsonObject &val)
{
    Q_Q(QJsonDbQueryModel);
    QJSValueList args;
    args << injectCallback.engine()->toScriptValue(val);
    QJSValue retVal = injectCallback.call(args);
    QJsonObject customData = qjsvalue_cast<QJsonObject>(retVal);
    QJsonObject::const_iterator it = customData.constBegin(), e = customData.constEnd();
    for (; it != e; ++it) {
        val.insert(it.key(), it.value());
    }
}

void QJsonDbQueryModelPrivate::generateCustomData(JsonDbModelObjectType &objects)
{
    if (!isCallable)
        return;
    JsonDbModelObjectType::iterator i = objects.begin();
    while (i != objects.end()) {
        generateCustomData(i.value());
        i++;
    }
}

void QJsonDbQueryModelPrivate::fillData(const QList<QJsonObject> &items, int partitionIndex)
{
    Q_Q(QJsonDbQueryModel);
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
        if (availablePartitions[i].state != PartitionStateOnline)
            continue;
        if (partitionObjectDetails[i].lastSize >= chunkSize || partitionObjectDetails[i].lastSize == -1) {
            allRequestsFinished = false;
            break;
        }
    }
    if (allRequestsFinished) {
#ifdef JSONDB_LISTMODEL_DEBUG
        qDebug()<<"Finished Req For:"<<currentCacheRequest.index<<currentCacheRequest.count;
        qDebug()<<"Finished Req received count: "<<tmpObjects.count()<<" Total Items:"<<objectUuids.count();
#endif
        generateCustomData(tmpObjects);
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

        for (int i = 0; i<req.count; i++) {
            emit q->objectAvailable(req.index + i,
                                    getJsonObject(req.index + i),
                                    getItemPartition(req.index + i));
        }

        if (resetModel) {
            q->beginResetModel();
            q->endResetModel();
            emit q->rowCountChanged(objectUuids.count());
            resetModel = false;
        }
        // retrieved all elements
        state = QJsonDbQueryModel::Ready;
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
void QJsonDbQueryModelPrivate::reset()
{
    Q_Q(QJsonDbQueryModel);
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
    requestQueue.clear();
    q->endResetModel();
    emit q->rowCountChanged(0);
    state = QJsonDbQueryModel::None;
    emit q->stateChanged(state);
}

bool QJsonDbQueryModelPrivate::checkForDefaultIndexTypes(int index)
{
    Q_Q(QJsonDbQueryModel);
    bool defaultType = false;
    if (!indexName.compare(QLatin1String("_uuid")) || !indexName.compare(QLatin1String("_type"))) {
        defaultType = true;
        QMetaObject::invokeMethod(q, "_q_verifyDefaultIndexType", Qt::QueuedConnection,
                                  QGenericReturnArgument(),
                                  Q_ARG(int, index));
    }
    return defaultType;
}

void QJsonDbQueryModelPrivate::fetchIndexSpec(int index)
{
    Q_Q(QJsonDbQueryModel);
    if (index >= partitionObjects.count())
        return;
    if (checkForDefaultIndexTypes(index))
        return;
    if (state != QJsonDbQueryModel::Querying) {
        state = QJsonDbQueryModel::Querying;
        emit q->stateChanged(state);
    }
    if (availablePartitions[index].state == PartitionStateOnline) {
        QString partitionName = partitionObjects[index];
        QJsonDbReadRequest *request = indexRequests[index]->newRequest(index);
        request->setQuery(queryForIndexSpec);
        request->setPartition(partitionName);
        if (mConnection)
            mConnection->send(request);
    }
}

void QJsonDbQueryModelPrivate::fetchPartitionKeys(int index)
{
    Q_Q(QJsonDbQueryModel);
    if (index >= partitionObjects.count())
        return;

    if (state != QJsonDbQueryModel::Querying) {
        state = QJsonDbQueryModel::Querying;
        emit q->stateChanged(state);
    }
    if (availablePartitions[index].state == PartitionStateOnline) {
        RequestInfo &r = partitionKeyRequestDetails[index];
        QString partitionName = partitionObjects[index];
        r.lastSize = -1;
        r.lastOffset = 0;
        QJsonDbReadRequest *request = keyRequests[index]->newRequest(index);
        request->setQuery(queryForSortKeys);
        request->setQueryLimit(chunkSize);
        request->setPartition(partitionName);
        setQueryBindings(request, queryBindings);
        if (mConnection)
            mConnection->send(request);
    }
}

void QJsonDbQueryModelPrivate::initializeModel(bool reset)
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

void QJsonDbQueryModelPrivate::fetchModel(bool reset)
{
    parseSortOrder();
    initializeModel(reset);
}

void QJsonDbQueryModelPrivate::fetchNextKeyChunk(int partitionIndex)
{
    RequestInfo &r = partitionKeyRequestDetails[partitionIndex];
    r.lastOffset += chunkSize;
    QJsonDbReadRequest *request = keyRequests[partitionIndex]->newRequest(partitionIndex);
    request->setQuery(queryForSortKeys);
    request->setProperty("queryOffset", r.lastOffset);
    request->setQueryLimit(chunkSize);
    request->setPartition(partitionObjects[partitionIndex]);
    setQueryBindings(request, queryBindings);
    if (mConnection)
        mConnection->send(request);
}

void QJsonDbQueryModelPrivate::fetchNextChunk(int partitionIndex)
{
    RequestInfo &r = partitionObjectDetails[partitionIndex];
    QJsonDbReadRequest *request = valueRequests[partitionIndex]->newRequest(partitionIndex);
    request->setQuery(query+sortOrder);
    request->setProperty("queryOffset", r.lastOffset);
    request->setQueryLimit(qMin(r.requestCount, chunkSize));
    request->setPartition(partitionObjects[partitionIndex]);
    setQueryBindings(request, queryBindings);
    if (mConnection)
        mConnection->send(request);
}

void QJsonDbQueryModelPrivate::prefetchNearbyPages(int index)
{
    int pos = objectCache.findPrefetchIndex(index, lowWaterMark);
    if (pos != -1 && index <= objectUuids.count()) {
        createObjectRequests(pos, objectCache.findChunkSize(pos));
    }
}
void QJsonDbQueryModelPrivate::addIndexToQueue(int index)
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

void QJsonDbQueryModelPrivate::requestPageContaining(int index)
{
#ifdef JSONDB_LISTMODEL_DEBUG
    qDebug()<<Q_FUNC_INFO<<index;
#endif
    if (state == QJsonDbQueryModel::Querying) {
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

void QJsonDbQueryModelPrivate::clearNotification(int index)
{
    if (index >= partitionObjects.count())
        return;

    RequestInfo &r = partitionObjectDetails[index];
    if (r.watcher && mConnection) {
        mConnection->removeWatcher(r.watcher);
    }
    r.clear();
}

void QJsonDbQueryModelPrivate::clearNotifications()
{
    for (int i = 0; i < partitionObjects.count(); i++)
        clearNotification(i);
}

void QJsonDbQueryModelPrivate::createOrUpdateNotification(int index)
{
    Q_Q(QJsonDbQueryModel);
    if (index >= partitionObjects.count())
        return;
    clearNotification(index);
    if (availablePartitions[index].state != PartitionStateOnline)
        return;
    QJsonDbWatcher *watcher = new QJsonDbWatcher();
    watcher->setQuery(query+sortOrder);
    watcher->setWatchedActions(QJsonDbWatcher::Created | QJsonDbWatcher::Updated |QJsonDbWatcher::Removed);
    watcher->setPartition(partitionObjects[index]);
    QVariantMap::ConstIterator i = queryBindings.constBegin();
    while (i != queryBindings.constEnd()) {
        watcher->bindValue(i.key(), QJsonValue::fromVariant(i.value()));
        ++i;
    }
    QObject::connect(watcher, SIGNAL(notificationsAvailable(int)),
                     q, SLOT(_q_notificationsAvailable()));
    QObject::connect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
                     q, SLOT(_q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    if (mConnection) {
        mConnection->addWatcher(watcher);
        partitionObjectDetails[index].watcher = watcher;
    }
}

void QJsonDbQueryModelPrivate::createOrUpdateNotifications()
{
    for (int i = 0; i < partitionObjects.count(); i++) {
        createOrUpdateNotification(i);
    }
}

void QJsonDbQueryModelPrivate::parseSortOrder()
{
    Q_Q(QJsonDbQueryModel);
    QRegExp orderMatch(QStringLiteral("\\[([/\\\\[\\]])[ ]*([^\\[\\]]+)[ ]*\\]"));
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

void QJsonDbQueryModelPrivate::setQueryForSortKeys()
{
    // Query to retrieve the sortKeys
    // TODO remove the "[= {}]" from query
    queryForSortKeys = query + QLatin1String("[= { _uuid: _uuid, _indexValue: _indexValue }]");
    queryForSortKeys += sortOrder;
}

int QJsonDbQueryModelPrivate::indexOfWatcher(QJsonDbWatcher *watcher)
{
    for (int i = 0; i < partitionObjectDetails.count(); i++) {
        if (watcher == partitionObjectDetails[i].watcher)
            return i;
    }
    return -1;
}

int QJsonDbQueryModelPrivate::indexOfPartitionObjectWatcher(QJsonDbWatcher *watcher)
{
    for (int i = 0; i < availablePartitions.count(); i++) {
        if (watcher == availablePartitions[i].watcher)
            return i;
    }
    return -1;
}

QJsonObject QJsonDbQueryModelPrivate::getJsonObject(int index)
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
        if (state == QJsonDbQueryModel::Ready) {
            // Start the request
            createObjectRequests(startIndex, count);
        } else {
            requestQueue.append(QPair<int, int>(startIndex, count));
        }
        if (!cacheMiss.contains(index))
            cacheMiss.append(index);
        return QJsonObject();
    }
    if (state == QJsonDbQueryModel::Ready) // Pre-fetch only, if in Ready state
        prefetchNearbyPages(index);
    QJsonObject ret = objectCache.valueAtPage(page, uuid);
    lastQueriedIndex = index;
    lastQueriedObject = ret;
    return ret;
}

QVariant QJsonDbQueryModelPrivate::getItem(int index)
{
    return QVariant(getJsonObject(index).toVariantMap());
}

QVariant QJsonDbQueryModelPrivate::getItem(int index, int role)
{
    QJsonObject obj = getJsonObject(index);
    return lookupJsonProperty(obj, properties[role]).toVariant();
}

QString QJsonDbQueryModelPrivate::getItemPartition(int index)
{
    if (index < 0 || index >= objectUuids.size())
        return QString();
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    int partitionIndex = (begin+index).key().partitionIndex();
    if (partitionIndex <= partitionObjects.count())
        return partitionObjects[partitionIndex];
    return QString();
}

int QJsonDbQueryModelPrivate::indexOf(const QString &uuid) const
{
    if (!objectSortValues.contains(uuid))
        return -1;
    const SortingKey &key = objectSortValues.value(uuid);
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    QMap<SortingKey, QString>::const_iterator end = objectUuids.constEnd();
    QMap<SortingKey, QString>::const_iterator i = objectUuids.find(key);
    return iterator_position(begin, end, i);
}

void QJsonDbQueryModelPrivate::sendNotification(int partitionIndex, const QJsonObject &object, QJsonDbWatcher::Action action)
{
    if (action == QJsonDbWatcher::Created) {
       addItem(object, partitionIndex);
    } else if (action == QJsonDbWatcher::Removed)
        deleteItem(object, partitionIndex);
    else if (action == QJsonDbWatcher::Updated) {
        updateItem(object, partitionIndex);
    }
}

void QJsonDbQueryModelPrivate::onPartitionStateChanged()
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
#ifdef JSONDB_LISTMODEL_DEBUG
    qDebug() << Q_FUNC_INFO;
#endif
    if (componentComplete && !query.isEmpty() && partitionsReady()) {
        createOrUpdateNotifications();
        fetchModel();
    }

#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

bool QJsonDbQueryModelPrivate::partitionsReady()
{
    for (int i = 0; i < partitionObjects.count(); i++) {
        if (availablePartitions[i].state == PartitionStateNone || availablePartitions[i].state == PartitionStateError)
            return false;
    }
    return true;
}

void QJsonDbQueryModelPrivate::_q_keyResponse(int index, const QList<QJsonObject> &v, const QString &sortKey)
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

void QJsonDbQueryModelPrivate::_q_valueResponse(int index, const QList<QJsonObject> &v)
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

void QJsonDbQueryModelPrivate::_q_indexResponse(int index, const QList<QJsonObject> &v)
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

void QJsonDbQueryModelPrivate::_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString & message)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(QJsonDbQueryModel);
    qWarning() << QString(QStringLiteral("JsonDb error: %1 %2")).arg(code).arg(message);
    if (code != QtJsonDb::QJsonDbRequest::PartitionUnavailable) {
        int oldErrorCode = errorCode;
        errorCode = code;
        errorString = message;
        if (oldErrorCode != errorCode)
            emit q->errorChanged(q->error());
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void QJsonDbQueryModelPrivate::_q_notificationsAvailable()
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(QJsonDbQueryModel);
    QJsonDbWatcher *watcher = qobject_cast<QJsonDbWatcher *>(q->sender());
    int partitionIndex = indexOfWatcher(watcher);
    if (!watcher || partitionIndex == -1)
        return;
    QList<QJsonDbNotification> list = watcher->takeNotifications();
    for (int i = 0; i < list.count(); i++) {
        const QJsonDbNotification & notification = list[i];
        QJsonObject object = notification.object();
        QJsonDbWatcher::Action action = notification.action();
        if (state == QJsonDbQueryModel::Querying) {
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

void QJsonDbQueryModelPrivate::_q_partitionWatcherNotificationsAvailable()
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(QJsonDbQueryModel);
    QJsonDbWatcher *watcher = qobject_cast<QJsonDbWatcher *>(q->sender());
    int partitionIndex = indexOfPartitionObjectWatcher(watcher);
    if (!watcher || partitionIndex == -1)
        return;
    QList<QJsonDbNotification> list = watcher->takeNotifications();
    for (int i = 0; i < list.count(); i++) {
        const QJsonDbNotification & notification = list[i];
        QJsonObject object = notification.object();
        QJsonDbWatcher::Action action = notification.action();
        QJsonDbQueryModelPrivate::JsonDbPartitionState previousState = availablePartitions[partitionIndex].state;
        if (action == QJsonDbWatcher::Removed) {
            availablePartitions[partitionIndex].state = PartitionStateOffline;
        } else {
            availablePartitions[partitionIndex].state = object.value(QStringLiteral("available"))
                    .toBool() ? PartitionStateOnline : PartitionStateOffline;
        }
        if (previousState != availablePartitions[partitionIndex].state)
            this->onPartitionStateChanged();
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void QJsonDbQueryModelPrivate::_q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(QJsonDbQueryModel);
    if (code != QtJsonDb::QJsonDbRequest::PartitionUnavailable) {
        int oldErrorCode = errorCode;
        errorCode = code;
        errorString = message;
        if (oldErrorCode != errorCode)
            emit q->errorChanged(q->error());
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void QJsonDbQueryModelPrivate::_q_partitionObjectQueryFinished()
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(QJsonDbQueryModel);
    QJsonDbReadRequest *request = qobject_cast<QJsonDbReadRequest *>(q->sender());
    if (request) {
        QList<QJsonObject> objects = request->takeResults();
        int count = objects.count();
        if (count) {
            QString name = objects[0].value(QStringLiteral("name")).toString();
            // Skip this  if name has been changed already
            int partitionIndex = partitionObjects.indexOf(name);
            if (partitionIndex == -1)
                return;
            JsonDbPartitionState state = objects[0].value(QStringLiteral("available"))
                    .toBool() ? PartitionStateOnline : PartitionStateOffline;
            availablePartitions[partitionIndex].state = state;
            onPartitionStateChanged();
        }
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void QJsonDbQueryModelPrivate::_q_partitionObjectQueryError()
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(QJsonDbQueryModel);
    QJsonDbReadRequest *request = qobject_cast<QJsonDbReadRequest *>(q->sender());
    //TODO: NEED A MAPPING BETWEEN REQUEST AND PARTITION NAME / INDEX
    if (request) {
        qWarning() << "Partition query error";
    }
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

void QJsonDbQueryModelPrivate::_q_partitionWatcherNotificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_Q(QJsonDbQueryModel);
    QJsonDbWatcher *watcher = qobject_cast<QJsonDbWatcher *>(q->sender());
    int partitionIndex = indexOfPartitionObjectWatcher(watcher);
    qWarning() << QStringLiteral("QJsonDbQueryModel PartitionObjectNotification error: %1 %2").arg(code).arg(message);
    availablePartitions[partitionIndex].state = PartitionStateError;
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}



void QJsonDbQueryModelPrivate::_q_verifyDefaultIndexType(int index)
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

void QJsonDbQueryModelPrivate::appendPartition(const QString& partitionName)
{
    Q_Q(QJsonDbQueryModel);
    partitionObjects.append(partitionName);

    partitionObjectDetails.append(RequestInfo());
    startWatchingPartitionObject(partitionName);
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
        if (state == QJsonDbQueryModel::None)
            resetModel = true;
        fetchIndexSpec(partitionObjects.count()-1);
    }
}

void QJsonDbQueryModelPrivate::startWatchingPartitionObject(const QString &partitionName)
{
    Q_Q(QJsonDbQueryModel);
    availablePartitions.append(JsonDbAvailablePartitionsInfo());

    QString query;
    if (partitionName.isEmpty())
        query= QLatin1String("[?_type=\"Partition\"][?default=true]");
    else
        query = QStringLiteral("[?_type=\"Partition\"][?name=\"%1\"]").arg(partitionName);

    // Create a watcher to watch changes in partition state
    QJsonDbWatcher *partitionWatcher = new QJsonDbWatcher();
    partitionWatcher->setQuery(query);
    partitionWatcher->setWatchedActions(QJsonDbWatcher::Created | QJsonDbWatcher::Removed | QJsonDbWatcher::Updated);
    partitionWatcher->setPartition(QStringLiteral("Ephemeral"));
    QObject::connect(partitionWatcher, SIGNAL(notificationsAvailable(int)),
                     q, SLOT(_q_partitionWatcherNotificationsAvailable()));
    QObject::connect(partitionWatcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
                     q, SLOT(_q_partitionWatcherNotificationError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    mConnection->addWatcher(partitionWatcher);

    // Create a query to ephemeral partition to find out the state (&name) of the partition
    QJsonDbReadRequest *request = new QJsonDbReadRequest;
    request->setQuery(query);
    request->setPartition(QStringLiteral("Ephemeral"));
    QObject::connect(request, SIGNAL(finished()), q, SLOT(_q_partitionObjectQueryFinished()));
    QObject::connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    QObject::connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            q, SLOT(_q_partitionObjectQueryError()));
    QObject::connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            request, SLOT(deleteLater()));
    mConnection->send(request);
}

void QJsonDbQueryModelPrivate::clearPartitions()
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

/*!
    \class QJsonDbQueryModel
    \inmodule QtJsonDb
    \internal

    The QJsonDbQueryModel provides a read-only QAbstractListModel usable with views such as
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

    \code
        QtJsonDb::QJsonDbQueryModel *model = new QtJsonDb::QJsonDbQueryModel(connection);
        model->setQueryRoleNames(roleMap);
        model->appendPartition(QStringLiteral("User"));
        model->setQuery(QStringLiteral("[?_type=\"MyType\"][/orderKey]");
        model->populate();
    \endcode

    \sa setQueryRoleNames(), appendPartition(), setQuery(), populate()
*/
/*!
    \enum QJsonDbQueryModel::State

    This enum describes current model state.

    \value None      Query returned zero items.
    \value Querying  Model has issued the query but not received a response.
    \value Ready     Model has received items.
    \value Error     Model received an error response from the partitions.
*/

QJsonDbQueryModel::QJsonDbQueryModel(QJsonDbConnection *dbConnection, QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(new QJsonDbQueryModelPrivate(this))
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(QJsonDbQueryModel);
    d->init(dbConnection);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}

QJsonDbQueryModel::~QJsonDbQueryModel()
{
}

/*!
    Loads objects matching the specified query from the specified
    partitions into the model cache.
*/
void QJsonDbQueryModel::populate()
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(QJsonDbQueryModel);
    d->componentComplete = true;
    if (!d->query.isEmpty() && d->partitionObjects.count() && d->partitionsReady()) {
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
    \property QJsonDbQueryModel::rowCount
    Indicates how many items match the query. Value is zero unless state is Ready.
*/
int QJsonDbQueryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    Q_D(const QJsonDbQueryModel);
    return d->objectUuids.count();
}

/*!
  Returns the \a role of the object at \a modelIndex.

  \sa QJsonDbQueryModel::setQueryRoleNames()
 */
QVariant QJsonDbQueryModel::data(const QModelIndex &modelIndex, int role) const
{
    QVariant ret;
    int index = modelIndex.row();
    return data (index, role);
}

QVariant QJsonDbQueryModel::data(int index, int role) const
{
    Q_D(const QJsonDbQueryModel);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    QVariant ret;
    if (index < 0 || index >= d->objectUuids.size())
        ret = QVariant();
    // Special synchronous handling for _uuid and _indexValue
    else if (d->properties[role].at(0) == QLatin1String("_uuid")) {
        JsonDbModelIndexType::const_iterator itr = d->objectUuids.constBegin() + index;
        ret = itr.value();
    }
    else if (d->properties[role].at(0) == QLatin1String("_indexValue")) {
        JsonDbModelIndexType::const_iterator itr = d->objectUuids.constBegin() + index;
        ret = itr.key().value();
    }
    else
        ret = const_cast<QJsonDbQueryModel*>(this)->d_func()->getItem(index, role);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
    return ret;
}

QHash<int, QByteArray> QJsonDbQueryModel::roleNames() const
{
    Q_D(const QJsonDbQueryModel);
    return d->roleNames;
}

/*!
    \property QJsonDbQueryModel::roleNames

    Controls which properties to expose from the objects matching the query.

    Setting \a queryRoleNames to a list of strings causes the model to expose
    corresponding object values as roles to the delegate for each item viewed.

    \code
      TBD
    \endcode

    Setting \a queryRoleNames to a dictionary remaps properties in the object
    to the specified roles in the model.

    In the following example, role \a a would yield the value of
    property \a aLongName in the objects. Role \a liftedProperty would
    yield the value of \a o.nested.property for each matching object \a
    o in the database.

    \code
      TBD
    \endcode

  */

QVariant QJsonDbQueryModel::queryRoleNames() const
{
    Q_D(const QJsonDbQueryModel);
    return d->roleMap;
}

void QJsonDbQueryModel::setQueryRoleNames(const QVariant &vroles)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(QJsonDbQueryModel);
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
    \property QJsonDbQueryModel::query

    The query string in JsonQuery format used by the model to fetch
    items from the database. Setting an empty query clears all the elements

    \sa QJsonDbQueryModel::bindings
 */
QString QJsonDbQueryModel::query() const
{
    Q_D(const QJsonDbQueryModel);
    return d->query;
}

void QJsonDbQueryModel::setQuery(const QString &newQuery)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(QJsonDbQueryModel);
    QString l_query = newQuery;

    // For JsonDbListModel compatibility
    if (!newQuery.isEmpty()) {
        QStringList queryParts = newQuery.split(QLatin1Char('['));
        if (queryParts.last().at(0) == QLatin1Char('/') ||
                queryParts.last().at(0) == QLatin1Char('\\')) {
            setSortOrder(queryParts.last().prepend(QLatin1Char('[')));
            l_query.chop(queryParts.last().count());
            qWarning() << "Having sortOrder as part of query is deprecated. Use property 'sortOrder' instead";
        }
    }

    const QString oldQuery = d->query;
    if (oldQuery == l_query)
        return;

    d->query = l_query;
    if (rowCount() && d->query.isEmpty()) {
        d->reset();
    }

    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count() || !d->partitionsReady())
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
    \property QJsonDbQueryModel::bindings

    Holds the bindings for the placeholders used in the query string. Note that
    the placeholder marker '%' should not be included as part of the keys.

    \sa QJsonDbQueryModel::query()
 */
QVariantMap QJsonDbQueryModel::bindings() const
{
    Q_D(const QJsonDbQueryModel);
    return d->queryBindings;
}

void QJsonDbQueryModel::setBindings(const QVariantMap &newBindings)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(QJsonDbQueryModel);
    d->queryBindings = newBindings;

    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count() || !d->partitionsReady())
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
    \property QJsonDbQueryModel::cacheSize
    Holds the maximum number of objects hold in memory by the model.
*/
int QJsonDbQueryModel::cacheSize() const
{
    Q_D(const QJsonDbQueryModel);
    return d->cacheSize;
}

void QJsonDbQueryModel::setCacheSize(int newCacheSize)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(QJsonDbQueryModel);
    if (newCacheSize == d->cacheSize)
        return;

    d->cacheSize = newCacheSize;
    d->setCacheParams(d->cacheSize);
    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count() || !d->partitionsReady())
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

/*!
    \property QJsonDbQueryModel::sortOrder

    The order used by the model to sort the items. Make sure that there
    is a matching Index in the database for this sortOrder. This has to be
    specified in the JsonQuery format.

    \sa QJsonDbQueryModel::bindings
*/
QString QJsonDbQueryModel::sortOrder() const
{
    Q_D(const QJsonDbQueryModel);
    return d->sortOrder;
}

void QJsonDbQueryModel::setSortOrder(const QString &newSortOrder)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(QJsonDbQueryModel);

    const QString oldSortOrder = d->sortOrder;
    d->sortOrder = newSortOrder;
    if (oldSortOrder != newSortOrder) {
        d->parseSortOrder();
        if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count() || !d->partitionsReady())
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
    \property QJsonDbQueryModel::state
    The current state of the model.
    \list
    \li QJsonDbQueryModel::None - The model is not initialized
    \li QJsonDbQueryModel::Querying - It is querying the results from server
    \li QJsonDbQueryModel::Ready - Results are ready
    \li QJsonDbQueryModel::Error - Cannot find a matching index on the server
    \endlist

*/
QJsonDbQueryModel::State QJsonDbQueryModel::state() const
{
    Q_D(const QJsonDbQueryModel);
    return d->state;
}

/*!
    Returns the index of the object with \a uuid from the model.

    Becaues the model caches all uuids the index can be returned
    immediately.
*/
int QJsonDbQueryModel::indexOf(const QString &uuid) const
{
    Q_D(const QJsonDbQueryModel);
    return d->indexOf(uuid);
}

/*!
    Fetches the object at position \a index in the model.

    Becaues the model caches objects, it may not have a copy of the
    object in memory. In that case, it queries the appropriate
    partition to fetch the object.

    The model emits signal objectAvailable() with \a index, the
    object, and the name of the partition containing the object.

    \sa QJsonDbQueryModel::objectAvailable()
*/
void QJsonDbQueryModel::fetchObject(int index)
{
#ifdef JSONDB_LISTMODEL_BENCHMARK
    QElapsedTimer elt;
    elt.start();
#endif
    Q_D(QJsonDbQueryModel);
    if (index < 0 || index >= d->objectUuids.size())
        return;
    int page = d->objectCache.findPage(index);
    if (page == -1) {
        d->requestPageContaining(index);
        return;
    }
    QJsonObject result = d->getJsonObject(index);
    QString partitionName = d->getItemPartition(index);
    emit objectAvailable(index, result, partitionName);
#ifdef JSONDB_LISTMODEL_BENCHMARK
    qint64 elap = elt.elapsed();
    if (elap > 3)
        qDebug() << Q_FUNC_INFO << "took more than 3 ms (" << elap << "ms )";
#endif
}


/*!
    Returns the name of partition at position \a index in the list of partition names used by the model.
*/
QString QJsonDbQueryModel::partitionName(int index) const
{
    QJsonDbQueryModel *pThis = const_cast<QJsonDbQueryModel *>(this);
    return pThis->d_func()->getItemPartition(index);
}

/*!
    Returns the list of partition names used by the model.
*/
QStringList QJsonDbQueryModel::partitionNames() const
{
    QJsonDbQueryModel *pThis = const_cast<QJsonDbQueryModel *>(this);
    return QStringList(pThis->d_func()->partitionObjects);
}

/*!
    Sets the list of partition names used by the model to \a partitionNames.
*/
void QJsonDbQueryModel::setPartitionNames(const QStringList &partitionNames)
{
    QJsonDbQueryModel *pThis = const_cast<QJsonDbQueryModel *>(this);
    pThis->d_func()->clearPartitions();
    foreach (const QString &partitionName, partitionNames) {
        pThis->d_func()->appendPartition(partitionName);
    }
}

/*!
    Add a partition named \a partitionName to the list of partition names used by the model.
*/
void QJsonDbQueryModel::appendPartitionName(const QString &partitionName)
{
    Q_D(QJsonDbQueryModel);
    d->appendPartition(partitionName);
}

/*!
    \property QJsonDbQueryModel::error
*/
QVariantMap QJsonDbQueryModel::error() const
{
    Q_D(const QJsonDbQueryModel);
    QVariantMap errorMap;
    errorMap.insert(QLatin1String("code"), d->errorCode);
    errorMap.insert(QLatin1String("message"), d->errorString);
    return errorMap;
}

QJSValue QJsonDbQueryModel::propertyInjector() const
{
    Q_D(const QJsonDbQueryModel);
    return d->injectCallback;

}

void QJsonDbQueryModel::setPropertyInjector(const QJSValue &callback)
{
    Q_D(QJsonDbQueryModel);
    d->injectCallback = callback;
    d->isCallable = callback.isCallable();
    refreshItems();
}

void QJsonDbQueryModel::refreshItems()
{
    Q_D(QJsonDbQueryModel);
    if (d->objectUuids.count()) {
        d->resetModel = true;
        d->objectCache.clear();
        d->createObjectRequests(0, qMin(d->objectCache.maxItems(), d->objectUuids.count()));
    }
}

#include "moc_qjsondbquerymodel_p.cpp"
QT_END_NAMESPACE_JSONDB
