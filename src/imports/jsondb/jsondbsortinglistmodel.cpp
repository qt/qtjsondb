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

#include "jsondbsortinglistmodel.h"
#include "jsondbsortinglistmodel_p.h"
#include "private/jsondb-strings_p.h"
#include "plugin.h"

#include <QJSEngine>
#include <QJSValueIterator>

/*!
  \internal
  \class JsonDbSortingListModel
*/

QVariant lookupProperty(QVariantMap object, const QStringList &path);
QString removeArrayOperator(QString propertyName);

JsonDbSortingListModelPrivate::JsonDbSortingListModelPrivate(JsonDbSortingListModel *q)
    : q_ptr(q)
    , componentComplete(false)
    , resetModel(true)
    , overflow(false)
    , limit(-1)
    , chunkSize(100)
    , state(JsonDbSortingListModel::None)
{
}

void JsonDbSortingListModelPrivate::init()
{
    Q_Q(JsonDbSortingListModel);
    q->connect(&dbClient, SIGNAL(response(int,const QVariant&)),
               q, SLOT(_q_jsonDbResponse(int,const QVariant&)));
    q->connect(&dbClient, SIGNAL(error(int,int,const QString&)),
               q, SLOT(_q_jsonDbErrorResponse(int,int,const QString&)));
}

JsonDbSortingListModelPrivate::~JsonDbSortingListModelPrivate()
{
    // Why do we need to do this while destroying the object
    clearNotifications();
}

template <typename T> int iterator_position(T &begin, T &end, T &value)
{
    int i = 0;
    for (T itr = begin;(itr != end && itr != value); i++, itr++) {}
    return i;
}

void JsonDbSortingListModelPrivate::removeLastItem()
{
    Q_Q(JsonDbSortingListModel);
    int index = objectUuids.count()-1;
    q->beginRemoveRows(parent, index, index);
    QMultiMap<SortingKey, QString>::iterator lastPos = objectUuids.begin() + index;
    const QString &uuid = (lastPos).value();
    objects.remove(uuid);
    objectSortValues.remove(uuid);
    objectUuids.erase(lastPos);
    q->endRemoveRows();
    emit q->rowCountChanged(objects.count());
}

// insert item notification handler
// + add items, for chunked read
void JsonDbSortingListModelPrivate::addItem(const QVariantMap &item, int partitionIndex)
{
    Q_Q(JsonDbSortingListModel);
    const QString &uuid = item.value(QLatin1String("_uuid")).toString();
    // ignore duplicates.
    if (objects.contains(uuid))
        return;
    SortingKey key(partitionIndex, item, ascendingOrders, orderPaths);
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    QMap<SortingKey, QString>::const_iterator end = objectUuids.constEnd();
    QMap<SortingKey, QString>::const_iterator i = objectUuids.upperBound(key);
    int index = iterator_position(begin, end, i);
    if (limit > 0  &&  objectUuids.count() == limit) {
        if (index < limit) {
            // when we hit the limit make space for a single item
            removeLastItem();
        } else {
            // beyond the last element, ignore the object.
            return;
        }
    }
    q->beginInsertRows(parent, index, index);
    objects.insert(uuid, item);
    objectUuids.insert(key, uuid);
    objectSortValues.insert(uuid, key);
    if (limit > 0  &&  objectUuids.count() == limit) {
        overflow = true;
    }
    q->endInsertRows();
    emit q->rowCountChanged(objects.count());
}

// deleteitem notification handler
void JsonDbSortingListModelPrivate::deleteItem(const QVariantMap &item, int partitionIndex)
{
    Q_UNUSED(partitionIndex);
    Q_Q(JsonDbSortingListModel);
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
            objects.remove(uuid);
            objectUuids.remove(key);
            objectSortValues.remove(uuid);
            q->endRemoveRows();
            emit q->rowCountChanged(objects.count());
            if (overflow) {
                // when overflow is set, we need to re-run the query to fill the list
                //QMetaObject::invokeMethod(q, "_q_refreshModel", Qt::QueuedConnection);
                _q_refreshModel();

            }
        }
    }
}

