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

#include "jsondb-client.h"
#include "jsondbmodelutils.h"
#include "jsondbmodelcache.h"

Q_USE_JSONDB_NAMESPACE

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

    QList<bool> ascendingOrders;
    QStringList orderProperties;
    QList<QStringList> orderPaths;

    QString query;
    QVariant queryOptions;
    QString sortOrder;
    QString queryForSortKeys;
    QString queryForIndexSpec;
    QList<IndexInfo> partitionIndexDetails;

    int cacheSize;
    int chunkSize;
    QVariantMap roleMap;
    QHash<int, QByteArray> roleNames;

    QHash<int, QStringList> properties;
    QList<NotifyItem> pendingNotifications;
    QList<int> cacheMiss;
    QMap<int, QJSValue> getCallbacks;
    QList< QPair<int,int> > requestQueue;

    JsonDbCachingListModel::State state;
    JsonDbClient dbClient;
    QModelIndex parent;
    int errorCode;
    QString errorString;

public:
    JsonDbCachingListModelPrivate(JsonDbCachingListModel *q);
    ~JsonDbCachingListModelPrivate();
    void init();
    void setCacheParams(int maxItems);

    void createObjectRequests(int startIndex, int maxItems);
    void clearCache();

    void removeLastItem();
    void addItem(const QVariantMap &item, int partitionIndex);
    void deleteItem(const QVariantMap &item, int partitionIndex);
    void updateItem(const QVariantMap &item, int partitionIndex);
    void fillKeys(const QVariant &v, int partitionIndex);
    void fillData(const QVariant &v, int partitionIndex);
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
    void verifyIndexSpec(const QVariant &v, int partitionIndex);

    int indexOfKeyRequestId(int requestId);
    int indexOfRequestId(int requestId);
    int indexOfNotifyUUID(const QString& notifyUuid);
    int indexOfKeyIndexSpecId(int requestId);

    QVariant getItem(int index);
    QVariant getItem(int index, int role);
    void queueGetCallback(int index, const QJSValue &callback);
    void callGetCallback(int index, QJSValue callback);
    JsonDbPartition* getItemPartition(int index);
    int indexOf(const QString &uuid) const;
    void set(int index, const QJSValue& valuemap,
             const QJSValue &successCallback,
             const QJSValue &errorCallback);
    void sendNotifications(const QString& currentNotifyUuid, const QVariant &v, JsonDbClient::NotifyType action);
    // private slots
    void _q_jsonDbResponse(int , const QVariant &);
    void _q_jsonDbErrorResponse(int , int, const QString&);
    void _q_dbNotified(const QString &notify_uuid, const QtAddOn::JsonDb::JsonDbNotification &_notification);
    void _q_dbNotifyReadyResponse(int id, const QVariant &result);
    void _q_dbNotifyErrorResponse(int id, int code, const QString &message);
    void _q_verifyDefaultIndexType(int index);

    static void partitions_append(QDeclarativeListProperty<JsonDbPartition> *p, JsonDbPartition *v);
    static int partitions_count(QDeclarativeListProperty<JsonDbPartition> *p);
    static JsonDbPartition* partitions_at(QDeclarativeListProperty<JsonDbPartition> *p, int idx);
    static void partitions_clear(QDeclarativeListProperty<JsonDbPartition> *p);

};

#endif // JSONDBCACHINGLISTMODEL_P_H
