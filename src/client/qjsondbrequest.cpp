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

#include "qjsondbrequest_p.h"
#include "qjsondbstrings_p.h"

#include <QVariant>

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \class QJsonDbRequest
    \inmodule QtJsonDb

    \brief The QJsonDbRequest class provides common functionality for database requests.
*/
/*!
    \enum QJsonDbRequest::ErrorCode

    This enum describes database connection errors.

    Error codes described by this enum are all possible error codes for all
    request types. To get error codes for a particular request, follow requests
    documentation - e.g. QJsonDbReadRequest::ErrorCode.

    \value NoError

    \value InvalidRequest
    \value OperationNotPermitted
    \value InvalidPartition Invalid partition.
    \value DatabaseConnectionError
    \value PartitionUnavailable
    \value MissingQuery Missing query field.
    \value InvalidMessage
    \value InvalidLimit Invalid limit field
    \value InvalidOffset
    \value InvalidStateNumber
    \value MissingObject Invalid or missing "object" field.
    \value DatabaseError
    \value MissingUUID
    \value MissingType Missing _type field
    \value UpdatingStaleVersion
    \value FailedSchemaValidation
    \value InvalidMap
    \value InvalidReduce
    \value InvalidSchemaOperation
    \value InvalidIndexOperation
    \value InvalidType

*/
/*!
    \enum QJsonDbRequest::Status

    This enum describes current request status.

    \value Inactive
    \value Queued The request is queued and pending to be sent.
    \value Sent The request was successfully sent to the database and waiting for the reply.
    \value Receiving The request started received reply and initial data is available.
    \value Finished The request was successfully complete.
    \value Canceled The request was canceled.
    \value Error The request failed.
*/

QJsonDbRequestPrivate::QJsonDbRequestPrivate(QJsonDbRequest *q)
    : q_ptr(q), status(QJsonDbRequest::Inactive), requestId(0), internal(false)
{
}

/*!
    \internal
*/
QJsonDbRequest::QJsonDbRequest(QJsonDbRequestPrivate *d, QObject *parent)
    : QObject(parent), d_ptr(d)
{
    Q_ASSERT(d != 0);
}

/*!
    Destroys the request object.
*/
QJsonDbRequest::~QJsonDbRequest()
{
}

/*!
    \property QJsonDbRequest::status
    Specifies the current request status.

    The statusChanged() signal is emitted when the status of the request is
    changed.
*/
QJsonDbRequest::Status QJsonDbRequest::status() const
{
    Q_D(const QJsonDbRequest);
    return d->status;
}

/*!
    \property QJsonDbRequest::partition
    Specifies the target partition for the request.
*/
QString QJsonDbRequest::partition() const
{
    Q_D(const QJsonDbRequest);
    return d->partition;
}

void QJsonDbRequest::setPartition(const QString &partition)
{
    Q_D(QJsonDbRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    d->partition = partition;
}

void QJsonDbRequestPrivate::setStatus(QJsonDbRequest::Status newStatus)
{
    Q_Q(QJsonDbRequest);
    if (status != newStatus) {
        status = newStatus;
        emit q->statusChanged(status);
    }
}

void QJsonDbRequestPrivate::setError(QJsonDbRequest::ErrorCode errorCode, const QString &errorString)
{
    Q_Q(QJsonDbRequest);
    lastError = errorCode;
    lastErrorString = errorString;
    if (status != QJsonDbRequest::Error) {
        status = QJsonDbRequest::Error;
        emit q->statusChanged(status);
    }
    emit q->error(errorCode, errorString);
}

void QJsonDbRequestPrivate::setRequestId(int id)
{
    Q_Q(QJsonDbRequest);
    requestId = id;
    q->setProperty("requestId", QVariant::fromValue(requestId));
}

bool QJsonDbRequestPrivate::isPrivatePartition() const
{
    return partition == JsonDbStrings::Partition::privatePartition() ||
            partition.endsWith(QString::fromLatin1(".%1").arg(JsonDbStrings::Partition::privatePartition()));
}

void QJsonDbRequestPrivate::_q_privatePartitionResults(const QList<QJsonObject> &res)
{
    Q_Q(QJsonDbRequest);
    results.append(res);
    q->resultsAvailable(results.count());
}

void QJsonDbRequestPrivate::_q_privatePartitionStatus(QtJsonDb::QJsonDbRequest::Status status)
{
    setStatus(status);
}

/*!
    Removes the first \a amount of request results and returns them.

    If amount is -1, retrieves all available results.

    \sa resultsAvailable()
*/
QList<QJsonObject> QJsonDbRequest::takeResults(int amount)
{
    Q_D(QJsonDbRequest);
    QList<QJsonObject> list;
    if (amount < 0 || amount >= d->results.size()) {
        list.swap(d->results);
    } else {
        list = d->results.mid(0, amount);
        d->results.erase(d->results.begin(), d->results.begin() + amount);
    }
    return list;
}

/*!
    Returns true if the request is active, i.e. in either QJsonDbRequest::Queued
    or QJsonDbRequest::Sent or QJsonDbRequest::Receiving state.

    \sa status
*/
bool QJsonDbRequest::isActive() const
{
    switch (status()) {
    case QJsonDbRequest::Queued:
    case QJsonDbRequest::Sent:
    case QJsonDbRequest::Receiving:
        return true;
    case QJsonDbRequest::Inactive:
    case QJsonDbRequest::Canceled:
    case QJsonDbRequest::Finished:
    case QJsonDbRequest::Error:
        return false;
    }
    return false;
}

/*!
    Returns the type of error that last occurred.
*/
QJsonDbRequest::ErrorCode QJsonDbRequest::error() const
{
    Q_D(const QJsonDbRequest);
    return d->lastError;
}

/*!
    Returns the description of an error that last occurred.
*/
QString QJsonDbRequest::errorString() const
{
    Q_D(const QJsonDbRequest);
    return d->lastErrorString;
}

/*!
    \fn void QJsonDbRequest::started()

    This signal is emitted when the request has started receiving the reply and
    some initial data is available.

    \sa finished(), error(), resultsAvailable(), statusChanged()
*/
/*!
    \fn void QJsonDbRequest::resultsAvailable(int amount)

    This signal is emitted incrementally when the request received results. \a
    amount specifies how many results are available so far. Received results
    can be retrieved using takeResults() function.

    \sa takeResults(), finished()
*/
/*!
    \fn void QJsonDbRequest::finished()

    This signal is emitted when the request was completed successfully.

    \sa takeResults(), error(), statusChanged()
*/
/*!
    \fn void QJsonDbRequest::error(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)

    This signal is emitted when an error occured while processing the request.
    \a code and \a message describe the error.

    \sa finished(), statusChanged()
*/

#include "moc_qjsondbrequest.cpp"

QT_END_NAMESPACE_JSONDB
