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
#include "testhelper.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTest>
#include <QFile>

#include <signal.h>

QT_USE_NAMESPACE_JSONDB

static const char dbfileprefix[] = "test-jsondb-request";

class TestQJsonDbRequest: public TestHelper
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void modifyPartitions();
};

void TestQJsonDbRequest::initTestCase()
{
    removeDbFiles();

    QStringList arg_list = QStringList() << "-validate-schemas";
    launchJsonDbDaemon(QString::fromLatin1(dbfileprefix), arg_list, __FILE__);
}

void TestQJsonDbRequest::cleanupTestCase()
{
    removeDbFiles();
    stopDaemon();
}

void TestQJsonDbRequest::init()
{
    connectToServer();
}

void TestQJsonDbRequest::cleanup()
{
    disconnectFromServer();
}

void TestQJsonDbRequest::modifyPartitions()
{
    QFile::remove("partitions-test.json");

    // create a notification on Partitions
    QJsonDbWatcher watcher;
    watcher.setPartition(QLatin1String("Ephemeral"));
    watcher.setQuery("[?_type=\"Partition\"]");
    mConnection->addWatcher(&watcher);

    // ensure that there's only one partition defined and that it's the default
    QLatin1String defaultPartition("com.qt-project.shared");

    QJsonDbReadRequest partitionQuery;
    partitionQuery.setPartition(QLatin1String("Ephemeral"));
    partitionQuery.setQuery(QLatin1String("[?_type=%type]"));
    partitionQuery.bindValue(QLatin1String("type"), QLatin1String("Partition"));

    mConnection->send(&partitionQuery);
    QVERIFY(waitForResponse(&partitionQuery));

    QList<QJsonObject> results = partitionQuery.takeResults();
    QCOMPARE(results.count(), 1);
    QCOMPARE(results[0].value(QLatin1String("name")).toString(), defaultPartition);
    QVERIFY(results[0].value(QLatin1String("default")).toBool());

    // write a new partitions file
    QJsonObject def1;
    def1.insert(QLatin1String("name"), QLatin1String("com.qt-project.test1"));
    QJsonObject def2;
    def2.insert(QLatin1String("name"), QLatin1String("com.qt-project.test2"));
    QJsonArray defs;
    defs.append(def1);
    defs.append(def2);

    QFile partitionsFile(QLatin1String("partitions-test.json"));
    partitionsFile.open(QFile::WriteOnly);
    partitionsFile.write(QJsonDocument(defs).toJson());
    partitionsFile.close();

    // send the daemon a SIGHUP to get it to reload the partitions
    kill(mProcess->pid(), SIGHUP);
    QVERIFY(waitForResponseAndNotifications(0, &watcher, 2));

    // query for the new partitions
    mConnection->send(&partitionQuery);
    QVERIFY(waitForResponse(&partitionQuery));

    results = partitionQuery.takeResults();
    QCOMPARE(results.count(), 3);

    // operate on the new partition to make sure it works
    QJsonDbWriteRequest writeRequest;
    QUuid testUuid = QJsonDbObject::createUuidFromString(QLatin1String("testobject1"));
    QJsonDbObject toWrite;
    toWrite.setUuid(testUuid);
    toWrite.insert(QLatin1String("_type"), QLatin1String("TestObject"));
    writeRequest.setObjects(QList<QJsonObject>() << toWrite);
    mConnection->send(&writeRequest);
    QVERIFY(waitForResponse(&writeRequest));
    QVERIFY(!mRequestErrors.contains(&writeRequest));

    QJsonDbReadObjectRequest readRequest(testUuid);
    mConnection->send(&readRequest);
    QVERIFY(waitForResponse(&readRequest));
    QVERIFY(!mRequestErrors.contains(&readRequest));
    results = readRequest.takeResults();
    QCOMPARE(results.count(), 1);
    QCOMPARE(results[0].value(QLatin1String("_type")).toString(), QLatin1String("TestObject"));

    // remove the new partitions file
    partitionsFile.remove();

    // send the daemon a SIGHUP to get it to unload the partitions
    kill(mProcess->pid(), SIGHUP);
    QVERIFY(waitForResponseAndNotifications(0, &watcher, 2));

    // verify that we're back to just the origin partition
    mConnection->send(&partitionQuery);
    QVERIFY(waitForResponse(&partitionQuery));

    results = partitionQuery.takeResults();
    QCOMPARE(results.count(), 1);
    QCOMPARE(results[0].value(QLatin1String("name")).toString(), defaultPartition);
    QVERIFY(results[0].value(QLatin1String("default")).toBool());

    // query one of the test partitions to ensure we get an InvalidPartition error
    QJsonDbReadRequest failingRequest;
    failingRequest.setPartition(QLatin1String("com.qt-project.test1"));
    failingRequest.setQuery(QLatin1String("[*]"));
    mConnection->send(&failingRequest);
    QVERIFY(waitForResponse(&failingRequest));

    QVERIFY(mRequestErrors.contains(&failingRequest));
    QCOMPARE(mRequestErrors[&failingRequest], QJsonDbRequest::InvalidPartition);

    mConnection->removeWatcher(&watcher);
}

QTEST_MAIN(TestQJsonDbRequest)

#include "testqjsondbrequest.moc"
