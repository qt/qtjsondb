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

#include <QDebug>
#include <QJSEngine>

#include "jsondb-component.h"
#include "private/jsondb-strings_p.h"
#include "jsondb-client.h"

JsonDbNotificationComponent::JsonDbNotificationComponent(JsonDbComponent *repo)
    : QObject(repo)
    , mId(-1)
{
}

JsonDbNotificationComponent::~JsonDbNotificationComponent()
{
    remove();
}

void JsonDbNotificationComponent::remove()
{
    if (!mUuid.isEmpty())
        emit removed(mUuid); // the JsonDbComponent connects to this and does the real work.
    else
        emit removed(mId);
}

JsonDbNotificationHandle::JsonDbNotificationHandle(JsonDbNotificationComponent *notification)
    : QObject(notification)
    , mNotification(notification)
{
}

JsonDbNotificationHandle::~JsonDbNotificationHandle()
{
}

QString JsonDbNotificationHandle::uuid() const
{
    if (mNotification.isNull())
        return QString();
    else
        return mNotification->mUuid;
}

void JsonDbNotificationHandle::remove()
{
    if (!mNotification.isNull())
        mNotification->remove();
}

/*!
    \qmlclass JsonDbComponent
    \inqmlmodule QtAddOn.JsonDb
    \since 1.x

    The JsonDb element allows you find, create, update, or remove objects from JsonDb.
    Most of the functions take script objects as parameters.
    Those can have different values "query", "limit" and "offset"

    Most of the functions take optional success and error callback functions. Those script
    functions are called in case of an error (or succes)
*/

JsonDbComponent::JsonDbComponent(QObject *parent)
    : QObject(parent)
    , mDebugOutput(false)
    , mJsonDb(new JsonDbClient(this))
{
    connect(mJsonDb, SIGNAL(response(int,QVariant)),
            this, SLOT(jsonDbResponse(int,QVariant)),
            Qt::QueuedConnection);
    connect(mJsonDb, SIGNAL(error(int,int,QString)),
            this, SLOT(jsonDbErrorResponse(int,int,QString)),
            Qt::QueuedConnection);
    connect(mJsonDb, SIGNAL(notified(QString,QVariant,QString)),
            this, SLOT(jsonDbNotified(QString,QVariant,QString)),
            Qt::QueuedConnection);
}

JsonDbComponent::~JsonDbComponent()
{
}

/*!
    \qmlsignal QtAddOn.JsonDb::JsonDb::onResponse()

    This handler is called when the database responds to a request.
*/

/*!
    \qmlsignal QtAddOn.JsonDb::JsonDb::onError()

    This handler is called when there is an error in a database request.
*/

/*!
  \qmlmethod QtAddOn.JsonDb::JsonDb::create(object)

  Creates the \a object in the database.
  The \a object must not have a "_uuid" field.
  On success, emits the response signal.
  Returns the request uuid.
*/
int JsonDbComponent::create(const QJSValue &object,
                            const QJSValue &successCallback,
                            const QJSValue &errorCallback)
{
    if (mDebugOutput)
        qDebug() << "[JSONDB] create:"<<object.toString();

    return addRequestInfo(mJsonDb->create(object.toVariant()), JsonDbComponent::Create, object, successCallback, errorCallback);
}

/*!
  \qmlmethod QtAddOn.JsonDb::JsonDb::update(object queryObject, object successFunction, object errorFunction)

  Updates the database to match the new object.
  The \a object must have a valid "uuid" field.
  On success, emits the response signal.
  Returns the request uuid.
 */
int JsonDbComponent::update(const QJSValue &object,
                             const QJSValue &successCallback,
                             const QJSValue &errorCallback)
{
    return addRequestInfo(mJsonDb->update(object.toVariant()), JsonDbComponent::Update, object, successCallback, errorCallback);
}

/*!
  \qmlmethod QtAddOn.JsonDb::JsonDb::remove(object queryObject, object successFunction, object errorFunction)

  Removes the object from the database.
  The \a object must have a valid "uuid" field.
  On success, emits the response signal.
  Returns the request uuid.
 */
int JsonDbComponent::remove(const QJSValue &object,
                            const QJSValue &successCallback,
                            const QJSValue &errorCallback)
{
    if (mDebugOutput)
        qDebug() << "[JSONDB] remove:"<<object.toString();

    return addRequestInfo(mJsonDb->remove(object.toVariant()), JsonDbComponent::Remove, object, successCallback, errorCallback);
}

