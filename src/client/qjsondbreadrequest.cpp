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

#include "qjsondbreadrequest_p.h"
#include "qjsondbstrings_p.h"

#include <QJsonArray>
#include <QVariant>
#include <QDebug>

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \class QJsonDbReadRequest
    \inmodule QtJsonDb

    \brief The QJsonDbReadRequest class allows to query database.

    See \l{Queries} for documentation on the query string format.

    \code
        QJsonDbReadRequest *request = new QJsonDbReadRequest;
        request->setQuery(QStringLiteral("[?_type=\"Foo\"]"));
        connect(request, SIGNAL(finished()), this, SLOT(onQueryFinished()));
        connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onQueryError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                request, SLOT(deleteLater()));
        QJsonDbConnection *connection = new QJsonDbConnection;
        connection->send(request);
    \endcode
*/
/*!
    \enum QJsonDbReadRequest::ErrorCode

    This enum describes database connection errors for read requests that can
    be emitted by the error() signal.

    \value NoError
    \value InvalidRequest
    \value OperationNotPermitted
    \value InvalidPartition Invalid partition.
    \value DatabaseConnectionError
    \value PartitionUnavailable
    \value MissingQuery Missing query field.
    \value InvalidMessage
    \value InvalidLimit Invalid limit field

    \sa error(), QJsonDbRequest::ErrorCode
*/

QJsonDbReadRequestPrivate::QJsonDbReadRequestPrivate(QJsonDbReadRequest *q)
    : QJsonDbRequestPrivate(q), queryLimit(-1), stateNumber(0)
{
}

/*!
    Constructs a new query request object with the given \a parent.
*/
QJsonDbReadRequest::QJsonDbReadRequest(QObject *parent)
    : QJsonDbRequest(new QJsonDbReadRequestPrivate(this), parent)
{
}

/*!
    Constructs a new query request object with the given \a query string and \a
    parent.
*/
QJsonDbReadRequest::QJsonDbReadRequest(const QString &query, QObject *parent)
    : QJsonDbRequest(new QJsonDbReadRequestPrivate(this), parent)
{
    Q_D(QJsonDbReadRequest);
    d->query = query;
}

/*!
    \internal
*/
QJsonDbReadRequest::QJsonDbReadRequest(QJsonDbReadRequestPrivate *dd, QObject *parent)
    : QJsonDbRequest(dd, parent)
{
    Q_ASSERT(dd != 0);
}

/*!
    Destroys the request object.
*/
QJsonDbReadRequest::~QJsonDbReadRequest()
{
}

/*!
    \property QJsonDbReadRequest::query

    \brief the query string

    Specifies the query string for the request in the format described in
    \l{Queries}.

    \sa queryLimit, bindValue()
*/
QString QJsonDbReadRequest::query() const
{
    Q_D(const QJsonDbReadRequest);
    return d->query;
}

void QJsonDbReadRequest::setQuery(const QString &query)
{
    Q_D(QJsonDbReadRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    d->query = query;
}

/*!
    \property QJsonDbReadRequest::queryLimit
    \brief the limit of a query

    Specifies the maximum amount of amount that should be fetched from the
    database.

    \sa query
*/
int QJsonDbReadRequest::queryLimit() const
{
    Q_D(const QJsonDbReadRequest);
    return d->queryLimit;
}

void QJsonDbReadRequest::setQueryLimit(int limit)
{
    Q_D(QJsonDbReadRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    d->queryLimit = limit;
}

/*!
    Set the placeholder \a placeHolder to be bound to value \a val in the query
    string. Note that '%' is the only placeholder mark supported by the query.
    The marker '%' should not be included in the \a placeHolder name.

    \code
        QJsonDbReadRequest *query = new QJsonDbReadRequest;
        query->setQuery(QStringLiteral("[?_type=\"Person\"][?firstName = %name]"));
        query->bindValue(QStringLiteral("name"), QLatin1String("Malcolm"));
    \endcode

    \sa query, boundValue(), boundValues(), clearBoundValues()
*/
void QJsonDbReadRequest::bindValue(const QString &placeHolder, const QJsonValue &val)
{
    Q_D(QJsonDbReadRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    d->bindings.insert(placeHolder, val);
}

/*!
    Returns the value for the \a placeHolder.
*/
QJsonValue QJsonDbReadRequest::boundValue(const QString &placeHolder) const
{
    Q_D(const QJsonDbReadRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    return d->bindings.value(placeHolder, QJsonValue(QJsonValue::Undefined));
}

/*!
    Returns a map of the bound values.
*/
QMap<QString,QJsonValue> QJsonDbReadRequest::boundValues() const
{
    Q_D(const QJsonDbReadRequest);
    return d->bindings;
}

/*!
    Clears all bound values.
*/
void QJsonDbReadRequest::clearBoundValues()
{
    Q_D(QJsonDbReadRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    d->bindings.clear();
}

/*!
    \property QJsonDbReadRequest::stateNumber

    Returns a database state number that the query was executed on.

    The property is populated after started() signal was emitted.

    \sa started()
*/
quint32 QJsonDbReadRequest::stateNumber() const
{
    Q_D(const QJsonDbReadRequest);
    return d->stateNumber;
}

/*!
    \property QJsonDbReadRequest::sortKey

    Returns a field that was used as a sort key when executing a query.

    The results of the query are ordered by that field.

    The property is populated after started() signal was emitted.

    \sa started(), takeResults()
*/
QString QJsonDbReadRequest::sortKey() const
{
    Q_D(const QJsonDbReadRequest);
    return d->sortKey;
}

QJsonObject QJsonDbReadRequestPrivate::getRequest() const
{
    Q_Q(const QJsonDbReadRequest);
    QJsonObject object;
    object.insert(JsonDbStrings::Property::query(), query);
    if (queryLimit != -1)
        object.insert(JsonDbStrings::Property::queryLimit(), queryLimit);
    QVariant v = q->property("queryOffset");
    if (v.isValid())
        object.insert(JsonDbStrings::Property::queryOffset(), v.toInt());
    if (!bindings.isEmpty()) {
        QJsonObject b;
        QMap<QString, QJsonValue>::const_iterator it, e;
        for (it = bindings.begin(), e = bindings.end(); it != e; ++it)
            b.insert(it.key(), it.value());
        object.insert(JsonDbStrings::Property::bindings(), b);
    }
    QJsonObject request;
    request.insert(JsonDbStrings::Protocol::action(), JsonDbStrings::Protocol::query());
    request.insert(JsonDbStrings::Protocol::object(), object);
    request.insert(JsonDbStrings::Protocol::partition(), partition);
    request.insert(JsonDbStrings::Protocol::requestId(), requestId);
    return request;
}

void QJsonDbReadRequestPrivate::handleResponse(const QJsonObject &response)
{
    Q_Q(QJsonDbReadRequest);

    stateNumber = static_cast<quint32>(response.value(JsonDbStrings::Property::state()).toDouble());
    sortKey = response.value(JsonDbStrings::Property::sortKeys()).toArray().at(0).toString();

    setStatus(QJsonDbRequest::Receiving);
    emit q->started();

    QJsonArray list = response.value(JsonDbStrings::Protocol::data()).toArray();
    results.reserve(results.size() + list.size());
    foreach (const QJsonValue &v, list)
        results.append(v.toObject());

    emit q->resultsAvailable(results.size());
    setStatus(QJsonDbRequest::Finished);
    emit q->finished();
}

void QJsonDbReadRequestPrivate::handleError(int code, const QString &message)
{
    Q_Q(QJsonDbReadRequest);
    setStatus(QJsonDbRequest::Error);
    emit q->error(QJsonDbRequest::ErrorCode(code), message);
}

/*!
    \class QJsonDbReadObjectRequest
    \inmodule QtJsonDb

    \brief The QJsonDbReadObjectRequest class allows to retrieve one object by its UUID.

    To retrieve object content for a known uuid:

    \code
        QJsonDbReadObjectRequest *request = new QJsonDbReadObjectRequest(this);
        request->setUuid(objectUuid);
        connect(request, SIGNAL(objectAvailable(QJsonObject), this, SLOT(onObjectAvailable(QJsonObject)));
        connect(request, SIGNAL(objectUnavailable(QUuid), this, SLOT(onObjectNotFound()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        connection->send(request);
    \endcode
*/

QJsonDbReadObjectRequestPrivate::QJsonDbReadObjectRequestPrivate(QJsonDbReadObjectRequest *q)
    : QJsonDbReadRequestPrivate(q)
{
}

/*!
    Constructs a new QJsonDbReadObjectRequest object with the given \a parent.
*/
QJsonDbReadObjectRequest::QJsonDbReadObjectRequest(QObject *parent)
    : QJsonDbReadRequest(new QJsonDbReadObjectRequestPrivate(this), parent)
{
    connect(this, SIGNAL(finished()), this, SLOT(_q_onFinished()));
}

/*!
    Constructs a new QJsonDbReadObjectRequest object with the given \a parent
    to retrieve content of the object with the given \a uuid.
*/
QJsonDbReadObjectRequest::QJsonDbReadObjectRequest(const QUuid &uuid, QObject *parent)
    : QJsonDbReadRequest(new QJsonDbReadObjectRequestPrivate(this), parent)
{
    connect(this, SIGNAL(finished()), this, SLOT(_q_onFinished()));
    setUuid(uuid);
}

/*!
    \property QJsonDbReadObjectRequest::uuid
    Specifies UUID of the object to retrieve from the database.
*/
QUuid QJsonDbReadObjectRequest::uuid() const
{
    Q_D(const QJsonDbReadObjectRequest);
    return d->uuid;
}

void QJsonDbReadObjectRequest::setUuid(const QUuid &uuid)
{
    Q_D(QJsonDbReadObjectRequest);
    d->uuid = uuid;
    setQuery(QStringLiteral("[?_uuid = %uuid]"));
    bindValue(QStringLiteral("uuid"), uuid.toString());
}

void QJsonDbReadObjectRequestPrivate::_q_onFinished()
{
    Q_Q(QJsonDbReadObjectRequest);
    if (results.size() > 1) {
        qWarning() << "QJsonDbReadObjectRequest: instead of 1 object, got" << results.size() << "object(s)";
        return;
    }
    if (results.size() == 0) {
        emit q->objectUnavailable(uuid);
        return;
    }
    QJsonObject object = results.at(0);
    emit q->objectAvailable(object);
}

/*!
    \fn void QJsonDbReadObjectRequest::objectAvailable(const QJsonObject &object)

    This signal is emitted when the request is complete and the \a object was
    successfully retrieve from the database.

    This is just a convenience signal that can be used instead of finished().

    \sa objectUnavailable() error()
*/

/*!
    \fn void QJsonDbReadObjectRequest::objectUnavailable(const QUuid &uuid)

    This signal is emitted when the request is complete, but the requested
    object with the given \a uuid was not found in the database.

    This is just a convenience signal that can be used instead of finished().

    \sa objectAvailable() error()
*/

#include "moc_qjsondbreadrequest.cpp"

QT_END_NAMESPACE_JSONDB
