/****************************************************************************
**
** Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef JsonDbComponent_H
#define JsonDbComponent_H

#include <QMap>
#include <QObject>
#include <QPointer>
#include <QJSValue>
#include <QSet>

#include "jsondb-client.h"

class JsonDbComponent;
class JsonDbNotificationHandle;

Q_USE_JSONDB_NAMESPACE

class JsonDbNotification: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString uuid READ uuid)
public:
    ~JsonDbNotification();

    QString uuid() const { return mUuid; }
    Q_INVOKABLE void remove();

signals:
    void notify(const QJSValue& object, const QString& action);
    void removed(int id);
    void removed(QString uuid);

private:
    JsonDbNotification(JsonDbComponent *repo);

    int mId; // the request id
    QString mUuid; // for a fully created notification, this is the uuid of the db entry
    QJSValue mCallback;

    friend class JsonDbComponent;
    friend class JsonDbNotificationHandle;
};

class JsonDbNotificationHandle: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString uuid READ uuid)
public:
    JsonDbNotificationHandle(JsonDbNotification *notification);
    ~JsonDbNotificationHandle();

    QString uuid() const;
    Q_INVOKABLE void remove();
private:
    QPointer<JsonDbNotification> mNotification;
};

class JsonDbComponent : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool debug READ hasDebugOutput WRITE setDebugOutput)

public:
    JsonDbComponent(QObject *parent = 0);
    virtual ~JsonDbComponent();

    bool hasDebugOutput() const { return mDebugOutput; }
    void setDebugOutput(bool value) { mDebugOutput = value; }

    Q_INVOKABLE int create(const QJSValue &object,
                           const QJSValue &successCallback = QJSValue(),
                           const QJSValue &errorCallback = QJSValue());
    Q_INVOKABLE int update(const QJSValue &object,
                           const QJSValue &successCallback = QJSValue(),
                           const QJSValue &errorCallback = QJSValue());
    Q_INVOKABLE int remove(const QJSValue &object,
                           const QJSValue &successCallback = QJSValue(),
                           const QJSValue &errorCallback = QJSValue());
    Q_INVOKABLE int find(const QJSValue &object,
                         const QJSValue &successCallback = QJSValue(),
                         const QJSValue &errorCallback = QJSValue());

    Q_INVOKABLE int query(const QJSValue &object,
                          const QJSValue &successCallback = QJSValue(),
                          const QJSValue &errorCallback = QJSValue());

    Q_INVOKABLE QJSValue notification(const QJSValue &object,
                                          const QJSValue &actions,
                                          const QJSValue &callback,
                                          QJSValue errorCallback = QJSValue());

 signals:
    void response(QJSValue result, int id);
    void error(const QString& message, int id);

 protected slots:
    void jsonDbResponse(int, const QsonObject &);
    void jsonDbErrorResponse(int id, int code, const QString &message);
    void jsonDbNotified(const QString& notify_uuid, const QsonObject& object, const QString& action);

    void notificationRemoved(int id);
    void notificationRemoved(QString uuid);

 private:
    enum RequestType { Find, Create, Update, Remove, Notification };

    int addRequestInfo(int id, RequestType type, const QJSValue &object, const QJSValue &successCallback, const QJSValue &errorCallback);

    bool mDebugOutput;
    JsonDbClient *mJsonDb; // TODO: shouldn't this be a singleton?

    struct RequestInfo {
        RequestType type;
        QJSValue object;
        QJSValue successCallback;
        QJSValue errorCallback;
    };

    QMap<int, RequestInfo> mRequests;
    QMap<int, JsonDbNotification*> mPendingNotifications;
    QSet<int> mKilledNotifications; // notifications that were removed before they were really created
    QMap<QString, JsonDbNotification*> mNotifications;
};

#endif
