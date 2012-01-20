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

#include "private/jsondb-strings_p.h"
#include "jsondbpartition.h"
#include "jsondbnotification.h"
#include "plugin.h"
#include "jsondb-client.h"
#include "jsondbqueryobject.h"
#include "jsondbchangessinceobject.h"
#include <qdebug.h>

QT_USE_NAMESPACE_JSONDB

/*!
    \qmlclass Partition
    \inqmlmodule QtJsonDb
    \since 1.x

    The partition object allows you find, create, update, or remove objects from JsonDb.
    It also allows to create query, notification  and changesSince objects, query. A partition
    object is identified by its name.

    Most of the methods take script objects as parameters. The last parameter can be
    a callback function.
*/

JsonDbPartition::JsonDbPartition(const QString &partitionName, QObject *parent)
    :QObject(parent)
    ,_name(partitionName)
{
    connect(&jsonDb, SIGNAL(response(int,const QVariant&)),
            this, SLOT(dbResponse(int,const QVariant&)));
    connect(&jsonDb, SIGNAL(error(int,int,QString)),
            this, SLOT(dbErrorResponse(int,int,QString)));
}

JsonDbPartition::~JsonDbPartition()
{
}

/*!
    \qmlproperty string QtJsonDb::Partition::name
     Holds the human readable name of the partition.
*/

QString JsonDbPartition::name() const
{
    return _name;
}

void JsonDbPartition::setName(const QString &partitionName)
{
    if (partitionName != _name) {
        _name = partitionName;
        foreach (QPointer<JsonDbNotify> notify, notifications) {
            removeNotification(notify);
        }
        notifications.clear();
        // tell notifications to resubscribe
        emit nameChanged(partitionName);
    }
}

/*!
    \qmlmethod int QtJsonDb::Partition::create(object newObject, object options, function callback)

    Creates the \a newObject (or list of objects) in the partition. The callback will be called
    in case of failure or success. It returns the id of the request. If it fails to create an
    object, the whole transaction will be aborted. The \a options is not used now. The \a options and \a
    callback parameters are optional. If the \a callback parameter is used, it has to be the
    last parameter specified in the function call. The \a callback has the following signature.

    \code
    import QtJsonDb 1.0 as JsonDb
    function createCallback(error, response) {
        if (error) {
            // communication error or failed to create one or more objects.
            // 'error' object is only defined in case of an error otherwise undefined.
            console.log("Create Error :"+JSON.stringify(error));
            return;
        }
        console.log("response.id = "+response.id +" count = "+response.items.length);
        // response.items is an array of objects, the order of the request is preserved
        for (var i = 0; i < response.items.length; i++) {
            console.log("_uuid = "+response.items[i]._uuid +" ._version = "+response.items[i]._version);
        }

    }
    nokiaSharedDb.create({"_type":"Contact", "firstName":firstName, "lastName":lastName }, createCallback)
    \endcode

    The \a error is an object of type  {code: errorCode, message: "plain text" }. This is
    only defined in case of an error. The \a response object conatins the following properties :
    \list
    \o id -  The id of the request.
    \o stateNumber - The state label of the partition this write was committed in.
    \o items - An array of objects (in the same order as the request). Each item in the array has
    the following properties:
    \list
    \o _uuid - The _uuid of the newly created object
    \o _version - The _version of the newly created object
    \endlist
    \endlist

*/

int JsonDbPartition::create(const QJSValue &object,  const QJSValue &options, const QJSValue &callback)
{
    QJSValue actualOptions = options;
    QJSValue actualCallback = callback;
    if (options.isFunction()) {
        if (callback.isValid()) {
            qWarning() << "Callback should be the last parameter.";
            return -1;
        }
        actualCallback = actualOptions;
        actualOptions = QJSValue();
    }
    //#TODO ADD options
    int id = jsonDb.create(object.toVariant(), _name);
    createCallbacks.insert(id, actualCallback);
    return id;
}