// updateitem notification handler
void JsonDbSortingListModelPrivate::updateItem(const QVariantMap &item, int partitionIndex)
{
    Q_Q(JsonDbSortingListModel);
    QString uuid = item.value(QLatin1String("_uuid")).toString();
    QMap<QString, SortingKey>::const_iterator keyIndex =  objectSortValues.constFind(uuid);
    if (keyIndex != objectSortValues.constEnd()) {
        SortingKey key = keyIndex.value();
        SortingKey newKey(partitionIndex, item, ascendingOrders, orderPaths);
        QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
        QMap<SortingKey, QString>::const_iterator end = objectUuids.constEnd();
        QMap<SortingKey, QString>::const_iterator oldPos = objectUuids.constFind(key);
        int oldIndex = iterator_position(begin, end, oldPos);
        // keys are same, modify the object
        if (key == newKey) {
            objects.remove(uuid);
            objects.insert(uuid, item);
            QModelIndex modelIndex = q->createIndex(oldIndex, 0);
            emit q->dataChanged(modelIndex, modelIndex);
            return;
        }
        // keys are different
        QMap<SortingKey, QString>::const_iterator newPos = objectUuids.upperBound(newKey);
        int newIndex = iterator_position(begin, end, newPos);
        if (overflow && oldIndex <= limit-1 && newIndex >= limit-1) {
            // new position is the last or beyond the limit
            // this will do a refresh
            deleteItem(item, partitionIndex);
            return;
        }
        if (newIndex != oldIndex && newIndex != oldIndex+1) {
            q->beginMoveRows(parent, oldIndex, oldIndex, parent, newIndex);
            objects.remove(uuid);
            objects.insert(uuid, item);
            objectUuids.remove(key);
            objectUuids.insert(newKey, uuid);
            objectSortValues.remove(uuid);
            objectSortValues.insert(uuid, newKey);
            q->endMoveRows();

        } else {
            // same position, update the object
            objects.remove(uuid);
            objects.insert(uuid, item);
            objectUuids.remove(key);
            objectUuids.insert(newKey, uuid);
            objectSortValues.remove(uuid);
            objectSortValues.insert(uuid, newKey);
        }
        newPos = objectUuids.constFind(newKey);
        begin = objectUuids.constBegin();
        end = objectUuids.constEnd();
        newIndex = iterator_position(begin, end, newPos);
        QModelIndex modelIndex = q->createIndex(newIndex, 0);
        emit q->dataChanged(modelIndex, modelIndex);
    } else {
        addItem(item, partitionIndex);
    }
}

void JsonDbSortingListModelPrivate::fillData(const QVariant &v, int partitionIndex)
{
    Q_Q(JsonDbSortingListModel);
    QVariantMap m = v.toMap();
    QVariantList items;
    if (m.contains(QLatin1String("data")))
        items = m.value(QLatin1String("data")).toList();
    RequestInfo &r = partitionObjectDetails[partitionIndex];
    r.lastSize = items.size();
    if (resetModel) {
        // for the first chunk, add all items
        q->beginResetModel();
        objects.clear();
        objectUuids.clear();
        objectSortValues.clear();
        for (int i = 0; i < r.lastSize; i++) {
            const QVariantMap &item = items.at(i).toMap();
            const QString &uuid = item.value(QLatin1String("_uuid")).toString();
            SortingKey key(partitionIndex, item, ascendingOrders, orderPaths);
            objects.insert(uuid, item);
            objectUuids.insertMulti(key, uuid);
            objectSortValues.insert(uuid, key);
        }
        if (limit > 0  && r.lastSize > limit) {
            // remove extra objects
            QMap<SortingKey, QString>::iterator start = objectUuids.begin();
            for (int i = limit; i < r.lastSize; i++) {
                const QString &uuid = (start+i).value();
                objects.remove(uuid);
                objectSortValues.remove(uuid);
            }
            // remove extra items from the sorted map
            start += limit;
            while (start != objectUuids.end()) {
                start = objectUuids.erase(start);
            }
       }
        q->endResetModel();
        emit q->rowCountChanged(objects.count());
        resetModel = false;
    } else {
        // subsequent chunks, insert items, preserving the limit.
        for (int i = 0; i < r.lastSize; i++) {
            addItem(items.at(i).toMap(), partitionIndex);
        }
    }

    // Check if requests from different partitions returned
    // all the results
    bool allRequestsFinished = true;
    for (int i = 0; i<partitionObjectDetails.count(); i++) {
        if (partitionObjectDetails[i].lastSize >= chunkSize || partitionObjectDetails[i].lastSize == -1) {
            allRequestsFinished = false;
            break;
        }
    }
    if (allRequestsFinished) {
        // retrieved all elements
        state = JsonDbSortingListModel::Ready;
        emit q->stateChanged(state);
        for (int i = 0; i<pendingNotifications.size(); i++) {
            const NotifyItem &pending = pendingNotifications[i];
            sendNotifications(pending.notifyUuid, pendingNotifications[i].item, pendingNotifications[i].action);
        }
        pendingNotifications.clear();
        // overflow status is used when handling notifications.
        if (limit > 0 && objects.count() >= limit)
            overflow = true;
        else
            overflow = false;
    } else if (r.lastSize >= chunkSize){
        // more items, fetch next chunk
        fetchNextChunk(partitionIndex);
    }
}

