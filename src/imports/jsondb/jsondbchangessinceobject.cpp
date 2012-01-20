/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
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
** $QT_END_LICENSE$
**
****************************************************************************/
#include "jsondb-global.h"
#include "jsondbchangessinceobject.h"
#include "jsondbpartition.h"
#include "private/jsondb-strings_p.h"
#include <qdebug.h>

QT_USE_NAMESPACE_JSONDB

/*!
    \qmlclass ChangesSince
    \inqmlmodule QtJsonDb
    \since 1.x

    This allows to query the list of changes that happened between the current state and a specified
    stateNumber. Users can execute this by calling the exec(). To retrieve the results, connect to
    onResultsReady and/or onFinished.

    \code
    JsonDb.Partition {
        id: nokiaPartition
        name: "com.nokia.shared"
    }
    JsonDb.ChangesSince {
        id:contactChanges
        partition:nokiaPartition
        types: ["Contact"]
        stateNumber : 10
        onFinished: {
            var results = contactsQuery.takeResults();
            console.log("Results: Count + results.length );
        }

        onError: {
            console.log("onError " + code + message);
        }

     }

     contactChanges.exec();
    \endcode

*/

JsonDbChangesSinceObject::JsonDbChangesSinceObject(QObject *parent)
    : QObject(parent)
    , completed(false)
    , startStateNumber(0)
    , partitionObject(0)
    , defaultPartitionObject(0)
    , jsondbChangesSince(0)
{
}

JsonDbChangesSinceObject::~JsonDbChangesSinceObject()
{
    if (defaultPartitionObject)
        delete defaultPartitionObject;
    if (jsondbChangesSince)
        delete jsondbChangesSince;
}


/*!
    \qmlproperty list QtJsonDb::ChangesSince::types
    Holds the list of object types which will be checked
    while executing the ChangesSince.
*/
QStringList JsonDbChangesSinceObject::types() const
{
    return filterTypes;
}

void JsonDbChangesSinceObject::setTypes(const QStringList &newTypes)
{
    filterTypes = newTypes;
}

/*!
    \qmlproperty object QtJsonDb::ChangesSince::partition
     Holds the partition object for the object.
*/

JsonDbPartition* JsonDbChangesSinceObject::partition()
{
    if (!partitionObject) {
        defaultPartitionObject = new JsonDbPartition();
        setPartition(defaultPartitionObject);
    }
    return partitionObject;
}

void JsonDbChangesSinceObject::setPartition(JsonDbPartition *newPartition)
{
    if (partitionObject == newPartition)
        return;
    if (partitionObject == defaultPartitionObject)
        delete defaultPartitionObject;
    partitionObject = newPartition;
}

/*!
    \qmlproperty int QtJsonDb::ChangesSince::stateNumber
    State Number from which the changesSince is computed.
*/
quint32 JsonDbChangesSinceObject::stateNumber() const
{
    return startStateNumber;
}

void JsonDbChangesSinceObject::setStateNumber(quint32 newStateNumber)
{
    startStateNumber = newStateNumber;
}

/*!
    \qmlproperty int QtJsonDb::ChangesSince::startingStateNumber
    State Number from which the changesSince is actually ccomputed. This
    can be different from the QtJsonDb::ChangesSince::stateNumber. Only
    valid after receiving the onResultsReady()
*/
quint32 JsonDbChangesSinceObject::startingStateNumber() const
{
    if (jsondbChangesSince)
        return jsondbChangesSince->startingStateNumber();
    return startStateNumber;
}

/*!
    \qmlproperty int QtJsonDb::ChangesSince::currentStateNumber
    The current state number when the changesSince was executed. Only
    valid after receiving the onResultsReady()
*/
quint32 JsonDbChangesSinceObject::currentStateNumber() const
{
    if (jsondbChangesSince)
        return jsondbChangesSince->currentStateNumber();
    return startStateNumber;
}


/*!
    \qmlmethod list QtJsonDb::ChangesSince::takeResults()

    Retrieves the list of results available in the object. This can be called multiple
    times for a single execution. Call this from onResultsReady or onFinished. This will
    remove the returned results from the query object.

    If the request was successful, the response will be an array of objects. Each item in
    the results array will be an object of type {"after" : {} , "before" : {}}. The \a after
    sub-object will be undefined for deleted objects. For newly created objects, the \a before
    sub-object will be undefined. If both sub-objects are valid the change represents an update.


    \code
    property var objects : [];
    JsonDb.ChangesSince {
        id:contactChanges
        partition:nokiaPartition
        types: ["Contact"]
        stateNumber : 10
        onResultsReady: {
            objects = objects.concat(contactChanges.takeResults());
            console.log("Length :" + objects.length);
        }
        onFinished: {
            objects = objects.concat(contactChanges.takeResults());
            console.log("Results: Count + objects.length );
        }

        onError: {
            console.log("onError " + result.code + result.message);
        }

     }
    \endcode

*/

QVariantList JsonDbChangesSinceObject::takeResults()
{
    QVariantList list;
    if (jsondbChangesSince) {
        list = jsondbChangesSince->takeResults();
    }
    return list;
}

/*!
    \qmlsignal QtJsonDb::ChangesSince::onResultsReady(int resultsAvailable)

    This handler is called when the a set of results are avaialable in the object. This
    will be called multiple times for an execution of the changesSince. Results can be
    retrievd here by calling takeResults() of the query object.

*/

/*!
    \qmlsignal QtJsonDb::ChangesSince::onFinished()

    This handler is called when the an execution of changesSince is finished. Results can be
    retrievd here by calling takeResults() of the changesSince object. Users can wait for
    onFinished to avoid chunked reading.
*/

/*!
    \qmlsignal QtJsonDb::ChangesSince::onError(code, message)

    This handler is called when there was an error executing the changesSince. \a code gives the
    error code and the \a message contains details of the error.
*/

void JsonDbChangesSinceObject::componentComplete()
{
    completed = true;
}

void JsonDbChangesSinceObject::emitError(QtAddOn::JsonDb::JsonDbError::ErrorCode code, const QString& message)
{
    emit error(int(code), message);
}


/*!
    \qmlmethod object QtJsonDb::ChangesSince::exec()

    Users should call this method to start the execution of changesSince on this partition.
    Once there are some results ready on the object, the onResultsReady will be triggered. This
    will be called whenever a new chunk of results are ready. Users can call takeResults() on
    this object to retrieve the results at any time. The ChangesSince also emits an onFinished()
    signal when the execution is finished.

*/
int JsonDbChangesSinceObject::exec()
{
    if (!completed) {
        qWarning("Component not ready");
        return -1;
    }
    if (jsondbChangesSince) {
        delete jsondbChangesSince;
    }
    jsondbChangesSince = partitionObject->jsonDb.changesSince();
    jsondbChangesSince->setTypes(filterTypes);
    jsondbChangesSince->setStateNumber(startStateNumber);
    jsondbChangesSince->setPartition(partitionObject->name());
    connect(jsondbChangesSince, SIGNAL(resultsReady(int)),
            this, SIGNAL(resultsReady(int)));
    connect(jsondbChangesSince, SIGNAL(finished()),
            this, SIGNAL(finished()));
    connect(jsondbChangesSince, SIGNAL(error(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)),
            this, SLOT(emitError(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)));

    jsondbChangesSince->start();

    return jsondbChangesSince->requestId();

}


