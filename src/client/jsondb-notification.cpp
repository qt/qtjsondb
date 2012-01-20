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

#include "jsondb-notification.h"
#include "jsondb-client.h"

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \class JsonDbNotification

    \brief The JsonDbNotification class describes the database notification.

    \sa JsonDbClient::registerNotification()
*/

/*!
    \internal
*/
JsonDbNotification::JsonDbNotification(const QVariantMap &object, JsonDbClient::NotifyType action, quint32 stateNumber)
    : mObject(object), mAction(action), mStateNumber(stateNumber)
{
}

/*!
    Returns the object that matched notification.

    If the action() is JsonDbClient::NotifyCreate, the object contains the
    full object that was created.

    If the action() is JsonDbClient::NotifyUpdate, the object contains the
    latest version of the object.

    If the action() is JsonDbClient::NotifyRemove, the object contains the
    _uuid and _version of the object that was removed.
*/
QVariantMap JsonDbNotification::object() const
{
    return mObject;
}

/*!
    Returns the notification action.
*/
JsonDbClient::NotifyType JsonDbNotification::action() const
{
    return mAction;
}

/*!
    Returns the state number that corresponds to the object in notification.
*/
quint32 JsonDbNotification::stateNumber() const
{
    return mStateNumber;
}

QT_END_NAMESPACE_JSONDB