void JsonDbSortingListModelPrivate::sortObjects()
{
    Q_Q(JsonDbSortingListModel);
    if (!objects.count())
        return;
    emit q->layoutAboutToBeChanged();
    QMap<QString, SortingKey> currentObjectSortValues(objectSortValues);
    objectUuids.clear();
    objectSortValues.clear();
    QHashIterator<QString, QVariantMap> i(objects);
    while (i.hasNext()) {
        i.next();
        const QVariantMap &object = i.value();
        const QString &uuid = object.value("_uuid", QString()).toString();
        int partitionIndex = currentObjectSortValues[uuid].partitionIndex();
        SortingKey key(partitionIndex, object, ascendingOrders, orderPaths);
        objectUuids.insertMulti(key, uuid);
        objectSortValues.insert(uuid, key);
    }
    emit q->layoutChanged();
    if (overflow && state != JsonDbSortingListModel::Querying)
        _q_refreshModel();
}

//Clears all the state information.
void JsonDbSortingListModelPrivate::reset()
{
    Q_Q(JsonDbSortingListModel);
    q->beginResetModel();
    clearNotifications();
    objects.clear();
    objectUuids.clear();
    objectSortValues.clear();
    q->endResetModel();
    emit q->rowCountChanged(0);
    state = JsonDbSortingListModel::None;
    emit q->stateChanged(state);
 }

void JsonDbSortingListModelPrivate::fetchPartition(int index)
{
    Q_Q(JsonDbSortingListModel);
    if (index >= partitionObjects.count())
        return;

    if (state != JsonDbSortingListModel::Querying) {
        state =  JsonDbSortingListModel::Querying;
        emit q->stateChanged(state);
    }
    RequestInfo &r = partitionObjectDetails[index];
    QPointer<JsonDbPartition> p = partitionObjects[index];
    if (p) {
        r.lastSize = -1;
        r.lastOffset = 0;
        r.requestId = dbClient.query(query, r.lastOffset, chunkSize, p->name());
    }
}

void JsonDbSortingListModelPrivate::fetchModel(bool reset)
{
    resetModel = reset;
    if (resetModel) {
        objects.clear();
        objectUuids.clear();
        objectSortValues.clear();
    }
    for (int i = 0; i<partitionObjects.count(); i++) {
        fetchPartition(i);
    }
}

void JsonDbSortingListModelPrivate::fetchNextChunk(int partitionIndex)
{
    RequestInfo &r = partitionObjectDetails[partitionIndex];
    r.lastOffset += chunkSize;
    r.requestId = dbClient.query(query, r.lastOffset, chunkSize, partitionObjects[partitionIndex]->name());
}

void JsonDbSortingListModelPrivate::clearNotification(int index)
{
    if (index >= partitionObjects.count())
        return;

    RequestInfo &r = partitionObjectDetails[index];
    if (!r.notifyUuid.isEmpty()) {
        dbClient.unregisterNotification(r.notifyUuid);
    }
    r.clear();
}

