/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/


#ifndef QJSONDBQUERYMODEL_P_P_H
#define QJSONDBQUERYMODEL_P_P_H

#include <QHash>
#include <QMultiMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QPointer>
#include <QUuid>
#include <QJsonObject>
#include <QJSValue>

#include "qjsondbquerymodel_p.h"
#include "qjsondbmodelutils_p.h"
#include "qjsondbmodelcache_p.h"

QT_BEGIN_HEADER

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

struct JsonDbAvailablePartitionsInfo;

class QJsonDbQueryModelPrivate
{
    Q_DECLARE_PUBLIC(QJsonDbQueryModel)

public:
    enum JsonDbPartitionState { PartitionStateNone,
                                PartitionStateOnline,
                                PartitionStateOffline,
                                PartitionStateError };
    QJsonDbQueryModel *q_ptr;

    QJsonDbConnection *mConnection;
    QList<RequestInfo> partitionObjectDetails;
    QList<QString> partitionObjects;
    QList<JsonDbAvailablePartitionsInfo> availablePartitions;

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
    QList<NotificationItem> pendingPartitionObjectNotifications;
    QList<int> cacheMiss;
    QList< QPair<int,int> > requestQueue;
    QList< QPointer<ModelRequest> >keyRequests;
    QList< QPointer<ModelRequest> >indexRequests;
    QList< QPointer<ModelRequest> >valueRequests;

    QJsonDbQueryModel::State state;
    QModelIndex parent;
    int errorCode;
    QString errorString;
    // data() is often called for each role in same index in a row
    // caching the last found object speeds this pattern quite a bit
    // as some time is spent on finding the object from cache
    // Note that this needs to be cleared on data changes
    int lastQueriedIndex;
    QJsonObject lastQueriedObject;
    bool isCallable;
    QJSValue injectCallback;

public:
    QJsonDbQueryModelPrivate(QJsonDbQueryModel *q);
    ~QJsonDbQueryModelPrivate();
    void init(QJsonDbConnection *dbConnection);
    void setCacheParams(int maxItems);

    void createObjectRequests(int startIndex, int maxItems);

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
    int indexOfPartitionObjectWatcher(QJsonDbWatcher *watcher);

    void appendPartition(const QString& partitionName);
    void clearPartitions();
    void startWatchingPartitionObject(const QString& partitionName);
    void onPartitionStateChanged();
    bool partitionsReady();
    QJsonObject getJsonObject(int index);
    QVariant getItem(int index);
    QVariant getItem(int index, int role);
    QString getItemPartition(int index);
    int indexOf(const QString &uuid) const;
    void sendNotification(int partitionIndex, const QJsonObject &object, QJsonDbWatcher::Action action);
    void generateCustomData(QJsonObject &val);
    void generateCustomData(JsonDbModelObjectType &objects);
    // private slots
    void _q_verifyDefaultIndexType(int index);
    void _q_notificationsAvailable();
    void _q_partitionWatcherNotificationsAvailable();
    void _q_partitionWatcherNotificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message);
    void _q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message);
    void _q_partitionObjectQueryFinished();
    void _q_partitionObjectQueryError();
    void _q_readError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString & message);
    void _q_keyResponse(int , const QList<QJsonObject>&, const QString&);
    void _q_valueResponse(int , const QList<QJsonObject>&);
    void _q_indexResponse(int , const QList<QJsonObject>&);
};

struct JsonDbAvailablePartitionsInfo
{
    QPointer<QJsonDbWatcher> watcher;
    QJsonDbQueryModelPrivate::JsonDbPartitionState state;
    JsonDbAvailablePartitionsInfo() { clear();}
    void clear()
    {
        state = QJsonDbQueryModelPrivate::PartitionStateNone;
        if (watcher) {
            delete watcher;
            watcher = 0;
        }
    }
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // QJSONDBQUERYMODEL_P_P_H
