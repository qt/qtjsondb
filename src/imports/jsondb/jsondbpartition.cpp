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
#include <private/qjsondbmodelutils_p.h>
#include <QJsonDbCreateRequest>

#include "jsondbpartition.h"
#include "jsondatabase.h"
#include "jsondbnotification.h"
#include "plugin.h"
#include "jsondbqueryobject.h"
#include <QJsonDbCreateRequest>
#include <QJsonDbWatcher>
#include <private/qjsondbstrings_p.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE_JSONDB
/*!
    \qmltype Partition
    \instantiates JsonDbPartition
    \inqmlmodule QtJsonDb 1.0
    \since 1.0
    \brief Find, create, update, or remove objects in a partition.

    The partition object allows you find, create, update, or remove objects from JsonDb.
    It also allows to create query, notification  and changesSince objects, query. A partition
    object is identified by its name.

    Most of the methods take script objects as parameters. The last parameter can be
    a callback function.
*/

JsonDbPartition::JsonDbPartition(const QString &partitionName, QObject *parent)
    :QObject(parent)
    ,_name(partitionName)
    ,_state(None)
{
    init();
}


JsonDbPartition::~JsonDbPartition()
{
    if (partitionWatcher) {
        JsonDatabase::sharedConnection().removeWatcher(partitionWatcher);
        delete partitionWatcher;
    }
}