void JsonDbSortingListModelPrivate::clearNotifications()
{
    for (int i = 0; i<partitionObjects.count(); i++)
        clearNotification(i);
}

void JsonDbSortingListModelPrivate::createOrUpdateNotification(int index)
{
    Q_Q(JsonDbSortingListModel);
    if (index >= partitionObjects.count())
        return;
    clearNotification(index);
    JsonDbClient::NotifyTypes notifyActions = JsonDbClient::NotifyCreate
            | JsonDbClient::NotifyUpdate| JsonDbClient::NotifyRemove;
    partitionObjectDetails[index].notifyUuid= dbClient.registerNotification(
                notifyActions , query, partitionObjects[index]->name(),
                q, SLOT(_q_dbNotified(QString,QtAddOn::JsonDb::JsonDbNotification)),
                q, SLOT(_q_dbNotifyReadyResponse(int,QVariant)),
                SLOT(_q_dbNotifyErrorResponse(int,int,QString)));

}

void JsonDbSortingListModelPrivate::createOrUpdateNotifications()
{
    for (int i = 0; i<partitionObjects.count(); i++) {
        createOrUpdateNotification(i);
    }
}

void JsonDbSortingListModelPrivate::parseSortOrder()
{
    QRegExp orderMatch("\\[([/\\\\[\\]])[ ]*([^\\[\\]]+)[ ]*\\]");
    ascendingOrders.clear();
    orderProperties.clear();
    orderPaths.clear();
    int matchIndex = 0, firstMatch = -1;
    while ((matchIndex = orderMatch.indexIn(sortOrder, matchIndex)) >= 0) {
        bool ascendingOrder = false;
        if (!orderMatch.cap(1).compare(QLatin1String("/")))
            ascendingOrder = true;
        ascendingOrders << ascendingOrder;
        orderProperties << orderMatch.cap(2);
        orderPaths << orderMatch.cap(2).split('.');
        if (firstMatch == -1)
            firstMatch = matchIndex;
        matchIndex += orderMatch.matchedLength();
    }
}

int JsonDbSortingListModelPrivate::indexOfrequestId(int requestId)
{
    for (int i = 0; i<partitionObjectDetails.count(); i++) {
        if (requestId == partitionObjectDetails[i].requestId)
            return i;
    }
    return -1;
}

int JsonDbSortingListModelPrivate::indexOfNotifyUUID(const QString& notifyUuid)
{
    for (int i = 0; i<partitionObjectDetails.count(); i++) {
        if (notifyUuid == partitionObjectDetails[i].notifyUuid)
            return i;
    }
    return -1;
}

QVariant JsonDbSortingListModelPrivate::getItem(int index)
{
    if (index < 0 || index >= objects.size())
        return QVariant();
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    return objects.value((begin+index).value());
}

QVariant JsonDbSortingListModelPrivate::getItem(int index, int role)
{
    if (index < 0 || index >= objects.size())
        return QVariant();
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    return lookupProperty(objects.value((begin+index).value()), properties[role]);
}

JsonDbPartition* JsonDbSortingListModelPrivate::getItemPartition(int index)
{
    if (index < 0 || index >= objects.size())
        return 0;
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    int partitionIndex = (begin+index).key().partitionIndex();
    if (partitionIndex <= partitionObjects.count())
        return partitionObjects[partitionIndex];
    return 0;
}

int JsonDbSortingListModelPrivate::indexOf(const QString &uuid) const
{
    if (!objects.contains(uuid))
        return -1;
    const SortingKey &key = objectSortValues.value(uuid);
    QMap<SortingKey, QString>::const_iterator begin = objectUuids.constBegin();
    QMap<SortingKey, QString>::const_iterator end = objectUuids.constEnd();
    QMap<SortingKey, QString>::const_iterator i = objectUuids.find(key);
    return iterator_position(begin, end, i);
}

