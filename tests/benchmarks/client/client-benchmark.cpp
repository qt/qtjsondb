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

#include <QtTest/QtTest>

#include "testhelper.h"
#include "qjsondbreadrequest.h"
#include "qjsondbwriterequest.h"
#include "qjsondbobject.h"
#include "private/qjsondblogrequest_p.h"

QT_USE_NAMESPACE_JSONDB

class ClientBenchmark: public TestHelper
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void createOneItem();
    void createThousandItems();
    void createThousandItemsInOneTransaction();
    void queryOneItem();
    void queryThousandItems_data();
    void queryThousandItems();
    void queryThousandItemsSorted();
    void updateOneItem();
    void updateThousandItems();
    void updateThousandItemsInOneTransaction();
    void removeOneItem();
    void removeThousandItems();
    void manyWatchers();

private:
    void sendLogMessage(const QString &msg);
};

int gMany = ::getenv("BENCHMARK_MANY") ? ::atoi(::getenv("BENCHMARK_MANY")) : 1000;
int gTransactionSize = ::getenv("BENCHMARK_TRANSACTION_SIZE") ? ::atoi(::getenv("BENCHMARK_TRANSACTION_SIZE")) : 100;
bool gPerformanceLog = ::getenv("JSONDB_PERFORMANCE_LOG") ? (::memcmp(::getenv("JSONDB_PERFORMANCE_LOG"), "true", 4) == 0) : false;

void ClientBenchmark::sendLogMessage(const QString &msg) {
    QJsonDbLogRequest log(msg);
    mConnection->send(&log);
    waitForResponse(&log);
}

#define BENCH_BEGIN_TAG if (gPerformanceLog) sendLogMessage(QString("[QBENCH-BEGIN] %1").arg(__FUNCTION__))
#define BENCH_END_TAG if (gPerformanceLog) sendLogMessage(QString("[QBENCH-END] %1").arg(__FUNCTION__))

void ClientBenchmark::initTestCase()
{
    removeDbFiles();
    launchJsonDbDaemon(QStringList() << "-validate-schemas", __FILE__);
    connectToServer();

    QJsonObject index;
    index.insert(QLatin1String("_uuid"), QLatin1String("{82fdd435-b026-418e-b6fc-decd2ef3590c}"));
    index.insert(QLatin1String("_type"), QLatin1String("Index"));
    index.insert(QLatin1String("name"), QLatin1String("name"));
    index.insert(QLatin1String("propertyName"), QLatin1String("name"));
    index.insert(QLatin1String("propertyType"), QLatin1String("string"));

    QJsonDbWriteRequest write;
    write.setObjects(QList<QJsonObject>() << index);
    mConnection->send(&write);
    QVERIFY(waitForResponse(&write));

    QByteArray friendJson("{\"type\": \"object\", \"properties\": {\"name\": {\"type\": \"string\", \"indexed\": true}}}");

    // Create schemas for the items
    QJsonObject friendSchema;
    friendSchema.insert(QStringLiteral("name"), QStringLiteral("Friends"));
    friendSchema.insert(QStringLiteral("schema"), QJsonDocument::fromJson(friendJson).object());
    friendSchema.insert(QStringLiteral("_type"), QStringLiteral("_schemaType"));
    write.setObjects(QList<QJsonObject>() << friendSchema);
    mConnection->send(&write);
    QVERIFY(waitForResponse(&write));
    QVERIFY(!mRequestErrors.contains(&write));

    // Create a lot of items in the database
    for (int k = 0; k < gMany; k += gTransactionSize) {
        QList<QJsonObject> friendsList;
        for (int i = k; i < k + gTransactionSize; i++) {
            QJsonObject item;
            item.insert(QStringLiteral("_type"), QStringLiteral("Friends"));
            item.insert(QStringLiteral("name"), QString::fromLatin1("Name-%1").arg(i));
            item.insert(QStringLiteral("phone"), QString::fromLatin1("%1").arg(qrand()));
            friendsList << item;
        }

        write.setObjects(friendsList);
        mConnection->send(&write);
        QVERIFY(waitForResponse(&write));
        QVERIFY(!mRequestErrors.contains(&write));
    }

    for (int k = 0; k < gMany; k += gTransactionSize) {
        QList<QJsonObject> imageList;
        for (int i = k; i < k+gTransactionSize; i++) {
            QJsonObject item;
            item.insert(QStringLiteral("_type"), QStringLiteral("Image"));
            item.insert(QStringLiteral("name"), QString::fromLatin1("Name-%1.jpg").arg(qrand()));
            item.insert(QStringLiteral("location"), QString::fromLatin1("/home/qt/Pictures/Myfolder-%1").arg(i));
            imageList << item;
        }

        write.setObjects(imageList);
        mConnection->send(&write);
        QVERIFY(waitForResponse(&write));
        QVERIFY(!mRequestErrors.contains(&write));
    }

    for (int k = 0; k < gMany; k += gTransactionSize) {
        QList<QJsonObject> numberList;
        for (int i = k; i < k+gTransactionSize; i++) {
            QJsonObject item;
            item.insert(QStringLiteral("_type"), QStringLiteral("randNumber"));
            item.insert(QStringLiteral("number"), qrand()%gMany);
            item.insert(QStringLiteral("idNumber"), i);
            numberList << item;
        }

        write.setObjects(numberList);
        mConnection->send(&write);
        QVERIFY(waitForResponse(&write));
        QVERIFY(!mRequestErrors.contains(&write));
    }

    disconnectFromServer();
}

