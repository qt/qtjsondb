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
#include "jsondbnotification.h"
#include "jsondbpartition.h"
#include "private/jsondb-strings_p.h"
#include "plugin.h"
#include <qdebug.h>

QT_USE_NAMESPACE_JSONDB

/*!
    \qmlclass Notification
    \inqmlmodule QtJsonDb
    \since 1.x

    This allows to register for different notifications that matches a query in a Partition.
    Users can connect to onNotification signal which is fired for objects matching the query.
    The notifications can be enabled/disabled by setting the enabled property.

    \qml
    JsonDb.Notification {
        partition:nokiaPartition
        query: '[?_type="Contact"]'
        onNotification: {
            switch (action) {
            case JsonDb.Notification.Create :
                console.log("{_uuid :" + result._uuid + "} created");
                break;
            case JsonDb.Notification.Update :
                console.log("{_uuid :" + result._uuid + "} was updated");
                break;
            }
        }
     }
    \endqml

    You can also create notification as a child item of JsonDb.Partition.In this case the partiton
    property of the notification will be set to this parent JsonDb.Partition.
    \qml
    JsonDb.Partition {
        name: "com.nokia.shared"
        JsonDb.Notification {
            query: '[?_type="MyContacts"]'
            onNotification: {
                switch (action) {
                case JsonDb.Notification.Create :
                    console.log("{_uuid :" + result._uuid + "} created");
                    break;
                case JsonDb.Notification.Update :
                    console.log("{_uuid :" + result._uuid + "} was updated");
                    break;
                case JsonDb.Notification.Remove :
                    console.log("{_uuid :" + result._uuid + "} was removed");
                    break;
                }
            }
            onStatusChanged: {
                if (status === JsonDb.Notification.Error) {
                    console.log("Notification Error " + JSON.stringify(error));
                }
            }
        }
    }
    \endqml

*/

JsonDbNotify::JsonDbNotify(QObject *parent)
    :QObject(parent)
    , completed(false)
    , partitionObject(0)
    , defaultPartitionObject(0)
    , errorCode(0)
    , objectStatus(Null)
    , active(true)
{
    actionsList << JsonDbNotify::Create << JsonDbNotify::Update<< JsonDbNotify::Remove;
}

JsonDbNotify::~JsonDbNotify()
{
    if (defaultPartitionObject)
        delete defaultPartitionObject;
}


/*!
    \qmlproperty string QtJsonDb::Notification::query
     Holds the query string for the notification. The query should be
    specified in JsonQuery format.
*/
QString JsonDbNotify::query() const
{
    return queryString;
}

void JsonDbNotify::setQuery(const QString &newQuery)
{
    queryString = newQuery;
    init();
}

/*!
    \qmlproperty ListOrObject QtJsonDb::Notification::actions
    Holds the list of registered actions for the notification.
    Supported actions are
    \list
    \o Notification.Create - Subscribe to create notifications
    \o Notification.Update  - Subscribe to update notifications
    \o Notification.Remove - Subscribe to remove notifications
    \endlist
    By default the object subscribes to all types of notifications.
    If you need to subscribe only to a subset of actions, it can
    be done by setting a new actions list.

    \qml
    JsonDb.Notification {
        id: createRemoveNotifications
        partition:nokiaPartition
        query: '[?_type="Contact"]'
        actions : [JsonDb.Notification.Create, JsonDb.Notification.Remove]
        onNotification: {
           if (action === JsonDb.Notification.Create) {
               console.log("object" + result._uuid + "was created successfully");
           }
           if (action === JsonDb.Notification.Remove) {
               console.log("object" + result._uuid + "was removed successfully");
           }
        }
     }
    \endqml
*/
QVariant JsonDbNotify::actions()
{
    return QVariant(actionsList);
}

void JsonDbNotify::setActions(const QVariant &newActions)
{
    if (QVariant::List == newActions.type()) {
        actionsList = newActions.toList();
    } else {
        actionsList.clear();
        actionsList.append(QVariant(newActions.toInt()));
    }
    init();
}

void JsonDbNotify::partitionNameChanged(const QString &partitionName)
{
    Q_UNUSED(partitionName);
    init();
}

/*!
    \qmlproperty object QtJsonDb::Notification::partition
     Holds the partition object for the notification.
*/

JsonDbPartition* JsonDbNotify::partition()
{
    if (!partitionObject)
        partitionObject = qobject_cast<JsonDbPartition*>(parent());

    if (!partitionObject) {
        defaultPartitionObject = new JsonDbPartition();
        setPartition(defaultPartitionObject);
    }
    return partitionObject;
}

void JsonDbNotify::setPartition(JsonDbPartition *newPartition)
{
    if (partitionObject == newPartition)
        return;
    if (partitionObject) {
        partitionObject->removeNotification(this);
    }
    if (partitionObject == defaultPartitionObject)
        delete defaultPartitionObject;
    partitionObject = newPartition;
    connect(partitionObject, SIGNAL(nameChanged(const QString&)),
            this, SLOT(partitionNameChanged(const QString&)));
    init();
}