void JsonDbSortingListModelPrivate::sendNotifications(const QString& currentNotifyUuid, const QVariant &v, JsonDbClient::NotifyType action)
{
    int idx = indexOfNotifyUUID(currentNotifyUuid);
    if (idx == -1)
        return;

    const QVariantMap &item = v.toMap();
    if (action == JsonDbClient::NotifyCreate) {
        addItem(item, idx);
    } else if (action == JsonDbClient::NotifyRemove) {
        deleteItem(item, idx);
    } else if (action == JsonDbClient::NotifyUpdate) {
        updateItem(item, idx);
    }
}

void JsonDbSortingListModelPrivate::_q_jsonDbResponse(int id, const QVariant &v)
{
    int idx = indexOfrequestId(id);
    if (idx != -1) {
        partitionObjectDetails[idx].requestId = -1;
        fillData(v, idx);
    }
}

void JsonDbSortingListModelPrivate::_q_jsonDbErrorResponse(int id, int code, const QString &message)
{
    int idx = -1;
    if ((idx = indexOfrequestId(id)) != -1) {
        partitionObjectDetails[idx].requestId = -1;
        qWarning() << QString("JsonDb error: %1 %2").arg(code).arg(message);
    }
}

void JsonDbSortingListModelPrivate::_q_refreshModel()
{
    // ignore active requests.
    fetchModel(false);
}

void JsonDbSortingListModelPrivate::_q_dbNotified(const QString &notify_uuid, const QtAddOn::JsonDb::JsonDbNotification &_notification)
{
    if (state == JsonDbSortingListModel::Querying) {
        NotifyItem  pending;
        pending.notifyUuid = notify_uuid;
        pending.item = _notification.object();
        pending.action = _notification.action();
        pendingNotifications.append(pending);
    } else if (state == JsonDbSortingListModel::Ready) {
        sendNotifications(notify_uuid, _notification.object(), _notification.action());
    }
}

void JsonDbSortingListModelPrivate::_q_dbNotifyReadyResponse(int /* id */, const QVariant &/* result */)
{
}

void JsonDbSortingListModelPrivate::_q_dbNotifyErrorResponse(int id, int code, const QString &message)
{
    Q_UNUSED(id);
    qWarning() << QString("JsonDbSortingList Notification error: %1 %2").arg(code).arg(message);
}

void JsonDbSortingListModelPrivate::partitions_append(QDeclarativeListProperty<JsonDbPartition> *p, JsonDbPartition *v)
{
    JsonDbSortingListModel *q = qobject_cast<JsonDbSortingListModel *>(p->object);
    JsonDbSortingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        pThis->partitionObjects.append(QPointer<JsonDbPartition>(v));\
        pThis->partitionObjectDetails.append(RequestInfo());
        if (pThis->componentComplete && !pThis->query.isEmpty()) {
            pThis->createOrUpdateNotification(pThis->partitionObjects.count()-1);
            if (pThis->state == JsonDbSortingListModel::None) {
                pThis->resetModel = true;
            }
            pThis->fetchPartition(pThis->partitionObjects.count()-1);
        }
    }
}

int JsonDbSortingListModelPrivate::partitions_count(QDeclarativeListProperty<JsonDbPartition> *p)
{
    JsonDbSortingListModel *q = qobject_cast<JsonDbSortingListModel *>(p->object);
    JsonDbSortingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        return pThis->partitionObjects.count();
    }
    return 0;
}

JsonDbPartition* JsonDbSortingListModelPrivate::partitions_at(QDeclarativeListProperty<JsonDbPartition> *p, int idx)
{
    JsonDbSortingListModel *q = qobject_cast<JsonDbSortingListModel *>(p->object);
    JsonDbSortingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis && idx < pThis->partitionObjects.count()) {
        return pThis->partitionObjects.at(idx);
    }
    return 0;
}

void JsonDbSortingListModelPrivate::partitions_clear(QDeclarativeListProperty<JsonDbPartition> *p)
{
    JsonDbSortingListModel *q = qobject_cast<JsonDbSortingListModel *>(p->object);
    JsonDbSortingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        pThis->partitionObjects.clear();
        pThis->partitionObjectDetails.clear();
        pThis->reset();
    }
}

