/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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
#include "jsondb-global.h"
#include "jsondbqueryobject.h"
#include "jsondbpartition.h"
#include "private/jsondb-strings_p.h"
#include <qdebug.h>

QT_USE_NAMESPACE_JSONDB

/*!
    \qmlclass Query
    \inqmlmodule QtJsonDb
    \since 1.x

    This allows to query for objects in a Partition. Users can execute the query by
    calling the start(). To retrieve the results, connect to onResultsReady and/or onFinished.

    \code
    JsonDb.Partition {
        id: nokiaPartition
        name: "com.nokia.shared"
    }
    JsonDb.Query {
        id:contactsQuery
        partition:nokiaPartition
        query: '[?_type="Contact"]'
        onFinished: {
            var results = contactsQuery.takeResults();
            console.log('Results: Count' + results.length );
        }
       onStatusChanged: {
            if (status === JsonDb.Query.Error)
                console.log("Query Failed to query " + error.code + " "+ error.message);
        }
     }

     contactsQuery.start();
    \endcode

*/

JsonDbQueryObject::JsonDbQueryObject(QObject *parent)
    : QObject(parent)
    , completed(false)
    , queryLimit(-1)
    , partitionObject(0)
    , defaultPartitionObject(0)
    , jsondbQuery(0)
{
}

JsonDbQueryObject::~JsonDbQueryObject()
{
    if (defaultPartitionObject)
        delete defaultPartitionObject;
    if (jsondbQuery)
        delete jsondbQuery;
}


/*!
    \qmlproperty string QtJsonDb::Query::query
     Holds the query string for the object.

    \sa QtJsonDb::Query::bindings

*/
QString JsonDbQueryObject::query()
{
    return queryString;
}

void JsonDbQueryObject::setQuery(const QString &newQuery)
{
    queryString = newQuery;
    checkForReadyStatus();
}

/*!
    \qmlproperty object QtJsonDb::Query::partition
     Holds the partition object for the query.
*/

JsonDbPartition* JsonDbQueryObject::partition()
{
    if (!partitionObject) {
        defaultPartitionObject = new JsonDbPartition();
        setPartition(defaultPartitionObject);
    }
    checkForReadyStatus();
    return partitionObject;
}

void JsonDbQueryObject::setPartition(JsonDbPartition *newPartition)
{
    if (partitionObject == newPartition)
        return;
    if (partitionObject == defaultPartitionObject)
        delete defaultPartitionObject;
    partitionObject = newPartition;
    checkForReadyStatus();
}

/*!
    \qmlproperty int QtJsonDb::Query::stateNumber
    The current state number when the query was executed. Only
    valid after receiving the onResultsReady()
*/
quint32 JsonDbQueryObject::stateNumber() const
{
    if (jsondbQuery)
        return jsondbQuery->stateNumber();
    return 0;
}

/*!
    \qmlproperty int QtJsonDb::Query::limit
     Holds the limit used while executing the query.
*/
int JsonDbQueryObject::limit()
{
    return queryLimit;
}

void JsonDbQueryObject::setLimit(int newLimit)
{
    queryLimit = newLimit;
    checkForReadyStatus();
}

/*!
    \qmlproperty object QtJsonDb::Query::bindings
    Holds the bindings for the placeholders used in the query string. Note that
    the placeholder marker '%' should not be included as part of the keys.

    \qml
    JsonDb.Query {
        id:typeQuery
        partition:queryPartition
        query:'[?_type="MyContact"][?name=%firstName]'
        bindings :{'firstName':'Book'}
        onFinished: {
            var results = typeQuery.takeResults();
            for (var i = 0; i < results.length; i++) {
                console.log("["+i+"] : "+ JSON.stringify(results[i]));
            }
        }
     }
    \endqml

    \sa QtJsonDb::Query::query

*/

QVariantMap JsonDbQueryObject::bindings() const
{
    return queryBindings;
}

void JsonDbQueryObject::setBindings(const QVariantMap &newBindings)
{
    queryBindings = newBindings;
    checkForReadyStatus();
}

/*!
    \qmlmethod list QtJsonDb::Query::takeResults()

    Retrieves the list of results available in the object. This can be called multiple
    times for a single execution. Call this from onResultsReady or onFinished. This will
    remove the returned results from the query object.
    If the request was successful, the results will be an array of objects. Each item in
    the array will be a complete object (depending on the query).


    \code
    property var objects : [];
    JsonDb.Query {
        id:contactsQuery
        partition:nokiaPartition
        query: '[?_type="Contact"]'
        onResultsReady: {
            objects = objects.concat(contactsQuery.takeResults());
            console.log("Length :" + objects.length);
        }
        onFinished: {
            objects = objects.concat(contactsQuery.takeResults());
            console.log("Results: Count + objects.length );
            for (var i = 0; i < objects.length; i++) {
                console.log("objects["+i+"] : "+ objects[i]._uuid)
            }

        }
       onStatusChanged: {
            if (status === JsonDb.Query.Error)
                console.log("Failed to query " + error.code + " "+ error.message);
        }
     }
    \endcode

*/

