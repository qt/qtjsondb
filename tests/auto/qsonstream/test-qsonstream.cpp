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
#include <QtTest/QtTest>

#include <qsonstream.h>
#include <QLocalServer>
#include <QLocalSocket>

#include <QDebug>

using namespace QtAddOn::JsonDb;

class TestQsonStream: public QObject
{
    Q_OBJECT

public slots:
    void handleSocketConnection();
    void receiveStream(const QsonObject &qson);

private slots:
    void testQsonStream();

private:
    QLocalServer *server;
    bool serverOk;
    int receiverOk;
};

void TestQsonStream::testQsonStream()
{
    serverOk = false;
    receiverOk = 0;

    QLocalServer::removeServer("testQsonStream");
    server = new QLocalServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(handleSocketConnection()));
    QVERIFY(server->listen("testQsonStream"));

    QLocalSocket *device = new QLocalSocket(this);
    device->connectToServer("testQsonStream");
    QVERIFY(device->waitForConnected());
    QVERIFY(device->state() == QLocalSocket::ConnectedState);

    QsonStream *stream = new QsonStream(device, this);

    connect(stream, SIGNAL(receive(const QsonObject&)),
                this, SLOT(receiveStream(const QsonObject&)));

    qApp->processEvents();
    qApp->processEvents();
    qApp->processEvents();
    QVERIFY(serverOk);
    QCOMPARE(receiverOk, 1);
}

void TestQsonStream::handleSocketConnection()
{
    qDebug() << "handleSocketConnection";
    serverOk = true;
    QsonStream sender(server->nextPendingConnection());

    QsonMap qson;
    qson.insert("hello", QString("world"));
    sender.send(qson);
}

void TestQsonStream::receiveStream(const QsonObject &incoming)
{
    qDebug() << "receiveStream";
    QsonMap qson(incoming);
    receiverOk++;
    QCOMPARE(qson.size(), 1);
    QCOMPARE(qson.valueString("hello"), QString("world"));
}


QTEST_MAIN(TestQsonStream)
#include "test-qsonstream.moc"
