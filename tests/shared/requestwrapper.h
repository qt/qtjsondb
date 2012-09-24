/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef RequestWrapper_H
#define RequestWrapper_H

#include <QCoreApplication>
#include <QList>
#include <QTest>
#include <QFile>
#include <QProcess>
#include <QEventLoop>
#include <QDebug>
#include <QLocalSocket>
#include <QTimer>
#include <QQmlEngine>
#include <QQmlComponent>

#include "qjsondbconnection.h"
#include "qjsondbwriterequest.h"
#include "qjsondbreadrequest.h"

QT_USE_NAMESPACE_JSONDB

#define waitForResponse(eventloop_, id_) \
{ \
    int givenid_ = (id_); \
    lastRequestId = -1; \
    lastResult.clear(); \
    lastErrorCode = 0; \
    lastErrorMessage.clear(); \
    eventLoop = &eventloop_; \
    \
    QTimer timer; \
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout())); \
    QObject::connect(&timer, SIGNAL(timeout()), eventLoop, SLOT(quit())); \
    timer.start(clientTimeout);                                       \
    elapsedTimer.start(); \
    do { \
        eventLoop->exec(QEventLoop::AllEvents); \
    } while (lastRequestId < givenid_); \
    eventLoop = 0; \
    if (givenid_ != -1) QVERIFY2((lastRequestId!=-1), "Failed to receive an answer from the db server"); \
    if (givenid_ != -1) QCOMPARE(lastRequestId, givenid_); \
}

#define waitForResponse1(id) waitForResponse(eventLoop1, id)

#define waitForCallbackGeneric(eventloop) \
{ \
    QTimer timer; \
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout())); \
    QObject::connect(&timer, SIGNAL(timeout()), &eventloop, SLOT(quit())); \
    timer.start(clientTimeout);                                       \
    elapsedTimer.start(); \
    mTimedOut = false;\
    callbackError = false; \
    eventloop.exec(QEventLoop::AllEvents); \
    QCOMPARE(false, mTimedOut); \
}

//#define waitForCallback() waitForCallbackGeneric(eventLoop)
#define waitForCallback1() waitForCallbackGeneric(eventLoop1)

class RequestWrapper: public QObject
{
    Q_OBJECT
public:
    RequestWrapper()
        :clientTimeout(20000)
    {
        if (qgetenv("JSONDB_CLIENT_TIMEOUT").size())
           clientTimeout = QString::fromLatin1(qgetenv("JSONDB_CLIENT_TIMEOUT")).toLong();

        connect(this, SIGNAL(response(int,QVariantList)),
                this, SLOT(onResponse(int,QVariantList)));
        connect(this, SIGNAL(error(int,int,QString)),
                this, SLOT(onError(int,int,QString)));
    }

    ~RequestWrapper()
    {
        if (connection)
            delete connection;
    }

    int create(const QVariantMap &item, const QString &partitionName = QString())
    {
        QVariantList list;
        list.append(item);
        return create(list, partitionName);
    }

