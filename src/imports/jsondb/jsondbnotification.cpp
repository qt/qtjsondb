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
#include "jsondb-global.h"
#include "jsondbnotification.h"
#include "jsondbpartition.h"
#include "private/jsondb-strings_p.h"
#include "plugin.h"
#include <qdebug.h>

Q_USE_JSONDB_NAMESPACE

/*!
    \qmlclass Notification
    \inqmlmodule QtJsonDb
    \since 1.x

    This allows to register for different notifications that matches a query in a Partition.
    Users can connect to onNotification signal which is fired for notifcations matiching the
    actions & query properties. The notifications can be enabled/disabled by setting the
    enabled property of the object.

    \code
    JsonDb.Notification {
        id: allNotifications
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
    \endcode

*/

JsonDbNotify::JsonDbNotify(QObject *parent)
    :QObject(parent)
    ,completed(false)
    ,partitionObject(0)
    ,defaultPartitionObject(0)
    ,active(true)
{
}

JsonDbNotify::~JsonDbNotify()
{
    if (defaultPartitionObject)
        delete defaultPartitionObject;
}


/*!
    \qmlproperty string QtJsonDb::Notification::query
     Holds the query object for the notification.
*/
QVariant JsonDbNotify::query()
{
    return queryObject;
}

void JsonDbNotify::setQuery(const QVariant &newQuery)
{
    queryObject = newQuery;
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

    \code
    JsonDb.Notification {
        id: allNotifications
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
    \endcode
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
    removeNotifications();
    if (partitionObject == defaultPartitionObject)
        delete defaultPartitionObject;
    partitionObject = newPartition;
    connect(partitionObject, SIGNAL(nameChanged(const QString&)),
            this, SLOT(partitionNameChanged(const QString&)));
    init();
}

/*!
    \qmlproperty object QtJsonDb::Notification::enabled
     This flags enables handling of notification. if true (default);
     otherwise no notification handlers will be called.
*/

bool JsonDbNotify::enabled()
{
    return active;
}

void JsonDbNotify::setEnabled(bool enabled)
{
    // ### DO we need to unsubscribe from notification?
    active = enabled;
}


/*!
    \qmlsignal QtJsonDb::Notification::onNotification(result, action, stateNumber)

    This handler is called when the a notification from the server matches one of the actions
    registered. The \a result is the object that triggered the notication. The action which
    caused this notification is passed in the \a action. The \a stateNumber of the partition
    when othe notification was triggerd.

    \code
    JsonDb.Partition {
        id: nokiaPartition
        name: "com.nokia.shared"
    }
    JsonDb.Notification {
        id:allNotifications
        partition:nokiaPartition
        query: '[?_type="Contact"]'
        onNotification: {
           if (action === JsonDb.Notification.Create) {
               console.log("object" + result._uuid + "was created");
               return;
           }
           if (action === JsonDb.Notification.Update) {
               console.log("object" + result._uuid + "was updated");
               return;
           }
           if (action === JsonDb.Notification.Remove) {
               console.log("object" + result._uuid + "was removed");
           }
        }

        onError: {
            console.log("onError object" + code + message);
        }

     }

    \endcode

*/

void JsonDbNotify::emitNotification(const QtAddOn::JsonDb::JsonDbNotification &_notification)
{
    if (active) {
        QJSValue obj = g_declEngine->toScriptValue(QVariant(_notification.object()));
        emit notification(obj, (Actions)_notification.action(), _notification.stateNumber());
    }
}



/*!
    \qmlsignal QtJsonDb::Notification::onError(code, message)

    This handler is called when there was an error creating the notiication. The \a code
    is the error code and \a message contains details of the error.
*/

void JsonDbNotify::emitError(int code, const QString &message)
{
    if (active) {
        emit error(code, message);
    }
}

void JsonDbNotify::componentComplete()
{
    completed = true;
    init();
}

void JsonDbNotify::init()
{

    if (!completed || !queryObject.isValid() || !actionsList.count() || !partitionObject) {
        return;
    }
    // remove the current notification
    partitionObject->removeNotification(this);
    partitionObject->updateNotification(this);
}

void  JsonDbNotify::removeNotifications()
{
    if (partitionObject) {
        partitionObject->removeNotification(this);
    }
}

