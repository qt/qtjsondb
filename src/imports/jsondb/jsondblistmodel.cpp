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

#include "jsondblistmodel.h"
#include "jsondblistmodel_p.h"
#include "jsondatabase.h"

#include <QJSEngine>
#include <QJSValueIterator>
#include <QThread>
#include <QString>
#include <qdebug.h>


#undef DEBUG_LIST_MODEL

#ifdef DEBUG_LIST_MODEL
#define DEBUG() qDebug() << Q_FUNC_INFO
#else
#define DEBUG() if (0) qDebug() << QString("%1:%2").arg(__FUNCTION__).arg(__LINE__)
#endif

/*!
  \internal
  \class JsonDbListModel
*/
QT_BEGIN_NAMESPACE_JSONDB

JsonDbListModelPrivate::JsonDbListModelPrivate(JsonDbListModel *q)
    : q_ptr(q)
    , chunkSize(40)
    , lowWaterMark(10)
    , maxCacheSize(0) // unlimited cache
    , totalRowCount(0)
    , cacheStart(0)
    , cacheEnd(0)
    , newChunkOffset(0)
    , lastFetchedIndex(-1)
    , requestInProgress(false)
    , componentComplete(false)
    , resetModel(true)
    , updateReceived(false)
    , totalRowCountRecieved(false)
    , state(None)
    , errorCode(0)
{
}

