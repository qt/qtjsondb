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

#ifndef JSONDBSORTINGLISTMODEL_P_H
#define JSONDBSORTINGLISTMODEL_P_H

#include <QHash>
#include <QMultiMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QPointer>
#include <QUuid>
#include <QJSEngine>
#include <QJSValueIterator>
#include <private/qjsondbmodelutils_p.h>

QT_BEGIN_NAMESPACE_JSONDB

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
    QVariantMap queryBindings;
    QString sortOrder;

    int limit;
    int chunkSize;
    QVariantMap roleMap;
    QHash<int, QByteArray> roleNames;

    QHash<int, QStringList> properties;
    QList<NotifyItem> pendingNotifications;
    QList< QPointer<ModelRequest> >valueRequests;

    JsonDbSortingListModel::State state;
    QModelIndex parent;
    int errorCode;
    QString errorString;

public:
    JsonDbSortingListModelPrivate(JsonDbSortingListModel *q);
    ~JsonDbSortingListModelPrivate();
    void init();

    void removeLastItem();
    void addItem(const QVariantMap &item, int partitionIndex);
    void deleteItem(const QVariantMap &item, int partitionIndex);
    void updateItem(const QVariantMap &item, int partitionIndex);
    void fillData(const QVariantList &items, int partitionIndex);
    void sortObjects();
    void reset();

    void fetchPartition(int index, bool reset = true);
    void fetchModel(bool reset = true);

    void clearNotification(int index);
    void clearNotifications();
    void createOrUpdateNotification(int index);
    void createOrUpdateNotifications();
    void parseSortOrder();

    int indexOfrequestId(int requestId);
    int indexOfNotifyUUID(const QString& notifyUuid);
    int indexOfWatcher(QJsonDbWatcher *watcher);

    void appendPartition(JsonDbPartition *v);
    void clearPartitions();
    QVariant getItem(int index);
    QVariant getItem(int index, int role);
    JsonDbPartition* getItemPartition(int index);
    int indexOf(const QString &uuid) const;
    void set(int index, const QJSValue& valuemap,
                                     const QJSValue &successCallback,
                                     const QJSValue &errorCallback);
    void sendNotifications(int partitionIndex, const QVariantMap &v, QJsonDbWatcher::Action action);

    // private slots
    void _q_refreshModel();
    void _q_notificationsAvailable();
    void _q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message);
    void _q_readError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString & message);
    void _q_valueResponse(int , const QList<QJsonObject>&);

    static void partitions_append(QQmlListProperty<JsonDbPartition> *p, JsonDbPartition *v);
    static int partitions_count(QQmlListProperty<JsonDbPartition> *p);
    static JsonDbPartition* partitions_at(QQmlListProperty<JsonDbPartition> *p, int idx);
    static void partitions_clear(QQmlListProperty<JsonDbPartition> *p);

};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBSORTINGLISTMODEL_P_H
