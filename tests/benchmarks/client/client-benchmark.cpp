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
#include "client-benchmark.h"
#include <json.h>

#include "util.h"

static const char dbfile[] = "dbFile-test-jsondb";

int gMany = ::getenv("BENCHMARK_MANY") ? ::atoi(::getenv("BENCHMARK_MANY")) : 1000;

TestJson::TestJson()
    : mProcess(0)
{
}

TestJson::~TestJson()
{
}

void TestJson::removeDbFiles()
{
    // remove all the test files.
    QDir currentDir;
    QStringList nameFilter;
    nameFilter << QString("*.db");
    nameFilter << "objectFile.bin" << "objectFile2.bin";
    QFileInfoList databaseFiles = currentDir.entryInfoList(nameFilter, QDir::Files);
    foreach (QFileInfo fileInfo, databaseFiles) {
        //qDebug() << "Deleted : " << fileInfo.fileName();
        QFile file(fileInfo.fileName());
        file.remove();
    }
}

void TestJson::initTestCase()
{
#ifndef DONT_START_SERVER
    removeDbFiles();
    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << dbfile);
#endif

    connectToServer();

    QByteArray friendJson("{\"type\": \"object\", \"properties\": {\"name\": {\"type\": \"string\", \"indexed\": true}}}");

    JsonReader reader;

    // Create schemas for the items
    reader.parse(friendJson);
    QVariantMap friendSchema;
    friendSchema.insert("name", "Friends");
    friendSchema.insert("schema", reader.result());
    friendSchema.insert("_type", "_schemaType");
    int id = mClient->create(friendSchema);
    waitForResponse1(id);

    // Create alot of items in the database
    for (int k = 0; k < gMany; k += 100) {
        QVariantList friendsList;
        for (int i = k; i < k+100; i++) { 
            QVariantMap item;
            item.insert("_type", "Friends");
            item.insert("name", QString("Name-%1").arg(i));
            item.insert("phone",QString("%1").arg(qrand()));
            friendsList << item;
        }
        id = mClient->create(friendsList);
        waitForResponse1(id);
    }

    for (int k = 0; k < gMany; k += 100) {
        QVariantList imageList;
        for (int i = k; i < k+100; i++) { 
            QVariantMap item;
            item.insert("_type", "Image");
            item.insert("name", QString("Name-%1.jpg").arg(qrand()));
            item.insert("location",QString("/home/qt/Pictures/Myfolder-%1").arg(i));
            imageList << item;
        }
        id = mClient->create(imageList);
        waitForResponse1(id);
    }

    for (int k = 0; k < gMany; k += 100) {
        QVariantList numberList;
        for (int i = k; i < k+100; i++) { 
            QVariantMap item;
            item.insert("_type", "randNumber");
            item.insert("number", qrand()%gMany);
            item.insert("idNumber", i);
            numberList << item;
        }
        id = mClient->create(numberList);
        waitForResponse1(id);
    }
}

void TestJson::cleanupTestCase()
{
    if (mClient) {
        delete mClient;
        mClient = NULL;
    }

    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
        mProcess = NULL;
    }

    removeDbFiles();
}

void TestJson::createOneItem()
{
    QVariantMap item;
    item.insert("_type", "Contact");
    item.insert("name", "Charlie");
    item.insert("phone","123456789");

    QBENCHMARK {
        int id = mClient->create(item);
        waitForResponse1(id);
    }
}


void TestJson::createThousandItems()
{
    QBENCHMARK {
        for (int i = 0; i < gMany; ++i) {
            QVariantMap item;
            item.insert("_type", "randNumber");
            item.insert("number", qrand()%100);
            item.insert("idNumber", i);
            int id = mClient->create(item);
            waitForResponse1(id);
        }
    }
}

void TestJson::findOneItem()
{
    QVariantMap item;
    item.insert("query","[?_type=\"Friends\"][?name=\"Name-49\"]");

    QBENCHMARK {
        int id = mClient->find(item);
        waitForResponse1(id);
    }

    QVariantMap mapResponse = mData.toMap();
    QVariantList items = mapResponse.value("data").toList();
    QVERIFY(items.count() == 1);
}