void JsonDbPartition::init()
{
    QString query;
    if (_name.isEmpty())
        query= QLatin1String("[?_type=\"Partition\"][?default=true]");
    else
        query = QStringLiteral("[?_type=\"Partition\"][?name=\"%1\"]").arg(_name);

    // Create a watcher to watch changes in partition state
    partitionWatcher = new QJsonDbWatcher();
    partitionWatcher->setQuery(query);
    partitionWatcher->setWatchedActions(QJsonDbWatcher::Created | QJsonDbWatcher::Removed | QJsonDbWatcher::Updated);
    partitionWatcher->setPartition(QStringLiteral("Ephemeral"));
    QObject::connect(partitionWatcher, SIGNAL(notificationsAvailable(int)),
                     this, SLOT(notificationsAvailable()));
    QObject::connect(partitionWatcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
                     this, SLOT(notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    JsonDatabase::sharedConnection().addWatcher(partitionWatcher);

    // Create a query to ephemeral partition to find out the state (&name) of the partition
    QJsonDbReadRequest *request = new QJsonDbReadRequest;
    request->setQuery(query);
    request->setPartition(QStringLiteral("Ephemeral"));
    connect(request, SIGNAL(finished()), this, SLOT(partitionQueryFinished()));
    connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(partitionQueryError()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            request, SLOT(deleteLater()));
    JsonDatabase::sharedConnection().send(request);
}


/*!
    \qmlproperty State QtJsonDb1::Partition::state
    The current state of the Partition.
    \list
    \li State.None - The partition object is not initialized
    \li State.Online - Partition is online and working
    \li State.Offline - Partition is offline and can not be accessed
    \li State.Error - No partition of given name was found
    \endlist
*/

/*!
    \qmlproperty string QtJsonDb1::Partition::name
     Holds the human readable name of the partition.
*/

QString JsonDbPartition::name() const
{
    return _name;
}

void JsonDbPartition::setName(const QString &partitionName)
{
    if (partitionName != _name) {
        _state = None;
        emit stateChanged(_state);

        _name = partitionName;
        if (partitionWatcher) {
            JsonDatabase::sharedConnection().removeWatcher(partitionWatcher);
            delete partitionWatcher;
            partitionWatcher = 0;
        }
        init();
        emit nameChanged(partitionName);
    }
}

namespace {
static QVariant qjsvalue_to_qvariant(const QJSValue &value)
{
    if (value.isQObject()) {
        // We need the QVariantMap & not the QObject wrapper
        return qjsvalue_cast<QVariantMap>(value);
    } else {
        // Converts to either a QVariantList or a QVariantMap
        return value.toVariant();
    }
}
}

QJSValue qjsonobject_list_to_qjsvalue(const QList<QJsonObject> &list)
{
    int count = list.count();
    QJSValue resultList = g_declEngine->newArray(count);
    for (int i = 0; i < count; i++) {
        resultList.setProperty(i, g_declEngine->toScriptValue(list[i]));
    }
    return resultList;
}

/*!
    \qmlmethod int QtJsonDb1::Partition::create(object newObject, object options, function callback)

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
    myPartition.create({"_type":"Contact", "firstName":firstName, "lastName":lastName }, createCallback)
    \endcode

    The \a error is an object of type  {code: errorCode, message: "plain text" }. This is
    only defined in case of an error. The \a response object conatins the following properties :
    \list
    \li id -  The id of the request.
    \li stateNumber - The state label of the partition this write was committed in.
    \li items - An array of objects (in the same order as the request). Each item in the array has
    the following properties:
    \list
    \li _uuid - The _uuid of the newly created object
    \li _version - The _version of the newly created object
    \endlist
    \endlist

*/

int JsonDbPartition::create(const QJSValue &object,  const QJSValue &options, const QJSValue &callback)
{
    QJSValue actualOptions = options;
    QJSValue actualCallback = callback;
    if (options.isCallable()) {
        if (!callback.isUndefined()) {
            qWarning() << "Callback should be the last parameter.";
            return -1;
        }
        actualCallback = actualOptions;
        actualOptions = QJSValue(QJSValue::UndefinedValue);
    }
    //#TODO ADD options
    QVariant obj = qjsvalue_to_qvariant(object);
    QJsonDbWriteRequest *request(0);
    if (obj.type() == QVariant::List) {
        request = new QJsonDbCreateRequest(qvariantlist_to_qjsonobject_list(obj.toList()));
    } else {
        request = new QJsonDbCreateRequest(QJsonObject::fromVariantMap(obj.toMap()));
    }
    request->setPartition(_name);
    connect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
    connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            request, SLOT(deleteLater()));
    JsonDatabase::sharedConnection().send(request);
    writeCallbacks.insert(request, actualCallback);
    return request->property("requestId").toInt();
}

/*!
    \qmlmethod int QtJsonDb1::Partition::update(object updatedObject, object options, function callback)

    Update the object \a updatedObject (or list of objects) in the partition. Returns the id of this
    request. If the request fails to update an object, the whole transaction will be aborted. The
    \a options is an object that specifies how the update should be handled.
    \list
    \li options.mode - Supported values: \c Partition.RejectStale, \c Partition.Replace
    \list
    \li \c Partition.RejectStale creates the document if it does not exist otherwise it enforces that the
    "_version" matches or fails - lost update prevention.
    \li \c Partition.Replace ignores the existing database content.
    \endlist
    Default is \c Partition.RejectStale
    \endlist

    The callback will be called in case of failure or success. The \a callback has the following signature.

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
    myPartition.update(updatedObject, updateCallback)
    \endcode

    The \a error is an object of type  {code: errorCode, message: "plain text" }. This is
    only defined in case of an error. The \a response object conatins the following properties :
    \list
    \li id -  The id of the request.
    \li stateNumber - The state label of the partition this write was committed in.
    \li items - An array of objects (in the same order as the request). Each item in the array has
    the following properties:
    \list
    \li _uuid - The _uuid of the newly created object
    \li _version - The _version of the newly created object
    \endlist
    \endlist

*/
/*!
    \qmlmethod int QtJsonDb1::Partition::update(object updatedObject, function callback)

    Update the object without specifying the update options.
*/
/*!
    \qmlmethod int QtJsonDb1::Partition::update(object updatedObject)

    Update the object without specifying the update options or callback function.
*/

int JsonDbPartition::update(const QJSValue &object,  const QJSValue &options, const QJSValue &callback)
{
    QJSValue actualOptions = options;
    QJSValue actualCallback = callback;
    if (options.isCallable()) {
        if (!callback.isUndefined()) {
            qWarning() << "Callback should be the last parameter.";
            return -1;
        }
        actualCallback = actualOptions;
        actualOptions = QJSValue(QJSValue::UndefinedValue);
    }

    QVariantMap opts = qjsvalue_to_qvariant(actualOptions).toMap();
    QJsonDbWriteRequest::ConflictResolutionMode conflictMode =
            static_cast<QJsonDbWriteRequest::ConflictResolutionMode>(opts.value(QStringLiteral("mode")).toInt());

    QVariant obj = qjsvalue_to_qvariant(object);
    QJsonDbWriteRequest *request(0);
    if (obj.type() == QVariant::List) {
        request = new QJsonDbUpdateRequest(qvariantlist_to_qjsonobject_list(obj.toList()));
    } else {
        request = new QJsonDbUpdateRequest(QJsonObject::fromVariantMap(obj.toMap()));
    }
    request->setPartition(_name);
    request->setConflictResolutionMode(conflictMode);
    connect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
    connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            request, SLOT(deleteLater()));
    JsonDatabase::sharedConnection().send(request);
    writeCallbacks.insert(request, actualCallback);
    return request->property("requestId").toInt();
}

