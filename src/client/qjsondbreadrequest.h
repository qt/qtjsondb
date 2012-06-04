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

#ifndef QJSONDB_READ_REQUEST_H
#define QJSONDB_READ_REQUEST_H

#include <QObject>
#include <QJsonValue>
#include <QJsonObject>

#include <QtJsonDb/qjsondbrequest.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbReadRequestPrivate;
class Q_JSONDB_EXPORT QJsonDbReadRequest : public QJsonDbRequest
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(int queryLimit READ queryLimit WRITE setQueryLimit)

    Q_PROPERTY(quint32 stateNumber READ stateNumber)
    Q_PROPERTY(QString sortKey READ sortKey)

public:
    QJsonDbReadRequest(QObject *parent = 0);
    QJsonDbReadRequest(const QString &query, QObject *parent = 0);
    ~QJsonDbReadRequest();

    enum ErrorCode {
        NoError = QJsonDbRequest::NoError,
        InvalidRequest = QJsonDbRequest::InvalidRequest,
        OperationNotPermitted = QJsonDbRequest::OperationNotPermitted,
        InvalidPartition = QJsonDbRequest::InvalidPartition,
        DatabaseConnectionError = QJsonDbRequest::DatabaseConnectionError,
        PartitionUnavailable = QJsonDbRequest::PartitionUnavailable,
        MissingQuery = QJsonDbRequest::MissingQuery,
        InvalidMessage= QJsonDbRequest::InvalidMessage,
        InvalidLimit = QJsonDbRequest::InvalidLimit
    };

    QString query() const;
    void setQuery(const QString &);

    int queryLimit() const;
    void setQueryLimit(int limit);

    void bindValue(const QString &placeHolder, const QJsonValue &val);
    QJsonValue boundValue(const QString &placeHolder) const;
    QMap<QString,QJsonValue> boundValues() const;
    void clearBoundValues();

    // query results. Data is only available after started() was emitted.
    quint32 stateNumber() const;
    QString sortKey() const;

protected:
    QJsonDbReadRequest(QJsonDbReadRequestPrivate *dd, QObject *parent);

private:
    Q_DISABLE_COPY(QJsonDbReadRequest)
    Q_DECLARE_PRIVATE(QJsonDbReadRequest)
    friend class QJsonDbConnectionPrivate;
};

class QJsonDbReadObjectRequestPrivate;
class Q_JSONDB_EXPORT QJsonDbReadObjectRequest : public QJsonDbReadRequest
{
    Q_OBJECT
    Q_PROPERTY(QUuid uuid READ uuid WRITE setUuid)

public:
    QJsonDbReadObjectRequest(QObject *parent = 0);
    QJsonDbReadObjectRequest(const QUuid &uuid, QObject *parent = 0);

    QUuid uuid() const;
    void setUuid(const QUuid &uuid);

Q_SIGNALS:
    void objectAvailable(const QJsonObject &object);
    void objectUnavailable(const QUuid &);

private:
    Q_DISABLE_COPY(QJsonDbReadObjectRequest)
    Q_DECLARE_PRIVATE(QJsonDbReadObjectRequest)

    Q_PRIVATE_SLOT(d_func(), void _q_onFinished())
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // QJSONDB_READ_REQUEST_H