/*!
    \qmlclass JsonDbSortingListModel
    \inqmlmodule QtJsonDb
    \inherits ListModel
    \since 1.x

    The JsonDbSortingListModel provides a read-only ListModel usable with views such as
    ListView or GridView displaying data items matching a query. The sorting is done by
    the model after retrieving the whole result set. Maximum number of items in the model
    can be set by queryLimit. Whan a limit is set and the query result has more items,
    only 'queryLimit' number of items will be saved in the model.

    The model is initalised by retrieving the results in chunk. After receving the first
    chunk, the model is reset with items from it. For subsequent chunks of data, the items
    might get removed or inserted. The state will be "Querying" during initialization and will
    be changed to "Ready".

    \code
    import QtJsonDb 1.0 as JsonDb

    JsonDb.JsonDbSortingListModel {
        id: contactsModel
        query: '[?_type="Contact"]'
        queryLimit: 100
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

JsonDbSortingListModel::JsonDbSortingListModel(QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(new JsonDbSortingListModelPrivate(this))
{
    Q_D(JsonDbSortingListModel);
    d->init();
}

JsonDbSortingListModel::~JsonDbSortingListModel()
{
}

void JsonDbSortingListModel::classBegin()
{
}

void JsonDbSortingListModel::componentComplete()
{
    Q_D(JsonDbSortingListModel);
    d->componentComplete = true;
    if (!d->query.isEmpty() && d->partitionObjects.count()) {
        d->createOrUpdateNotifications();
        d->fetchModel();
    }
}

/*!
    \qmlproperty int QtJsonDb::JsonDbSortingListModel::rowCount
    The number of items in the model.
*/
int JsonDbSortingListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    Q_D(const JsonDbSortingListModel);
    return d->objects.count();
}

QVariant JsonDbSortingListModel::data(const QModelIndex &modelIndex, int role) const
{
    JsonDbSortingListModel *pThis = const_cast<JsonDbSortingListModel *>(this);
    return pThis->d_func()->getItem(modelIndex.row(), role);
}

/*!
    \qmlproperty ListOrObject QtJsonDb::JsonDbSortingListModel::roleNames

    Controls which properties to expose from the objects matching the query.

    Setting \a roleNames to a list of strings causes the model to expose
    corresponding object values as roles to the delegate for each item viewed.

    \code
    JsonDb.JsonDbSortingListModel {
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
    JsonDb.JsonDbSortingListModel {
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

QVariant JsonDbSortingListModel::scriptableRoleNames() const
{
    Q_D(const JsonDbSortingListModel);
    return d->roleMap;
}

void JsonDbSortingListModel::setScriptableRoleNames(const QVariant &vroles)
{
    Q_D(JsonDbSortingListModel);
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
}

/*!
    \qmlproperty string QtJsonDb::JsonDbSortingListModel::query

    The query string in JsonQuery format used by the model to fetch
    items from the database. Setting an empty query clears all the elements

    In the following example, the JsonDbSortingListModel would contain all
    the objects with \a _type contains the value \a "CONTACT" from partition
    called "com.nokia.shared"

    \qml
    JsonDb.JsonDbSortingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        partitions:[ JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
    }
    \endqml

*/
QString JsonDbSortingListModel::query() const
{
    Q_D(const JsonDbSortingListModel);
    return d->query;
}

void JsonDbSortingListModel::setQuery(const QString &newQuery)
{
    Q_D(JsonDbSortingListModel);

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
}

/*!
    \qmlproperty int QtJsonDb::JsonDbSortingListModel::queryLimit
    Holds the maximum no of items for the model.

    \code
    JsonDb.JsonDbSortingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        queryLimit: 100
        partitions: [JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
    }
    \endcode

*/

int JsonDbSortingListModel::queryLimit() const
{
    Q_D(const JsonDbSortingListModel);
    return d->limit;
}

void JsonDbSortingListModel::setQueryLimit(int newQueryLimit)
{
    Q_D(JsonDbSortingListModel);
    if (newQueryLimit == d->limit)
        return;

    d->limit = newQueryLimit;
    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count())
        return;

    d->fetchModel();
}

