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

#ifndef JSONDBNOTIFICATION_H1
#define JSONDBNOTIFICATION_H1

#include <QObject>
#include <QVariant>
#include <QPointer>
#include <QJSValue>
#include <QDeclarativeParserStatus>
#include <QDeclarativeListProperty>
#include "jsondb-client.h"

QT_USE_NAMESPACE_JSONDB

class JsonDbPartition;
class JsonDbPartitionPrivate;

class JsonDbNotify : public QObject, public QDeclarativeParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QDeclarativeParserStatus)

public:
    Q_ENUMS(Actions)
    Q_ENUMS(Status)

    enum Actions { Create = 1, Update = 2, Remove = 4 };
    enum Status { Null, Registering, Ready, Error };

    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(QVariant actions READ actions WRITE setActions)
    Q_PROPERTY(JsonDbPartition* partition READ partition WRITE setPartition)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap error READ error NOTIFY errorChanged)

    JsonDbNotify(QObject *parent = 0);
    ~JsonDbNotify();

    QString query() const;
    void setQuery(const QString &newQuery);

    QVariant actions();
    void setActions(const QVariant &newActions);

    JsonDbPartition* partition();
    void setPartition(JsonDbPartition* newPartition);

    bool enabled() const;
    void setEnabled(bool enabled);

    JsonDbNotify::Status status() const;
    QVariantMap error() const;

    void classBegin() {}
    void componentComplete();

Q_SIGNALS:
    void notification(const QJSValue &result, Actions action, int stateNumber);
    void statusChanged(JsonDbNotify::Status newStatus);
    void errorChanged(QVariantMap newError);

private Q_SLOTS:
    void partitionNameChanged(const QString &partitionName);
    void dbNotified(const QString &notify_uuid, const QtAddOn::JsonDb::JsonDbNotification &_notification);
    void dbNotifyReadyResponse(int id, const QVariant &result);
    void dbNotifyErrorResponse(int id, int code, const QString &message);

private:
    bool completed;
    QString queryString;
    QVariantList actionsList;
    QString uuid;
    QString version;
    QPointer<JsonDbPartition> partitionObject;
    QPointer<JsonDbPartition> defaultPartitionObject;
    int errorCode;
    QString errorString;
    Status objectStatus;
    bool active;

    void init();
    void clearError();
    inline bool parametersReady();
    friend class JsonDbPartition;
    friend class JsonDbPartitionPrivate;
};

#endif
