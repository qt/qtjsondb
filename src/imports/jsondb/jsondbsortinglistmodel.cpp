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
#include "plugin.h"
#include "jsondatabase.h"
#include <qdebug.h>
/*!
  \internal
  \class JsonDbSortingListModel
*/

QT_BEGIN_NAMESPACE_JSONDB

JsonDbSortingListModelPrivate::JsonDbSortingListModelPrivate(JsonDbSortingListModel *q)
    : q_ptr(q)
    , componentComplete(false)
    , resetModel(true)
    , overflow(false)
    , limit(-1)
    , chunkSize(100)
    , state(JsonDbSortingListModel::None)
    , errorCode(0)
{
}

void JsonDbSortingListModelPrivate::init()
{
}

JsonDbSortingListModelPrivate::~JsonDbSortingListModelPrivate()
{
    // Why do we need to do this while destroying the object
    clearNotifications();
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

void JsonDbSortingListModelPrivate::fillData(const QVariantList &items, int partitionIndex)
{
    Q_Q(JsonDbSortingListModel);
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
            sendNotifications(pending.partitionIndex, pending.item, pending.action);
        }
        pendingNotifications.clear();
        // overflow status is used when handling notifications.
        if (limit > 0 && objects.count() >= limit)
            overflow = true;
        else
            overflow = false;
    } else if (r.lastSize >= chunkSize){
        // more items, fetch next chunk
        fetchPartition(partitionIndex, false);
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

void JsonDbSortingListModelPrivate::fetchPartition(int index, bool reset)
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
    Q_ASSERT(p);
    if (reset) {
        r.lastSize = -1;
        r.lastOffset = 0;
    } else {
        r.lastOffset += chunkSize;
    }
    QJsonDbReadRequest *request = valueRequests[index]->newRequest(index);
    request->setQuery(query);
    QVariantMap::ConstIterator i = queryBindings.constBegin();
    while (i != queryBindings.constEnd()) {
        request->bindValue(i.key(), QJsonValue::fromVariant(i.value()));
        ++i;
    }
    request->setProperty("queryOffset", r.lastOffset);
    request->setQueryLimit(chunkSize);
    request->setPartition(p->name());
    JsonDatabase::sharedConnection().send(request);
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

void JsonDbSortingListModelPrivate::clearNotification(int index)
{
    if (index >= partitionObjects.count())
        return;

    RequestInfo &r = partitionObjectDetails[index];
    if (r.watcher) {
        JsonDatabase::sharedConnection().removeWatcher(r.watcher);
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
    QJsonDbWatcher *watcher = new QJsonDbWatcher();
    watcher->setQuery(query);
    watcher->setWatchedActions(QJsonDbWatcher::Created | QJsonDbWatcher::Updated |QJsonDbWatcher::Removed);
    watcher->setPartition(partitionObjects[index]->name());
    QVariantMap::ConstIterator i = queryBindings.constBegin();
    while (i != queryBindings.constEnd()) {
        watcher->bindValue(i.key(), QJsonValue::fromVariant(i.value()));
        ++i;
    }
    QObject::connect(watcher, SIGNAL(notificationsAvailable(int)),
                     q, SLOT(_q_notificationsAvailable()));
    QObject::connect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
                     q, SLOT(_q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    JsonDatabase::sharedConnection().addWatcher(watcher);
    partitionObjectDetails[index].watcher = watcher;
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

int JsonDbSortingListModelPrivate::indexOfWatcher(QJsonDbWatcher *watcher)
{
    for (int i = 0; i < partitionObjectDetails.count(); i++) {
        if (watcher == partitionObjectDetails[i].watcher)
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

void JsonDbSortingListModelPrivate::sendNotifications(int partitionIndex, const QVariantMap &v, QJsonDbWatcher::Action action)
{
    if (action == QJsonDbWatcher::Created) {
        addItem(v, partitionIndex);
    } else if (action == QJsonDbWatcher::Removed) {
        deleteItem(v, partitionIndex);
    } else if (action == QJsonDbWatcher::Updated) {
        updateItem(v, partitionIndex);
    }
}

void JsonDbSortingListModelPrivate::_q_valueResponse(int index, const QList<QJsonObject> &v)
{
    fillData(qjsonobject_list_to_qvariantlist(v), index);
}

void JsonDbSortingListModelPrivate::_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString & message)
{
    Q_Q(JsonDbSortingListModel);
    qWarning() << QString("JsonDb error: %1 %2").arg(code).arg(message);
    int oldErrorCode = errorCode;
    errorCode = code;
    errorString = message;
    if (oldErrorCode != errorCode)
        emit q->errorChanged(q->error());
}

void JsonDbSortingListModelPrivate::_q_refreshModel()
{
    // ignore active requests.
    fetchModel(false);
}

void JsonDbSortingListModelPrivate::_q_notificationsAvailable()
{
    Q_Q(JsonDbSortingListModel);
    QJsonDbWatcher *watcher = qobject_cast<QJsonDbWatcher *>(q->sender());
    int partitionIndex = indexOfWatcher(watcher);
    if (!watcher || partitionIndex == -1)
        return;
    QList<QJsonDbNotification> list = watcher->takeNotifications();
    for (int i = 0; i < list.count(); i++) {
        const QJsonDbNotification & notification = list[i];
        QVariantMap object = notification.object().toVariantMap();
        if (state == JsonDbSortingListModel::Querying) {
            NotifyItem  pending;
            pending.partitionIndex = partitionIndex;
            pending.item = object;
            pending.action = notification.action();
            pendingNotifications.append(pending);
        } else if (state == JsonDbSortingListModel::Ready) {
            sendNotifications(partitionIndex, object, notification.action());
        }
    }
}

void JsonDbSortingListModelPrivate::_q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message)
{
    Q_Q(JsonDbSortingListModel);
    qWarning() << QString("JsonDbSortingListModel Notification error: %1 %2").arg(code).arg(message);
    int oldErrorCode = errorCode;
    errorCode = code;
    errorString = message;
    if (oldErrorCode != errorCode)
        emit q->errorChanged(q->error());
}

void JsonDbSortingListModelPrivate::appendPartition(JsonDbPartition *v)
{
    Q_Q(JsonDbSortingListModel);
    partitionObjects.append(QPointer<JsonDbPartition>(v));
    partitionObjectDetails.append(RequestInfo());
    ModelRequest *valueRequest = new ModelRequest();
    QObject::connect(valueRequest, SIGNAL(finished(int,QList<QJsonObject>,QString)),
                     q, SLOT(_q_valueResponse(int,QList<QJsonObject>)));
    QObject::connect(valueRequest, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     q, SLOT(_q_readError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    valueRequests.append(valueRequest);
    if (componentComplete && !query.isEmpty()) {
        createOrUpdateNotification(partitionObjects.count()-1);
        if (state == JsonDbSortingListModel::None) {
            resetModel = true;
        }
        fetchPartition(partitionObjects.count()-1);
    }
}

void JsonDbSortingListModelPrivate::partitions_append(QQmlListProperty<JsonDbPartition> *p, JsonDbPartition *v)
{
    JsonDbSortingListModel *q = qobject_cast<JsonDbSortingListModel *>(p->object);
    JsonDbSortingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        pThis->appendPartition(v);
    }
}

int JsonDbSortingListModelPrivate::partitions_count(QQmlListProperty<JsonDbPartition> *p)
{
    JsonDbSortingListModel *q = qobject_cast<JsonDbSortingListModel *>(p->object);
    JsonDbSortingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        return pThis->partitionObjects.count();
    }
    return 0;
}

JsonDbPartition* JsonDbSortingListModelPrivate::partitions_at(QQmlListProperty<JsonDbPartition> *p, int idx)
{
    JsonDbSortingListModel *q = qobject_cast<JsonDbSortingListModel *>(p->object);
    JsonDbSortingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis && idx < pThis->partitionObjects.count()) {
        return pThis->partitionObjects.at(idx);
    }
    return 0;
}

void JsonDbSortingListModelPrivate::clearPartitions()
{
    partitionObjects.clear();
    partitionObjectDetails.clear();
    while (!valueRequests.isEmpty()) {
        delete valueRequests[0];
        valueRequests.removeFirst();
    }
    reset();
}

void JsonDbSortingListModelPrivate::partitions_clear(QQmlListProperty<JsonDbPartition> *p)
{
    JsonDbSortingListModel *q = qobject_cast<JsonDbSortingListModel *>(p->object);
    JsonDbSortingListModelPrivate *pThis = (q) ? q->d_func() : 0;
    if (pThis) {
        pThis->clearPartitions();
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

    \sa QtJsonDb::JsonDbSortingListModel::bindings

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
    \qmlproperty object QtJsonDb::JsonDbSortingListModel::bindings
    Holds the bindings for the placeholders used in the query string. Note that
    the placeholder marker '%' should not be included as part of the keys.

    \qml
    JsonDb.JsonDbSortingListModel {
        query: '[?_type="Contact"][?name=%firstName]'
        bindings :{'firstName':'Book'}
        partitions:[ JsonDb.Partiton {
            name:"com.nokia.shared"
        }]
    }
    \endqml

    \sa QtJsonDb::JsonDbSortingListModel::query

*/

QVariantMap JsonDbSortingListModel::bindings() const
{
    Q_D(const JsonDbSortingListModel);
    return d->queryBindings;
}

void JsonDbSortingListModel::setBindings(const QVariantMap &newBindings)
{
    Q_D(JsonDbSortingListModel);
    d->queryBindings = newBindings;

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
    \readonly
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


QQmlListProperty<JsonDbPartition> JsonDbSortingListModel::partitions()
{
    return QQmlListProperty<JsonDbPartition>(this, 0
                                                     , &JsonDbSortingListModelPrivate::partitions_append
                                                     , &JsonDbSortingListModelPrivate::partitions_count
                                                     , &JsonDbSortingListModelPrivate::partitions_at
                                                     , &JsonDbSortingListModelPrivate::partitions_clear);
}

/*!
    \qmlproperty string QtJsonDb::JsonDbSortingListModel::sortOrder

    The order used by the model to sort the items. The sortOrder has to be
    specified in the JsonQuery format. The sorting is done by the model and not
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
    \readonly
    The current state of the model.
    \list
    \li State.None - The model is not initialized
    \li State.Querying - It is querying the results from server
    \li State.Ready - Results are ready
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

/*!
    \qmlproperty object QtJsonDb::JsonDbSortingListModel::error
    \readonly

    This property holds the current error information for the object. It contains:
    \list
    \o error.code -  code for the current error.
    \o error.message - detailed explanation of the error
    \endlist
*/

QVariantMap JsonDbSortingListModel::error() const
{
    Q_D(const JsonDbSortingListModel);
    QVariantMap errorMap;
    errorMap.insert(QLatin1String("code"), d->errorCode);
    errorMap.insert(QLatin1String("message"), d->errorString);
    return errorMap;
}

#include "moc_jsondbsortinglistmodel.cpp"
QT_END_NAMESPACE_JSONDB