void TestJson::findThousandItems()
{
    QVariantMap item;
    item.insert("query","[?_type=\"Friends\"]");

    QBENCHMARK {
        int id = mClient->find(item);
        waitForResponse1(id);
    }

    QVariantMap mapResponse = mData.toMap();
    QVariantList items = mapResponse.value("data").toList();
    QVERIFY(items.count() >= gMany);
}

void TestJson::findThousandItemsSorted()
{
    QVariantMap item;
    item.insert("query","[?_type=\"Friends\"][/name]");

    QBENCHMARK {
        int id = mClient->find(item);
        waitForResponse1(id);
    }

    QVariantMap mapResponse = mData.toMap();
    QVariantList items = mapResponse.value("data").toList();
    QVERIFY(items.count() >= gMany);
}

void TestJson::queryOneItem()
{
    QString queryString("[?_type=\"Friends\"][?name=\"Name-49\"]");
    QBENCHMARK {
        int id = mClient->query(queryString);
        waitForResponse1(id);
    }

    QVariantMap mapResponse = mData.toMap();
    QVariantList items = mapResponse.value("data").toList();
    QVERIFY(items.count() == 1);
}

void TestJson::queryThousandItems_data()
{
    QTest::addColumn<QString>("queryString");
    QTest::newRow("Friends") << QString("[?_type=\"Friends\"]");
    QTest::newRow("Friends[=_uuid]") << QString("[?_type=\"Friends\"][=_uuid]");
    QTest::newRow("Friends[={_uuid:_uuid}]") << QString("[?_type=\"Friends\"][={_uuid:_uuid}]");
}

void TestJson::queryThousandItems()
{
    QFETCH(QString, queryString);
    QBENCHMARK {
        int id = mClient->query(queryString);
        waitForResponse1(id);
    }

    QVariantMap mapResponse = mData.toMap();
    QVariantList items = mapResponse.value("data").toList();
    QVERIFY(items.count() >= gMany);
}

void TestJson::updateOneItem()
{
    QVariantMap item;
    item.insert("_type", "Contact");
    item.insert("name", "Markus");
    item.insert("phone","123456789");
    int id = mClient->create(item);
    waitForResponse1(id);

    item.insert("phone","111122223");
    item.insert("_uuid", mLastUuid);

    QBENCHMARK {
        int id = mClient->update(item);
        waitForResponse1(id);
    }
}

void TestJson::updateThousandItems()
{
    // create thousand items
    for (int i = 0; i < gMany; i++) {
        QVariantMap item;
        item.insert("_type", "updateThousandItems");
        item.insert("number", qrand()%gMany);
        item.insert("idNumber", i);
        int id = mClient->create(item);
        waitForResponse1(id);
    }

    QBENCHMARK {
        QString queryString("[?_type=\"updateThousandItems\"]");
        int id = mClient->query(queryString);
        waitForResponse1(id);

        QVariantMap mapResponse = mData.toMap();
        QVariantList items = mapResponse.value("data").toList();
        QCOMPARE(items.count(), gMany);
        for (int i = 0; i < gMany; i++) {
            QVariantMap mapItem = items.at(i).toMap();
            mapItem.insert("number",gMany+i);
            int id = mClient->update(mapItem);
            waitForResponse1(id);
        }
    }

    // remove those items
    QString queryString("[?_type=\"updateThousandItems\"]");
    int id = mClient->remove(queryString);
    waitForResponse1(id);
}

void TestJson::removeOneItem()
{
    QVariantMap item;
    item.insert("_type", "Contact");
    item.insert("name", "Marius");
    item.insert("phone","123456789");
    int id = mClient->create(item);
    waitForResponse1(id);

    item.insert("_uuid", mLastUuid);
    QBENCHMARK_ONCE {
        int id = mClient->remove(item);
        waitForResponse1(id);
    }
}

void TestJson::removeThousandItems()
{
    for (int i = 0; i < gMany; i++) {
        QVariantMap item;
        item.insert("_type", "removeThousandItems");
        item.insert("number", qrand()%gMany);
        item.insert("idNumber", i);
        int id = mClient->create(item);
        waitForResponse1(id);
    }

    QString queryString("[?_type=\"removeThousandItems\"]");
    QBENCHMARK_ONCE {
        int id = mClient->query(queryString);
        waitForResponse1(id);

        QVariantMap mapResponse = mData.toMap();
        QVariantList items = mapResponse.value("data").toList();
        QCOMPARE(items.count(), gMany);
        for (int i = 0; i < gMany; i++) {
            QVariantMap mapItem = items.at(i).toMap();
            int id = mClient->remove(mapItem);
            waitForResponse1(id);
        }
    }
}

void TestJson::removeWithQuery()
{
    for (int i = 0; i < gMany; i++) {
        QVariantMap item;
        item.insert("_type", "removeThousandItems");
        item.insert("number", qrand()%gMany);
        item.insert("idNumber", i);
        int id = mClient->create(item);
        waitForResponse1(id);
    }

    QString queryString("[?_type=\"removeThousandItems\"]");
    QBENCHMARK_ONCE {
        int id = mClient->remove(queryString);
        waitForResponse1(id);
    }

    QVariantMap mapResponse = mData.toMap();
    QCOMPARE(mapResponse.value("count").toInt(), gMany);
    QVariantList items = mapResponse.value("data").toList();
    QVERIFY(items.count() >= gMany);
}

void TestJson::notifyUpdateOneItem()
{
    QVariantMap item;
    item.insert("_type", "Friends");
    item.insert("name", "Malte");
    item.insert("phone","123456789");
    int id = mClient->create(item);
    waitForResponse1(id);

    QString itemUuid = mLastUuid;

    QVariantMap item2;
    item2.insert("_type","notification");
    QString queryString;
    queryString = QString("[?_uuid=\"%1\"]").arg(mLastUuid);
    item2.insert("query",queryString);
    QVariantList actions;
    actions << "update";
    item2.insert("actions",actions);
    id = mClient->create(item2);
    waitForResponse1(id);

    QString notifyUuid = mLastUuid;
    QVERIFY(!notifyUuid.isEmpty());

    item.insert("phone","111122223");
    item.insert("_uuid", itemUuid );

    int i = 0;
    QBENCHMARK {
        item.insert("phone",QString("11112%1").arg(i++, 4));
        int id = mClient->update(item);
        waitForResponse4(id, -1, notifyUuid, 1);
    }

    // remove notification object
    item2.insert("_uuid",notifyUuid);
    id = mClient->remove(item2);
    waitForResponse1(id);
}

void TestJson::notifyCreateOneItem()
{
    QVariantMap item;
    QVariantList actions;
    actions << "create";
    item.insert("_type","notification");
    item.insert("query","[?_type=\"Friends\"]");
    item.insert("actions",actions);
    int id = mClient->create(item);
    waitForResponse1(id);

    QString notifyUuid = mLastUuid;
    QVERIFY(!notifyUuid.isEmpty());

    QVariantMap item2;
    item2.insert("_type", "Friends");
    item2.insert("name", "Marve");
    item2.insert("phone","123456789");

    QBENCHMARK {
        int id = mClient->create(item2);
        waitForResponse4(id, -1, notifyUuid, 1);
    }

    // remove notification object
    item.insert("_uuid",notifyUuid);
    id = mClient->remove(item);
    waitForResponse1(id);
}

void TestJson::notifyRemoveOneItem()
{
    QVariantMap item;
    item.insert("_type", "Friends");
    item.insert("name", "Micke");
    item.insert("phone","123456789");
    int id = mClient->create(item);
    waitForResponse1(id);

    QString itemUuid = mLastUuid;
    QVERIFY(!itemUuid.isEmpty());

    QVariantMap item2;
    QVariantList actions;
    actions << "remove";
    item2.insert("_type","notification");
    item2.insert("query","[?_type=\"Friends\"]");
    item2.insert("actions",actions);
    id = mClient->create(item2);
    waitForResponse1(id);

    QString notifyUuid = mLastUuid;
    QVERIFY(!notifyUuid.isEmpty());

    item.insert("_uuid", itemUuid);

    QBENCHMARK_ONCE {
        int id = mClient->remove(item);
        waitForResponse4(id, -1, notifyUuid, 1);
    }

    // remove notification object
    item2.insert("_uuid",notifyUuid);
    id = mClient->remove(item2);
    waitForResponse1(id);
}

QTEST_MAIN(TestJson)