QVariantList JsonDbQueryObject::takeResults()
{
    QVariantList list;
    if (jsondbQuery) {
        list  = jsondbQuery->takeResults();
    }
    return list;
}

/*!
    \qmlsignal QtJsonDb::Query::onResultsReady(int resultsAvailable)

    This handler is called when the a set of results are avaialable in the query object. This
    will be called multiple times for an ececution of the query. Results can be retrievd here
    by calling takeResults() of the query object.

*/

/*!
    \qmlsignal QtJsonDb::Query::onFinished()

    This handler is called when the an execution of query is finished. Results can be retrievd here
    by calling takeResults() of the query object. Users can wait for onFinished to avoid chunked
    reading.
*/

/*!
    \qmlproperty object QtJsonDb::Query::error
    \readonly

    This property holds the current error information for the Query object. It contains:
    \list
    \o error.code -  code for the current error.
    \o error.message - detailed explanation of the error
    \endlist
*/

QVariantMap JsonDbQueryObject::error() const
{
    QVariantMap errorMap;
    errorMap.insert(QLatin1String("code"), errorCode);
    errorMap.insert(QLatin1String("message"), errorString);
    return errorMap;
}

/*!
    \qmlproperty enumeration QtJsonDb::Query::status
    \readonly

    This property holds the current status of the Query object.  It can be one of:
    \list
    \o Query.Null - waiting for component to finish loading or for all the pararamters to be set.
    \o Query.Loading - Executing the query
    \o Query.Ready - object is ready, users can call start()
    \o Query.Error - an error occurred while executing the query
    \endlist

    \sa QtJsonDb::Query::error
*/

JsonDbQueryObject::Status JsonDbQueryObject::status() const
{
    return objectStatus;
}

void JsonDbQueryObject::componentComplete()
{
    completed = true;
}

/*!
    \qmlmethod object QtJsonDb::Query::start()

    Users should call this method to start the execution of the query. When a set of results are
    ready on the object, the onResultsReady() will be triggered. This will be called whenever a new
    chunk of results are ready. Users can call takeResults() on this object to retrieve the results
    at any time. The ChangesSince also emits an onFinished() when the execution is finished.

*/

int JsonDbQueryObject::start()
{
    if (!completed) {
        qWarning("Component not ready");
        return -1;
    }
    if (jsondbQuery) {
        delete jsondbQuery;
    }
    jsondbQuery = partitionObject->jsonDb.query();
    jsondbQuery->setQuery(queryString);
    jsondbQuery->setQueryLimit(queryLimit);
    jsondbQuery->setQueryOffset(0);
    jsondbQuery->setPartition(partitionObject->name());
    QVariantMap::ConstIterator i = queryBindings.constBegin();
    while (i != queryBindings.constEnd()) {
        jsondbQuery->bindValue(i.key(), i.value());
        ++i;
    }
    connect(jsondbQuery, SIGNAL(resultsReady(int)),
            this, SIGNAL(resultsReady(int)));
    connect(jsondbQuery, SIGNAL(finished()),
            this, SIGNAL(finished()));
    connect(jsondbQuery, SIGNAL(error(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)),
            this, SLOT(setError(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)));

    jsondbQuery->start();
    objectStatus = JsonDbQueryObject::Loading;
    emit statusChanged(objectStatus);
    return jsondbQuery->requestId();

}

void JsonDbQueryObject::clearError()
{
    int oldErrorCode = errorCode;
    errorCode = 0;
    errorString.clear();
    if (oldErrorCode != Error) {
        emit errorChanged(error());
    }
}

bool JsonDbQueryObject::parametersReady()
{
    return (completed && !queryString.isEmpty() && partitionObject);
}

void JsonDbQueryObject::checkForReadyStatus()
{
    if (objectStatus != JsonDbQueryObject::Null)
        return;

    JsonDbQueryObject::Status oldStatus = objectStatus;

    if (!partitionObject)
        partitionObject = qobject_cast<JsonDbPartition*>(parent());
    if (!parametersReady()) {
        objectStatus = JsonDbQueryObject::Null;
        if (objectStatus != oldStatus)
            emit statusChanged(objectStatus);
        return;
    } else {
        objectStatus = JsonDbQueryObject::Ready;
        if (objectStatus != oldStatus)
            emit statusChanged(objectStatus);
    }
}

void JsonDbQueryObject::setError(QtAddOn::JsonDb::JsonDbError::ErrorCode code, const QString& message)
{
    int oldErrorCode = errorCode;
    errorCode = code;
    errorString = message;
    if (objectStatus != JsonDbQueryObject::Error) {
        objectStatus = JsonDbQueryObject::Error;
        emit statusChanged(objectStatus);
    }
    if (oldErrorCode != JsonDbQueryObject::Error) {
        emit errorChanged(error());
    }
}