void ClientBenchmark::cleanupTestCase()
{
    removeDbFiles();
    stopDaemon();
}

void ClientBenchmark::init()
{
    clearHelperData();
    connectToServer();
}

void ClientBenchmark::cleanup()
{
    disconnectFromServer();
}

void ClientBenchmark::createOneItem()
{
    QJsonObject item;
    item.insert(QStringLiteral("_type"), QStringLiteral("Contact"));
    item.insert(QStringLiteral("name"), QStringLiteral("Charlie"));
    item.insert(QStringLiteral("phone"), QStringLiteral("123456789"));

    QBENCHMARK {
        BENCH_BEGIN_TAG;
        QJsonDbCreateRequest create(item);
        mConnection->send(&create);
        waitForResponse(&create);
        BENCH_END_TAG;
    }
}


void ClientBenchmark::createThousandItems()
{
    QBENCHMARK {
        BENCH_BEGIN_TAG;
        for (int i = 0; i < gMany; ++i) {
            QJsonObject item;
            item.insert(QStringLiteral("_type"), QStringLiteral("randNumber"));
            item.insert(QStringLiteral("number"), qrand() % 100);
            item.insert(QStringLiteral("idNumber"), i);
            QJsonDbCreateRequest create(item);
            mConnection->send(&create);
            waitForResponse(&create);
        }
        BENCH_END_TAG;
    }
}

void ClientBenchmark::createThousandItemsInOneTransaction()
{
    QList<QJsonObject> objects;
    for (int i = 0; i < gMany; ++i) {
        QJsonDbObject item;
        item.setUuid(QUuid::createUuid());
        item.insert(QLatin1String("_type"), QLatin1String("createThousandItemsInOneTransaction"));
        item.insert(QLatin1String("number"), qrand()%100);
        item.insert(QLatin1String("idNumber"), i);
        objects.append(item);
    }

    QBENCHMARK {
        QJsonDbWriteRequest request;
        request.setObjects(objects);
        mConnection->send(&request);
        waitForResponse(&request);
    }
}

void ClientBenchmark::queryOneItem()
{
    QJsonDbReadRequest read;
    read.setQuery(QStringLiteral("[?_type=\"Friends\"][?name=\"Name-49\"]"));

    QBENCHMARK {
        BENCH_BEGIN_TAG;
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        QList<QJsonObject> items = read.takeResults();
        QCOMPARE(items.count(), 1);
        BENCH_END_TAG;
    }
}

void ClientBenchmark::queryThousandItems_data()
{
    QTest::addColumn<QString>("queryString");
    QTest::newRow("Friends") << QString("[?_type=\"Friends\"]");
    QTest::newRow("Friends[={_uuid:_uuid}]") << QString("[?_type=\"Friends\"][={_uuid:_uuid}]");
}

void ClientBenchmark::queryThousandItems()
{
    QFETCH(QString, queryString);
    QJsonDbReadRequest read;
    read.setQuery(queryString);

    QBENCHMARK {
        BENCH_BEGIN_TAG;
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        QList<QJsonObject> items = read.takeResults();
        QVERIFY(items.count() >= gMany);
        BENCH_END_TAG;
    }
}

