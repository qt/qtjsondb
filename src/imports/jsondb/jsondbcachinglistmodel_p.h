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


#ifndef JSONDBCACHINGLISTMODEL_P_H
#define JSONDBCACHINGLISTMODEL_P_H

#include <QHash>
#include <QMultiMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QPointer>
#include <QUuid>
#include <QJsonObject>

#include "jsondatabase.h"
#include "jsondbcachinglistmodel.h"
#include "jsondbmodelutils.h"
#include "jsondbmodelcache.h"

QT_BEGIN_NAMESPACE_JSONDB

struct JsonDbModelIndexNSize
{
    int index;
    int count;

    JsonDbModelIndexNSize() { clear();}
    void clear()
    {
        index = -1;
        count = 0;
    }
};

class JsonDbCachingListModelPrivate
{
    Q_DECLARE_PUBLIC(JsonDbCachingListModel)
public:
    JsonDbCachingListModel *q_ptr;

    QList<RequestInfo> partitionObjectDetails;
    QList<QPointer<JsonDbPartition> >partitionObjects;

    bool componentComplete;
    bool resetModel;
    int lowWaterMark;

    JsonDbModelIndexNSize currentCacheRequest;
    ModelCache objectCache;

    JsonDbModelObjectType tmpObjects; // uuid -> object
    JsonDbModelIndexType objectUuids; // sortvalue -> uuid
    QMap<QString, SortingKey> objectSortValues; // uuid -> sortvalue

    QList<RequestInfo> partitionKeyRequestDetails;
    QList<JsonDbModelIndexType> partitionObjectUuids;

    bool ascendingOrder;
    QString indexName;

    QString query;
    QVariantMap queryBindings;
    QString sortOrder;
    QString queryForSortKeys;
    QString queryForIndexSpec;
    QList<IndexInfo> partitionIndexDetails;

    int cacheSize;
    int chunkSize;
    QVariantMap roleMap;
    QHash<int, QByteArray> roleNames;

    QHash<int, QStringList> properties;
    QList<NotificationItem> pendingNotifications;
    QList<int> cacheMiss;
    QMap<int, QJSValue> getCallbacks;
    QList< QPair<int,int> > requestQueue;
    QList< QPointer<ModelRequest> >keyRequests;
    QList< QPointer<ModelRequest> >indexRequests;
    QList< QPointer<ModelRequest> >valueRequests;

    JsonDbCachingListModel::State state;
    QModelIndex parent;
    int errorCode;
    QString errorString;

    // data() is often called for each role in same index in a row
    // caching the last found object speeds this pattern quite a bit
    // as some time is spent on finding the object from cache
    // Note that this needs to be cleared on data changes
    int lastQueriedIndex;
    QJsonObject lastQueriedObject;

public:
    JsonDbCachingListModelPrivate(JsonDbCachingListModel *q);
    ~JsonDbCachingListModelPrivate();
    void init();
    bool partitionsReady();
    void setCacheParams(int maxItems);

    void createObjectRequests(int startIndex, int maxItems);
    void clearCache();

    void removeLastItem();
    void addItem(const QJsonObject &item, int partitionIndex);
    void deleteItem(const QJsonObject &item, int partitionIndex);
    void updateItem(const QJsonObject &item, int partitionIndex);
    void fillKeys(const QList<QJsonObject> &items, int partitionIndex);
    void fillData(const QList<QJsonObject> &items, int partitionIndex);
    void reset();
    void emitDataChanged(int from, int to);

    bool checkForDefaultIndexTypes(int index);
    void fetchIndexSpec(int index);
    void fetchPartitionKeys(int index);
    void initializeModel(bool reset = true);
    void fetchModel(bool reset = true);
    void fetchNextKeyChunk(int partitionIndex);
    void fetchNextChunk(int partitionIndex);
    void prefetchNearbyPages(int currentIndex);
    void addIndexToQueue(int index);
    void requestPageContaining(int index);

    void clearNotification(int index);
    void clearNotifications();
    void createOrUpdateNotification(int index);
    void createOrUpdateNotifications();
    void parseSortOrder();
    void setQueryForSortKeys();
    void verifyIndexSpec(const QList<QJsonObject> &items, int partitionIndex);

    int indexOfWatcher(QJsonDbWatcher *watcher);

    void initPartition(JsonDbPartition *v);
    void appendPartition(JsonDbPartition *v);
    void clearPartitions();
    QJsonObject getJsonObject(int index);
    QVariant getItem(int index);
    QVariant getItem(int index, int role);
    void queueGetCallback(int index, const QJSValue &callback);
    void callGetCallback(int index, QJSValue callback);
    JsonDbPartition* getItemPartition(int index);
    int indexOf(const QString &uuid) const;
    void set(int index, const QJSValue& valuemap,
             const QJSValue &successCallback,
             const QJSValue &errorCallback);
    void sendNotification(int partitionIndex, const QJsonObject &object, QJsonDbWatcher::Action action);

    // private slots
    void _q_verifyDefaultIndexType(int index);
    void _q_notificationsAvailable();
    void _q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message);
    void _q_readError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString & message);
    void _q_keyResponse(int , const QList<QJsonObject>&, const QString&);
    void _q_valueResponse(int , const QList<QJsonObject>&);
    void _q_indexResponse(int , const QList<QJsonObject>&);
    void _q_partitionStateChanged(JsonDbPartition::State state);

    static void partitions_append(QQmlListProperty<JsonDbPartition> *p, JsonDbPartition *v);
    static int partitions_count(QQmlListProperty<JsonDbPartition> *p);
    static JsonDbPartition* partitions_at(QQmlListProperty<JsonDbPartition> *p, int idx);
    static void partitions_clear(QQmlListProperty<JsonDbPartition> *p);

};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBCACHINGLISTMODEL_P_H
