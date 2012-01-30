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

#include "jsondb-listmodel.h"
#include "jsondb-listmodel_p.h"
#include "private/jsondb-strings_p.h"

#include <QJSEngine>
#include <QJSValueIterator>


#undef DEBUG_LIST_MODEL

#ifdef DEBUG_LIST_MODEL
#define DEBUG() qDebug() << Q_FUNC_INFO
#else
#define DEBUG() if (0) qDebug() << QString("%1:%2").arg(__FUNCTION__).arg(__LINE__)
#endif


JsonDbListModelPrivate::JsonDbListModelPrivate(JsonDbListModel *q)
    : q_ptr(q)
    , chunkSize(40)
    , lowWaterMark(10)
    , maxCacheSize(0) // unlimited cache
    , totalRowCount(0)
    , cacheStart(0)
    , cacheEnd(0)
    , newChunkOffset(0)
    , totalCountRequestId(-1)
    , lastFetchedIndex(-1)
    , requestInProgress(false)
    , componentComplete(false)
    , resetModel(true)
    , updateRecieved(false)
    , totalRowCountRecieved(false)
    , state(None)
    , jsonDbConnection(JsonDbConnection::instance())
{
}

void JsonDbListModelPrivate::init()
{
    Q_Q(JsonDbListModel);
    q->connect(&jsonDb, SIGNAL(response(int,QVariant)),
               q, SLOT(_q_jsonDbResponse(int,QVariant)),
               Qt::QueuedConnection);
    q->connect(&jsonDb, SIGNAL(error(int,int,QString)),
               q, SLOT(_q_jsonDbErrorResponse(int,int,QString)),
               Qt::QueuedConnection);
    q->connect(&jsonDb, SIGNAL(notified(QString,QVariant,QString)),
               q, SLOT(_q_jsonDbNotified(QString,QVariant,QString)),
               Qt::QueuedConnection);
    q->connect(q, SIGNAL(needAnotherChunk(int)), q, SLOT(_q_requestAnotherChunk(int)), Qt::QueuedConnection);
}

