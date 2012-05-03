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

#include "qjsondbconnection.h"
#include "qjsondbobject.h"
#include "qjsondbreadrequest.h"
#include "qjsondbwriterequest.h"
#include "private/qjsondbflushrequest_p.h"

#include "testhelper.h"

#include <QDebug>
#include <QTest>

QT_USE_NAMESPACE_JSONDB

class TestQJsonDbFlushRequest: public TestHelper
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testFlush();
};

void TestQJsonDbFlushRequest::initTestCase()
{
    removeDbFiles();

    QStringList arg_list = QStringList() << "-validate-schemas";
    launchJsonDbDaemon(arg_list, __FILE__);
}

void TestQJsonDbFlushRequest::cleanupTestCase()
{
    removeDbFiles();
    stopDaemon();
}

void TestQJsonDbFlushRequest::init()
{
    connectToServer();
}

void TestQJsonDbFlushRequest::cleanup()
{
    disconnectFromServer();
}

void TestQJsonDbFlushRequest::testFlush()
{
    QJsonDbReadRequest read;
    read.setQuery(QStringLiteral("[?_type=%type]"));
    read.bindValue(QStringLiteral("type"), QStringLiteral("foo"));
    mConnection->send(&read);
    QVERIFY(waitForResponse(&read));

    uint state = read.stateNumber();

    QJsonDbObject obj;
    obj.setUuid(QJsonDbObject::createUuid());
    obj.insert(QStringLiteral("_type"), QStringLiteral("foo"));
    QJsonDbWriteRequest write;
    write.setObjects(QList<QJsonObject>() << obj);
    mConnection->send(&write);
    QVERIFY(waitForResponse(&write));

    QJsonDbFlushRequest flush;
    mConnection->send(&flush);
    QVERIFY(waitForResponse(&flush));
    QVERIFY(flush.stateNumber() > state);
    state = flush.stateNumber();

    obj.insert(QStringLiteral("_deleted"), true);
    write.setObjects(QList<QJsonObject>() << obj);
    mConnection->send(&write);
    QVERIFY(waitForResponse(&write));

    mConnection->send(&flush);
    QVERIFY(waitForResponse(&flush));
    QVERIFY(flush.stateNumber() > state);
}

QTEST_MAIN(TestQJsonDbFlushRequest)

#include "testqjsondbflushrequest.moc"
