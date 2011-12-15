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

namespace QtAddOn { namespace JsonDb {

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
    QList<QVariantMap> results;

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
    QList<QVariantMap> moreResults;

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
    int count = r.size() / 2;
    results.reserve(results.size() + count);
    for (int i = 0; i < count; ++i)
        results.append(r.at(i).toMap());
    for (int i = count; i < r.size(); ++i)
        moreResults.append(r.at(i).toMap());

    r = QVariantList(); // just to free this memory

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
    moreResults = QList<QVariantMap>();

    isFinished = true;

    if (!results.isEmpty())
        emit q->resultsReady(results.size());
    emit q->finished();
}

void JsonDbQueryPrivate::_q_error(int code, const QString &message)
{
    Q_Q(JsonDbQuery);
    emit q->error(code, message);
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

QList<QVariantMap> JsonDbResultBase::takeResults()
{
    Q_D(JsonDbResultBase);
    QList<QVariantMap> results;
    results.swap(d->results);
    return results;
}

void JsonDbResultBase::start()
{
}

//
// JsonDbQuery
//

JsonDbQuery::JsonDbQuery(JsonDbClient *client, QObject *parent)
    : JsonDbResultBase(new JsonDbQueryPrivate(client, this), parent)
{
    Q_ASSERT(client);
}

JsonDbQuery::~JsonDbQuery()
{
}

quint32 JsonDbQuery::stateNumber() const
{
    return d_func()->header.value(QLatin1String("state"), quint32(0)).value<quint32>();
}

QString JsonDbQuery::sortKey() const
{
    return d_func()->header.value(QLatin1String("sortKey")).value<QString>();
}

QString JsonDbQuery::query() const
{
    return d_func()->query;
}

void JsonDbQuery::setQuery(const QString &query)
{
    Q_D(JsonDbQuery);
    d->query = query;
}

int JsonDbQuery::queryOffset() const
{
    return d_func()->queryOffset;
}

void JsonDbQuery::setQueryOffset(int offset)
{
    Q_D(JsonDbQuery);
    d->queryOffset = offset;
}

int JsonDbQuery::queryLimit() const
{
    return d_func()->queryLimit;
}

void JsonDbQuery::setQueryLimit(int limit)
{
    Q_D(JsonDbQuery);
    d->queryLimit = limit;
}

void JsonDbQuery::start()
{
    Q_D(JsonDbQuery);
    d->isFinished = false;
    d->requestId = d->client->query(d->query, d->queryOffset, d->queryLimit, d->bindings, d->partition,
                                    this, SLOT(_q_response(int,QVariant)), SLOT(_q_error(int,QString)));
}

void JsonDbQuery::bindValue(const QString &placeHolder, const QVariant &val)
{
    Q_D(JsonDbQuery);
    d->bindings.insert(placeHolder, val);
}

QVariant JsonDbQuery::boundValue(const QString &placeHolder) const
{
    return d_func()->bindings.value(placeHolder);
}

QMap<QString,QVariant> JsonDbQuery::boundValues() const
{
    return d_func()->bindings;
}

//
// JsonDbChangesSince
//

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
    QList<QVariantMap> moreResults;

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
    results.reserve(results.size() + count);
    for (int i = 0; i < count; ++i)
        results.append(r.at(i).toMap());
    for (int i = count; i < r.size(); ++i)
        moreResults.append(r.at(i).toMap());

    r = QVariantList(); // just to free this memory

    emit q->resultsReady(results.size());
    QMetaObject::invokeMethod(q, "_q_emitMoreData", Qt::QueuedConnection);
}

void JsonDbChangesSincePrivate::_q_emitMoreData()
{
    Q_Q(JsonDbChangesSince);
    results += moreResults;
    moreResults = QList<QVariantMap>();

    isFinished = true;

    if (!results.isEmpty())
        emit q->resultsReady(results.size());
    emit q->finished();
}

void JsonDbChangesSincePrivate::_q_error(int code, const QString &message)
{
    Q_Q(JsonDbChangesSince);
    emit q->error(code, message);
}

JsonDbChangesSince::JsonDbChangesSince(JsonDbClient *client, QObject *parent)
    : JsonDbResultBase(new JsonDbChangesSincePrivate(client, this), parent)
{
    Q_ASSERT(client);
}

JsonDbChangesSince::~JsonDbChangesSince()
{
}

quint32 JsonDbChangesSince::startingStateNumber() const
{
    return d_func()->header.value(QLatin1String("startingStateNumber"), quint32(0)).value<quint32>();
}

quint32 JsonDbChangesSince::currentStateNumber() const
{
    return d_func()->header.value(QLatin1String("currentStateNumber"), quint32(0)).value<quint32>();
}

QStringList JsonDbChangesSince::types() const
{
    return d_func()->types;
}

void JsonDbChangesSince::setTypes(const QStringList &types)
{
    Q_D(JsonDbChangesSince);
    d->types = types;
}

quint32 JsonDbChangesSince::stateNumber() const
{
    return d_func()->stateNumber;
}

void JsonDbChangesSince::setStateNumber(quint32 stateNumber)
{
    Q_D(JsonDbChangesSince);
    d->stateNumber = stateNumber;
}

void JsonDbChangesSince::start()
{
    Q_D(JsonDbChangesSince);
    d->isFinished = false;
    d->requestId = d->client->changesSince(d->stateNumber, d->types, d->partition,
                                           this, SLOT(_q_response(int,QVariant)), SLOT(_q_error(int,QString)));
}

#include "moc_jsondb-query.cpp"

} } // end namespace QtAddOn::JsonDb
