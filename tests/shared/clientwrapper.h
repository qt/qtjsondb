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

#ifndef CLIENTWRAPPER_H
#define CLIENTWRAPPER_H

#include <QEventLoop>

#include "private/jsondb-connection_p.h"
#include "jsondb-client.h"

#define waitForResponse(eventloop, result, id_, code, notificationId, count) \
{ \
    int givenid_ = (id_); \
    (result)->mNotificationId = QVariant(); \
    (result)->mNeedId      = (givenid_ != -1); \
    (result)->mId          = QVariant(); \
    (result)->mMessage     = QString(); \
    (result)->mCode        = -1; \
    (result)->mData        = QVariant(); \
    (result)->mNotificationWaitCount = count; \
    (result)->mLastUuid    = QString(); \
    \
    QTimer timer; \
    QObject::connect(&timer, SIGNAL(timeout()), &eventloop, SLOT(quit())); \
    timer.start(10000); \
    eventloop.exec(QEventLoop::AllEvents); \
    if (debug_output) { \
        qDebug() << "waitForResponse" << "expected id" << givenid_ << "got id" << (result)->mId; \
        qDebug() << "waitForResponse" << "expected code" << int(code) << "got code" << (result)->mCode; \
        qDebug() << "waitForResponse" << "expected notificationId" << notificationId << "got notificationId" << (result)->mNotificationId; \
    } \
    if ((result)->mNeedId) QVERIFY2(!(result)->mId.isNull(), "Failed to receive an answer from the db server"); \
    if ((result)->mNeedId) QCOMPARE((result)->mId, QVariant(givenid_)); \
    if ((result)->mNotificationId != QVariant(notificationId)) { \
        if ((result)->mNotificationId.isNull()) { \
            QVERIFY2(false, "we expected notification but did not get it :("); \
        } else { \
            QString data = JsonWriter().toString((result)->mNotifications.last().mObject); \
            QByteArray ba = QString("we didn't expect notification but got it. %1").arg(data).toLatin1(); \
            QVERIFY2(false, ba.constData()); \
        } \
    } \
    QCOMPARE((result)->mNotificationId, QVariant(notificationId)); \
    if ((result)->mCode != int(code)) \
        qDebug() << (result)->mMessage; \
    QCOMPARE((result)->mCode, int(code)); \
}
#define waitForResponse1(id) waitForResponse(mEventLoop, this, id, -1, QVariant(), 0)
#define waitForResponse2(id, code) waitForResponse(mEventLoop, this, id, code, QVariant(), 0)
#define waitForResponse3(id, code, notificationId) waitForResponse(mEventLoop, this, id, code, notificationId, 0)
#define waitForResponse4(id, code, notificationId, count) waitForResponse(mEventLoop, this, id, code, notificationId, count)

class Notification
{
public:
    Notification(const QString &notifyUuid, const QVariant &object, const QString &action)
        : mNotifyUuid(notifyUuid), mObject(object), mAction(action) {}

    QString  mNotifyUuid;
    QVariant mObject;
    QString  mAction;
};

class ClientWrapper : public QObject
{
    Q_OBJECT
public:
    ClientWrapper(QObject *parent = 0)
        : QObject(parent), debug_output(false),
          mClient(0), mCode(0), mNeedId(false), mNotificationWaitCount(0)
    {
    }

    void connectToServer()
    {
        mClient = new Q_ADDON_JSONDB_PREPEND_NAMESPACE(JsonDbClient)(this);
        connect(mClient, SIGNAL(response(int,QVariant)),
                this, SLOT(response(int,QVariant)));
        connect(mClient, SIGNAL(error(int,int,QString)),
                this, SLOT(error(int,int,QString)));
        connect(mClient, SIGNAL(notified(QString,QVariant,QString)),
                this, SLOT(notified(QString,QVariant,QString)));
    }

    bool debug_output;

    Q_ADDON_JSONDB_PREPEND_NAMESPACE(JsonDbClient) *mClient;

    QEventLoop mEventLoop;
    QString mMessage;
    int  mCode;
    bool mNeedId;
    QVariant mId, mData, mNotificationId;
    QString mLastUuid;
    int mNotificationWaitCount;
    QList<Notification> mNotifications;

protected slots:
    virtual void notified(const QString &notifyUuid, const QVariant &object, const QString &action)
    {
        mNotificationId = notifyUuid;
        mNotifications << Notification(notifyUuid, object, action);
        mNotificationWaitCount -= 1;
        if (mId.isValid() && !mNotificationWaitCount)
            mEventLoop.quit();
        if (!mNeedId && !mNotificationWaitCount)
            mEventLoop.quit();
    }

    virtual void response(int id, const QVariant &data)
    {
        mId = id;
        mData = data;
        mLastUuid = data.toMap().value("_uuid").toString();
        if (mId.isValid() && !mNotificationWaitCount)
            mEventLoop.quit();
        if (!mNeedId && !mNotificationWaitCount)
            mEventLoop.quit();
    }

    virtual void error(int id, int code, const QString &message)
    {
        mId = id;
        mCode = code;
        mMessage = message;
        mEventLoop.quit();
    }
    virtual void disconnected()
    {
    }
};

#endif // CLIENTWRAPPER_H