void ClientBenchmark::queryThousandItemsSorted()
{
    QJsonDbReadRequest read;
    read.setQuery(QStringLiteral("[?_type=\"Friends\"][/name]"));

    QBENCHMARK {
        BENCH_BEGIN_TAG;
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        QList<QJsonObject> items = read.takeResults();
        QVERIFY(items.count() >= gMany);
        BENCH_END_TAG;
    }

    QCOMPARE(read.sortKey(), QLatin1String("name"));
}

void ClientBenchmark::updateOneItem()
{
    QJsonObject item;
    item.insert(QStringLiteral("_type"), QStringLiteral("Contact"));
    item.insert(QStringLiteral("name"), QStringLiteral("Markus"));
    item.insert(QStringLiteral("phone"), QStringLiteral("123456789"));
    QJsonDbWriteRequest write;
    write.setObjects(QList<QJsonObject>() << item);
    mConnection->send(&write);
    QVERIFY(waitForResponse(&write));
    QVERIFY(!mRequestErrors.contains(&write));

    QList<QJsonObject> res = write.takeResults();
    QCOMPARE(res.count(), 1);

    item.insert(QStringLiteral("phone"), QStringLiteral("111122223"));
    item.insert(QStringLiteral("_uuid"), res.at(0).value(QStringLiteral("_uuid")));
    item.insert(QStringLiteral("_version"), res.at(0).value(QStringLiteral("_version")));

    QBENCHMARK {
        BENCH_BEGIN_TAG;
        write.setObjects(QList<QJsonObject>() << item);
        mConnection->send(&write);
        QVERIFY(waitForResponse(&write));
        QVERIFY(!mRequestErrors.contains(&write));
        res = write.takeResults();
        QCOMPARE(res.count(), 1);
        item.insert(QStringLiteral("_version"), res.at(0).value(QStringLiteral("_version")));
        BENCH_END_TAG;
    }
}

void ClientBenchmark::updateThousandItems()
{
    // create thousand items
    for (int i = 0; i < gMany; i++) {
        QJsonObject item;
        item.insert(QStringLiteral("_type"), QStringLiteral("updateThousandItems"));
        item.insert(QStringLiteral("number"), qrand() % gMany);
        item.insert(QStringLiteral("idNumber"), i);
        QJsonDbCreateRequest create(item);
        mConnection->send(&create);
        QVERIFY(waitForResponse(&create));
        QVERIFY(!mRequestErrors.contains(&create));
    }

    QJsonDbReadRequest read;
    read.setQuery("[?_type=\"updateThousandItems\"]");

    QBENCHMARK {
        BENCH_BEGIN_TAG;

        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        QVERIFY(!mRequestErrors.contains(&read));

        QList<QJsonObject> items = read.takeResults();
        QCOMPARE(items.count(), gMany);

        for (int i = 0; i < gMany; i++) {
            QJsonObject item = items.at(i);
            item.insert(QStringLiteral("number"), gMany + i);
            QJsonDbUpdateRequest update(item);
            mConnection->send(&update);
            QVERIFY(waitForResponse(&update));
            QVERIFY(!mRequestErrors.contains(&update));
        }

        BENCH_END_TAG;
    }

    // remove those items
    mConnection->send(&read);
    QVERIFY(waitForResponse(&read));
    QVERIFY(!mRequestErrors.contains(&read));

    QList<QJsonObject> items = read.takeResults();
    QCOMPARE(items.count(), gMany);

    QJsonDbRemoveRequest remove(items);
    mConnection->send(&remove);
    QVERIFY(waitForResponse(&remove));
    QVERIFY(!mRequestErrors.contains(&remove));
}

void ClientBenchmark::updateThousandItemsInOneTransaction()
{
    // create thousand items
    QList<QJsonObject> objects;
    for (int i = 0; i < gMany; i++) {
        QJsonDbObject item;
        item.setUuid(QUuid::createUuid());
        item.insert(QLatin1String("_type"), QLatin1String("updateThousandItemsInOneTransaction"));
        item.insert(QLatin1String("number"), qrand()%gMany);
        item.insert(QLatin1String("idNumber"), i);
        objects.append(item);
    }
    QJsonDbWriteRequest request;
    request.setObjects(objects);
    mConnection->send(&request);
    waitForResponse(&request);

    QList<QJsonObject> items;
    QBENCHMARK {
        QJsonDbReadRequest readRequest;
        readRequest.setQuery(QLatin1String("[?_type=\"updateThousandItemsInOneTransaction\"]"));
        mConnection->send(&readRequest);
        waitForResponse(&readRequest);

        items = readRequest.takeResults();
        QCOMPARE(items.count(), gMany);
        for (int i = 0; i < gMany; i++) {
            QJsonObject &item = items[i];
            item.insert(QLatin1String("number"), gMany+i);
        }
        QJsonDbUpdateRequest request(items);
        mConnection->send(&request);
        waitForResponse(&request);
    }

    // remove those items
    QJsonDbReadRequest readRequest;
    readRequest.setQuery(QLatin1String("[?_type=\"updateThousandItemsInOneTransaction\"]"));
    mConnection->send(&readRequest);
    waitForResponse(&readRequest);

    QJsonDbRemoveRequest removeRequest(readRequest.takeResults());
    mConnection->send(&removeRequest);
    waitForResponse(&removeRequest);
}