/*!
    \qmlmethod int QtJsonDb::Partition::update(object updatedObject, object options, function callback)

    Update the object \a updatedObject (or list of objects) in the partition. Returns the id of this
    request. If the request fails to update an object, the whole transaction will be aborted. The
    \a options specifies how update should be handled.
    \list
    \o options.mode - Supported values: "normal", "forced".
    \list
    \o"normal" creates the document if it does not exist otherwise it enforces that the
    "_version" matches or fails - lost update prevention.
    \o"forced" ignores the existing database content.
    \endlist
    Default is "normal"
    \endlist

    The callback will be called in case of failure or success. The \a options and \a callback parameters
    are optional. If the \a callback parameter is used, it has to be the last parameter specified in the
    function call. The \a callback has the following signature.

    \code
    import QtJsonDb 1.0 as JsonDb
    function updateCallback(error, response) {
        if (error) {
            // communication error or failed to create one or more objects.
            // 'error' object is only defined in case of an error otherwise undefined.
            console.log("Update Error :"+JSON.stringify(error));
            return;
        }
        console.log("response.id = "+response.id +" count = "+response.items.length);
        // response.items is an array of objects, the order of the request is preserved
        for (var i = 0; i < response.items.length; i++) {
            console.log("_uuid = "+response.items[i]._uuid +" ._version = "+response.items[i]._version);
        }

    }
    nokiaSharedDb.update(updatedObject, updateCallback)
    \endcode

    The \a error is an object of type  {code: errorCode, message: "plain text" }. This is
    only defined in case of an error. The \a response object conatins the following properties :
    \list
    \o id -  The id of the request.
    \o stateNumber - The state label of the partition this write was committed in.
    \o items - An array of objects (in the same order as the request). Each item in the array has
    the following properties:
    \list
    \o _uuid - The _uuid of the newly created object
    \o _version - The _version of the newly created object
    \endlist
    \endlist

*/


int JsonDbPartition::update(const QJSValue &object,  const QJSValue &options, const QJSValue &callback)
{
    QJSValue actualOptions = options;
    QJSValue actualCallback = callback;
    if (options.isFunction()) {
        if (callback.isValid()) {
            qWarning() << "Callback should be the last parameter.";
            return -1;
        }
        actualCallback = actualOptions;
        actualOptions = QJSValue();
    }
    //#TODO ADD options
    int id = jsonDb.update(object.toVariant(), _name);
    updateCallbacks.insert(id, actualCallback);
    return id;
}

/*!
    \qmlmethod int QtJsonDb::Partition::remove(object objectToRemove, object options, function callback)

    Removes the \a objectToRemove (or list of objects) from the partition. It returns the id of this
    request. The \a options specifies how removal should be handled.
    \list
    \o options.mode - Supported values: "normal", which requires a an object with _uuid and
    _version set to be passed.
    \endlist

    The callback will be called in case of failure or success. The \a options and \a callback parameters
    are optional. If the \a callback parameter is used, it has to be the last parameter specified in the
    function call. The \a callback has the following signature.

    \code
    import QtJsonDb 1.0 as JsonDb
    function removeCallback(error, response) {
        if (error) {
            // communication error or failed to create one or more objects.
            // 'error' object is only defined in case of an error otherwise undefined.
            console.log("Update Error :"+JSON.stringify(error));
            return;
        }
        console.log("response.id = "+response.id +" count = "+response.items.length);
        // response.items is an array of objects, the order of the request is preserved
        for (var i = 0; i < response.items.length; i++) {
            console.log("_uuid = "+response.items[i]._uuid);
        }

    }
    nokiaSharedDb.remove({"_uuid":"xxxx-xxxx-xxxx", "_version":"1-xxxx-xxxx-xxxx"}, removeCallback)
    \endcode

    The \a error is an object of type  {code: errorCode, message: "plain text" }. This is
    only defined in case of an error. The \a response object conatins the following properties :
    \list
    \o id -  The id of the request.
    \o stateNumber - The state label of the partition this write was committed in.
    \o items - An array of objects (in the same order as the request). Each item in the array has
    the following properties:
    \list
    \o _uuid - The _uuid of the newly created object
    \endlist
    \endlist

*/

int JsonDbPartition::remove(const QJSValue &object,  const QJSValue &options, const QJSValue &callback)
{
    QJSValue actualOptions = options;
    QJSValue actualCallback = callback;
    if (options.isFunction()) {
        if (callback.isValid()) {
            qWarning() << "Callback should be the last parameter.";
            return -1;
        }
        actualCallback = actualOptions;
        actualOptions = QJSValue();
    }
    //#TODO ADD options
    int id = jsonDb.remove(object.toVariant(), _name);
    removeCallbacks.insert(id, actualCallback);
    return id;
}

/*!
    \qmlmethod object QtJsonDb::Partition::createNotification(query, parentItem)

    Create the Notification object for the specifed \a query. The \a parentItem
    decides the life time of the returned object. If the \a parentItem is null,
    the script engine will destroy the object during garbage collection.

    \code
    import QtJsonDb 1.0 as JsonDb
    function onCreateNotification(result, action, stateNumber)
    {
        console.log("create Notification : object " + result._uuid );
        console.log(result._type + result.firstName + " " + result.lastName );
    }

    Component.onCompleted: {
        var createNotification = nokiaPartition.createNotification('[?_type="Contact"]', topLevelItem);
        createNotification.notification.connect(onCreateNotification);
    }
    \endcode
    \sa QtJsonDb::Notification

*/

JsonDbNotify* JsonDbPartition::createNotification(const QString &query, QObject *parentItem)
{
    JsonDbNotify* notify = new JsonDbNotify(parentItem);
    notify->setPartition(this);
    notify->setQuery(query);
    notify->componentComplete();
    return notify;
}

/*!
    \qmlmethod object QtJsonDb::Partition::createQuery(query, limit, bindings, parentItem)

    Create the Query object with the specified \a query string and other parameters.
    Users have to call start() to start the query in this partition. The \a parentItem
    decides the life time of the returned object. If the \a parentItem
    is null, the script engine will destroy the object during garbage collection.

    \code
    import QtJsonDb 1.0 as JsonDb
    function onFinished()
    {
        var results = queryObject.takeResults();
        console.log("Results: Count + results.length );
    }

    Component.onCompleted: {
        var bindings = {'firstName':'Book'};
        queryObject = nokiaPartition.createQuery('[?_type="Contact"][?name=%firstName]', -1, bindings, topLevelItem);
        queryObject.finished.connect(onFinished);
        queryObject.exec();
    }
    \endcode
    \sa QtJsonDb::Query

*/

JsonDbQueryObject* JsonDbPartition::createQuery(const QString &query, int limit, QVariantMap bindings, QObject *parentItem)
{
    JsonDbQueryObject* queryObject = new JsonDbQueryObject(parentItem);
    queryObject->setQuery(query);
    queryObject->setBindings(bindings);
    queryObject->setLimit(limit);
    queryObject->setPartition(this);
    queryObject->componentComplete();
    return queryObject;
}

/*!
    \qmlmethod object QtJsonDb::Partition::createChangesSince(stateNumber, types, parentItem)

    Create the ChangesSince object. It will set the \a stateNumber, filter \a types parameters
    of the object. Users have to call exec() to start the changesSince query in this partition.
    The \a parentItem decides the life time of the returned object. If the \a parentItem
    is null, the script engine will destroy the object during garbage collection.

    \code
    import QtJsonDb 1.0 as JsonDb
    function onFinished()
    {
        var results = queryObject.takeResults();
        console.log("Results: Count + results.length );
    }

    Component.onCompleted: {
        changesObject = nokiaPartition.createChangesSince(10, ["Contact"], topLevelItem);
        changesObject.finished.connect(onFinished);
        changesObject.exec()

    }
    \endcode
    \sa QtJsonDb::ChangesSince

*/

JsonDbChangesSinceObject* JsonDbPartition::createChangesSince(int stateNumber, const QStringList &types, QObject *parentItem)
{
    JsonDbChangesSinceObject* changesSinceObject = new JsonDbChangesSinceObject(parentItem);
    changesSinceObject->setTypes(types);
    changesSinceObject->setStateNumber(stateNumber);
    changesSinceObject->setPartition(this);
    changesSinceObject->componentComplete();
    return changesSinceObject;
}

QDeclarativeListProperty<QObject> JsonDbPartition::childElements()
{
    return QDeclarativeListProperty<QObject>(this, childQMLElements);
}

void JsonDbPartition::updateNotification(JsonDbNotify *notify)
{
    JsonDbClient::NotifyTypes notifyActions = JsonDbClient::NotifyCreate
            | JsonDbClient::NotifyUpdate| JsonDbClient::NotifyRemove;
    notify->uuid= jsonDb.registerNotification(notifyActions, notify->query(), _name
                                              , notify, SLOT(dbNotified(QString,QtAddOn::JsonDb::JsonDbNotification))
                                              , notify, SLOT(dbNotifyReadyResponse(int,QVariant))
                                              , SLOT(dbNotifyErrorResponse(int,int,QString)));
    notifications.insert(notify->uuid, notify);
}


void JsonDbPartition::removeNotification(JsonDbNotify *notify)
{
    if (notifications.contains(notify->uuid)) {
        jsonDb.unregisterNotification(notify->uuid);
        notifications.remove(notify->uuid);
    }
}

void JsonDbPartition::call(QMap<int, QJSValue> &callbacks, int id, const QVariant &result)
{
    // Make sure that id exists in the map.
    QJSValue callback = callbacks[id];
    QJSEngine *engine = callback.engine();
    if (!engine) {
        callbacks.remove(id);
        return;
    }
    QJSValueList args;
    QVariantMap object = result.toMap();
    // object : id  , statenumber , items
    QJSValue response= engine->newObject();
    response.setProperty(JsonDbString::kStateNumberStr, object.value(JsonDbString::kStateNumberStr).toInt());
    response.setProperty(JsonDbString::kIdStr,  id);

    // response object : object { _version & _uuid } (can be a list)
    if (object.contains(QLatin1String("data"))) {
        QJSValue items = engine->toScriptValue(object.value(QLatin1String("data")));
        response.setProperty(QLatin1String("items"), items);
    } else {
        // Create an array with a single element
        QJSValue responseObject = engine->newObject();
        responseObject.setProperty(JsonDbString::kUuidStr, object.value(JsonDbString::kUuidStr).toString());
        responseObject.setProperty(JsonDbString::kVersionStr, object.value(JsonDbString::kVersionStr).toString());
        QJSValue items = engine->newArray(1);
        items.setProperty(0, responseObject);
        response.setProperty(QLatin1String("items"), items);
    }
    args << QJSValue(QJSValue::UndefinedValue) << response;
    callback.call(QJSValue(), args);
    callbacks.remove(id);
}

void JsonDbPartition::callErrorCallback(QMap<int, QJSValue> &callbacks, int id, int code, const QString &message)
{
    // Make sure that id exists in the map.
    QJSValue callback = callbacks[id];
    QJSEngine *engine = callback.engine();
    if (!engine) {
        callbacks.remove(id);
        return;
    }
    QJSValueList args;
    QVariantMap error;
    error.insert(QLatin1String("code"), code);
    error.insert(QLatin1String("message"), message);

    // object : id
    QJSValue response = engine->newObject();
    response.setProperty(JsonDbString::kStateNumberStr, -1);
    response.setProperty(JsonDbString::kIdStr,  id);
    response.setProperty(QLatin1String("items"), engine->newArray());

    args << engine->toScriptValue(QVariant(error))<< response;
    callback.call(QJSValue(), args);
    callbacks.remove(id);
}


void JsonDbPartition::dbResponse(int id, const QVariant &result)
{
    if (createCallbacks.contains(id)) {
        call(createCallbacks, id, result);
    } else if (updateCallbacks.contains(id)) {
        call(updateCallbacks, id, result);
    } else if (removeCallbacks.contains(id)) {
        call(removeCallbacks, id, result);
    }
}

void JsonDbPartition::dbErrorResponse(int id, int code, const QString &message)
{
    if (createCallbacks.contains(id)) {
        callErrorCallback(createCallbacks, id, code, message);
    } else if (removeCallbacks.contains(id)) {
        callErrorCallback(removeCallbacks, id, code, message);
    } else if (updateCallbacks.contains(id)) {
        callErrorCallback(updateCallbacks, id, code, message);
    }
}
