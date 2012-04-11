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

#ifndef CLIENTWRAPPER_H
#define CLIENTWRAPPER_H

#include <QEventLoop>
#include <QElapsedTimer>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>

#include "jsondb-client.h"

QT_USE_NAMESPACE_JSONDB

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
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout())); \
    QObject::connect(&timer, SIGNAL(timeout()), &eventloop, SLOT(quit())); \
    timer.start(mClientTimeout);                                       \
    mElapsedTimer.start(); \
    do { \
        eventloop.exec(QEventLoop::AllEvents); \
    } while ((result)->mNeedId && (result)->mId.toInt() < QVariant(givenid_).toInt()); \
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
            QJsonValue value = QJsonValue::fromVariant((result)->mNotifications.last().mObject); \
            QString data = QString::fromUtf8(value.isArray() ? QJsonDocument(value.toArray()).toJson() : QJsonDocument(value.toObject()).toJson()); \
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

#define waitForCallbackGeneric(eventloop) \
{ \
    QTimer timer; \
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout())); \
    QObject::connect(&timer, SIGNAL(timeout()), &eventloop, SLOT(quit())); \
    timer.start(mClientTimeout);                                       \
    mElapsedTimer.start(); \
    mTimedOut = false;\
    callbackError = false; \
    eventloop.exec(QEventLoop::AllEvents); \
    QCOMPARE(false, mTimedOut); \
}

#define waitForCallback() waitForCallbackGeneric(mEventLoop)
#define waitForCallback2() waitForCallbackGeneric(mEventLoop2)

class JsonDbTestNotification
{
public:
    JsonDbTestNotification(const QString &notifyUuid, const QVariant &object, const QString &action)
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
        : QObject(parent), debug_output(false)
        , mClient(0), mCode(0), mNeedId(false), mNotificationWaitCount(0), mClientTimeout(20000)
    {
        if (qgetenv("JSONDB_CLIENT_TIMEOUT").size())
            mClientTimeout = QString::fromLatin1(qgetenv("JSONDB_CLIENT_TIMEOUT")).toLong();
    }

    void connectToServer()
    {
        mClient = new QT_PREPEND_NAMESPACE_JSONDB(JsonDbClient)(this);
        connect(mClient, SIGNAL(response(int,QVariant)),
                this, SLOT(response(int,QVariant)));
        connect(mClient, SIGNAL(error(int,int,QString)),
                this, SLOT(error(int,int,QString)));
        connect(mClient, SIGNAL(notified(QString,QtAddOn::JsonDb::JsonDbNotification)),
                this, SLOT(notified(QString,QtAddOn::JsonDb::JsonDbNotification)));
    }

    QString addNotification(JsonDbClient::NotifyTypes types, const QString &query, const QString &partition = QString()) {
        QEventLoop ev;
        QString uuid = mClient->registerNotification(types, query, partition, 0, 0, &ev, SLOT(quit()));
        ev.exec();
        return uuid;
    }

    bool debug_output;

    QT_PREPEND_NAMESPACE_JSONDB(JsonDbClient) *mClient;

    QEventLoop mEventLoop;
    QString mMessage;
    int  mCode;
    bool mNeedId;
    QVariant mId, mData, mNotificationId;
    QString mLastUuid;
    int mNotificationWaitCount;
    QList<JsonDbTestNotification> mNotifications;
    quint32 mClientTimeout;
    QElapsedTimer mElapsedTimer;

protected slots:
    virtual void notified(const QString &notifyUuid, const QtAddOn::JsonDb::JsonDbNotification &notification)
    {
        if (debug_output)
            qDebug() << "notified" << notifyUuid << notification.action() << endl << notification.object();
        mNotificationId = notifyUuid;
        QString action;
        switch (notification.action()) {
        case QT_PREPEND_NAMESPACE_JSONDB(JsonDbClient)::NotifyCreate: action = QLatin1String("create"); break;
        case QT_PREPEND_NAMESPACE_JSONDB(JsonDbClient)::NotifyUpdate: action = QLatin1String("update"); break;
        case QT_PREPEND_NAMESPACE_JSONDB(JsonDbClient)::NotifyRemove: action = QLatin1String("remove"); break;
        }

        mNotifications << JsonDbTestNotification(notifyUuid, notification.object(), action);
        mNotificationWaitCount -= 1;
        if (mId.isValid() && !mNotificationWaitCount)
            mEventLoop.quit();
        if (!mNeedId && !mNotificationWaitCount)
            mEventLoop.quit();
    }

    virtual void response(int id, const QVariant &data)
    {
        if (debug_output)
            qDebug() << "response" << id << endl << data;
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
        if (debug_output)
            qDebug() << "response" << id << code << message;
        mId = id;
        mCode = code;
        mMessage = message;
        mEventLoop.quit();
    }
    virtual void disconnected()
    {
    }
    virtual void timeout()
    {
        qDebug() << "timeout" << mElapsedTimer.elapsed();
    }
};

#endif // CLIENTWRAPPER_H