    int create(const QVariantList &list, const QString &partitionName = QString())
    {
        QList<QJsonObject> objects;
        for (int i = 0; i<list.count(); i++) {
            objects.append(QJsonObject::fromVariantMap(list[i].toMap()));
        }
        QtJsonDb::QJsonDbWriteRequest *request = new QtJsonDb::QJsonDbCreateRequest(objects);
        request->setPartition(partitionName);
        connect(request, SIGNAL(finished()), this, SLOT(onWriteFinished()));
        connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onWriteError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                request, SLOT(deleteLater()));
        connection->send(request);
        return request->property("requestId").toInt();
    }

    int update(const QVariantMap &item, const QString &partitionName = QString())
    {
        QVariantList list;
        list.append(item);
        return update(list, partitionName);
    }

    int update(const QVariantList &list, const QString &partitionName = QString())
    {
        QList<QJsonObject> objects;
        for (int i = 0; i<list.count(); i++) {
            objects.append(QJsonObject::fromVariantMap(list[i].toMap()));
        }
        QtJsonDb::QJsonDbWriteRequest *request = new QtJsonDb::QJsonDbUpdateRequest(objects);
        request->setPartition(partitionName);
        connect(request, SIGNAL(finished()), this, SLOT(onWriteFinished()));
        connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onWriteError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                request, SLOT(deleteLater()));
        connection->send(request);
        return request->property("requestId").toInt();
    }

    int remove(const QVariantMap &item, const QString &partitionName = QString())
    {
        QVariantList list;
        list.append(item);
        return remove(list, partitionName);
    }

    int remove(const QVariantList &list, const QString &partitionName = QString())
    {
        QList<QJsonObject> objects;
        for (int i = 0; i<list.count(); i++) {
            objects.append(QJsonObject::fromVariantMap(list[i].toMap()));
        }
        QtJsonDb::QJsonDbWriteRequest *request = new QtJsonDb::QJsonDbRemoveRequest(objects);
        request->setPartition(partitionName);
        connect(request, SIGNAL(finished()), this, SLOT(onWriteFinished()));
        connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onWriteError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                request, SLOT(deleteLater()));
        connection->send(request);
        return request->property("requestId").toInt();
    }

    int query(const QString &queryString, const QString &partitionName = QString())
    {
        QJsonDbReadRequest *request = new QJsonDbReadRequest;
        request->setQuery(queryString);
        request->setPartition(partitionName);
        connect(request, SIGNAL(finished()), this, SLOT(onQueryFinished()));
        connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onWriteError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                request, SLOT(deleteLater()));
        connection->send(request);
        return request->property("requestId").toInt();
    }

public Q_SLOTS:
    void onWriteFinished()
    {
        QtJsonDb::QJsonDbWriteRequest *request = qobject_cast<QtJsonDb::QJsonDbWriteRequest*>(sender());
        if (request) {
            QList<QJsonObject> objects = request->takeResults();
            QVariantList list;
            for (int i = 0; i<objects.count(); i++) {
                list.append(objects[i].toVariantMap());
            }
            emit response(request->property("requestId").toInt(), list);
        }
    }
    void onQueryFinished()
    {
        QtJsonDb::QJsonDbReadRequest *request = qobject_cast<QtJsonDb::QJsonDbReadRequest*>(sender());
        if (request) {
            QList<QJsonObject> objects = request->takeResults();
            QVariantList list;
            for (int i = 0; i<objects.count(); i++) {
                list.append(objects[i].toVariantMap());
            }
            emit response(request->property("requestId").toInt(), list);
        }
    }
    void onWriteError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
    {
        QtJsonDb::QJsonDbWriteRequest *request = qobject_cast<QtJsonDb::QJsonDbWriteRequest*>(sender());
        if (request) {
            emit error(request->property("requestId").toInt(), int(code), message);
        }
    }
    void onResponse(int id, const QVariantList& list)
    {
        //qDebug() << "onResponse" << id;
        lastRequestId = id;
        lastResult = list;
        if (eventLoop)
            eventLoop->quit();
    }
    void onError(int id, int code, const QString &message)
    {
        qDebug() << "onError" << id << code << message;
        lastRequestId = id;
        lastErrorCode = code;
        lastErrorMessage = message;
        if (eventLoop)
            eventLoop->quit();
    }
    virtual void timeout()
    {
        qDebug() << "RequestWrapper::timeout() " << elapsedTimer.elapsed();
    }
Q_SIGNALS:
    void response(int, const QVariantList&);
    void error(int, int, const QString&);
protected:
    QPointer<QtJsonDb::QJsonDbConnection> connection;
    QPointer<QEventLoop> eventLoop;
    int lastRequestId;
    QVariantList lastResult;
    int lastErrorCode;
    QString lastErrorMessage;
    QEventLoop eventLoop1;
    QElapsedTimer elapsedTimer;
    int clientTimeout;

    //Liang is trying
    bool callbackError;
    int callbackErrorCode;
    QString callbackErrorMessage;
    QVariant callbackMeta;
    QVariant callbackResponse;
    bool mCallbackReceived;
};

#endif
