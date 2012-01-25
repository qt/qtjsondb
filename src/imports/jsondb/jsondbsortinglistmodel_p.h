/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
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
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDBSORTINGLISTMODEL_P_H
#define JSONDBSORTINGLISTMODEL_P_H

#include <QHash>
#include <QMultiMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QPointer>
#include <QUuid>

#include "jsondb-client.h"
#include "private/jsondb-connection_p.h"

QT_USE_NAMESPACE_JSONDB

struct CallbackInfo {
    int index;
    QJSValue successCallback;
    QJSValue errorCallback;
};

struct NotifyItem {
    QString  notifyUuid;
    QVariant item;
    JsonDbClient::NotifyType action;
};

class SortingKeyPrivate;

class SortingKey {
public:
    SortingKey(int partitionIndex, const QVariantMap &object, const QList<bool> &directions, const QList<QStringList> &paths);
    SortingKey(const SortingKey&);
    SortingKey() {}
    int partitionIndex() const;
    bool operator <(const SortingKey &rhs) const;
    bool operator ==(const SortingKey &rhs) const;
private:
    QSharedDataPointer<SortingKeyPrivate> d;
};

struct RequestInfo
{
    int requestId;
    int lastOffset;
    QString notifyUuid;
    int lastSize;

    RequestInfo() { clear();}
    void clear()
    {
        requestId = -1;
        lastOffset = 0;
        lastSize = -1;
        notifyUuid.clear();
    }
};

class JsonDbSortingListModelPrivate
{
    Q_DECLARE_PUBLIC(JsonDbSortingListModel)
public:
    JsonDbSortingListModel *q_ptr;

    QList<RequestInfo> partitionObjectDetails;
    QList<QPointer<JsonDbPartition> >partitionObjects;

    bool componentComplete;
    bool resetModel;
    bool overflow;

    QHash<QString, QVariantMap> objects; // uuid -> object
    QMap<SortingKey, QString> objectUuids; // sortvalue -> uuid
    QMap<QString, SortingKey> objectSortValues; // uuid -> sortvalue

    QList<bool> ascendingOrders;
    QStringList orderProperties;
    QList<QStringList> orderPaths;

    QString query;
    QVariant queryOptions;
    QString sortOrder;

    int limit;
    int chunkSize;
    QVariantMap roleMap;
    QHash<int, QByteArray> roleNames;

    QHash<int, QStringList> properties;
    QList<NotifyItem> pendingNotifications;

    JsonDbSortingListModel::State state;
    JsonDbClient dbClient;
    QModelIndex parent;

public:
    JsonDbSortingListModelPrivate(JsonDbSortingListModel *q);
    ~JsonDbSortingListModelPrivate();
    void init();

    void removeLastItem();
    void addItem(const QVariantMap &item, int partitionIndex);
    void deleteItem(const QVariantMap &item, int partitionIndex);
    void updateItem(const QVariantMap &item, int partitionIndex);
    void fillData(const QVariant &v, int partitionIndex);
    void sortObjects();
    void reset();

    void fetchPartition(int index);
    void fetchModel(bool reset = true);
    void fetchNextChunk(int partitionIndex);

    void clearNotification(int index);
    void clearNotifications();
    void createOrUpdateNotification(int index);
    void createOrUpdateNotifications();
    void parseSortOrder();

    int indexOfrequestId(int requestId);
    int indexOfNotifyUUID(const QString& notifyUuid);

    QVariant getItem(int index);
    QVariant getItem(int index, int role);
    JsonDbPartition* getItemPartition(int index);
    int indexOf(const QString &uuid) const;
    void set(int index, const QJSValue& valuemap,
                                     const QJSValue &successCallback,
                                     const QJSValue &errorCallback);
    void sendNotifications(const QString&, const QVariant &, JsonDbClient::NotifyType);

    // private slots
    void _q_jsonDbResponse(int , const QVariant &);
    void _q_jsonDbErrorResponse(int , int, const QString&);
    void _q_refreshModel();
    void _q_dbNotified(const QString &notify_uuid, const QtAddOn::JsonDb::JsonDbNotification &_notification);
    void _q_dbNotifyReadyResponse(int id, const QVariant &result);
    void _q_dbNotifyErrorResponse(int id, int code, const QString &message);

    static void partitions_append(QDeclarativeListProperty<JsonDbPartition> *p, JsonDbPartition *v);
    static int partitions_count(QDeclarativeListProperty<JsonDbPartition> *p);
    static JsonDbPartition* partitions_at(QDeclarativeListProperty<JsonDbPartition> *p, int idx);
    static void partitions_clear(QDeclarativeListProperty<JsonDbPartition> *p);

};

class SortingKeyPrivate : public QSharedData {
public:
    SortingKeyPrivate(int index, const QUuid &objectUuid, QList<bool> objectDirections, QVariantList objectKeys)
        :uuid(objectUuid), directions(objectDirections), values(objectKeys), partitionIndex(index) {}
    QUuid uuid;
    QList<bool>  directions;
    QVariantList values;
    int partitionIndex;
};

#endif // JSONDBSORTINGLISTMODEL_P_H