void JsonDbListModelPrivate::init()
{
    Q_Q(JsonDbListModel);
    QObject::connect(&valueRequest, SIGNAL(finished(int,QList<QJsonObject>,QString)),
                     q, SLOT(_q_valueResponse(int,QList<QJsonObject>)));
    QObject::connect(&valueRequest, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     q, SLOT(_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    QObject::connect(&countRequest, SIGNAL(finished(int,QList<QJsonObject>,QString)),
                     q, SLOT(_q_countResponse(int,QList<QJsonObject>)));
    QObject::connect(&countRequest, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     q, SLOT(_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));

}

JsonDbListModelPrivate::~JsonDbListModelPrivate()
{
    clearNotification();
}

void JsonDbListModelPrivate::clearCache(int newStart)
{
    cacheStart = newStart;
    cacheEnd = newStart;
    objectUuids.clear();
    objectSortValues.clear();
    data.clear();
    cachedUuids.clear();
    lastFetchedItem.clear();
    lastFetchedIndex = -1;
}

int JsonDbListModelPrivate::makeSpaceFor(int count, int insertAt)
{
    int itemsToRemove = itemsInCache() + count - maxCacheSize;
    // Check for unlimited cache
    if (maxCacheSize <= 0)
        itemsToRemove = 0;
    int index = 0;
    int newCacheStart = insertAt;
    int newCacheEnd = newCacheStart + count;
    if (newCacheStart == cacheEnd) {
        // adding elements in the end.
    } else if (newCacheEnd == cacheStart) {
        // adding elements in the beginning.
        index = itemsInCache() - itemsToRemove;
    } else if (newCacheStart >= cacheStart && newCacheEnd <= cacheEnd) {
        // adding elements within the cached elements
        // don't remove any, we should ignore duplicates.
        return 0;
    } else if (newCacheStart <= cacheStart && newCacheEnd >= cacheEnd) {
        // new cache will include the current one, so skip it
        return 0;
    } else if (newCacheEnd < cacheStart || newCacheStart > cacheEnd) {
        // we need to invalidate the cache, we only store elements in sequence.
        itemsToRemove = itemsInCache();
        clearCache(insertAt);
        return itemsToRemove;
    } else if (newCacheEnd < cacheEnd) {
        // adding items in the beginning, with overlap.
        itemsToRemove -= (newCacheEnd - cacheStart);
        index = itemsInCache() - itemsToRemove;
    } else if (newCacheStart > cacheStart) {
        // adding items towards the end, with overlap
        itemsToRemove -= (cacheEnd - newCacheStart);
    }
    if (itemsToRemove <= 0)
        return 0;
    for (int i = 0; i < itemsToRemove; i++) {
        const QString &uuid = cachedUuids.at(i+index);
        Q_ASSERT(objectSortValues.contains(uuid));
        const JsonDbSortKey &key = objectSortValues.value(uuid);
        Q_ASSERT(objectUuids.contains(key, uuid));
        objectUuids.remove(key, uuid);
        objectSortValues.remove(uuid);
        data.remove(uuid);
    }
    QList<QString>::iterator first = cachedUuids.begin()+index;
    QList<QString>::iterator last = first+itemsToRemove;
    cachedUuids.erase(first, last);

    if (index > 0)
        cacheEnd -= itemsToRemove;
    else
        cacheStart += itemsToRemove;
    return itemsToRemove;
}

JsonDbSortKey JsonDbListModelPrivate::sortKey(const QVariantMap &object)
{
    return JsonDbSortKey(object, orderDirections, orderPaths);
}

// removes an item from the cache
void JsonDbListModelPrivate::removeItem(int index)
{
    if (cachedUuids.size() <= index) {
        qWarning() << "removeItem index not in cache";
        return;
    }

    const QString &uuid = cachedUuids[index];
    const JsonDbSortKey &key = objectSortValues.value(uuid);
    objectUuids.remove(key, uuid);
    objectSortValues.remove(uuid);
    data.remove(uuid);
    cachedUuids.removeAt(index);
    cacheEnd = qMax(cacheStart, cacheEnd-1);
}

// finds the object position within the cache limits
int JsonDbListModelPrivate::findSortedPosition(const QString& uuid)
{
    int pos = 0;
    QMap<JsonDbSortKey,QString>::const_iterator it;
    QMap<JsonDbSortKey,QString>::const_iterator end = objectUuids.end();
    for (it = objectUuids.begin(); it != end; it++, pos++) {
        if (it.value() == uuid)
            return pos;
    }
    // uuid not found in objectUuids
    return -1;
}

// insert item notification handler
void JsonDbListModelPrivate::insertItem(const QVariantMap &item, bool emitSignals)
{
    Q_Q(JsonDbListModel);
    Q_UNUSED(item);

    lastFetchedItem.clear();
    lastFetchedIndex = -1;
    clearCache(cacheStart);
    totalRowCount++;
    if (emitSignals) {
        // When a new item is added, the position of the item is not known
        // to the model. We will clear the cache and notify that an item
        // is added at the end + all data is changed.
        q->beginInsertRows(parent, totalRowCount-1, totalRowCount-1);
        q->endInsertRows();
        emit q->countChanged();
        emit q->rowCountChanged();
        QModelIndex start = q->createIndex(0,0);
        QModelIndex end = q->createIndex(totalRowCount-1, 0);
        emit q->dataChanged(start, end);
    }
}

// deleteitem notification handler
void JsonDbListModelPrivate::deleteItem(const QVariantMap &item, bool emitSignals)
{
    Q_Q(JsonDbListModel);

    lastFetchedItem.clear();
    lastFetchedIndex = -1;
    const QString &uuid = item.value(QLatin1String("_uuid")).toString();
    int index = cachedUuids.indexOf(uuid);
    if (index != -1) {
        // When item is in the cache emit signals using the exact position.
        if (emitSignals)
            q->beginRemoveRows(parent, cacheStart+index, cacheStart+index);
        removeItem(index);
        totalRowCount = qMax(0, totalRowCount -1);
        if (emitSignals) {
            q->endRemoveRows();
            emit q->countChanged();
            emit q->rowCountChanged();
        }
    } else {
        // Model dosen't know the position from where the item is deleted.
        // We will clear the cache and notify that an item is removed
        // from the end + all data is changed.
        if (!totalRowCount)
            emitSignals = false;
        if (emitSignals)
            q->beginRemoveRows(parent, totalRowCount-1, totalRowCount-1);
        clearCache(cacheStart);
        totalRowCount = qMax(0, totalRowCount -1);
        if (emitSignals) {
            q->endRemoveRows();
            emit q->countChanged();
            emit q->rowCountChanged();
            QModelIndex start = q->createIndex(0,0);
            QModelIndex end = q->createIndex((totalRowCount ? totalRowCount-1 :0), 0);
            emit q->dataChanged(start, end);
        }
    }
}

// updateitem notification handler
void JsonDbListModelPrivate::updateItem(const QVariantMap &item)
{
    Q_Q(JsonDbListModel);
    lastFetchedItem.clear();
    lastFetchedIndex = -1;
    const QString &uuid = item.value(QLatin1String("_uuid")).toString();
    // if item is currently in cache.
    if (objectSortValues.contains(uuid)) {
        int currentIndex = findSortedPosition(uuid);
        deleteItem(item, false);
        insertItem(item, false);
        int newIndex = findSortedPosition(uuid);
        if (currentIndex == newIndex) {
            // emit signal for the changed item.
            QModelIndex modelIndex = q->createIndex(newIndex, 0);
            emit q->dataChanged(modelIndex, modelIndex);
            return;
        }
    }
    // We are not sure about the position of the updated item,
    // clear the cache and notify that all data is changed.
    clearCache(cacheStart);
    QModelIndex start = q->createIndex(0,0);
    QModelIndex end = q->createIndex((totalRowCount ? totalRowCount-1 :0), 0);
    emit q->dataChanged(start, end);
}

void JsonDbListModelPrivate::_q_requestAnotherChunk(int offset)
{
    if (requestInProgress || query.isEmpty())
        return;
    int maxItemsToFetch = chunkSize;
    if (offset < 0) {
        maxItemsToFetch += offset;
        offset = 0;
    }
    newChunkOffset = offset;
    if (newChunkOffset >= cacheStart && newChunkOffset < cacheEnd)
        newChunkOffset = cacheEnd;
    // now fetch more
    resetModel = false;
    QJsonDbReadRequest *request = valueRequest.newRequest(0);
    request->setQuery(query);
    request->setProperty("queryOffset", newChunkOffset);
    request->setQueryLimit(maxItemsToFetch);
    request->setPartition(partitionObject->name());
    JsonDatabase::sharedConnection().send(request);
    requestInProgress = true;
}

ModelSyncCall::ModelSyncCall(const QString &_query, int _offset, int _maxItems,
                             const QString & _partitionName, QVariantList *_data)
    : query(_query),
      offset(_offset),
      maxItems(_maxItems),
      partitionName(_partitionName),
      data(_data)
{

}
ModelSyncCall::~ModelSyncCall()
{
    if (request)
        delete request;
    if (connection)
        delete connection;
}

void ModelSyncCall::createSyncRequest()
{
    connection = new QJsonDbConnection();
    connection->connectToServer();
    request = new QJsonDbReadRequest;
    connect(request, SIGNAL(finished()), this, SLOT(onQueryFinished()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(onQueryError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    request->setQuery(query);
    request->setProperty("queryOffset", offset);
    request->setQueryLimit(maxItems);
    request->setPartition(partitionName);
    connection->send(request);
    //qDebug()<<"createSyncRequest Query :"<<query<<request->property("requestId");
}

void ModelSyncCall::onQueryFinished()
{
    *data = qjsonobject_list_to_qvariantlist(request->takeResults());
    //qDebug()<<"onQueryFinished Query :"<<query<<request->property("requestId");
    QThread::currentThread()->quit();
}

void ModelSyncCall::onQueryError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    qWarning() << QString("JsonDbListModel error: %1 %2").arg(code).arg(message);
    QThread::currentThread()->quit();
}

void JsonDbListModelPrivate::fetchChunkSynchronous(int offset)
{
    // Ignore previous reqests
    if (requestInProgress) {
        //requestIds.clear();
        valueRequest.resetRequest();
    }
    Q_ASSERT(!query.isEmpty());
    int maxItemsToFetch = chunkSize;
    if (offset < 0) {
        maxItemsToFetch += offset;
        offset = 0;
    }
    newChunkOffset = offset;
    if (newChunkOffset >= cacheStart && newChunkOffset < cacheEnd)
        newChunkOffset = cacheEnd;
    // now fetch more
    resetModel = false;
    requestInProgress = true;
    QVariantList resultList;
    QThread syncThread;
    ModelSyncCall *call = new ModelSyncCall(query, newChunkOffset, maxItemsToFetch,
                                            partitionObject->name(), &resultList);
    QObject::connect(&syncThread, SIGNAL(started()),
            call, SLOT(createSyncRequest()));
    QObject::connect(&syncThread, SIGNAL(finished()),
            call, SLOT(deleteLater()));
    call->moveToThread(&syncThread);
    syncThread.start();
    syncThread.wait();
    updateCache(resultList);
    requestInProgress = false;
}

void JsonDbListModelPrivate::populateModel()
{
    Q_Q(JsonDbListModel);
    clearCache();
    updateReceived = false;
    totalRowCountRecieved = false;
    totalRowCount = 0;
    newChunkOffset = 0;
    // Request the total count
    QString countQuery = query+"[count]";
    QJsonDbReadRequest *cRequest = countRequest.newRequest(0);
    cRequest->setQuery(countQuery);
    cRequest->setPartition(partitionObject->name());
    JsonDatabase::sharedConnection().send(cRequest);
    //qDebug()<<"Count Query :"<<countQuery<<cRequest->property("requestId");
    //Request at least 2 chunks of data
    int itemsToGet = chunkSize*2;
    if (maxCacheSize)
        itemsToGet = qMin(itemsToGet, maxCacheSize);
    resetModel = true;
    QJsonDbReadRequest *request = valueRequest.newRequest(0);
    request->setQuery(query);
    request->setProperty("queryOffset", newChunkOffset); //TODO change newChunkOffset to 0
    request->setQueryLimit(itemsToGet);
    request->setPartition(partitionObject->name());
    JsonDatabase::sharedConnection().send(request);
    //qDebug()<<"Query :"<<query<<request->property("requestId")<<newChunkOffset;
    createOrUpdateNotification();
    requestInProgress = true;
    state =  JsonDbListModelPrivate::Querying;
    emit q->stateChanged();
}

QVariantMap JsonDbListModelPrivate::getItem(int index, bool handleCacheMiss, bool &cacheMiss)
{
    QVariantMap item;
    if (index < 0 || index >= totalRowCount)
        return item;
    if (index < cacheStart || index >= cacheEnd) {
        DEBUG()<<"Index "<< index<<"Out of Range, fetch more...."<<cacheStart<<cacheEnd;
        // clear and fetch new set of items
        if (handleCacheMiss) {
            // clearing the cache is done by makeSpaceFor()
            fetchChunkSynchronous(qMax(index-(chunkSize/2), 0));
        } else {
            cacheMiss = true;
            return item;
        }
    }

    if (cachedUuids.size() <= (index - cacheStart)) {
        qWarning() << "Could not get Item";
        return item;
    }
    QString uuid = cachedUuids[index - cacheStart];
    item = data.value(uuid);
    if ((lowWaterMark > 0) && (data.size() < totalRowCount)) {
        if ((cacheEnd - index) < lowWaterMark) {
            _q_requestAnotherChunk(cacheEnd);
        } else if (cacheStart && (index - cacheStart) < lowWaterMark) {
            _q_requestAnotherChunk(qMax(cacheStart-chunkSize, 0));
        }
    }
    cacheMiss = false;
    lastFetchedItem = item;
    lastFetchedIndex = index;
    return item;
}

QVariantMap JsonDbListModelPrivate::getItem(const QModelIndex &modelIndex, int role,
                                            bool handleCacheMiss, bool &cacheMiss)
{
    Q_UNUSED(role);
    return getItem(modelIndex.row(), handleCacheMiss, cacheMiss);
}

static QVariantMap updateProperty(QVariantMap item, const QStringList &propertyChain, QVariant value)
{
    if (propertyChain.size() < 1) {
        qCritical() << "updateProperty" << "empty property chain" << item;
    } else if (propertyChain.size() == 1) {
        item.insert(propertyChain[0], value.toString());
    } else {
        QString property = propertyChain[0];
        QVariant newChild = updateProperty(item.value(property).toMap(), propertyChain.mid(1), value);
        item.insert(property, newChild);
    }
    return item;
}

/*!
  \qmlmodule QtJsonDb 1.0

  QML interface to Json Database.
*/

/*!
    \qmlclass JsonDbListModel JsonDbListModel
    \inqmlmodule QtJsonDb 1.0
    \inherits ListModel
    \since 1.x

    The JsonDbListModel provides a ListModel usable with views such as
    ListView or GridView displaying data items matching a query.

    \code
    import QtJsonDb 1.0 as JsonDb

    JsonDb.JsonDbListModel {
      id: contactsModel
      partition: nokiaPartition
      query: "[?_type=\"Contact\"]"
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

/*!
    \qmlproperty int QtJsonDb1::JsonDbListModel::rowCount

    Returns the number of rows in the model.
*/

/*!
    \qmlproperty string QtJsonDb1::JsonDbListModel::query

    The query string in JsonQuery format used by the model to fetch
    items from the database.

    In the following example, the model would contain all
    the objects with \a _type contains the value \a "CONTACT" from partition
    called "com.nokia.shared"

    \qml
    JsonDb.JsonDbListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        partition:JsonDb.Partiton {
            name:"com.nokia.shared"
        }
    }
    \endqml
*/

JsonDbListModel::JsonDbListModel(QObject *parent)
    : QAbstractListModel(parent), d_ptr(new JsonDbListModelPrivate(this))
{
    Q_D(JsonDbListModel);
    d->init();
}

JsonDbListModel::~JsonDbListModel()
{
}

void JsonDbListModel::classBegin()
{
}

void JsonDbListModel::componentComplete()
{
    Q_D(JsonDbListModel);
    d->componentComplete = true;
    if (d->query.isEmpty() || !d->partitionObject)
        return;
    d->populateModel();
}

void JsonDbListModelPrivate::clearNotification()
{
    if (watcher) {
        JsonDatabase::sharedConnection().removeWatcher(watcher);
        delete watcher;
        watcher = 0;
    }
}

void JsonDbListModelPrivate::createOrUpdateNotification()
{
    Q_Q(JsonDbListModel);
    clearNotification();
    watcher = new QJsonDbWatcher();
    watcher->setQuery(query);
    watcher->setWatchedActions(QJsonDbWatcher::Created | QJsonDbWatcher::Updated |QJsonDbWatcher::Removed);
    watcher->setPartition(partitionObject->name());
    QObject::connect(watcher, SIGNAL(notificationsAvailable(int)),
                     q, SLOT(_q_notificationsAvailable()));
    QObject::connect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
                     q, SLOT(_q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    JsonDatabase::sharedConnection().addWatcher(watcher);
}

int JsonDbListModel::sectionIndex(const QString &section,
                                  const QJSValue &successCallback,
                                  const QJSValue &errorCallback)
{
    Q_D(JsonDbListModel);
    if (!successCallback.isCallable()) {
        qWarning("JsonDbListModel Cannot call sectionIndex without a success callbak function");
        return -1;
    }
    // Find the count of items "< section"
    QString sectionCountQueryLT = d->queryWithoutSort+"[?"+d->orderProperties[0]+"<\""+section+"\"][count]";

    QJsonDbReadRequest *request = new QJsonDbReadRequest();
    connect(request, SIGNAL(finished()), this, SLOT(_q_sectionIndexResponse()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(_q_sectionIndexError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    request->setQuery(sectionCountQueryLT);
    request->setPartition(d->partitionObject->name());
    JsonDatabase::sharedConnection().send(request);
    int id = request->property("requestId").toInt();
    CallbackInfo info;
    info.successCallback = successCallback;
    info.errorCallback = errorCallback;
    d->sectionIndexCallbacks.insert(request, info);
    return id;
}

void JsonDbListModelPrivate::_q_sectionIndexResponse()
{
    Q_Q(JsonDbListModel);
    QJsonDbReadRequest *request = qobject_cast<QJsonDbReadRequest*>(q->sender());
    if (request) {
        QList<QJsonObject> v = request->takeResults();
        if (v.count()) {
            int id = request->property("requestId").toInt();
            int count =  (int) v[0].value(QStringLiteral("count")).toDouble();
            CallbackInfo info = sectionIndexCallbacks.value(request);
            if (info.successCallback.isCallable()) {
                QJSValueList args;
                args << info.successCallback.engine()->toScriptValue(id);
                args << info.successCallback.engine()->toScriptValue(count);
                info.successCallback.call(args);
            }
        }
        sectionIndexCallbacks.remove(request);
        request->deleteLater();
    }
}

void JsonDbListModelPrivate::_q_sectionIndexError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    Q_Q(JsonDbListModel);
    QJsonDbReadRequest *request = qobject_cast<QJsonDbReadRequest*>(q->sender());
    if (request) {
        int id = request->property("requestId").toInt();
        CallbackInfo info = sectionIndexCallbacks.value(request);
        if (info.errorCallback.isCallable()) {
            QJSValueList args;
            args << info.successCallback.engine()->toScriptValue(id);
            args << info.successCallback.engine()->toScriptValue(int(code));
            args << info.successCallback.engine()->toScriptValue(message);
            info.errorCallback.call(args);
        }
        sectionIndexCallbacks.remove(request);
        request->deleteLater();
    }
}

int JsonDbListModel::count() const
{
    Q_D(const JsonDbListModel);
    return d->totalRowCount;
}

QModelIndex JsonDbListModel::index(int row, int , const QModelIndex &) const
{
    return createIndex(row, 0);
}

int JsonDbListModel::rowCount(const QModelIndex &) const
{
    Q_D(const JsonDbListModel);
    return d->totalRowCount;
}

QVariant JsonDbListModel::data(const QModelIndex &modelIndex, int role) const
{
    Q_D(const JsonDbListModel);
    QVariantMap item;
    if (!(d->lastFetchedIndex == modelIndex.row()) || (d->lastFetchedIndex == -1)) {
        JsonDbListModel * pThis = const_cast<JsonDbListModel *>(this);
        bool cacheMiss = false;
        item = pThis->d_func()->getItem(modelIndex, role, true, cacheMiss);
    } else {
        item = d->lastFetchedItem;
    }

    QVariant result;
    QStringList property = d->properties[role];
    result = lookupProperty(item, property);

    return result;
}

QHash<int, QByteArray> JsonDbListModel::roleNames() const
{
    Q_D(const JsonDbListModel);
    return d->roleNames;
}

void JsonDbListModel::set(int index, const QJSValue& valuemap,
                          const QJSValue &successCallback,
                          const QJSValue &errorCallback)
{
    Q_D(JsonDbListModel);
    d->set(index, valuemap, successCallback, errorCallback);
}

void JsonDbListModelPrivate::set(int index, const QJSValue& valuemap,
                                 const QJSValue &successCallback,
                                 const QJSValue &errorCallback)
{
    Q_Q(JsonDbListModel);
    if (!valuemap.isObject() || valuemap.isArray()) {
        qDebug() << q->tr("set: value is not an object");
        return;
    }
    // supports only changing an exixting item.
    if (index >= totalRowCount || index < 0) {
        qDebug() << q->tr("set: index %1 out of range").arg(index);
        return;
    }

    bool cacheMiss = false;
    QVariantMap item = getItem(index, false, cacheMiss);
    if (cacheMiss) {
        fetchChunkSynchronous(qMax(index-(chunkSize/2), 0));
        //TODO, do some error checking.
        item = getItem(index, false, cacheMiss);
        if (cacheMiss)
            DEBUG() << "Could not fetch item at index : %1" << index;
    }

    QJSValueIterator it(valuemap);
    while (it.hasNext()) {
        it.next();
        QString name = it.name();
        QVariant v = it.value().toVariant();
        int role = q->roleFromString(name);
        if (role == -1) {
            qDebug() << q->tr("set: property %1 invalid").arg(name);
            continue;
        }
        item = updateProperty(item, properties[role], v);
    }

    lastFetchedItem.clear();
    lastFetchedIndex = -1;
    update(index, item, successCallback, errorCallback);
}

void JsonDbListModel::setProperty(int index, const QString& property, const QVariant& value,
                                  const QJSValue &successCallback,
                                  const QJSValue &errorCallback)
{
    Q_D(JsonDbListModel);
    d->setProperty(index, property, value, successCallback, errorCallback);
}

void JsonDbListModelPrivate::setProperty(int index, const QString& property, const QVariant& value,
                                         const QJSValue &successCallback,
                                         const QJSValue &errorCallback)
{
    Q_Q(JsonDbListModel);
    // supports only changing an exixting item.
    if (index >= totalRowCount || index < 0) {
        qDebug() << q->tr("set: index %1 out of range").arg(index);
        return;
    }

    bool cacheMiss = false;
    QVariantMap item = getItem(index, false, cacheMiss);
    if (cacheMiss) {
        fetchChunkSynchronous(qMax(index-(chunkSize/2), 0));
        //TODO, do some error checking.
        item = getItem(index, false, cacheMiss);
        if (cacheMiss)
            DEBUG() << "Could not fetch item with index : " << index;
    }

    int role = q->roleFromString(property);
    if (role == -1) {
        qDebug() << q->tr("set: property %1 invalid").arg(property);
        return;
    }
    item = updateProperty(item, properties[role], value);

    lastFetchedItem.clear();
    lastFetchedIndex = -1;
    update(index, item, successCallback, errorCallback);
}

void JsonDbListModelPrivate::update(int index, const QVariantMap &item,
                                    const QJSValue &successCallback, const QJSValue &errorCallback)
{
    Q_Q(JsonDbListModel);
    QJsonDbUpdateRequest *request = new QJsonDbUpdateRequest(QJsonObject::fromVariantMap(item));
    QObject::connect(request, SIGNAL(finished()), q, SLOT(_q_updateResponse()));
    QObject::connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            q, SLOT(_q_updateError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    request->setPartition(partitionObject->name());
    JsonDatabase::sharedConnection().send(request);
    CallbackInfo info;
    info.index = index;
    info.successCallback = successCallback;
    info.errorCallback = errorCallback;
    updateCallbacks.insert(request, info);
}

void JsonDbListModelPrivate::_q_updateResponse()
{
    Q_Q(JsonDbListModel);
    QJsonDbWriteRequest *request = qobject_cast<QJsonDbWriteRequest*>(q->sender());
    if (request) {
        int id = request->property("requestId").toInt();
        CallbackInfo info = updateCallbacks.value(request);
        if (info.successCallback.isCallable()) {
            QJSValueList args;
            args << info.successCallback.engine()->toScriptValue(id);
            args << info.successCallback.engine()->toScriptValue(info.index);
            info.successCallback.call(args);
        }
    }
    updateCallbacks.remove(request);
    request->deleteLater();
}

void JsonDbListModelPrivate::_q_updateError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    Q_Q(JsonDbListModel);
    QJsonDbWriteRequest *request = qobject_cast<QJsonDbWriteRequest*>(q->sender());
    if (request) {
        int id = request->property("requestId").toInt();
        CallbackInfo info = updateCallbacks.value(request);
        if (info.errorCallback.isCallable()) {
            QJSValueList args;
            args << info.successCallback.engine()->toScriptValue(id);
            args << info.successCallback.engine()->toScriptValue(info.index);
            args << info.successCallback.engine()->toScriptValue(int(code));
            args << info.successCallback.engine()->toScriptValue(message);
            info.errorCallback.call(args);
        }
        updateCallbacks.remove(request);
        request->deleteLater();
    }
}

void JsonDbListModel::fetchMore(const QModelIndex &)
{
    DEBUG() << endl;
}

bool JsonDbListModel::canFetchMore(const QModelIndex &) const
{
    DEBUG() << endl;
    return false;
}

void JsonDbListModel::setQuery(const QString &newQuery)
{
    Q_D(JsonDbListModel);

    const QString oldQuery = d->query;
    d->query = newQuery;
    if (oldQuery != newQuery) {
        d->findSortOrder();
    }

    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObject)
        return;

    d->populateModel();
}

QString JsonDbListModel::query() const
{
    Q_D(const JsonDbListModel);
    return d->query;
}

/*!
    \qmlproperty object QtJsonDb1::JsonDbListModel::partition

    Holds the partition object for the model.
    \qml
    JsonDb.JsonDbListModel {
        id: contacts
        query: '[?_type="Contact"]'
        roleNames: ["firstName", "lastName", "_uuid"]
        partition: nokiaPartition
    }

    \endqml
*/

JsonDbPartition* JsonDbListModel::partition()
{
    Q_D(JsonDbListModel);

    if (!d->partitionObject) {
        d->defaultPartitionObject = new JsonDbPartition();
        setPartition(d->defaultPartitionObject);
    }
    return d->partitionObject;
}

void JsonDbListModel::setPartition(JsonDbPartition *newPartition)
{
    Q_D(JsonDbListModel);

    if (d->partitionObject == newPartition)
        return;
    if (d->partitionObject == d->defaultPartitionObject)
        delete d->defaultPartitionObject;
    d->partitionObject = newPartition;

    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObject)
        return;

    d->populateModel();
}

void JsonDbListModel::partitionNameChanged(const QString &partitionName)
{
    Q_UNUSED(partitionName);
    Q_D(JsonDbListModel);

    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObject)
        return;

    d->populateModel();
}

void JsonDbListModel::setLimit(int newLimit)
{
    Q_D(JsonDbListModel);
    d->maxCacheSize = newLimit;
}

/*!
  \qmlproperty int QtJsonDb1::JsonDbListModel::limit

  The number of items to be cached. This is not the query limit.
*/
int JsonDbListModel::limit() const
{
    Q_D(const JsonDbListModel);
    return d->maxCacheSize;
}

void JsonDbListModel::setChunkSize(int newChunkSize)
{
    Q_D(JsonDbListModel);
    d->chunkSize = newChunkSize;
}

/*!
  \qmlproperty int QtJsonDb1::JsonDbListModel::chunkSize

  The number of items to fetch at a time from the database.

  The model uses a heuristic to fetch only as many items as needed by
  the view. Each time it requests items it fetches \a chunkSize items.
*/
int JsonDbListModel::chunkSize() const
{
    Q_D(const JsonDbListModel);
    return d->chunkSize;
}

void JsonDbListModel::setLowWaterMark(int newLowWaterMark)
{
    Q_D(JsonDbListModel);
    d->lowWaterMark = newLowWaterMark;
}

/*!
  \qmlproperty int QtJsonDb1::JsonDbListModel::lowWaterMark

  Controls when to fetch more items from the database.

  JsonDbListModel fetch \a chunkSize more items when the associated
  view requests an item within \a lowWaterMark from the end of the
  items previously fetched.
*/
int JsonDbListModel::lowWaterMark() const
{
    Q_D(const JsonDbListModel);
    return d->lowWaterMark;
}


/*!
  \qmlproperty ListOrObject QtJsonDb1::JsonDbListModel::roleNames

  Controls which properties to expose from the objects matching the query.

  Setting \a roleNames to a list of strings causes the model to fetch
  those properties of the objects and expose them as roles to the
  delegate for each item viewed.

  \code
  JsonDb.JsonDbListModel {
    query: "[?_type=\"MyType\"]"
    roleNames: ['a', 'b']
    partition: nokiaPartition
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
  JsonDb.JsonDbListModel {
    id: listModel
    query: "[?_type=\"MyType\"]"
    partition: nokiaPartition
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
QVariant JsonDbListModel::scriptableRoleNames() const
{
    Q_D(const JsonDbListModel);

    DEBUG() << d->roleMap;
    return d->roleMap;
}

void JsonDbListModel::setScriptableRoleNames(const QVariant &vroles)
{
    Q_D(JsonDbListModel);
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
    DEBUG() << d->roleNames;
}

QString JsonDbListModel::state() const
{
    Q_D(const JsonDbListModel);
    switch (d->state) {
    case JsonDbListModelPrivate::None:
        return QString("None");
    case JsonDbListModelPrivate::Querying:
        return QString("Querying");
    case JsonDbListModelPrivate::Ready:
        return QString("Ready");
    default:
        return QString("Error");
    }
}

QString JsonDbListModel::toString(int role) const
{
    Q_D(const JsonDbListModel);
    if (d->roleNames.contains(role))
        return QString::fromLatin1(d->roleNames.value(role));
    else
        return QString();
}

int JsonDbListModel::roleFromString(const QString &roleName) const
{
    Q_D(const JsonDbListModel);
    return d->roleNames.key(roleName.toLatin1(), -1);
}

void JsonDbListModelPrivate::updateCache(const QVariantList &items)
{
    Q_Q(JsonDbListModel);
    int size = items.size();
    int sizeAdded = 0;
    if (size) {
        DEBUG()<<"OLD Cache Start"<<cacheStart<<"Cache End:"<<cacheEnd;
        if (resetModel)
            q->beginResetModel();
        makeSpaceFor(size, newChunkOffset);
        bool appendToCache = (newChunkOffset >= cacheEnd) ? true : false;
        int insertAt = appendToCache ? itemsInCache() : 0;
        DEBUG()<<"INSERT AT :"<<insertAt<<"Elements Retrieved:"<<size<<"ChunkOffset:"<<newChunkOffset<<appendToCache << itemsInCache();
        DEBUG()<<"Cache Start"<<cacheStart<<"Cache End:"<<cacheEnd<<"Total Rows = "<<totalRowCount;
        // Add the new result to cache.
        for (int i = 0; i < size; i++) {
            QVariantMap item = items.at(i).toMap();
            const QString &uuid = item.value(QStringLiteral("_uuid")).toString();
            if (objectSortValues.contains(uuid)){
                break;
            }
            JsonDbSortKey key = sortKey(item);
            objectUuids.insert(key, uuid);
            objectSortValues.insert(uuid, key);
            data[uuid] = item;
            cachedUuids.insert(insertAt++, uuid);
            sizeAdded++;
        }
        DEBUG()<<"sizeAdded:"<<sizeAdded;
        // Update the cache limits
        if (!itemsInCache()) {
            cacheStart = newChunkOffset;
            cacheEnd = cacheStart + sizeAdded;
        } else if (appendToCache) {
            cacheEnd += sizeAdded;
        } else {
            cacheStart = qMax(0, cacheStart-sizeAdded);
        }
        Q_ASSERT(cachedUuids.count() == data.count());
    }
    if (resetModel && totalRowCountRecieved)
        resetModelFinished();
    else
        updateReceived = true;
}

void JsonDbListModelPrivate::_q_valueResponse(int, const QList<QJsonObject> &v)
{
    requestInProgress = false;
    updateCache(qjsonobject_list_to_qvariantlist(v));
}

void JsonDbListModelPrivate::_q_countResponse(int, const QList<QJsonObject> &v)
{
    if (v.count()) {
        totalRowCount =  (int) v[0].value(QStringLiteral("count")).toDouble();
        totalRowCountRecieved = true;

        if (updateReceived)
            resetModelFinished();
    }
}

void JsonDbListModelPrivate::resetModelFinished()
{
    Q_Q(JsonDbListModel);
    q->endResetModel();
    emit q->countChanged();
    emit q->rowCountChanged();
    state =  Ready;
    emit q->stateChanged();
    resetModel = false;
    for (int i = 0; i<pendingNotifications.size(); i++) {
        const NotifyItem &pending = pendingNotifications[i];
        sendNotifications(pending.item, pending.action);
    }
    pendingNotifications.clear();
    //createOrUpdateNotification();
}

bool operator<(const QVariant& a, const QVariant& b)
{
    if ((a.type() == QVariant::Int) && (b.type() == QVariant::Int))
        return a.toInt() < b.toInt();
    else if ((a.type() == QVariant::Double) && (b.type() == QVariant::Double))
        return a.toFloat() < b.toFloat();
    return (QString::compare( a.toString(), b.toString(), Qt::CaseInsensitive ) < 0);
}

bool operator>(const QVariant& a, const QVariant& b) { return b < a; }

bool JsonDbListModelPrivate::findSortOrder()
{
    QRegExp orderMatch("\\[([/\\\\[\\]])[ ]*([^\\[\\]]+)[ ]*\\]");
    orderDirections.clear();
    orderProperties.clear();
    orderPaths.clear();
    int matchIndex = 0, firstMatch = -1;
    DEBUG() << query;
    while ((matchIndex = orderMatch.indexIn(query, matchIndex)) >= 0) {
        orderDirections << orderMatch.cap(1);
        orderProperties << orderMatch.cap(2);
        orderPaths << orderMatch.cap(2).split('.');
        DEBUG() << matchIndex;
        if (firstMatch == -1)
            firstMatch = matchIndex;
        matchIndex += orderMatch.matchedLength();
    }
    queryWithoutSort = query.mid(0,firstMatch);
    if (orderPaths.isEmpty()) {
        orderDirections << "/";
        orderProperties << "_uuid";
        orderPaths << orderProperties[0].split('.');
    }
    DEBUG() << orderDirections << orderProperties << orderPaths;
    return true;
}

void JsonDbListModelPrivate::_q_notificationsAvailable()
{
    QList<QJsonDbNotification> list = watcher->takeNotifications();
    for (int i = 0; i < list.count(); i++) {
        const QJsonDbNotification & notification = list[i];
        QVariantMap object = notification.object().toVariantMap();
        if (resetModel) {
            NotifyItem  pending;
            pending.item = object;
            pending.action = notification.action();
            pendingNotifications.append(pending);
        } else {
            //qDebug()<<"Notify "<<object<<notification.action();
            sendNotifications(object, notification.action());
        }
    }
}

void JsonDbListModelPrivate::sendNotifications(const QVariantMap &v, QJsonDbWatcher::Action action)
{
    if (action == QJsonDbWatcher::Created) {
        insertItem(v);
    } else if (action == QJsonDbWatcher::Removed) {
        deleteItem(v);
    } else if (action == QJsonDbWatcher::Updated) {
        updateItem(v);
    }
}

void JsonDbListModelPrivate::_q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message)
{
    Q_Q(JsonDbListModel);
    qWarning() << QString("JsonDbListModel Notification error: %1 %2").arg(code).arg(message);
    int oldErrorCode = errorCode;
    errorCode = code;
    errorString = message;
    if (oldErrorCode != errorCode)
        emit q->errorChanged(q->error());
}

void JsonDbListModelPrivate::_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString & message)
{
    Q_Q(JsonDbListModel);
    qWarning() << QString("JsonDbListModel error: %1 %2").arg(code).arg(message);
    int oldErrorCode = errorCode;
    errorCode = code;
    errorString = message;
    if (oldErrorCode != errorCode)
        emit q->errorChanged(q->error());
}

/*!
    \qmlmethod object QtJsonDb1::JsonDbListModel::get(int index, string property)

    Retrieves the value of the \a property for the object at \a index. If the index
    is out of range or the property name is not valid it returns an empty object.
*/

QVariant JsonDbListModel::get(int idx, const QString &property) const
{
    int role = roleFromString(property);
    if (role >= 0)
        return data(index(idx, 0), role);
    else {
        qDebug()<<"JsonDbListModel::get Invalid role";
        return QVariant();
    }
}

/*!
    \qmlproperty object QtJsonDb1::JsonDbListModel::error
    \readonly

    This property holds the current error information for the object. It contains:
    \list
    \li error.code -  code for the current error.
    \li error.message - detailed explanation of the error
    \endlist
*/

QVariantMap JsonDbListModel::error() const
{
    Q_D(const JsonDbListModel);
    QVariantMap errorMap;
    errorMap.insert(QLatin1String("code"), d->errorCode);
    errorMap.insert(QLatin1String("message"), d->errorString);
    return errorMap;
}

class JsonDbSortKeyPrivate : public QSharedData {
public:
    JsonDbSortKeyPrivate(QStringList directions, QVariantList keys) : mDirections(directions), mKeys(keys) {}
    const QStringList &directions() const { return mDirections; }
    const QVariantList &keys() const { return mKeys; }
private:
    QStringList mDirections;
    QVariantList mKeys;
    JsonDbSortKeyPrivate(const JsonDbSortKeyPrivate&);
};

JsonDbSortKey::JsonDbSortKey()
{
}

JsonDbSortKey::JsonDbSortKey(const QVariantMap &object, const QStringList &directions, const QList<QStringList> &paths)
{
    QVariantList keys;
    for (int i = 0; i < paths.size(); i++)
        keys.append(lookupProperty(object, paths[i]));
    d = new JsonDbSortKeyPrivate(directions, keys);
}

JsonDbSortKey::JsonDbSortKey(const JsonDbSortKey&other)
    : d(other.d)
{
}

const QVariantList &JsonDbSortKey::keys() const { return d->keys(); }
const QStringList &JsonDbSortKey::directions() const { return d->directions(); }

QString JsonDbSortKey::toString() const
{
    QStringList result;
    for (int i = 0; i < d->keys().size(); i++) {
        result.append(QString("%1%2")
                      .arg(d->directions()[i])
                      .arg(d->keys()[i].toString()));
    }
    return result.join(", ");
}

bool operator <(const JsonDbSortKey &a, const JsonDbSortKey &b)
{
    const QVariantList &akeys = a.keys();
    const QVariantList &bkeys = b.keys();
    const QStringList &adirs = a.directions();
    for (int i = 0; i < akeys.size(); i++) {
        const QVariant &akey = akeys[i];
        const QVariant &bkey = bkeys[i];
        if (akey != bkey) {
            if (adirs[i] == "/")
                return akey < bkey;
            else
                return akey > bkey;
        }
    }
    return false;
}

#include "moc_jsondblistmodel.cpp"
QT_END_NAMESPACE_JSONDB