/*!
    \qmlproperty bool QtJsonDb::Notification::enabled
     This flags enables handling of notification. if true (default);
     otherwise the onNotification signal handlers won't be called.
*/

bool JsonDbNotify::enabled() const
{
    return active;
}

void JsonDbNotify::setEnabled(bool enabled)
{
    if (active == enabled)
        return;
    JsonDbNotify::Status oldStatus = objectStatus;
    if (active && parametersReady())  {
        partitionObject->removeNotification(this);
        objectStatus = Null;
        if (objectStatus != oldStatus)
            emit statusChanged(objectStatus);
    }
    active = enabled;
    if (active)
        init();
}


/*!
    \qmlsignal QtJsonDb::Notification::onNotification(result, action, stateNumber)

    This handler is called when the an object matching the query is created, updated or
    removed. The \a result is the object that triggered the notication. The action which
    caused this notification is passed in the \a action. \a stateNumber is the state number
    of the partition when the notification was triggerd.

    \qml
    JsonDb.Notification {
        partition:nokiaPartition
        query: '[?_type="Contact"]'
        onNotification: {
            switch (action) {
            case JsonDb.Notification.Create :
                console.log("{_uuid :" + result._uuid + "} created");
                break;
            case JsonDb.Notification.Update :
                console.log("{_uuid :" + result._uuid + "} was updated");
                break;
            case JsonDb.Notification.Remove :
                console.log("{_uuid :" + result._uuid + "} was removed");
                break;
            }
        }
        onStatusChanged: {
            if (status === JsonDb.Notification.Error)
              console.log("Notification Error " + JSON.stringify(error));
        }
    }
    \endqml
*/

/*!
    \qmlproperty object QtJsonDb::Notification::error
    \readonly

    This property holds the current error information for the notification object. It contains:
    \list
    \o error.code -  code for the current error.
    \o error.message - detailed explanation of the error
    \endlist
*/

QVariantMap JsonDbNotify::error() const
{
    QVariantMap errorMap;
    errorMap.insert(QLatin1String("code"), errorCode);
    errorMap.insert(QLatin1String("message"), errorString);
    return errorMap;
}

/*!
    \qmlproperty enumeration QtJsonDb::Notification::status
    \readonly

    This property holds the status of the notification object.  It can be one of:
    \list
    \o Notification.Null - waiting for component to finish loading or for all the pararamters to be set.
    \o Notification.Registering - notification is being registered with the server
    \o Notification.Ready - object is ready, will send notifications
    \o Notification.Error - an error occurred while registering
    \endlist
*/

JsonDbNotify::Status JsonDbNotify::status() const
{
    return objectStatus;
}

void JsonDbNotify::componentComplete()
{
    completed = true;
    init();
}

void JsonDbNotify::init()
{
    JsonDbNotify::Status oldStatus = objectStatus;
    // if partition is not set, use the parent element if it
    // is an object of type JsonDbPartition.
    if (!partitionObject) {
        partitionObject = qobject_cast<JsonDbPartition*>(parent());
        if (partitionObject) {
            connect(partitionObject, SIGNAL(nameChanged(const QString&)),
                this, SLOT(partitionNameChanged(const QString&)));
        }
    }
    if (!parametersReady()) {
        objectStatus = Null;
        if (objectStatus != oldStatus)
            emit statusChanged(objectStatus);
        return;
    }
    if (partitionObject && active) {
        // remove the current notification
        partitionObject->removeNotification(this);
        partitionObject->updateNotification(this);
        objectStatus = Registering;
        if (objectStatus != oldStatus)
            emit statusChanged(objectStatus);
    }
}

void JsonDbNotify::clearError()
{
    int oldErrorCode = errorCode;
    errorCode = 0;
    errorString.clear();
    if (oldErrorCode != Error) {
        emit errorChanged(error());
    }
}

bool JsonDbNotify::parametersReady()
{
    return (completed && !queryString.isEmpty() && actionsList.count() && partitionObject);
}

void JsonDbNotify::dbNotified(const QString &notify_uuid, const QtAddOn::JsonDb::JsonDbNotification &_notification)
{
    Q_UNUSED(notify_uuid);
    if (objectStatus != Ready) {
        clearError();
        objectStatus = Ready;
        emit statusChanged(objectStatus);
    }
    if (active) {
        QJSValue obj = g_declEngine->toScriptValue(QVariant(_notification.object()));
        emit notification(obj, (Actions)_notification.action(), _notification.stateNumber());
    }
}

void JsonDbNotify::dbNotifyReadyResponse(int id, const QVariant &result)
{
    Q_UNUSED(id);
    Q_UNUSED(result);
    clearError();
    if (objectStatus != Ready) {
        objectStatus = Ready;
        emit statusChanged(objectStatus);
    }
}

void JsonDbNotify::dbNotifyErrorResponse(int id, int code, const QString &message)
{
    Q_UNUSED(id);
    int oldErrorCode = errorCode;
    errorCode = code;
    errorString = message;
    if (objectStatus != Error) {
        objectStatus = Error;
        emit statusChanged(objectStatus);
    }
    if (oldErrorCode != Error) {
        emit errorChanged(error());
    }
}

