/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "clientjsonstream.h"
#include "jsondbsettings.h"
#include "jsondbstrings.h"

#include <QDebug>

QT_USE_NAMESPACE_JSONDB_PARTITION

ClientJsonStream::ClientJsonStream(QObject *parent) :
    QtJsonDbJsonStream::JsonStream(parent)
{
}

void ClientJsonStream::addNotification(const QString &uuid, JsonDbNotification *notification)
{
    mNotifications.insert(notification, uuid);
    connect(notification, SIGNAL(notified(QJsonObject,quint32,JsonDbNotification::Action)),
            this, SLOT(notified(QJsonObject,quint32,JsonDbNotification::Action)));
}

void ClientJsonStream::removeNotification(JsonDbNotification *notification)
{
    if (notification) {
        mNotifications.remove(notification);
        disconnect(notification, SIGNAL(notified(QJsonObject,quint32,JsonDbNotification::Action)),
                   this, SLOT(notified(QJsonObject,quint32,JsonDbNotification::Action)));
    }
}

JsonDbNotification *ClientJsonStream::takeNotification(const QString &uuid)
{
    JsonDbNotification *n = mNotifications.key(uuid);
    removeNotification(n);
    return n;
}

QList<JsonDbNotification *> ClientJsonStream::takeAllNotifications()
{
    QList<JsonDbNotification *> res;
    foreach (QPointer<JsonDbNotification> n, mNotifications.keys()) {
        res.append(n);
        disconnect(n, SIGNAL(notified(QJsonObject,quint32,JsonDbNotification::Action)),
                   this, SLOT(notified(QJsonObject,quint32,JsonDbNotification::Action)));
    }

    mNotifications.clear();
    return res;
}

QList<JsonDbNotification *> ClientJsonStream::notificationsByPartition(JsonDbPartition *partition)
{
    QList<JsonDbNotification *> res;
    if (!partition)
        return res;

    foreach (JsonDbNotification *n, mNotifications.keys()) {
        if (n->partition() == partition)
            res.append(n);
    }

    return res;
}

void ClientJsonStream::notified(const QJsonObject &object, quint32 stateNumber, JsonDbNotification::Action action)
{
    JsonDbNotification *notification = qobject_cast<JsonDbNotification *>(sender());
    if (!notification)
        return;

    QString uuid = mNotifications.value(notification);
    QString actionString = JsonDbString::kCreateStr;
    if (action == JsonDbNotification::Update)
        actionString = JsonDbString::kUpdateStr;
    else if (action == JsonDbNotification::Remove)
        actionString = JsonDbString::kRemoveStr;
    else if (action == JsonDbNotification::StateChange)
        actionString = QStringLiteral("stateChange");

    if (jsondbSettings->debug())
        qDebug() << "notificationId" << uuid << "object" << object << "action" << actionString;

    QJsonObject map, obj;
    obj.insert(JsonDbString::kObjectStr, object);
    obj.insert(JsonDbString::kActionStr, actionString);
    obj.insert(JsonDbString::kStateNumberStr, static_cast<int>(stateNumber));
    map.insert(JsonDbString::kNotifyStr, obj);
    map.insert(JsonDbString::kUuidStr, uuid);

    if (device() && device()->isWritable()) {
        if (jsondbSettings->debug())
            qDebug() << "Sending notify" << map;
        send(map);
    }
}