/*!
    \qmlmethod int QtJsonDb1::Partition::remove(object objectToRemove, object options, function callback)

    Removes the \a objectToRemove (or list of objects) from the partition. It returns the id of this
    request.
    \a options is an object that specifies how the remove should be handled.
    \list
    \li options.mode - Supported values: \c Partition.RejectStale, \c Partition.Replace
    \list
    \li \c Partition.RejectStale removes the document if it exists and the _version value matches that of the object
    in the database. Otherwise, if the _version doesn't match it will be rejected as a stale update.
    \li \c Partition.Replace ignores the existing database content.
    \endlist
    Default is \c Partition.RejectStale
    \endlist

    The callback will be called in case of failure or success. The \a callback has the following signature.

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
    myPartition.remove({"_uuid":"xxxx-xxxx-xxxx", "_version":"1-xxxx-xxxx-xxxx"}, removeCallback)
    \endcode

    The \a error is an object of type  {code: errorCode, message: "plain text" }. This is
    only defined in case of an error. The \a response object conatins the following properties :
    \list
    \li id -  The id of the request.
    \li stateNumber - The state label of the partition this write was committed in.
    \li items - An array of objects (in the same order as the request). Each item in the array has
    the following properties:
    \list
    \li _uuid - The _uuid of the newly created object
    \endlist
    \endlist

*/
/*!
    \qmlmethod int QtJsonDb1::Partition::remove(object updatedObject, function callback)

    Remove the object without specifying the update options.
*/

/*!
    \qmlmethod int QtJsonDb1::Partition::remove(object updatedObject)

    Remove the object without specifying the update options or callback function.
*/

int JsonDbPartition::remove(const QJSValue &object,  const QJSValue &options, const QJSValue &callback)
{
    QJSValue actualOptions = options;
    QJSValue actualCallback = callback;
    if (options.isCallable()) {
        if (!callback.isUndefined()) {
            qWarning() << "Callback should be the last parameter.";
            return -1;
        }
        actualCallback = actualOptions;
        actualOptions = QJSValue(QJSValue::UndefinedValue);
    }

    QVariantMap opts = qjsvalue_to_qvariant(actualOptions).toMap();
    QJsonDbWriteRequest::ConflictResolutionMode conflictMode =
            static_cast<QJsonDbWriteRequest::ConflictResolutionMode>(opts.value(QStringLiteral("mode")).toInt());

    QVariant obj = qjsvalue_to_qvariant(object);
    QJsonDbWriteRequest *request(0);
    if (obj.type() == QVariant::List) {
        request = new QJsonDbRemoveRequest(qvariantlist_to_qjsonobject_list(obj.toList()));
    } else {
        request = new QJsonDbRemoveRequest(QJsonObject::fromVariantMap(obj.toMap()));
    }
    request->setPartition(_name);
    request->setConflictResolutionMode(conflictMode);
    connect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
    connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            request, SLOT(deleteLater()));
    JsonDatabase::sharedConnection().send(request);
    writeCallbacks.insert(request, actualCallback);
    return request->property("requestId").toInt();
}

