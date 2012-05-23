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

#ifndef CLIENTJSONSTREAM_H
#define CLIENTJSONSTREAM_H

#include "jsondbnotification.h"
#include "jsonstream.h"

#include <QHash>
#include <QPointer>

QT_BEGIN_HEADER

QT_USE_NAMESPACE_JSONDB_PARTITION

class ClientJsonStream : public QtJsonDbJsonStream::JsonStream
{
    Q_OBJECT
public:
    explicit ClientJsonStream(QObject *parent = 0);
    void addNotification(const QString &uuid, JsonDbNotification *notification);
    void removeNotification(JsonDbNotification *notification);
    JsonDbNotification *takeNotification(const QString &uuid);

    QList<JsonDbNotification *> takeAllNotifications();
    QList<JsonDbNotification *> notificationsByPartition(JsonDbPartition *partition);

protected Q_SLOTS:
    void notified(const QJsonObject &object, quint32 stateNumber, JsonDbNotification::Action action);

private:
    QHash<JsonDbNotification *, QString> mNotifications;
};

QT_END_HEADER

#endif // CLIENTJSONSTREAM_H