void ClientBenchmark::removeOneItem()
{
    QJsonObject item;
    item.insert(QStringLiteral("_type"), QStringLiteral("Contact"));
    item.insert(QStringLiteral("name"), QStringLiteral("Marius"));
    item.insert(QStringLiteral("phone"), QStringLiteral("123456789"));
    QJsonDbCreateRequest create(item);
    mConnection->send(&create);
    QVERIFY(waitForResponse(&create));
    QVERIFY(!mRequestErrors.contains(&create));

    QList<QJsonObject> res = create.takeResults();
    QCOMPARE(res.count(), 1);

    item.insert(QStringLiteral("_uuid"), res.at(0).value(QStringLiteral("_uuid")));
    item.insert(QStringLiteral("_version"), res.at(0).value(QStringLiteral("_version")));

    QBENCHMARK_ONCE {
        BENCH_BEGIN_TAG;
        QJsonDbRemoveRequest remove(item);
        mConnection->send(&remove);
        QVERIFY(waitForResponse(&remove));
        QVERIFY(!mRequestErrors.contains(&remove));
        BENCH_END_TAG;
    }
}

void ClientBenchmark::removeThousandItems()
{
    for (int i = 0; i < gMany; i++) {
        QJsonObject item;
        item.insert(QStringLiteral("_type"), QStringLiteral("removeThousandItems"));
        item.insert(QStringLiteral("number"), qrand() % gMany);
        item.insert(QStringLiteral("idNumber"), i);
        QJsonDbCreateRequest create(item);
        mConnection->send(&create);
        QVERIFY(waitForResponse(&create));
        QVERIFY(!mRequestErrors.contains(&create));
    }


    QBENCHMARK_ONCE {
        BENCH_BEGIN_TAG;
        QJsonDbReadRequest read;
        read.setQuery(QStringLiteral("[?_type=\"removeThousandItems\"]"));
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        QVERIFY(!mRequestErrors.contains(&read));

        QList<QJsonObject> items = read.takeResults();
        QCOMPARE(items.count(), gMany);

        for (int i = 0; i < gMany; i++) {
            QJsonDbRemoveRequest remove(items.at(i));
            mConnection->send(&remove);
            QVERIFY(waitForResponse(&remove));
            QVERIFY(!mRequestErrors.contains(&remove));
        }
        BENCH_END_TAG;
    }
}

void ClientBenchmark::manyWatchers()
{
    static const int NumWatchers = 50;
    QList<QJsonDbWatcher *> watchers;
    for (int i = 0; i < NumWatchers; ++i) {
        QJsonDbWatcher *w = new QJsonDbWatcher;
        w->setQuery(QStringLiteral("[?_type=%type]"));
        w->bindValue(QStringLiteral("type"), QStringLiteral("manyWatchers")+QString::number(i));
        watchers.append(w);
        QVERIFY(mConnection->addWatcher(w));
    }
    waitForStatus(watchers.last(), QJsonDbWatcher::Active);
    QVERIFY(watchers.first()->status() == QJsonDbWatcher::Active);

    QBENCHMARK {
        QJsonDbObject item;
        item.setUuid(QUuid::createUuid());
        item.insert(QStringLiteral("_type"), QStringLiteral("manyWatchers1"));
        QJsonDbWriteRequest request;
        request.setObject(item);
        QVERIFY(mConnection->send(&request));
        QVERIFY(waitForResponse(&request));
    }

    foreach (QJsonDbWatcher *w, watchers) {
        QVERIFY(mConnection->removeWatcher(w));
        waitForStatus(w, QJsonDbWatcher::Inactive);
    }
}

QTEST_MAIN(ClientBenchmark)

#include "client-benchmark.moc"