/*!
    \qmlmethod QtJsonDb1::Partition::find(string query, object options, function callback)

    Finds the objects matching the \a query string in the partition. The \a options specifies
    how query should be handled. The \a query should be specified in JsonQuery format.
    \a options support the following properties.
    \list
    \li options.limit - Maximum number of items to be fetched
    \li options.bindings - Holds the bindings object for the placeholders used in the query string. Note that
    the placeholder marker '%' should not be included as part of the keys.
    \endlist

    The callback will be called in case of failure or success. It has the following signature
    \code
    function findCallback(error, response) {
        if (error) {
            // 'error' object is only defined in case of an error otherwise undefined.
            console.log("Update Error :"+JSON.stringify(error));
            return;
        }
        console.log("response.id = "+response.id +" count = "+response.items.length);
        // response.items is an array of objects
        for (var i = 0; i < response.items.length; i++) {
            console.log("_uuid = "+response.items[i]._uuid);
        }
    }
    \endcode

    The \a error is an object of type  {code: errorCode, message: "plain text" }. This is
    only defined in case of an error. The \a response object contains the following properties :
    \list
    \li id -  The id of the request.
    \li stateNumber - The state label of the partition this write was committed in.
    \li items - An array of objects
    \endlist

    \sa QtJsonDb1::Query

*/

int JsonDbPartition::find(const QString &query, const QJSValue &options, const QJSValue &callback)
{
    QJSValue actualOptions = options;
    QJSValue actualCallback = callback;
    if (options.isCallable()) {
        if (!callback.isUndefined()) {
            qWarning() << "Callback should be the last parameter.";
            return -1;
        }
        actualCallback = actualOptions;
        actualOptions = QJSValue(QJSValue::UndefinedValue);
    }
    JsonDbQueryObject *newQuery = new JsonDbQueryObject();
    newQuery->setQuery(query);
    if (!actualOptions.isUndefined()) {
        QVariantMap opt = actualOptions.toVariant().toMap();
        if (opt.contains(QLatin1String("limit")))
            newQuery->setLimit(opt.value(QLatin1String("limit")).toInt());
        if (opt.contains(QLatin1String("bindings")))
            newQuery->setBindings(opt.value(QLatin1String("bindings")).toMap());
    }
    newQuery->setPartition(this);
    connect(newQuery, SIGNAL(finished()), this, SLOT(queryFinished()));
    connect(newQuery, SIGNAL(statusChanged(JsonDbQueryObject::Status)), this, SLOT(queryStatusChanged()));
    findCallbacks.insert(newQuery, actualCallback);
    newQuery->componentComplete();
    int id = newQuery->start();
    findIds.insert(newQuery, id);
    return id;
}

/*!
    \qmlmethod object QtJsonDb1::Partition::createNotification(query)

    Create the Notification object for the specifed \a query.The script engine
    decides the life time of the returned object. The returned object can be saved
    in a 'property var' until it is required.

    \code
    import QtJsonDb 1.0 as JsonDb
    property var createNotification;
    function onCreateNotification(result, action, stateNumber)
    {
        console.log("create Notification : object " + result._uuid );
        console.log(result._type + result.firstName + " " + result.lastName );
    }

    Component.onCompleted: {
        createNotification = nokiaPartition.createNotification('[?_type="Contact"]');
        createNotification.notification.connect(onCreateNotification);
    }
    \endcode
    \sa QtJsonDb1::Notification

*/

JsonDbNotify* JsonDbPartition::createNotification(const QString &query)
{
    JsonDbNotify* notify = new JsonDbNotify();
    notify->setPartition(this);
    notify->setQuery(query);
    notify->componentComplete();
    QQmlEngine::setObjectOwnership(notify, QQmlEngine::JavaScriptOwnership);
    return notify;
}

/*!
    \qmlmethod object QtJsonDb1::Partition::createQuery(query, limit, bindings)

    Create the Query object with the specified \a query string and other parameters.
    Users have to call start() to start the query in this partition. The script engine
    decides the life time of the returned object. The returned object can be saved
    in a 'property var' until it is required.

    \code
    import QtJsonDb 1.0 as JsonDb
    property var queryObject;
    function onFinished()
    {
        var results = queryObject.takeResults();
        console.log("Results: Count" + results.length );
    }

    Component.onCompleted: {
        var bindings = {'firstName':'Book'};
        queryObject = nokiaPartition.createQuery('[?_type="Contact"][?name=%firstName]', -1, bindings);
        queryObject.finished.connect(onFinished);
        queryObject.start();
    }
    \endcode
    \sa QtJsonDb1::Query

*/

JsonDbQueryObject* JsonDbPartition::createQuery(const QString &query, int limit, QVariantMap bindings)
{
    JsonDbQueryObject* queryObject = new JsonDbQueryObject();
    queryObject->setQuery(query);
    queryObject->setBindings(bindings);
    queryObject->setLimit(limit);
    queryObject->setPartition(this);
    queryObject->componentComplete();
    QQmlEngine::setObjectOwnership(queryObject, QQmlEngine::JavaScriptOwnership);
    return queryObject;
}

QQmlListProperty<QObject> JsonDbPartition::childElements()
{
    return QQmlListProperty<QObject>(this, childQMLElements);
}

void JsonDbPartition::updateNotification(JsonDbNotify *notify)
{
    JsonDatabase::sharedConnection().addWatcher(notify->watcher);
    watchers.append(notify->watcher);
}

void JsonDbPartition::removeNotification(JsonDbNotify *notify)
{
    if (watchers.contains(notify->watcher)) {
        JsonDatabase::sharedConnection().removeWatcher(notify->watcher);
        watchers.removeAll(notify->watcher);
    }
}

void JsonDbPartition::call(QMap<QJsonDbWriteRequest*, QJSValue> &callbacks, QJsonDbWriteRequest *request)
{
    QJSValue callback = callbacks[request];
    QJSEngine *engine = callback.engine();
    if (!engine) {
        callbacks.remove(request);
        return;
    }
    QList<QJsonObject> objects = request->takeResults();
    QJSValueList args;
    // object : id  , statenumber , items
    QJSValue response= engine->newObject();
    response.setProperty(JsonDbStrings::Protocol::stateNumber(), request->stateNumber());
    response.setProperty(JsonDbStrings::Protocol::requestId(), request->property("requestId").toInt());
    response.setProperty(QLatin1String("items"), qjsonobject_list_to_qjsvalue(objects));
    args << QJSValue(QJSValue::UndefinedValue) << response;
    callback.call(args);
    callbacks.remove(request);
}

void JsonDbPartition::callErrorCallback(QMap<QJsonDbWriteRequest*, QJSValue> &callbacks, QJsonDbWriteRequest *request,
                                        QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    QJSValue callback = callbacks[request];
    QJSEngine *engine = callback.engine();
    if (!engine) {
        callbacks.remove(request);
        return;
    }
    QJSValueList args;
    QVariantMap error;
    error.insert(QLatin1String("code"), code);
    error.insert(QLatin1String("message"), message);

    // object : id
    QJSValue response = engine->newObject();
    response.setProperty(JsonDbStrings::Protocol::stateNumber(), -1);
    response.setProperty(JsonDbStrings::Protocol::requestId(), request->property("requestId").toInt());
    response.setProperty(QLatin1String("items"), engine->newArray());

    args << engine->toScriptValue(QVariant(error))<< response;
    callback.call(args);
    callbacks.remove(request);
}

void JsonDbPartition::queryFinished()
{
    JsonDbQueryObject *object = qobject_cast<JsonDbQueryObject*>(sender());
    if (object) {
        int id = findIds.value(object);
        QJSValue callback = findCallbacks.value(object);
        QJSEngine *engine = callback.engine();
        if (engine && callback.isCallable()) {
            QJSValueList args;
            // object : id  , statenumber , items
            QJSValue response= engine->newObject();
            response.setProperty(JsonDbStrings::Protocol::stateNumber(), object->stateNumber());
            response.setProperty(JsonDbStrings::Protocol::requestId(),  id);
            response.setProperty(QLatin1String("items"), object->takeResults());
            args << QJSValue(QJSValue::UndefinedValue) << response;
            callback.call(args);
        }
        findIds.remove(object);
        findCallbacks.remove(object);
        object->deleteLater();
    }
}

void JsonDbPartition::queryStatusChanged()
{
    JsonDbQueryObject *object = qobject_cast<JsonDbQueryObject*>(sender());
    if (object && object->status() == JsonDbQueryObject::Error) {
        int id = findIds.value(object);
        QJSValue callback = findCallbacks.value(object);
        QJSEngine *engine = callback.engine();
        if (engine && callback.isCallable()) {
            QJSValueList args;

            QJSValue response = engine->newObject();
            response.setProperty(JsonDbStrings::Protocol::stateNumber(), -1);
            response.setProperty(JsonDbStrings::Protocol::requestId(),  id);
            response.setProperty(QLatin1String("items"), engine->newArray());

            args << engine->toScriptValue(object->error())<< response;
            callback.call(args);
        }
        findIds.remove(object);
        findCallbacks.remove(object);
        object->deleteLater();
    }

}
void JsonDbPartition::requestFinished()
{
    QJsonDbWriteRequest *request = qobject_cast<QJsonDbWriteRequest *>(sender());
    if (writeCallbacks.contains(request)) {
        call(writeCallbacks, request);
    }
}

void JsonDbPartition::requestError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    QJsonDbWriteRequest *request = qobject_cast<QJsonDbWriteRequest *>(sender());
    if (writeCallbacks.contains(request)) {
        callErrorCallback(writeCallbacks, request, code, message);
    }

}

void JsonDbPartition::partitionQueryFinished()
{
    QJsonDbReadRequest *request = qobject_cast<QJsonDbReadRequest *>(sender());
    if (request) {
        QList<QJsonObject> objects = request->takeResults();
        int count = objects.count();
        if (count) {
            QString name = objects[0].value(QStringLiteral("name")).toString();
            // Skip this  if name has been changed already
            if (!_name.isEmpty() && name != _name)
                return;
            State state = objects[0].value(QStringLiteral("available")).toBool() ? Online : Offline;
            if (state != _state) {
                _state = state;
                emit stateChanged(_state);
            }
            if (_name.isEmpty()) {
                _name = name;
                emit nameChanged(_name);
            }
        }
    }
}

void JsonDbPartition::partitionQueryError()
{
    qWarning() << "Partition query error";
    _state = Error;
    emit stateChanged(_state);
}


void JsonDbPartition::notificationsAvailable()
{
    QJsonDbWatcher *watcher = qobject_cast<QJsonDbWatcher *>(sender());
    if (!watcher)
        return;
    QList<QJsonDbNotification> list = watcher->takeNotifications();
    for (int i = 0; i < list.count(); i++) {
        const QJsonDbNotification & notification = list[i];
        QJsonObject object = notification.object();
        QJsonDbWatcher::Action action = notification.action();
        State state;
        if (action == QJsonDbWatcher::Removed)
            state = Offline;
        else
            state = object.value(QStringLiteral("available")).toBool() ? Online : Offline;
        if (state != _state) {
            _state = state;
            emit stateChanged(_state);
        }
    }
}

void JsonDbPartition::notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message)
{
    qWarning() << QString("JsonDbPartition Notification error: %1 %2").arg(code).arg(message);
    _state = Error;
    emit stateChanged(_state);
}

#include "moc_jsondbpartition.cpp"
QT_END_NAMESPACE_JSONDB