JsonDbListModelPrivate::~JsonDbListModelPrivate()
{
    // Why do we need to do this while destroying the object
    if (!notifyUuid.isEmpty()) {
        QVariantMap notificationObject;
        notificationObject.insert("_uuid", notifyUuid);
        jsonDb.remove(notificationObject);
    }
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
        QModelIndex parent;
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
    const QString &uuid = item.value("_uuid", QString()).toString();
    int index = cachedUuids.indexOf(uuid);
    if (index != -1) {
        // When item is in the cache emit signals using the exact position.
        QModelIndex parent;
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
        QModelIndex parent;
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
    const QString &uuid = item.value("_uuid").toString();
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
    QVariantMap request;
    request.insert(JsonDbString::kQueryStr, query);
    request.insert("offset", newChunkOffset);
    request.insert("limit", maxItemsToFetch);
    resetModel = false;
    requestIds.insert(jsonDb.find(request));
    requestInProgress = true;
}

void JsonDbListModelPrivate::fetchChunkSynchronous(int offset)
{
    // Ignore previous reqests
    if (requestInProgress) {
        requestIds.clear();
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
    QVariantMap request;
    request.insert(JsonDbString::kQueryStr, query);
    request.insert("offset", newChunkOffset);
    request.insert("limit", maxItemsToFetch);
    resetModel = false;
    requestInProgress = true;
    QVariantMap v = jsonDbConnection->sync(JsonDbConnection::makeQueryRequest(query, newChunkOffset, maxItemsToFetch)).toMap();
    requestInProgress = false;
    updateCache(v);
}

void JsonDbListModelPrivate::populateModel()
{
    Q_Q(JsonDbListModel);
    clearCache();
    updateRecieved = false;
    totalRowCountRecieved = false;
    totalRowCount = 0;
    // Request the total count
    QVariantMap requestCount;
    QString countQuery = query+"[count]";
    requestCount.insert(JsonDbString::kQueryStr, countQuery);

    totalCountRequestId = jsonDb.find(requestCount);

    QVariantMap requestQuery;
    //Request at least 2 chunks of data
    requestQuery.insert(JsonDbString::kQueryStr, query);
    requestQuery.insert("offset", newChunkOffset);
    int itemsToGet = chunkSize*2;
    if (maxCacheSize)
        itemsToGet = qMin(itemsToGet, maxCacheSize);
    requestQuery.insert("limit",itemsToGet);
    resetModel = true;
    requestIds.insert(jsonDb.find(requestQuery));
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

static QVariant lookupProperty(QVariantMap object, const QStringList &path)
{
    if (!path.size()) {
        return QVariant();
    }
    QVariantMap emptyMap;
    QVariantList emptyList;
    QVariantList objectList;
    for (int i = 0; i < path.size() - 1; i++) {
        const QString &key = path.at(i);
        // this part of the property is a list
        if (!objectList.isEmpty()) {
            bool ok = false;
            int index = key.toInt(&ok);
            if (ok && (index >= 0) && (objectList.count() > index)) {
                if (objectList.at(index).type() == QVariant::List) {
                    objectList = objectList.at(index).toList();
                    object = emptyMap;
                } else  {
                    object = objectList.at(index).toMap();
                    objectList = emptyList;
                }
                continue;
            }
        }
        // this part is a map
        if (object.contains(key)) {
            if (object.value(key).type() == QVariant::List) {
                objectList = object.value(key).toList();
                object = emptyMap;
            } else  {
                object = object.value(key).toMap();
                objectList = emptyList;
            }
        } else {
            return QVariant();
        }
    }
    const QString &key = path.last();
    // get the last part from the list
    if (!objectList.isEmpty()) {
        bool ok = false;
        int index = key.toInt(&ok);
        if (ok && (index >= 0) && (objectList.count() > index)) {
            return objectList.at(index);
        }
    }
    // if the last part is in a map
    return object.value(key);
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
  \qmlmodule QtAddOn.JsonDb

  QML interface to Json Database.
*/

/*!
    \qmlclass JsonDbListModel
    \ingroup qml-working-with-data
    \inqmlmodule QtAddOn.JsonDb
    \inherits ListModel
    \since 1.x

    The JsonDbListModel provides a ListModel usable with views such as
    ListView or GridView displaying data items matching a query.

    \code
    JsonDbListModel {
      id: contactsModel
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
    \qmlproperty int QtAddOn.JsonDb::JsonDbListModel::rowCount

    Returns the number of rows in the model.
*/

/*!
    \qmlproperty string QtAddOn.JsonDb::JsonDbListModel::query

    Returns the model's query.
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
    if (!d->query.isEmpty()) {
        d->populateModel();
    }
}

void JsonDbListModelPrivate::createOrUpdateNotification()
{
    if (!notifyUuid.isEmpty()) {
        QVariantMap notificationObject;
        notificationObject.insert("_uuid", notifyUuid);
        jsonDb.remove(notificationObject);
        notifyUuid.clear();
    }

    const JsonDbClient::NotifyTypes actions = JsonDbClient::NotifyCreate | JsonDbClient::NotifyUpdate | JsonDbClient::NotifyRemove;
    notificationObjectRequestIds.insert(jsonDb.notify(actions, query));
    DEBUG() << notificationObjectRequestIds;
}

/*!
  \qmlmethod QtAddOn.JsonDb::JsonDbListModel::sectionIndex(section, successCallback, errorCallback)
 */
int JsonDbListModel::sectionIndex(const QString &section,
                                  const QJSValue &successCallback,
                                  const QJSValue &errorCallback)
{
    Q_D(JsonDbListModel);
    // Find the count of items "< section"
    QString sectionCountQueryLT = d->queryWithoutSort+"[?"+d->orderProperties[0]+"<\""+section+"\"][count]";
    QVariantMap request;
    request.insert(JsonDbString::kQueryStr, sectionCountQueryLT);
    int id = d->jsonDb.find(request);
    // Register any valid callbacks
    CallbackInfo info;
    if (successCallback.isCallable()
            || errorCallback.isCallable()) {
        info.successCallback = successCallback;
        info.errorCallback = errorCallback;
        d->sectionIndexRequestIds.insert(id, info);
    }
    return id;
}

/*!
  \qmlproperty int QtAddOn.JsonDb::JsonDbListModel::count

  Returns the number of items in the model.
*/
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
    // Item will be updated through the update notification
    CallbackInfo info;
    info.index = index;
    int id = jsonDb.update(item); // possibly change to variantToQson(item)..
    // Register any valid callbacks
    if (successCallback.isCallable()
            || errorCallback.isCallable()) {
        info.successCallback = successCallback;
        info.errorCallback = errorCallback;
        updateRequestIds.insert(id, info);
    }
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

    int id = jsonDb.update(item); // possibly change to variantToQson(item)..
    // Register any valid callbacks
    if (successCallback.isCallable()
            || errorCallback.isCallable()) {

        // Item will be updated through the update notification
        CallbackInfo info;
        info.index = index;
        info.successCallback = successCallback;
        info.errorCallback = errorCallback;
        updateRequestIds.insert(id, info);
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
    d->state = JsonDbListModelPrivate::Querying;
    emit stateChanged();
    if (oldQuery != newQuery) {
        d->findSortOrder();
    }

    if (!d->componentComplete)
        return;

    d->populateModel();
}

/*!
  \qmlproperty string QtAddOn.JsonDb::JsonDbListModel::query

  Returns the query used by the model to fetch items from the database.

  In the following example, the \a JsonDbListModel would contain all the objects with \a _type contains the value \a "CONTACT"

  \qml
  JsonDbListModel {
      id: listModel
      query: "[?_type=\"CONTACT\"]"
  }
  \endqml

*/
QString JsonDbListModel::query() const
{
    Q_D(const JsonDbListModel);
    return d->query;
}

void JsonDbListModel::setLimit(int newLimit)
{
    Q_D(JsonDbListModel);
    d->maxCacheSize = newLimit;
}

/*!
  \qmlproperty int QtAddOn.JsonDb::JsonDbListModel::limit

  The number of items to be cached.
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
  \qmlproperty int QtAddOn.JsonDb::JsonDbListModel::chunkSize

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
  \qmlproperty int QtAddOn.JsonDb::JsonDbListModel::lowWaterMark

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
  \qmlproperty ListOrObject QtAddOn.JsonDb::JsonDbListModel::roleNames

  Controls which properties to expose from the objects matching the query.

  Setting \a roleNames to a list of strings causes the model to fetch
  those properties of the objects and expose them as roles to the
  delegate for each item viewed.

  \code
  JsonDbListModel {
    query: "[?_type=\"MyType\"]"
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
  JsonDbListModel {
    id: listModel
    query: "[?_type=\"MyType\"]"
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

QString removeArrayOperator(QString propertyName)
{
    propertyName.replace(QLatin1String("["), QLatin1String("."));
    propertyName.remove(QLatin1Char(']'));
    return propertyName;
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
    QAbstractItemModel::setRoleNames(d->roleNames);
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

void JsonDbListModelPrivate::updateCache(const QVariantMap &v)
{
    Q_Q(JsonDbListModel);
    QVariantMap m = v;
    if (m.contains("data")) {
        QVariantList items = m.value("data").toList();
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
                QVariantMap item = items.value(i).toMap();
                const QString &uuid = item.value(JsonDbString::kUuidStr).toString();
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
            updateRecieved = true;
    }
}

void JsonDbListModelPrivate::_q_jsonDbResponse(int id, const QVariant &v)
{
    QVariantMap m = v.toMap();
    if (requestIds.contains(id)) {
        requestIds.remove(id);
        requestInProgress = false;
        updateCache(m);
    } else if (totalCountRequestId == id) {
        QVariantList items = m.value("data").toList();
        m =  items.value(0).toMap();
        totalRowCount =  m.value("count").toInt();
        totalRowCountRecieved = true;
        if (updateRecieved)
            resetModelFinished();
    } else if (notificationObjectRequestIds.contains(id)) {
        notificationObjectRequestIds.remove(id);
        notifyUuid = m.value(JsonDbString::kUuidStr).toString();
    } else if (updateRequestIds.constFind(id) != updateRequestIds.constEnd()) {
        CallbackInfo info = updateRequestIds.value(id);
        if (info.successCallback.isCallable()) {
            QJSValueList args;
            QJSValue scriptResult = info.successCallback.engine()->toScriptValue(id);
            args << scriptResult;
            scriptResult = info.successCallback.engine()->toScriptValue(info.index);
            args << scriptResult;
            info.successCallback.call(args);
        }
        updateRequestIds.remove(id);
    } else if (sectionIndexRequestIds.constFind(id) != sectionIndexRequestIds.constEnd()) {
        CallbackInfo info = sectionIndexRequestIds.value(id);
        if (info.successCallback.isCallable()) {
            QVariantList items = m.value("data").toList();
            m = items.value(0).toMap();
            QJSValueList args;
            QJSValue scriptResult = info.successCallback.engine()->toScriptValue(id);
            args << scriptResult;
            scriptResult = info.successCallback.engine()->toScriptValue(m.value("count").toInt());
            args << scriptResult;
            info.successCallback.call(args);
        }
        sectionIndexRequestIds.remove(id);
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
    pendingNotifications.clear();
    createOrUpdateNotification();
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

void JsonDbListModelPrivate::_q_jsonDbNotified(const QString& currentNotifyUuid, const QVariant &v, const QString &action)
{
    QVariantMap item = v.toMap();
    if (currentNotifyUuid != notifyUuid) {
        return;
    }
    if (resetModel) {
        // we have not received the first chunk, wait for it before processing notifications
        NotifyItem  pending;
        pending.notifyUuid = currentNotifyUuid;
        pending.item = item;
        pending.action = action;
        pendingNotifications.append(pending);
        return;
    }
    if (action == JsonDbString::kCreateStr) {
        insertItem(item);
        return ;
    } else if (action == JsonDbString::kRemoveStr) {
        deleteItem(item);
        return ;
    } else if (action == JsonDbString::kUpdateStr) {
        updateItem(item);
        return ;
    }

}

void JsonDbListModelPrivate::_q_jsonDbErrorResponse(int id, int code, const QString &message)
{
    if (requestIds.contains(id)) {
        requestIds.remove(id);
        qWarning() << QString("JsonDb error: %1 %2").arg(code).arg(message);
    }  else if (updateRequestIds.constFind(id) != updateRequestIds.constEnd()) {
        CallbackInfo info = updateRequestIds.value(id);
        if (info.errorCallback.isCallable()) {
            QJSValueList args;
            args << info.errorCallback.engine()->toScriptValue(id);
            args << info.errorCallback.engine()->toScriptValue(info.index);
            args << info.errorCallback.engine()->toScriptValue(code);
            args << info.errorCallback.engine()->toScriptValue(message);
            info.errorCallback.call(args);
        }
        updateRequestIds.remove(id);
    }
}

void JsonDbListModelPrivate::_q_jsonDbErrorResponse(int code, const QString &message)
{
    qWarning() << QString("JsonDb error: %1 %2").arg(code).arg(message);
}

/*!
    \qmlmethod QVariant JsonDbListModel::get(int idx, const QString &property) const
    \since 1.x
*/
QVariant JsonDbListModel::get(int idx, const QString &property) const
{
    int role = roleFromString(property);
    if (role >= 0)
        return data(index(idx, 0), role);
    else
        return QVariant();
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

#include "moc_jsondb-listmodel.cpp"
