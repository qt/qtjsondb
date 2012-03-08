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

#ifndef JSONDBCACHINGLISTMODEL_H
#define JSONDBCACHINGLISTMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QMultiMap>
#include <QSet>
#include <QSharedDataPointer>
#include <QStringList>
#include <QQmlParserStatus>
#include <QQmlListProperty>
#include <QJSValue>
#include <QScopedPointer>

#include "jsondb-global.h"
#include "jsondbpartition.h"

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbCachingListModelPrivate;
class JsonDbPartition;

class JsonDbCachingListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_ENUMS(State)
public:
    enum State { None, Querying, Ready, Error };

    JsonDbCachingListModel(QObject *parent = 0);
    virtual ~JsonDbCachingListModel();

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QVariant roleNames READ scriptableRoleNames WRITE setScriptableRoleNames)
    Q_PROPERTY(int cacheSize READ cacheSize WRITE setCacheSize)
    Q_PROPERTY(QQmlListProperty<JsonDbPartition> partitions READ partitions)
    Q_PROPERTY(QVariantMap error READ error NOTIFY errorChanged)

    virtual void classBegin();
    virtual void componentComplete();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    QVariant scriptableRoleNames() const;
    void setScriptableRoleNames(const QVariant &roles);

    QString query() const;
    void setQuery(const QString &newQuery);

    QQmlListProperty<JsonDbPartition> partitions();

    int cacheSize() const;
    void setCacheSize(int newCacheSize);

    QString sortOrder() const;
    void setSortOrder(const QString &newSortOrder);


    JsonDbCachingListModel::State state() const;

    Q_INVOKABLE int indexOf(const QString &uuid) const;
    Q_INVOKABLE void get(int index, const QJSValue &callback);
    Q_INVOKABLE JsonDbPartition* getPartition(int index) const;
    QVariantMap error() const;

signals:
    void stateChanged(State state) const;
    void rowCountChanged(int newCount) const;
    void errorChanged(QVariantMap newError);

private Q_SLOTS:
    void partitionNameChanged(const QString &partitionName);

private:
    Q_DISABLE_COPY(JsonDbCachingListModel)
    Q_DECLARE_PRIVATE(JsonDbCachingListModel)
    QScopedPointer<JsonDbCachingListModelPrivate> d_ptr;
    Q_PRIVATE_SLOT(d_func(), void _q_jsonDbResponse(int, const QVariant&))
    Q_PRIVATE_SLOT(d_func(), void _q_jsonDbErrorResponse(int, int, const QString&))
    Q_PRIVATE_SLOT(d_func(), void _q_dbNotified(QString, QtAddOn::JsonDb::JsonDbNotification))
    Q_PRIVATE_SLOT(d_func(), void _q_dbNotifyReadyResponse(int, QVariant))
    Q_PRIVATE_SLOT(d_func(), void _q_dbNotifyErrorResponse(int, int, QString))
    Q_PRIVATE_SLOT(d_func(), void _q_verifyDefaultIndexType(int))

};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBCACHINGLISTMODEL_H
