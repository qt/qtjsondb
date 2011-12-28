/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
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

#include "jsondb-query.h"
#include "jsondb-client.h"
#include "jsondb-strings_p.h"

QT_ADDON_JSONDB_BEGIN_NAMESPACE

/*!
    \class QtAddOn::JsonDb::JsonDbResultBase
    \internal
*/

class JsonDbResultBasePrivate
{
    Q_DECLARE_PUBLIC(JsonDbResultBase)
public:
    JsonDbResultBasePrivate(JsonDbClient *c, JsonDbResultBase *q)
        : q_ptr(q), client(c), requestId(-1), isFinished(true)
    { }

    JsonDbResultBase *q_ptr;
    JsonDbClient *client;

    int requestId;
    bool isFinished;

    QVariantMap header;
    QVariantList results;

    QString partition;
};

class JsonDbQueryPrivate : public JsonDbResultBasePrivate
{
    Q_DECLARE_PUBLIC(JsonDbQuery)
public:
    JsonDbQueryPrivate(JsonDbClient *c, JsonDbQuery *q)
        : JsonDbResultBasePrivate(c, q), queryOffset(0), queryLimit(-1)
    { }

    void _q_response(int reqId, const QVariant &);
    void _q_error(int code, const QString &message);
    void _q_emitMoreData();

    // HACK HACK HACK
    QVariantList moreResults;

    QString query;
    int queryOffset;
    int queryLimit;
    QMap<QString,QVariant> bindings;
};

void JsonDbQueryPrivate::_q_response(int reqId, const QVariant &response_)
{
    Q_UNUSED(reqId);
    Q_Q(JsonDbQuery);

    QVariantMap response = response_.toMap();
    header = QVariantMap();
    header.insert(QLatin1String("state"), response.value(QLatin1String("state")).value<quint32>());
    header.insert(QLatin1String("sortKey"), response.value(QLatin1String("sortKey")).value<QString>());

    emit q->started();

    // to mimic future behavior with streaming / client-side reads split data into two chunks
    QVariantList r = response.value(JsonDbString::kDataStr).toList();
    if (r.size()) {
        int count = qMax((r.size() / 2), 1);
        results = r.mid(0, count);
        r.erase(r.begin(), r.begin() + count);
        moreResults = r;
    }

    if (!results.isEmpty()) {
        emit q->resultsReady(results.size());
        QMetaObject::invokeMethod(q, "_q_emitMoreData", Qt::QueuedConnection);
    } else {
        emit q->finished();
    }
}

void JsonDbQueryPrivate::_q_emitMoreData()
{
    Q_Q(JsonDbQuery);
    results += moreResults;
    moreResults = QVariantList();

    isFinished = true;

    if (!results.isEmpty())
        emit q->resultsReady(results.size());
    emit q->finished();
}

void JsonDbQueryPrivate::_q_error(int code, const QString &message)
{
    Q_Q(JsonDbQuery);
    emit q->error(JsonDbError::ErrorCode(code), message);
}

JsonDbResultBase::JsonDbResultBase(JsonDbResultBasePrivate *d, QObject *parent)
    : QObject(parent), d_ptr(d)
{
    Q_ASSERT(d != 0);
}

JsonDbResultBase::~JsonDbResultBase()
{
}

int JsonDbResultBase::requestId() const
{
    return d_func()->requestId;
}

bool JsonDbResultBase::isFinished() const
{
    return d_func()->isFinished;
}

QVariantMap JsonDbResultBase::header() const
{
    return d_func()->header;
}

int JsonDbResultBase::resultsAvailable() const
{
    return d_func()->results.size();
}

QString JsonDbResultBase::partition() const
{
    return d_func()->partition;
}

void JsonDbResultBase::setPartition(const QString &partition)
{
    Q_D(JsonDbResultBase);
    d->partition = partition;
}

QVariantList JsonDbResultBase::takeResults()
{
    Q_D(JsonDbResultBase);
    QVariantList results;
    results.swap(d->results);
    return results;
}

void JsonDbResultBase::start()
{
}

/*!
    \class QtAddOn::JsonDb::JsonDbQuery

    \brief The JsonDbQuery class allows to execute a given database query and
    retrieve results.

    \code
        #include <jsondb-client.h>

        QT_ADDON_JSONDB_USE_NAMESPACE

        class QueryHandler : public QObject
        {
            Q_OBJECT
        public:
            QueryHandler()
            {
                JsonDbClient *client = new JsonDbClient(this);
                JsonDbQuery *query = client.query();
                query->setQuery(QLatin1String("[?_type=\"Person\"]"));
                QObject::connect(query, SIGNAL(resultsReady(int)), this, SLOT(onResultsReady(int)));
                QObject::connect(query, SIGNAL(finished()), this, SLOT(onFinished()));
                QObject::connect(query, SIGNAL(finished()), query, SLOT(deleteLater()));
                query->start();
            }

        public slots:
            void onResultsReady(int resultsAvailable)
            {
                qDebug() << "So far fetched" << resultsAvailable << "result(s)";
            }
            void onFinished()
            {
                JsonDbQuery *query = qobject_cast<JsonDbQuery *>(sender());
                Q_ASSERT(query);
                qDebug() << "Query complete, fetched" << query->resultsAvailable() << "result(s):";
                qDebug() << query->takeResults();
            }
        };
    \endcode

    \sa QtAddOn::JsonDb::JsonDbClient
*/

/*!
    \property QtAddOn::JsonDb::JsonDbQuery::partition

    Specifies the partition name the query operates on.
*/

/*!
    \fn int QtAddOn::JsonDb::JsonDbQuery::requestId() const

    Returns a request id for the query request.
*/

/*!
    \fn bool QtAddOn::JsonDb::JsonDbQuery::isFinished() const

    Returns true if the query is complete and the finished() signal was already
    emitted.

    \sa finished()
*/

/*!
    \fn void QtAddOn::JsonDb::JsonDbQuery::started()

    Signal is emitted after the query execution was started and some initial
    data is available.

    \sa start(), stateNumber, sortKey
*/
/*!
    \fn void QtAddOn::JsonDb::JsonDbQuery::resultsReady(int resultsAvailable)

    Signal is emitted after you start() a query and there are new results
    available that match it. \a resultsAvailable tells you how many results are
    available at this point, and you can retrieve them with takeResults().

    \sa finished(), takeResults()
*/
/*!
    \fn void QtAddOn::JsonDb::JsonDbQuery::finished()

    Signal is emitted after the query is complete.

    \sa takeResults(), resultsReady(), isFinished(), started()
*/
/*!
    \fn void QtAddOn::JsonDb::JsonDbQuery::error(JsonDbError::ErrorCode code, const QString &message)

    Signal is emitted when an error with a given \a code occured while
    executing a query. Extended information about the error can be retrieved
    from \a message.
*/
/*!
    \fn QVariantList QtAddOn::JsonDb::JsonDbQuery::takeResults()

    Returns the results of the query that are retrieved so far and clears the
    internal result list.

    Unless the results are "taken", they are accumulated on every resultsReady()
    signal, so there is no need to "take" data before finished() signal is
    emitted unless you want to process results in chunks.

    \sa resultsReady(), finished()
*/
/*!
    \property QtAddOn::JsonDb::JsonDbQuery::resultsAvailable

    Returns the amount of results of the query that are accumulated so far.

    \sa takeResults(), resultsReady(), isFinished()
*/

/*!
    \internal
*/
JsonDbQuery::JsonDbQuery(JsonDbClient *client, QObject *parent)
    : JsonDbResultBase(new JsonDbQueryPrivate(client, this), parent)
{
    Q_ASSERT(client);
}

/*!
    Destroys the object.
*/
JsonDbQuery::~JsonDbQuery()
{
}

/*!
    \property JsonDbQuery::stateNumber

    Returns a database state number that the query was executed on.

    The property is populated after started() signal was emitted.

    \sa started()
*/
quint32 JsonDbQuery::stateNumber() const
{
    return d_func()->header.value(QLatin1String("state"), quint32(0)).value<quint32>();
}

/*!
    \property JsonDbQuery::sortKey

    Returns a field that was used as a sort key when executing a query.

    The results of the query are ordered by that field.

    The property is populated after started() signal was emitted.

    \sa started(), takeResults()
*/
QString JsonDbQuery::sortKey() const
{
    return d_func()->header.value(QLatin1String("sortKey")).value<QString>();
}

/*!
    \property QtAddOn::JsonDb::JsonDbQuery::query

    \brief the query string

    Set this property to the query string that you want to execute.

    \sa queryOffset, queryLimit, start()
*/
QString JsonDbQuery::query() const
{
    return d_func()->query;
}

void JsonDbQuery::setQuery(const QString &query)
{
    Q_D(JsonDbQuery);
    d->query = query;
}

/*!
    \property QtAddOn::JsonDb::JsonDbQuery::queryOffset

    \brief the initial offset of a query

    Set this property to the numeric value from which the results will be returned.

    \sa query, queryLimit, start()
*/
int JsonDbQuery::queryOffset() const
{
    return d_func()->queryOffset;
}

void JsonDbQuery::setQueryOffset(int offset)
{
    Q_D(JsonDbQuery);
    d->queryOffset = offset;
}

/*!
    \property QtAddOn::JsonDb::JsonDbQuery::queryLimit

    \brief the limit of a query

    This property defines how many results will be retrieved at most.

    \sa query, queryOffset, start()
*/
int JsonDbQuery::queryLimit() const
{
    return d_func()->queryLimit;
}

void JsonDbQuery::setQueryLimit(int limit)
{
    Q_D(JsonDbQuery);
    d->queryLimit = limit;
}

/*!
    \fn void QtAddOn::JsonDb::JsonDbQuery::start()

    Starts the query.

    \sa query, queryLimit, started(), resultsReady(), finished()
*/
void JsonDbQuery::start()
{
    Q_D(JsonDbQuery);
    d->isFinished = false;
    d->requestId = d->client->query(d->query, d->queryOffset, d->queryLimit, d->bindings, d->partition,
                                    this, SLOT(_q_response(int,QVariant)), SLOT(_q_error(int,QString)));
}

/*!
    \fn void QtAddOn::JsonDb::JsonDbQuery::bindValue(const QString &placeHolder, const QVariant &val)

    Set the placeholder \a placeHolder to be bound to value \a val in the query
    string. Note that the placeholder mark (e.g $) must be included when
    specifying the placeholder name.

    \code
        JsonDbQuery *query = jsonDbClient->query();
        query->setQuery(QLatin1String("[?_type=\"Person\"][?firstName = $name]"));
        query->bindValue(QLatin1String("$name"), QLatin1String("Malcolm"));
    \endcode

    \sa query, boundValue(), boundValues()
*/
void JsonDbQuery::bindValue(const QString &placeHolder, const QVariant &val)
{
    Q_D(JsonDbQuery);
    d->bindings.insert(placeHolder, val);
}

/*!
    \fn QVariant QtAddOn::JsonDb::JsonDbQuery::boundValue(const QString &placeHolder) const

    Returns the value for the \a placeHolder.
*/
QVariant JsonDbQuery::boundValue(const QString &placeHolder) const
{
    return d_func()->bindings.value(placeHolder);
}

/*!
    \fn QMap<QString,QVariant> QtAddOn::JsonDb::JsonDbQuery::boundValues() const

    Returns a map of the bound values
*/
QMap<QString,QVariant> JsonDbQuery::boundValues() const
{
    return d_func()->bindings;
}

/*!
    \class QtAddOn::JsonDb::JsonDbChangesSince

    \brief The JsonDbChangesSince class allows to retrieve history of changes
    to objects in a database.

    \sa QtAddOn::JsonDb::JsonDbClient
*/

/*!
    \property QtAddOn::JsonDb::JsonDbChangesSince::partition

    Specifies the partition name the request operates on.
*/

/*!
    \fn int QtAddOn::JsonDb::JsonDbChangesSince::requestId() const

    Returns a request id for the "changes since" request.
*/

/*!
    \fn bool QtAddOn::JsonDb::JsonDbChangesSince::isFinished() const

    Returns true if the request is complete and the finished() signal was
    already emitted.

    \sa finished()
*/

/*!
    \fn void QtAddOn::JsonDb::JsonDbChangesSince::started()

    Signal is emitted after the request execution was started and some initial
    data is available.

    \sa start(), startingStateNumber, currentStateNumber
*/
/*!
    \fn void QtAddOn::JsonDb::JsonDbChangesSince::resultsReady(int resultsAvailable)

    Signal is emitted after you start() a request and there are new results
    available that match it. \a resultsAvailable tells you how many results are
    available at this point, and you can retrieve them with takeResults().

    \sa finished(), takeResults()
*/
/*!
    \fn void QtAddOn::JsonDb::JsonDbChangesSince::finished()

    Signal is emitted after the request is complete.

    \sa takeResults(), resultsReady(), isFinished(), started()
*/
/*!
    \fn void QtAddOn::JsonDb::JsonDbChangesSince::error(JsonDbError::ErrorCode code, const QString &message)

    Signal is emitted when an error with a given \a code occured while
    executing a request. Extended information about the error can be retrieved
    from \a message.
*/
/*!
    \fn QVariantList QtAddOn::JsonDb::JsonDbChangesSince::takeResults()

    Returns the results of the request that are retrieved so far and clears the
    internal result list.

    Unless the results are "taken", they are accumulated on every resultsReady()
    signal, so there is no need to "take" data before finished() signal is
    emitted unless you want to process results in chunks.

    \sa resultsReady(), finished()
*/
/*!
    \property QtAddOn::JsonDb::JsonDbChangesSince::resultsAvailable

    Returns the amount of results of the request that are accumulated so far.

    \sa takeResults(), resultsReady(), isFinished()
*/

class JsonDbChangesSincePrivate : public JsonDbResultBasePrivate
{
    Q_DECLARE_PUBLIC(JsonDbChangesSince)
public:
    JsonDbChangesSincePrivate(JsonDbClient *c, JsonDbChangesSince *q)
        : JsonDbResultBasePrivate(c, q), stateNumber(0)
    { }

    void _q_response(int reqId, const QVariant &);
    void _q_error(int code, const QString &message);
    void _q_emitMoreData();

    // HACK HACK HACK
    QVariantList moreResults;

    QStringList types;
    quint32 stateNumber;
};

void JsonDbChangesSincePrivate::_q_response(int reqId, const QVariant &response_)
{
    Q_UNUSED(reqId);
    Q_Q(JsonDbChangesSince);

    QVariantMap response = response_.toMap();
    header = QVariantMap();
    header.insert(QLatin1String("startingStateNumber"), response.value(QLatin1String("startingStateNumber")).value<quint32>());
    header.insert(QLatin1String("currentStateNumber"), response.value(QLatin1String("currentStateNumber")).value<quint32>());

    emit q->started();

    // to mimic future behavior with streaming / client-side reads split data into two chunks
    QVariantList r = response.value(QLatin1String("changes")).toList();
    int count = r.size() / 2;
    results = r.mid(0, count);
    r.erase(r.begin(), r.begin() + count);
    moreResults = r;

    emit q->resultsReady(results.size());
    QMetaObject::invokeMethod(q, "_q_emitMoreData", Qt::QueuedConnection);
}

void JsonDbChangesSincePrivate::_q_emitMoreData()
{
    Q_Q(JsonDbChangesSince);
    results += moreResults;
    moreResults = QVariantList();

    isFinished = true;

    if (!results.isEmpty())
        emit q->resultsReady(results.size());
    emit q->finished();
}

void JsonDbChangesSincePrivate::_q_error(int code, const QString &message)
{
    Q_Q(JsonDbChangesSince);
    emit q->error(JsonDbError::ErrorCode(code), message);
}

/*!
    \internal
*/
JsonDbChangesSince::JsonDbChangesSince(JsonDbClient *client, QObject *parent)
    : JsonDbResultBase(new JsonDbChangesSincePrivate(client, this), parent)
{
    Q_ASSERT(client);
}

/*!
    Destroys the object.
*/
JsonDbChangesSince::~JsonDbChangesSince()
{
}

/*!
    \property QtAddOn::JsonDb::JsonDbChangesSince::startingStateNumber

    Returns the starting state number for the changesSince request.

    The property is populated after started() signal was emitted.

    \sa started()
*/
quint32 JsonDbChangesSince::startingStateNumber() const
{
    return d_func()->header.value(QLatin1String("startingStateNumber"), quint32(0)).value<quint32>();
}

/*!
    \property QtAddOn::JsonDb::JsonDbChangesSince::currentStateNumber

    Returns the ending state number for the changesSince request.

    The property is populated after started() signal was emitted.

    \sa started()
*/
quint32 JsonDbChangesSince::currentStateNumber() const
{
    return d_func()->header.value(QLatin1String("currentStateNumber"), quint32(0)).value<quint32>();
}

/*!
    \property QtAddOn::JsonDb::JsonDbChangesSince::types

    \brief the list of object types for which we return changes

    \sa stateNumber, start()
*/
QStringList JsonDbChangesSince::types() const
{
    return d_func()->types;
}

void JsonDbChangesSince::setTypes(const QStringList &types)
{
    Q_D(JsonDbChangesSince);
    d->types = types;
}

/*!
    \property QtAddOn::JsonDb::JsonDbChangesSince::stateNumber

    \brief the initial state number from which changes should be retrieved.

    \sa types, start()
*/
quint32 JsonDbChangesSince::stateNumber() const
{
    return d_func()->stateNumber;
}

void JsonDbChangesSince::setStateNumber(quint32 stateNumber)
{
    Q_D(JsonDbChangesSince);
    d->stateNumber = stateNumber;
}

/*!
    \fn void QtAddOn::JsonDb::JsonDbChangesSince::start()

    Starts the "changes since" request.

    \sa stateNumber, types, started(), resultsReady(), finished()
*/
void JsonDbChangesSince::start()
{
    Q_D(JsonDbChangesSince);
    d->isFinished = false;
    d->requestId = d->client->changesSince(d->stateNumber, d->types, d->partition,
                                           this, SLOT(_q_response(int,QVariant)), SLOT(_q_error(int,QString)));
}

#include "moc_jsondb-query.cpp"

QT_ADDON_JSONDB_END_NAMESPACE