/*!
  \qmlmethod QtAddOn.JsonDb::JsonDb::find(string query, int limit, int offset)

  Takes a JsonQuery string, a limit, and an offset, and issues a query to the database.
  Returns the request uuid.
 */
int JsonDbComponent::find(const QJSValue &object,
                          const QJSValue &successCallback,
                          const QJSValue &errorCallback)
{
    if (mDebugOutput)
        qDebug() << "[JSONDB] find:"<<object.toString();

    return addRequestInfo(mJsonDb->find(object.toVariant()), JsonDbComponent::Find, object, successCallback, errorCallback);
}

/*!
  \qmlmethod QtAddOn.JsonDb::JsonDb::query(string query)

  Takes a JsonQuery string and issues a query to the database.
  This function is a simple find function with no limit.
*/
int JsonDbComponent::query(const QJSValue &object,
                           const QJSValue &successCallback,
                           const QJSValue &errorCallback)
{
    if (object.isString()) {
        QJSValue request = object.engine()->newObject();
        request.setProperty(JsonDbString::kQueryStr, object);
        request.setProperty(JsonDbString::kLimitStr, -1);
        request.setProperty(JsonDbString::kOffsetStr, 0);
        return find(request, successCallback, errorCallback);
    }

    return find(object, successCallback, errorCallback);
}

/*!
  \qmlmethod QtAddOn.JsonDb::JsonDb::notification(object query, object actions, object callbackFunction, object errorFunction)

  Takes a JsonQuery string and creates a notification object from it.
  The callbackFunction will called every time the notification is triggered.
  The errorFunction is called if the creation of the notification fails.
*/
QJSValue JsonDbComponent::notification(const QJSValue &object,
                                           const QJSValue &actions,
                                           const QJSValue &callback,
                                           QJSValue errorCallback)
{
    // -- refuse to create notification without callback
    if (!callback.isFunction()) {
        qWarning() << "Refusing to create notification without callback.";
        if (errorCallback.isFunction()) {
            QJSValueList args;
            errorCallback.call(QJSValue(), args);
        }
        return QJSValue();
    }

    if (mDebugOutput)
        qDebug() << "[JSONDB] notification:"<<object.toString();

    int id;

    if (object.isString()) {
        QJSValue request = object.engine()->newObject();
        request.setProperty(JsonDbString::kQueryStr, object);
        request.setProperty(JsonDbString::kTypeStr,
                            JsonDbString::kNotificationTypeStr);
        request.setProperty(JsonDbString::kActionsStr, actions);

        id = addRequestInfo(mJsonDb->create(request.toVariant()), JsonDbComponent::Notification, object, QJSValue(QJSValue::UndefinedValue), errorCallback);

    } else {
        id = addRequestInfo(mJsonDb->create(object.toVariant()), JsonDbComponent::Notification, object, QJSValue(QJSValue::UndefinedValue), errorCallback);
    }

    if (id <= 0)
        return QJSValue();

    // -- create notification object
    JsonDbNotificationComponent* notification = new JsonDbNotificationComponent(this);
    notification->mId = id;
    notification->mCallback = callback;
    mPendingNotifications.insert(id, notification);

    connect(notification, SIGNAL(removed(int)),
            this, SLOT(notificationRemoved(int)),
            Qt::QueuedConnection);
    connect(notification, SIGNAL(removed(QString)),
            this, SLOT(notificationRemoved(QString)),
            Qt::QueuedConnection);

    JsonDbNotificationHandle *handle = new JsonDbNotificationHandle(notification);
    return callback.engine()->newQObject(handle);
}


