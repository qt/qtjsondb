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

#ifndef JSONDBSORTINGLISTMODEL_H
#define JSONDBSORTINGLISTMODEL_H

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

#include "jsondbpartition.h"

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbSortingListModelPrivate;
class JsonDbPartition;

class JsonDbSortingListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_ENUMS(State)
public:
    enum State { None, Querying, Ready };

    JsonDbSortingListModel(QObject *parent = 0);
    virtual ~JsonDbSortingListModel();

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(QVariantMap bindings READ bindings WRITE setBindings)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QVariant roleNames READ scriptableRoleNames WRITE setScriptableRoleNames)
    Q_PROPERTY(int queryLimit READ queryLimit WRITE setQueryLimit)
    Q_PROPERTY(bool overflow READ overflow)
    Q_PROPERTY(QQmlListProperty<QT_PREPEND_NAMESPACE_JSONDB(JsonDbPartition)> partitions READ partitions)
    Q_PROPERTY(QVariantMap error READ error NOTIFY errorChanged)
    Q_PROPERTY(QJSValue propertyInjector READ propertyInjector WRITE setPropertyInjector)

    virtual void classBegin();
    virtual void componentComplete();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual QHash<int,QByteArray> roleNames() const;

    QVariant scriptableRoleNames() const;
    void setScriptableRoleNames(const QVariant &roles);

    QString query() const;
    void setQuery(const QString &newQuery);

    QVariantMap bindings() const;
    void setBindings(const QVariantMap &newBindings);

    QQmlListProperty<QtJsonDb::JsonDbPartition> partitions();

    int queryLimit() const;
    void setQueryLimit(int newQueryLimit);

    bool overflow() const;

    QString sortOrder() const;
    void setSortOrder(const QString &newSortOrder);


    JsonDbSortingListModel::State state() const;

    QJSValue propertyInjector() const;
    void setPropertyInjector(const QJSValue &callback);
    Q_INVOKABLE void refreshItems();

    Q_INVOKABLE int indexOf(const QString &uuid) const;
    Q_INVOKABLE QJSValue get(int index) const;
    Q_INVOKABLE QVariant get(int index, const QString &property) const;
    Q_INVOKABLE QT_PREPEND_NAMESPACE_JSONDB(JsonDbPartition)* getPartition(int index) const;
    QVariantMap error() const;

signals:
    void stateChanged(State state) const;
    void rowCountChanged(int newCount) const;
    void errorChanged(QVariantMap newError);

private Q_SLOTS:
    void partitionNameChanged(const QString &partitionName);

private:
    Q_DISABLE_COPY(JsonDbSortingListModel)
    Q_DECLARE_PRIVATE(JsonDbSortingListModel)
    QScopedPointer<JsonDbSortingListModelPrivate> d_ptr;

    Q_PRIVATE_SLOT(d_func(), void _q_refreshModel())
    Q_PRIVATE_SLOT(d_func(), void _q_notificationsAvailable())
    Q_PRIVATE_SLOT(d_func(), void _q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode, QString))
    Q_PRIVATE_SLOT(d_func(), void _q_valueResponse(int, QList<QJsonObject>))
    Q_PRIVATE_SLOT(d_func(), void _q_readError(QtJsonDb::QJsonDbRequest::ErrorCode, QString))

};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBSORTINGLISTMODEL_H
