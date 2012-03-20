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

#ifndef JsonDbListModel_H
#define JsonDbListModel_H

#include <QAbstractListModel>
#include <QHash>
#include <QMultiMap>
#include <QSet>
#include <QSharedDataPointer>
#include <QStringList>
#include <QQmlParserStatus>
#include <QJSValue>
#include <QScopedPointer>

#include <QJsonDbReadRequest>
#include "jsondbpartition.h"

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbSortKeyPrivate;
class JsonDbSortKey {
public:
    JsonDbSortKey();
    JsonDbSortKey(const QVariantMap &object, const QStringList &directions, const QList<QStringList> &paths);
    JsonDbSortKey(const JsonDbSortKey&);

    const QVariantList &keys() const;
    const QStringList &directions() const;
    QString toString() const;
private:
    QSharedDataPointer<JsonDbSortKeyPrivate> d;
};
bool operator <(const JsonDbSortKey &a, const JsonDbSortKey &b);

class JsonDbListModelPrivate;
class JsonDbPartition;

class JsonDbListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
public:
    JsonDbListModel(QObject *parent = 0);
    virtual ~JsonDbListModel();

    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(int limit READ limit WRITE setLimit)
    Q_PROPERTY(int chunkSize READ chunkSize WRITE setChunkSize)
    Q_PROPERTY(int lowWaterMark READ lowWaterMark WRITE setLowWaterMark)
    Q_PROPERTY(QVariant roleNames READ scriptableRoleNames WRITE setScriptableRoleNames)
    Q_PROPERTY(JsonDbPartition* partition READ partition WRITE setPartition)
    Q_PROPERTY(QVariantMap error READ error NOTIFY errorChanged)

    virtual void classBegin();
    virtual void componentComplete();
    virtual int count() const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    virtual void fetchMore(const QModelIndex &parent);
    virtual bool canFetchMore(const QModelIndex &parent) const;

    QVariant scriptableRoleNames() const;
    void setScriptableRoleNames(const QVariant &roles);

    QString state() const;

    virtual QString toString(int role) const;
    int roleFromString(const QString &roleName) const;

    QString query() const;
    void setQuery(const QString &newQuery);

    JsonDbPartition* partition();
    void setPartition(JsonDbPartition *newPartiton);

    void setLimit(int newLimit);
    int limit() const;

    void setChunkSize(int newChunkSize);
    int chunkSize() const;

    void setLowWaterMark(int newLowWaterMark);
    int lowWaterMark() const;

    Q_INVOKABLE QVariant get(int idx, const QString &property) const;
    Q_INVOKABLE void set(int index, const QJSValue &valuemap,
                         const QJSValue &successCallback = QJSValue(QJSValue::UndefinedValue),
                         const QJSValue &errorCallback = QJSValue(QJSValue::UndefinedValue));
    Q_INVOKABLE void setProperty(int index, const QString &property, const QVariant &value,
                                 const QJSValue &successCallback = QJSValue(QJSValue::UndefinedValue),
                                 const QJSValue &errorCallback = QJSValue(QJSValue::UndefinedValue));
    Q_INVOKABLE int sectionIndex(const QString &section, const QJSValue &successCallback = QJSValue(QJSValue::UndefinedValue),
                                  const QJSValue &errorCallback = QJSValue(QJSValue::UndefinedValue));
    QVariantMap error() const;

signals:
    void stateChanged() const;
    void countChanged() const;
    void rowCountChanged() const;
    void errorChanged(QVariantMap newError);

private Q_SLOTS:
    void partitionNameChanged(const QString &partitionName);

private:
    Q_DISABLE_COPY(JsonDbListModel)
    Q_DECLARE_PRIVATE(JsonDbListModel)
    QScopedPointer<JsonDbListModelPrivate> d_ptr;
    Q_PRIVATE_SLOT(d_func(), void _q_requestAnotherChunk(int))
    Q_PRIVATE_SLOT(d_func(), void _q_notificationsAvailable())
    Q_PRIVATE_SLOT(d_func(), void _q_notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode, QString))
    Q_PRIVATE_SLOT(d_func(), void _q_valueResponse(int, QList<QJsonObject>))
    Q_PRIVATE_SLOT(d_func(), void _q_countResponse(int, QList<QJsonObject>))
    Q_PRIVATE_SLOT(d_func(), void _q_readError(QtJsonDb::QJsonDbRequest::ErrorCode, QString))
    Q_PRIVATE_SLOT(d_func(), void _q_sectionIndexResponse())
    Q_PRIVATE_SLOT(d_func(), void _q_sectionIndexError(QtJsonDb::QJsonDbRequest::ErrorCode, QString))
    Q_PRIVATE_SLOT(d_func(), void _q_updateResponse())
    Q_PRIVATE_SLOT(d_func(), void _q_updateError(QtJsonDb::QJsonDbRequest::ErrorCode, QString))

};

class ModelSyncCall : public QObject
{
    Q_OBJECT
public:
    ModelSyncCall(const QString &_query, int _offset, int _maxItems, const QString & _partitionName, QVariantList *_data);
    ~ModelSyncCall();
public Q_SLOTS:
    void createSyncRequest();
    void onQueryFinished();
    void onQueryError(QtJsonDb::QJsonDbRequest::ErrorCode, const QString&);
private:
    QString query;
    int offset;
    int maxItems;
    QString partitionName;
    QVariantList *data;
    QPointer<QJsonDbConnection> connection;
    QPointer<QJsonDbReadRequest> request;
};

QT_END_NAMESPACE_JSONDB

#endif