void JsonDbComponent::jsonDbResponse(int id, const QVariant &result)
{
    if (mRequests.contains(id)) {
        JsonDbComponent::RequestInfo &info = mRequests[id];

        QJSEngine *engine = info.object.engine();
        QJSValue scriptResult = engine->toScriptValue(result);
        if (!scriptResult.property(JsonDbString::kUuidStr).isUndefined())
            info.object.setProperty(JsonDbString::kUuidStr, scriptResult.property(JsonDbString::kUuidStr));
        if (!scriptResult.property(JsonDbString::kVersionStr).isUndefined())
            info.object.setProperty(JsonDbString::kVersionStr, scriptResult.property(JsonDbString::kVersionStr));
        if (!scriptResult.property(JsonDbString::kOwnerStr).isUndefined())
            info.object.setProperty(JsonDbString::kOwnerStr, scriptResult.property(JsonDbString::kOwnerStr));

        emit response(scriptResult, id);

        if (mDebugOutput)
            qDebug() << "[JSONDB] response:" << scriptResult.toString();

        // -- creating the notification object was successful
        if (info.type == JsonDbComponent::Notification) {
            if (mDebugOutput)
                qDebug() << "successful created notification with" << scriptResult.property(JsonDbString::kUuidStr).toString();

            // -- finish the notification with the new uuid
            JsonDbNotificationComponent* notification = mPendingNotifications.take(id);
            if (notification) {
                // - if the removal was requested before the actual success:
                // note: at this state the notification object is already deleted
                if (mKilledNotifications.contains(id)) {
                    if (mDebugOutput)
                        qDebug() << "[JSONDB] kill notification again";
                    notificationRemoved(scriptResult.property(JsonDbString::kUuidStr).toString()); // remove it again
                    mKilledNotifications.remove(id);

                } else {
                    notification->mId = -1;
                    notification->mUuid = scriptResult.property(JsonDbString::kUuidStr).toString();
                    if (mDebugOutput)
                        qDebug() << "finish notification"<<notification->mUuid;
                    mNotifications.insert(notification->mUuid, notification);
                }

            } else {
                qWarning() << "Got response for notification that is not pending";
            }
        }

        if (mDebugOutput)
            qDebug() << "[JSONDB] response:" << scriptResult.toString();

        // -- call the success callback function
        if (info.successCallback.isFunction()) {
            QJSValueList args;
            args << scriptResult << info.successCallback.engine()->toScriptValue(id);
            info.successCallback.call(QJSValue(), args);
        }

        mRequests.remove(id);
    }
}
void JsonDbComponent::jsonDbErrorResponse(int id, int code, const QString& message)
{
    Q_UNUSED(code);

    if (mRequests.contains(id)) {
        JsonDbComponent::RequestInfo &info = mRequests[id];

        emit error(message, id);

        if (mDebugOutput)
            qDebug() << "[JSONDB] error:" << message;

        if (info.type == JsonDbComponent::Notification) {
            // -- creating the notification object was successful
            qWarning() << "failed to create notification";
            mPendingNotifications.remove(id);
        }

        // -- call the error callback function
        if (info.errorCallback.isFunction()) {
            QJSValueList args;
            args << info.errorCallback.engine()->toScriptValue(message);
            args << info.errorCallback.engine()->toScriptValue(code);
            args << info.errorCallback.engine()->toScriptValue(id);
            info.errorCallback.call(QJSValue(), args);
        }

        mRequests.remove(id);
    }
}

void JsonDbComponent::jsonDbNotified(const QString& notify_uuid, const QVariant& object, const QString& action)
{
    JsonDbNotificationComponent* notification = mNotifications.value(notify_uuid);
    if (notification) {
        QJSValueList args;
        args << notification->mCallback.engine()->toScriptValue(object);
        args << notification->mCallback.engine()->toScriptValue(action);
        notification->mCallback.call(QJSValue(), args);

        if (mDebugOutput)
            qDebug() << "[JSONDB] notification received, Id:" << notify_uuid  << "Action : " << action;
    }
}

void JsonDbComponent::notificationRemoved(int id)
{
    if (mDebugOutput)
        qDebug() << "[JSONDB2] pending notification removed";

    // ok, we have a pending notification
    if (mPendingNotifications.contains(id))
        mKilledNotifications.insert(id);
}

void JsonDbComponent::notificationRemoved(QString uuid)
{
    if (mDebugOutput)
        qDebug() << "[JSONDB2] notification removed";

    // remove a finished notification
    QVariantMap arguments;
    arguments.insert(JsonDbString::kUuidStr, uuid);
    mJsonDb->remove(arguments);

    mNotifications.remove(uuid);
}

int JsonDbComponent::addRequestInfo(int id, RequestType type, const QJSValue &object, const QJSValue &successCallback, const QJSValue &errorCallback)
{
    if (id < 0) {
        qWarning() << "Missing database connection";
        return id;
    }
    if (!successCallback.isUndefined() && !successCallback.isFunction())
        qWarning() << "Success callback parameter "<<successCallback.toString()<<"is not a function.";
    if (!errorCallback.isUndefined() && !errorCallback.isFunction())
        qWarning() << "Error callback parameter "<<errorCallback.toString()<<"is not a function.";

    JsonDbComponent::RequestInfo &info = mRequests[id];
    info.type = type;
    info.object = object;
    info.successCallback = successCallback;
    info.errorCallback = errorCallback;

    return id;
}