/*!
    \qmlproperty bool QtJsonDb::JsonDbSortingListModel::overflow
    This will be true if actual numer of results is more than the queryLimit
*/

bool JsonDbSortingListModel::overflow() const
{
    Q_D(const JsonDbSortingListModel);
    return d->overflow;
}

void JsonDbSortingListModel::partitionNameChanged(const QString &partitionName)
{
    Q_UNUSED(partitionName);
    Q_D(JsonDbSortingListModel);

    if (!d->componentComplete || d->query.isEmpty() || !d->partitionObjects.count())
        return;

    d->createOrUpdateNotifications();
    d->fetchModel();
}

/*!
    \qmlproperty list QtJsonDb::JsonDbSortingListModel::partitions
    Holds the list of partition objects for the model.
    \code
    JsonDb.JsonDbSortingListModel {
        id: contacts
        query: '[?_type="Contact"]'
        partitions :[nokiaPartition, nokiaPartition2]
        roleNames: ["firstName", "lastName", "_uuid", "_version"]
        sortOrder:"[/firstName]"
    }

    \endcode
*/


QDeclarativeListProperty<JsonDbPartition> JsonDbSortingListModel::partitions()
{
    return QDeclarativeListProperty<JsonDbPartition>(this, 0
                                                     , &JsonDbSortingListModelPrivate::partitions_append
                                                     , &JsonDbSortingListModelPrivate::partitions_count
                                                     , &JsonDbSortingListModelPrivate::partitions_at
                                                     , &JsonDbSortingListModelPrivate::partitions_clear);
}

/*!
    \qmlproperty string QtJsonDb::JsonDbSortingListModel::sortOrder

    The order used by the model to sort the items. The sortOrder has to be
    specided in the JsonQuery format. The sorting is done by the model and not
    by the database. This makes it possible to support multiple sortkeys.
    When a qyeryLimit is set and the query result contains more items than the
    the limit (an overflow), the model will discard the rest of the items. The
    items are discarded based on their sorted position.

    If the queryLimit is set, changing a sortkey can trigger a refetch of the
    model in case of overflows.

    In the following example, the JsonDbSortingListModel would contain all
    the objects of type \a "CONTACT" sorted by their \a firstName field

    \qml
    JsonDb.JsonDbSortingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        partitions: [ JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
        sortOrder: "[/firstName]"
    }
    \endqml

*/

QString JsonDbSortingListModel::sortOrder() const
{
    Q_D(const JsonDbSortingListModel);
    return d->sortOrder;
}

void JsonDbSortingListModel::setSortOrder(const QString &newSortOrder)
{
    Q_D(JsonDbSortingListModel);

    const QString oldSortOrder = d->sortOrder;
    d->sortOrder = newSortOrder;
    if (oldSortOrder != newSortOrder) {
        d->parseSortOrder();
        d->sortObjects();
    }
}

/*!
    \qmlproperty State QtJsonDb::JsonDbSortingListModel::state
    The current state of the model.
    \list
    \o State.None - The model is not initialized
    \o State.Querying - It is querying the results from server
    \o State.Ready - Results are ready
    \endlist
*/

JsonDbSortingListModel::State JsonDbSortingListModel::state() const
{
    Q_D(const JsonDbSortingListModel);
    return d->state;
}

/*!
    \qmlmethod int QtJsonDb::JsonDbSortingListModel::indexOf(string uuid)

    Returns the index of the object with the \a uuid in the model. If the object is
    not found it returns -1
*/
int JsonDbSortingListModel::indexOf(const QString &uuid) const
{
    Q_D(const JsonDbSortingListModel);
    return d->indexOf(uuid);
}

/*!
    \qmlmethod object QtJsonDb::JsonDbSortingListModel::get(int index)

    Returns the object at the specified \a index in the model. The result.object property
    contains the object in its raw form as returned by the query, the rolenames
    are not applied. The object.partition is the partition for the returned.
    If the index is out of range it returns an object with empty partition & object properties.
    \code
        onClicked: {
            var item = contacts.get(listView.currentIndex);
            item.object.firstName = item.object.firstName+ "*";
            item.partition.update(item.object, updateCallback);
        }

    \endcode
*/

