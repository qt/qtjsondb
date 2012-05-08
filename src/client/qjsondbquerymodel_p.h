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

#ifndef QJSONDBQUERYMODEL_P_H
#define QJSONDBQUERYMODEL_P_H

#include <QAbstractListModel>
#include <QHash>
#include <QMultiMap>
#include <QSet>
#include <QSharedDataPointer>
#include <QStringList>
#include <QScopedPointer>
#include <QJsonObject>
#include <QtJsonDb/qjsondbglobal.h>
#include <QJSValue>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbConnection;

class QJsonDbQueryModelPrivate;

class Q_JSONDB_EXPORT QJsonDbQueryModel: public QAbstractListModel
{
    Q_OBJECT
    Q_ENUMS(State)
public:
    enum State { None, Querying, Ready, Error };

    Q_PROPERTY(QVariantMap bindings READ bindings WRITE setBindings)
    Q_PROPERTY(int cacheSize READ cacheSize WRITE setCacheSize)
    Q_PROPERTY(QVariantMap error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(QVariant roleNames READ queryRoleNames WRITE setQueryRoleNames)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QJSValue propertyInjector READ propertyInjector WRITE setPropertyInjector)

    QJsonDbQueryModel(QJsonDbConnection *dbConnection, QObject *parent = 0);
    virtual ~QJsonDbQueryModel();

    //From QAbstractItemModel
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual QVariant data(int index, int role = Qt::DisplayRole) const;
    virtual QHash<int,QByteArray> roleNames() const;

    QVariantMap error() const;

    QJSValue propertyInjector() const;
    void setPropertyInjector(const QJSValue &callback);
    Q_INVOKABLE void refreshItems();

    QVariantMap bindings() const;
    void setBindings(const QVariantMap &newBindings);
    int cacheSize() const;
    void setCacheSize(int newCacheSize);

    int indexOf(const QString &uuid) const;
    // fetchObject is async (see objectAvailable -signal) as the object at
    // specifiec index might not be in model cache
    void fetchObject(int index);

    QString query() const;
    void setQuery(const QString &newQuery);

    QVariant queryRoleNames() const;
    void setQueryRoleNames(const QVariant &roles);

    QString sortOrder() const;
    void setSortOrder(const QString &newSortOrder);

    State state() const;

    void populate();

    //Partition handling
    QString partitionName(int index) const;
    QStringList partitionNames() const;
    void setPartitionNames(const QStringList &partitions);
    void appendPartitionName(const QString &partitionName);

Q_SIGNALS:
    void stateChanged(State state) const;
    void rowCountChanged(int newCount) const;
    void errorChanged(QVariantMap);
    void objectAvailable(int index, QJsonObject availableObject, QString objectPartition);

private:
    Q_DISABLE_COPY(QJsonDbQueryModel)
    Q_DECLARE_PRIVATE(QJsonDbQueryModel)
    QScopedPointer<QJsonDbQueryModelPrivate> d_ptr;

    Q_PRIVATE_SLOT(d_func(), void _q_verifyDefaultIndexType(int))
    Q_PRIVATE_SLOT(d_func(), void _q_notificationsAvailable())
    Q_PRIVATE_SLOT(d_func(), void _q_partitionWatcherNotificationsAvailable())
    Q_PRIVATE_SLOT(d_func(), void _q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode, QString))
    Q_PRIVATE_SLOT(d_func(), void _q_partitionWatcherNotificationError(QtJsonDb::QJsonDbWatcher::ErrorCode, QString))
    Q_PRIVATE_SLOT(d_func(), void _q_partitionObjectQueryFinished())
    Q_PRIVATE_SLOT(d_func(), void _q_partitionObjectQueryError())
    Q_PRIVATE_SLOT(d_func(), void _q_keyResponse(int, QList<QJsonObject>, QString))
    Q_PRIVATE_SLOT(d_func(), void _q_valueResponse(int, QList<QJsonObject>))
    Q_PRIVATE_SLOT(d_func(), void _q_indexResponse(int, QList<QJsonObject>))
    Q_PRIVATE_SLOT(d_func(), void _q_readError(QtJsonDb::QJsonDbRequest::ErrorCode, QString))
};

QT_END_HEADER

QT_END_NAMESPACE_JSONDB

#endif // QJSONDBQUERYMODEL_P_H
