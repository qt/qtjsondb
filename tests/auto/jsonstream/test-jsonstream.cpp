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
#include <QtTest/QtTest>

#include "jsonstream.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QBuffer>

#include <QDebug>

using QtJsonDbJsonStream::JsonStream;

class TestJsonStream: public QObject
{
    Q_OBJECT

public slots:
    void handleSocketConnection();
    void receiveStream(const QJsonObject &json);

private slots:
    void testJsonStream();

private:
    QLocalServer *server;
    bool serverOk;
    int receiverOk;
};

void TestJsonStream::testJsonStream()
{
    serverOk = false;
    receiverOk = 0;

    QLocalServer::removeServer("testJsonStream");
    server = new QLocalServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(handleSocketConnection()));
    QVERIFY(server->listen("testJsonStream"));

    QLocalSocket *device = new QLocalSocket(this);
    device->connectToServer("testJsonStream");
    QVERIFY(device->waitForConnected());
    QVERIFY(device->state() == QLocalSocket::ConnectedState);

    JsonStream *stream = new JsonStream(this);
    stream->setDevice(device);
    connect(stream, SIGNAL(receive(QJsonObject)), this, SLOT(receiveStream(QJsonObject)));

    qApp->processEvents();
    qApp->processEvents();
    qApp->processEvents();
    qApp->processEvents();
    qApp->processEvents();
    QVERIFY(serverOk);
    QCOMPARE(receiverOk, 3);
}

void TestJsonStream::handleSocketConnection()
{
    qDebug() << "handleSocketConnection";
    serverOk = true;
    JsonStream sender;
    sender.setDevice(server->nextPendingConnection());

    QJsonObject json1;
    json1.insert("hello", QString("world"));
    sender.send(json1);
    QJsonObject json2;
    json2.insert("hello", QString("again"));
    json2.insert("a", 1);
    sender.send(json2);
    QJsonObject json3;
    json3.insert("hello", QString("document"));
    sender.send(json3);
}

void TestJsonStream::receiveStream(const QJsonObject &json)
{
    qDebug() << "receiveStream" << ++receiverOk;
    QVERIFY(json.size() > 0);
    switch (receiverOk) {
    case 1:
        QCOMPARE(json.value("hello").toString(), QString("world"));
        break;
    case 2:
        QCOMPARE(json.value("hello").toString(), QString("again"));
        break;
    case 3:
        QCOMPARE(json.value("hello").toString(), QString("document"));
        break;
    default:
        QFAIL("test case ran off");
    }
}


QTEST_MAIN(TestJsonStream)
#include "test-jsonstream.moc"