QJSValue JsonDbSortingListModel::get(int index) const
{
    JsonDbSortingListModel *pThis = const_cast<JsonDbSortingListModel *>(this);
    QVariant object = pThis->d_func()->getItem(index);
    JsonDbPartition*partition = pThis->d_func()->getItemPartition(index);
    QJSValue result = g_declEngine->newObject();
    result.setProperty(QLatin1String("object"), g_declEngine->toScriptValue(object));
    result.setProperty(QLatin1String("partition"), g_declEngine->newQObject(partition));
    return result;
}

/*!
    \qmlmethod object QtJsonDb::JsonDbSortingListModel::get(int index, string property)

    Retrieves the value of the \a property for the object at \a index. If the index
    is out of range or the property name is not valid it returns an empty object.
*/

QVariant JsonDbSortingListModel::get(int index, const QString &property) const
{
    Q_D(const JsonDbSortingListModel);
    return data(createIndex(index, 0),  d->roleNames.key(property.toLatin1(), -1));
}

/*!
    \qmlmethod object QtJsonDb::JsonDbSortingListModel::getPartition(int index)

    Returns the partition object at the specified \a index in the model. If
    the index is out of range it returns an empty object.
*/

JsonDbPartition* JsonDbSortingListModel::getPartition(int index) const
{
    JsonDbSortingListModel *pThis = const_cast<JsonDbSortingListModel *>(this);
    return pThis->d_func()->getItemPartition(index);
}
/*!
    \qmlsignal QtJsonDb::JsonDbSortingListModel::onStateChanged(State state)

    This handler is called when the a model \a state is changed.
*/

/*!
    \qmlsignal QtJsonDb::JsonDbSortingListModel::onRowCountChanged(int newRowCount)

    This handler is called when the number of items in the model has changed.
*/

SortingKey::SortingKey(int partitionIndex, const QVariantMap &object, const QList<bool> &directions, const QList<QStringList> &paths)
{
    QVariantList values;
    for (int i = 0; i < paths.size(); i++)
        values.append(lookupProperty(object, paths[i]));
    d = new SortingKeyPrivate(partitionIndex, QUuid(object[QLatin1String("_uuid")].toString()), directions, values);

}

SortingKey::SortingKey(const SortingKey &other)
    :d(other.d)
{
}

int SortingKey::partitionIndex() const
{
    return d->partitionIndex;
}

static bool operator<(const QVariant& lhs, const QVariant& rhs)
{
    if ((lhs.type() == QVariant::Int) && (rhs.type() == QVariant::Int))
        return lhs.toInt() < rhs.toInt();
    else if ((lhs.type() == QVariant::LongLong) && (rhs.type() == QVariant::LongLong))
        return lhs.toLongLong() < rhs.toLongLong();
    else if ((lhs.type() == QVariant::Double) && (rhs.type() == QVariant::Double))
        return lhs.toFloat() < rhs.toFloat();
    return (QString::compare(lhs.toString(), rhs.toString(), Qt::CaseInsensitive ) < 0);
}

bool SortingKey::operator <(const SortingKey &rhs) const
{
    for (int i = 0; i < d->values.size(); i++) {
        const QVariant &lhsValue = d->values[i];
        const QVariant &rhsValue = rhs.d->values[i];
        if (lhsValue != rhsValue) {
            bool result = lhsValue < rhsValue;
            return (d->directions[i] ? result :!result);
        }
    }
    return (d->uuid < rhs.d->uuid);
}

bool SortingKey::operator ==(const SortingKey &rhs) const
{
    bool equal = true;
    for (int i = 0; i < d->values.size(); i++) {
        const QVariant &lhsValue = d->values[i];
        const QVariant &rhsValue = rhs.d->values[i];
        if (lhsValue != rhsValue) {
            equal = false;
            break;
        }
    }
    return (equal && (d->uuid == rhs.d->uuid));
}

#include "moc_jsondbsortinglistmodel.cpp"
