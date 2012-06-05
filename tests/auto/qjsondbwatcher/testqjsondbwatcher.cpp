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

#include <QCoreApplication>
#include <QFile>
#include <QList>
#include <QTest>
#include <QProcess>
#include <QEventLoop>
#include <QDebug>
#include <QTimer>
#include <QUuid>

#include "qjsonobject.h"
#include "qjsonvalue.h"
#include "qjsonarray.h"
#include "qjsondocument.h"

#include "qjsondbconnection.h"
#include "qjsondbobject.h"
#include "qjsondbreadrequest.h"
#include "qjsondbwatcher.h"
#include "qjsondbwriterequest.h"
#include "private/qjsondbstrings_p.h"
#include "private/qjsondbstandardpaths_p.h"

#include "testhelper.h"

QT_USE_NAMESPACE_JSONDB

// #define EXTRA_DEBUG

class TestQJsonDbWatcher: public TestHelper
{
    Q_OBJECT
    public:
    TestQJsonDbWatcher();
    ~TestQJsonDbWatcher();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void createAndRemove_data();
    void createAndRemove();
    void indexValue_data();
    void indexValue();
    void history();
    void currentState();
    void notificationTriggersView();
    void notificationTriggersMapReduce();
    void notificationTriggersMultiViews();
    void typeChangeEagerViewSource();
    void invalid();
    void privatePartition();
    void addAndRemove();
    void removeWatcherStatus();
};

TestQJsonDbWatcher::TestQJsonDbWatcher()
{
}

TestQJsonDbWatcher::~TestQJsonDbWatcher()
{
}

void TestQJsonDbWatcher::initTestCase()
{
    QJsonDbStandardPaths::setAutotestMode(true);
    removeDbFiles();

    QStringList arg_list = QStringList() << "-validate-schemas";
    launchJsonDbDaemon(arg_list, __FILE__);
}

void TestQJsonDbWatcher::cleanupTestCase()
{
    removeDbFiles();
    stopDaemon();
}

void TestQJsonDbWatcher::init()
{
    clearHelperData();
    connectToServer();
}

void TestQJsonDbWatcher::cleanup()
{
    disconnectFromServer();
}

void TestQJsonDbWatcher::createAndRemove_data()
{
    QTest::addColumn<QString>("partition");

    QTest::newRow("persistent") << "";
    QTest::newRow("ephemeral") << "Ephemeral";
}

/*
 * Watch for an item creation
 */

void TestQJsonDbWatcher::createAndRemove()
{
    QVERIFY(mConnection);
    QFETCH(QString, partition);

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=%type]"));
    watcher.bindValue(QLatin1String("type"), QLatin1String("com.test.qjsondbwatcher-test"));
    watcher.setPartition(partition);
    mConnection->addWatcher(&watcher);
    QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));

    QJsonObject item;
    item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
    item.insert(QLatin1String("create-test"), 22);

    // Create an item
    QJsonDbCreateRequest request(item);
    request.setPartition(partition);
    mConnection->send(&request);
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

    QList<QJsonObject> results = request.takeResults();
    QCOMPARE(results.size(), 1);
    QJsonObject info = results.at(0);
    item.insert(JsonDbStrings::Property::uuid(), info.value(JsonDbStrings::Property::uuid()));
    item.insert(JsonDbStrings::Property::version(), info.value(JsonDbStrings::Property::version()));

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);

    // remove the object
    item.remove(QLatin1String("create-test"));
    QJsonDbRemoveRequest remove(item);
    remove.setPartition(partition);
    mConnection->send(&remove);
    QVERIFY(waitForResponseAndNotifications(&remove, &watcher, 1));

    notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);
    QJsonDbNotification n = notifications[0];
    QJsonObject o = n.object();

    // make sure we got notified on the right object
    QCOMPARE(o.value(JsonDbStrings::Property::uuid()), info.value(JsonDbStrings::Property::uuid()));
    QCOMPARE(o.value(QLatin1String("create-test")).toDouble(), 22.);

    // we do now expect a tombstone
    QVERIFY(o.contains(JsonDbStrings::Property::deleted()));

    mConnection->removeWatcher(&watcher);
}

void TestQJsonDbWatcher::indexValue_data()
{
    QTest::addColumn<QString>("partition");

    QTest::newRow("persistent") << "";
    QTest::newRow("ephemeral") << "Ephemeral";
}

void TestQJsonDbWatcher::indexValue()
{
    QVERIFY(mConnection);
    QFETCH(QString, partition);

    // create an index
    QJsonObject index;
    index.insert(JsonDbStrings::Property::type(), QLatin1String("Index"));
    index.insert(QLatin1String("name"), QLatin1String("create-test"));
    index.insert(QLatin1String("propertyName"), QLatin1String("create-test"));
    index.insert(QLatin1String("propertyType"), QLatin1String("string"));
    index.insert(QLatin1String("objectType"), QLatin1String("com.test.qjsondbwatcher-test"));
    QJsonDbCreateRequest create(index);
    mConnection->send(&create);
    QVERIFY(waitForResponse(&create));
    QList<QJsonObject> toDelete = create.takeResults();

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"][/create-test]"));
    watcher.setPartition(partition);
    mConnection->addWatcher(&watcher);
    QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));

    QJsonObject item;
    item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
    item.insert(QLatin1String("create-test"), 22);

    // Create an item
    QJsonDbCreateRequest request(item);
    request.setPartition(partition);
    mConnection->send(&request);
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

    QList<QJsonObject> results = request.takeResults();
    QCOMPARE(results.size(), 1);
    QJsonObject info = results.at(0);
    item.insert(JsonDbStrings::Property::uuid(), info.value(JsonDbStrings::Property::uuid()));
    item.insert(JsonDbStrings::Property::version(), info.value(JsonDbStrings::Property::version()));

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);
    QJsonDbNotification n = notifications[0];
    QJsonObject o = n.object();
    // verify _indexValue was set unless partition was Ephemeral
    if (partition != QLatin1String("Ephemeral")) {
        QVERIFY(o.contains(QLatin1String("_indexValue")));
        QCOMPARE(o.value(QLatin1String("_indexValue")), item.value(QLatin1String("create-test")));
    }

    // remove the object
    QJsonDbRemoveRequest remove(item);
    remove.setPartition(partition);
    mConnection->send(&remove);
    QVERIFY(waitForResponseAndNotifications(&remove, &watcher, 1));

    notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);
    n = notifications[0];
    o = n.object();

    // make sure we got notified on the right object
    QCOMPARE(o.value(JsonDbStrings::Property::uuid()), info.value(JsonDbStrings::Property::uuid()));
    QCOMPARE(o.value(QLatin1String("create-test")).toDouble(), 22.);

    // verify the index value was set unless partition was Ephemeral
    if (partition != QLatin1String("Ephemeral")) {
        QVERIFY(o.contains(QLatin1String("_indexValue")));
        QCOMPARE(o.value(QLatin1String("_indexValue")), item.value(QLatin1String("create-test")));
    }

    // we do now expect a tombstone
    QVERIFY(o.contains(JsonDbStrings::Property::deleted()));

    mConnection->removeWatcher(&watcher);
}

void TestQJsonDbWatcher::history()
{
    QVERIFY(mConnection);

    QFile dataFile(":/partition/json/largeContactsTest.json");
    QVERIFY(dataFile.exists());
    dataFile.open(QIODevice::ReadOnly);
    QByteArray json = dataFile.readAll();
    QVERIFY(json.size());
    dataFile.close();
    QJsonDocument doc(QJsonDocument::fromJson(json));
    QVERIFY(doc.isArray());
    QJsonArray array = doc.array();
    // make a request and connect it
    quint32 firstStateNumber = 0;

    // pass the empty object list to make the constructor happy
    QList<QJsonObject> objects;
    QJsonDbCreateRequest request(objects);

    for (int i = 0; i < qMin(100, array.size()); i++) {
        QJsonObject item = array.at(i).toObject();

        item.insert(JsonDbStrings::Property::uuid(), QUuid::createUuid().toString());
        item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
        item.insert(QLatin1String("i"), i);

        QJsonObject otherItem(item);
        otherItem.insert(JsonDbStrings::Property::uuid(), QUuid::createUuid().toString());
        otherItem.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test-other"));
        item.insert(QLatin1String("i"), i);

        request.setObjects(QList<QJsonObject>() << item << otherItem);

        // Create the items
        mConnection->send(&request);
        QVERIFY(waitForResponse(&request));
        if (!firstStateNumber)
            firstStateNumber = request.stateNumber();

        objects.append(request.takeResults());
    }
    QVERIFY(firstStateNumber);

    // create a historical watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));

    // set the starting state
    watcher.setInitialStateNumber(firstStateNumber-1);
    mConnection->addWatcher(&watcher);

    // expecting one notification per com.test.qjsondbwatcher-test object
    QVERIFY(waitForResponseAndNotifications(0, &watcher, objects.size() / 2));
    QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), objects.size() / 2);
    // we received one Create notification per object
    foreach (const QJsonDbNotification n, notifications) {
        QCOMPARE(n.action(), QJsonDbWatcher::Created);
        QVERIFY(n.stateNumber() >= firstStateNumber);
    }

    mConnection->removeWatcher(&watcher);

    {
        // create a historical watcher with an "in" query matching one test type
        QJsonDbWatcher watcherIn;
        watcherIn.setWatchedActions(QJsonDbWatcher::All);
        watcherIn.setQuery(QLatin1String("[?_type in [\"com.test.qjsondbwatcher-test\", \"com.test.qjsondbwatcher-test2\"]]"));

        // set the starting state
        watcherIn.setInitialStateNumber(firstStateNumber-1);
        mConnection->addWatcher(&watcherIn);

        // expecting one notification per com.test.qjsondbwatcher-test object
        QVERIFY(waitForResponseAndNotifications(0, &watcherIn, objects.size() / 2));
        QVERIFY(waitForStatus(&watcherIn, QJsonDbWatcher::Active));

        notifications = watcherIn.takeNotifications();
        QCOMPARE(notifications.size(), objects.size() / 2);
        // we received one Create notification per object
        foreach (const QJsonDbNotification n, notifications) {
            QCOMPARE(n.action(), QJsonDbWatcher::Created);
            QVERIFY(n.stateNumber() >= firstStateNumber);
        }

        mConnection->removeWatcher(&watcherIn);
    }

    {
        // create a historical watcher with an "in" query matching both test types
        QJsonDbWatcher watcherIn;
        watcherIn.setWatchedActions(QJsonDbWatcher::All);
        watcherIn.setQuery(QLatin1String("[?_type in [\"com.test.qjsondbwatcher-test\", \"com.test.qjsondbwatcher-test-other\"]]"));

        // set the starting state
        watcherIn.setInitialStateNumber(firstStateNumber-1);
        mConnection->addWatcher(&watcherIn);

        // expecting two notification per original object
        QVERIFY(waitForResponseAndNotifications(0, &watcherIn, objects.size()));
        QVERIFY(waitForStatus(&watcherIn, QJsonDbWatcher::Active));

        notifications = watcherIn.takeNotifications();
        QCOMPARE(notifications.size(), objects.size());
        // we received one Create notification per object
        foreach (const QJsonDbNotification n, notifications) {
            QCOMPARE(n.action(), QJsonDbWatcher::Created);
            QVERIFY(n.stateNumber() >= firstStateNumber);
        }

        mConnection->removeWatcher(&watcherIn);
    }

    {
        // create a historical watcher with an "in" query matching both test types but with constraint [?i < 50]
        QJsonDbWatcher watcherIn;
        watcherIn.setWatchedActions(QJsonDbWatcher::All);
        watcherIn.setQuery(QLatin1String("[?_type in [\"com.test.qjsondbwatcher-test\", \"com.test.qjsondbwatcher-test-other\"]][?i < 50]"));

        // set the starting state
        watcherIn.setInitialStateNumber(firstStateNumber-1);
        mConnection->addWatcher(&watcherIn);

        // expecting two notification per original object
        QVERIFY(waitForResponseAndNotifications(0, &watcherIn, objects.size() / 2));
        QVERIFY(waitForStatus(&watcherIn, QJsonDbWatcher::Active));

        notifications = watcherIn.takeNotifications();
        QCOMPARE(notifications.size(), objects.size() / 2);
        // we received one Create notification per object
        foreach (const QJsonDbNotification n, notifications) {
            QCOMPARE(n.action(), QJsonDbWatcher::Created);
            QVERIFY(n.stateNumber() >= firstStateNumber);
            QVERIFY(n.object().value(QLatin1String("i")).toDouble() < 50);
        }

        mConnection->removeWatcher(&watcherIn);
    }

    // create a new historical watcher that should retrieve all the changes
    QJsonDbWatcher watcher2;
    watcher2.setWatchedActions(QJsonDbWatcher::All);
    watcher2.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));
    watcher2.setInitialStateNumber(0);

    mConnection->addWatcher(&watcher2);
    QVERIFY(waitForResponseAndNotifications(0, &watcher2, objects.size() / 2));

    QList<QJsonDbNotification> notifications2 = watcher2.takeNotifications();
    QCOMPARE(notifications2.size(), objects.size() / 2);
    // we received one Create notification per object
    foreach (const QJsonDbNotification n, notifications2)
        QCOMPARE(n.action(), QJsonDbWatcher::Created);

    // create another one
    quint32 stateNumberBeforeLastCreate = watcher2.lastStateNumber();

    {
        QJsonObject item;
        item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
        item.insert(JsonDbStrings::Property::uuid(), QUuid::createUuid().toString());
        item.insert("anotherone", QLatin1String("abc"));
        QJsonDbCreateRequest request(item);
        mConnection->send(&request);
        // wait for response from request, one create notification
        QVERIFY(waitForResponseAndNotifications(&request, &watcher2, 1));

        QList<QJsonObject> results = request.takeResults();
        QCOMPARE(results.size(), 1);
        watcher2.takeNotifications();

        QJsonObject item2;
        item2.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
        item2.insert(QLatin1String("_uuid"), results.at(0).value(QLatin1String("_uuid")));
        item2.insert(QLatin1String("_version"), results.at(0).value(QLatin1String("_version")));
        item2.insert("anotherone", QLatin1String("def"));
        item2.insert("field42", QLatin1String("HoG"));

        QJsonDbUpdateRequest update(item2);
        mConnection->send(&update);
        // wait for response from request, one create notification
        QVERIFY(waitForResponseAndNotifications(&update, &watcher2, 1));
        QList<QJsonDbNotification> notifications = watcher2.takeNotifications();
    }
    mConnection->removeWatcher(&watcher2);

    {
        // now verify that queries are being tested for stateNumber > 0
        QJsonDbWatcher watcher22;
        watcher22.setWatchedActions(QJsonDbWatcher::All);
        watcher22.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"][?field42 = \"HoG\"]"));
        watcher22.setInitialStateNumber(stateNumberBeforeLastCreate+1);

        mConnection->addWatcher(&watcher22);
        QVERIFY(waitForResponseAndNotifications(0, &watcher22, 1));

        QList<QJsonDbNotification> notifications22 = watcher22.takeNotifications();
        QCOMPARE(notifications22.size(), 1);
    }

    {
        // now verify that queries are being tested for stateNumber > 0
        // and that changes are being summarized properly
        QJsonDbWatcher watcher22;
        watcher22.setWatchedActions(QJsonDbWatcher::All);
        watcher22.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"][?anotherone = \"def\"]"));
        watcher22.setInitialStateNumber(stateNumberBeforeLastCreate);

        mConnection->addWatcher(&watcher22);
        QVERIFY(waitForResponseAndNotifications(0, &watcher22, 1));

        QList<QJsonDbNotification> notifications22 = watcher22.takeNotifications();
        QCOMPARE(notifications22.size(), 1);
        QCOMPARE(notifications22[0].action(), QJsonDbWatcher::Created);
    }

    QJsonDbRemoveRequest remove(objects);
    mConnection->send(&remove);
    QVERIFY(waitForResponse(&remove));
}


void TestQJsonDbWatcher::currentState()
{
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));

    // set the starting state to 0 to get the current state
    watcher.setInitialStateNumber(0);
    mConnection->addWatcher(&watcher);

    // expecting one notification per create
    QVERIFY(waitForResponseAndNotifications(0, &watcher, 1, 1));
    watcher.takeNotifications();

    // now create another object
    QJsonObject item;
    item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
    item.insert("create-test", 22);

    // Create an item
    QJsonDbCreateRequest request(item);
    mConnection->send(&request);
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);

    mConnection->removeWatcher(&watcher);

    QJsonDbRemoveRequest remove(request.takeResults());
    mConnection->send(&remove);
    QVERIFY(waitForResponse(&remove));
}

void TestQJsonDbWatcher::notificationTriggersView()
{
    QVERIFY(mConnection);

    QLatin1String query("[?_type=\"com.test.TestView\"]");
    QJsonArray array(readJsonFile(":/partition/json/map-array-conversion.json").array());

    QList<QJsonObject> objects;
    foreach (const QJsonValue v, array)
        objects.append(v.toObject());
    QJsonObject testObject;
    testObject.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.Test"));
    objects.append(testObject);

    // create the objects
    QJsonDbCreateRequest request(objects);
    mConnection->send(&request);
    QVERIFY(waitForResponse(&request));
    QList<QJsonObject> toDelete;
    foreach (const QJsonObject result, request.takeResults())
        toDelete.prepend(result);

    {
        QJsonDbReadRequest read(query);
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        QList<QJsonObject> objects = read.takeResults();
        int numObjects = objects.size();
        QCOMPARE(numObjects, 1);
    }

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String(query));
    mConnection->addWatcher(&watcher);
    QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));

    {
        QJsonObject item;
        item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.Test"));
        QJsonDbCreateRequest request(item);
        mConnection->send(&request);
        QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

        QList<QJsonDbNotification> notifications = watcher.takeNotifications();
        QCOMPARE(notifications.size(), 1);
        QJsonDbNotification n = notifications[0];
        QJsonObject o = n.object();
        // make sure we got notified on the right object
        //QCOMPARE(o.value(JsonDbStrings::Property::uuid()), info.value(JsonDbStrings::Property::uuid()));
    }
    mConnection->removeWatcher(&watcher);

    foreach (const QJsonObject &object, request.takeResults())
        toDelete.prepend(object);

    QJsonDbRemoveRequest remove(toDelete);
    mConnection->send(&remove);
    QVERIFY(waitForResponse(&remove));
}

void TestQJsonDbWatcher::notificationTriggersMapReduce()
{
    QVERIFY(mConnection);

    QJsonParseError error;
    QJsonArray array(readJsonFile(":/partition/json/map-reduce.json", &error).array());
    QVERIFY(error.error == QJsonParseError::NoError);
    QList<QJsonObject> objects;
    foreach (const QJsonValue v, array)
        objects.append(v.toObject());

    // create the objects
    QJsonDbCreateRequest request(objects);
    mConnection->send(&request);
    QVERIFY(waitForResponse(&request));
    QList<QJsonObject> toDelete;
    foreach (const QJsonObject result, request.takeResults())
        toDelete.prepend(result);

    QString query = QLatin1String("[?_type=\"Phone\"]");

    {
        QJsonDbReadRequest read(query);
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        int numObjects = read.takeResults().size();
        QCOMPARE(numObjects, 5);
    }

    query = QLatin1String("[?_type=\"PhoneCount\"]");

    {
        QJsonDbReadRequest read(query);
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        int numObjects = read.takeResults().size();
        QCOMPARE(numObjects, 5);
    }

    {
        const char json[] = "{\"_type\":\"Contact\",\"displayName\":\"Will Robinson\",\"phoneNumbers\":[{\"type\":\"satellite\",\"number\":\"+614159\"}]}";
        QJsonObject object(QJsonDocument::fromJson(json).object());

        QJsonDbCreateRequest write(object);
        mConnection->send(&write);
        QVERIFY(waitForResponse(&write));
        toDelete.append(write.takeResults());
    }

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(query);
    mConnection->addWatcher(&watcher);

    QVERIFY(waitForResponseAndNotifications(0, &watcher, 1));
    int numNotifications = watcher.takeNotifications().size();
    QCOMPARE(numNotifications, 1);

    {
        QJsonDbReadRequest read(query);
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));

        QList<QJsonObject> results = read.takeResults();
        QCOMPARE(results.size(), 6);
    }

    // now write another one
    {
        const char json[] = "{\"_type\":\"Contact\",\"displayName\":\"Jeffrey Goines\",\"phoneNumbers\":[{\"type\":\"satellite\",\"number\":\"+2718281828\"}]}";
        QJsonObject object(QJsonDocument::fromJson(json).object());

        QJsonDbCreateRequest write(object);
        mConnection->send(&write);
        QVERIFY(waitForResponseAndNotifications(&write, &watcher, 1));
        toDelete.append(write.takeResults());

        int numNotifications = watcher.takeNotifications().size();
        QCOMPARE(numNotifications, 1);
    }

    QJsonDbRemoveRequest remove(toDelete);
    mConnection->send(&remove);
    QVERIFY(waitForResponse(&remove));
}

void TestQJsonDbWatcher::notificationTriggersMultiViews()
{
    QVERIFY(mConnection);

    QLatin1String query1("[?_type=\"MultiMapView1\"]");
    QLatin1String query2("[?_type=\"MultiMapView2\"]");
    QJsonArray array(readJsonFile(":/partition/json/multi-map.json").array());

    QList<QJsonObject> objects;
    foreach (const QJsonValue v, array)
        objects.append(v.toObject());

    // create the objects
    QJsonDbCreateRequest request(objects);
    mConnection->send(&request);
    QVERIFY(waitForResponse(&request));
    QList<QJsonObject> toDelete;
    foreach (const QJsonObject result, request.takeResults())
        toDelete.prepend(result);

    {
        QJsonDbReadRequest read(query1);
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        QList<QJsonObject> objects = read.takeResults();
        int numObjects = objects.size();
        QCOMPARE(numObjects, 0);
    }

    // create two watchers
    QJsonDbWatcher watcher1;
    watcher1.setWatchedActions(QJsonDbWatcher::All);
    watcher1.setQuery(QLatin1String(query1));
    mConnection->addWatcher(&watcher1);
    QVERIFY(waitForStatus(&watcher1, QJsonDbWatcher::Active));

    QJsonDbWatcher watcher2;
    watcher2.setWatchedActions(QJsonDbWatcher::All);
    watcher2.setQuery(QLatin1String(query2));
    mConnection->addWatcher(&watcher2);
    QVERIFY(waitForStatus(&watcher2, QJsonDbWatcher::Active));

    {
        QJsonObject item;
        item.insert(JsonDbStrings::Property::type(), QLatin1String("MultiMapSourceType"));
        QJsonDbCreateRequest request(item);
        mConnection->send(&request);
        QVERIFY(waitForResponseAndNotifications(&request, &watcher1, 1));

        QList<QJsonDbNotification> notifications = watcher1.takeNotifications();
        QCOMPARE(notifications.size(), 1);
        QJsonDbNotification n = notifications[0];
        QJsonObject o = n.object();
        // make sure we got notified on the right object
        //QCOMPARE(o.value(JsonDbStrings::Property::uuid()), info.value(JsonDbStrings::Property::uuid()));

        // make sure watcher2 got one also
        notifications = watcher2.takeNotifications();
        QCOMPARE(notifications.size(), 1);

    }
    mConnection->removeWatcher(&watcher1);
    mConnection->removeWatcher(&watcher2);

    foreach (const QJsonObject &object, request.takeResults())
        toDelete.prepend(object);

    QJsonDbRemoveRequest remove(toDelete);
    mConnection->send(&remove);
    QVERIFY(waitForResponse(&remove));
}

void TestQJsonDbWatcher::typeChangeEagerViewSource()
{
    QVERIFY(mConnection);

    QJsonParseError error;
    QJsonArray array(readJsonFile(":/partition/json/map-reduce.json", &error).array());
    QVERIFY(error.error == QJsonParseError::NoError);
    QList<QJsonObject> objects;
    foreach (const QJsonValue v, array)
        objects.append(v.toObject());

    // create the objects
    QJsonDbCreateRequest request(objects);
    mConnection->send(&request);
    QVERIFY(waitForResponse(&request));
    QList<QJsonObject> toDelete;
    foreach (const QJsonObject result, request.takeResults())
        toDelete.prepend(result);

    QString query = QLatin1String("[?_type=\"PhoneCount\"]");

    // verify that we get what we expect
    {
        QJsonDbReadRequest read(query);
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        int numObjects = read.takeResults().size();
        QCOMPARE(numObjects, 5);
    }

    // create an object that's not of the source type of the view
    const char json[] = "{\"_type\":\"not.a.Contact\",\"displayName\":\"Will Robinson\",\"phoneNumbers\":[{\"type\":\"satellite\",\"number\":\"+614159\"}]}";
    QJsonDbObject object(QJsonDocument::fromJson(json).object());
    object.setUuid(QJsonDbObject::createUuidFromString("typeChangeEagerViewSource"));

    QJsonDbWriteRequest write;
    write.setObjects(QList<QJsonObject>() << object);
    mConnection->send(&write);
    QVERIFY(waitForResponse(&write));
    object.insert(QStringLiteral("_version"), write.takeResults().at(0).value(QStringLiteral("_version")).toString());

    // verify that the view didn't change
    {
        QJsonDbReadRequest read(query);
        mConnection->send(&read);
        QVERIFY(waitForResponse(&read));
        int numObjects = read.takeResults().size();
        QCOMPARE(numObjects, 5);
    }

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(query);
    mConnection->addWatcher(&watcher);
    QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));

    // change the object so that it's now a source type of the view
    object.insert(QLatin1String("_type"), QLatin1String("Contact"));
    write.setObjects(QList<QJsonObject>() << object);
    mConnection->send(&write);
    QVERIFY(waitForResponseAndNotifications(&write, &watcher, 1));
    object.insert(QStringLiteral("_version"), write.takeResults().at(0).value(QStringLiteral("_version")).toString());
    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.count(), 1);
    QCOMPARE(notifications[0].action(), QJsonDbWatcher::Created);

    // change it back, which should result in a remove notification
    object.insert(QLatin1String("_type"), QLatin1String("not.a.Contact"));
    write.setObjects(QList<QJsonObject>() << object);
    mConnection->send(&write);
    QVERIFY(waitForResponseAndNotifications(&write, &watcher, 1));
    notifications = watcher.takeNotifications();
    QCOMPARE(notifications.count(), 1);
    QCOMPARE(notifications[0].action(), QJsonDbWatcher::Removed);

    mConnection->removeWatcher(&watcher);
    QJsonDbRemoveRequest remove(toDelete);
    mConnection->send(&remove);
    QVERIFY(waitForResponse(&remove));
}

void TestQJsonDbWatcher::invalid()
{
    // missing query
    QJsonDbWatcher watcher;
    watcher.setQuery(QStringLiteral(""));
    mConnection->addWatcher(&watcher);
    QVERIFY(waitForError(&watcher, QJsonDbWatcher::MissingQuery));
    mConnection->removeWatcher(&watcher);

    // garbage query
    QJsonDbWatcher watcher2;
    watcher2.setQuery(QStringLiteral("not a valid query"));
    mConnection->addWatcher(&watcher2);
    QVERIFY(waitForError(&watcher2, QJsonDbWatcher::MissingQuery));
    mConnection->removeWatcher(&watcher2);

    // non-existent partition
    QJsonDbWatcher watcher3;
    watcher3.setQuery(QStringLiteral("[?_type=\"Contact\"]"));
    watcher3.setPartition(QStringLiteral("this.partition.does.not.exist"));
    mConnection->addWatcher(&watcher3);
    QVERIFY(waitForError(&watcher3, QJsonDbWatcher::InvalidPartition));
    mConnection->removeWatcher(&watcher3);

    // invalid initial state number
    QJsonDbWatcher watcher4;
    watcher4.setQuery(QStringLiteral("[?_type=\"Contact\"]"));
    watcher4.setInitialStateNumber(1234567);
    mConnection->addWatcher(&watcher4);
    QVERIFY(waitForError(&watcher4, QJsonDbWatcher::InvalidStateNumber));
    mConnection->removeWatcher(&watcher4);

    // unavailable partition
    QJsonDbWatcher watcher5;
    watcher5.setQuery(QStringLiteral("[?_type=\"Contact\"]"));
    watcher5.setPartition(QStringLiteral("com.qt-project.removable"));
    mConnection->addWatcher(&watcher5);
    QVERIFY(waitForError(&watcher5, QJsonDbWatcher::InvalidPartition));
    mConnection->removeWatcher(&watcher5);

    // watcher over multiple object tables
    {
        static const char schemaJson[] =
"{ \
    \"_type\": \"_schemaType\", \
    \"name\": \"invalidMapType\", \
    \"schema\": { \
        \"type\": \"object\", \
        \"extends\": {\"$ref\": \"View\"} \
     } \
}";
        QJsonDbObject schema = QJsonDocument::fromJson(QByteArray(schemaJson)).object();

        QJsonDbObject map;
        map.insert(QLatin1String("_type"), QLatin1String("Map"));
        map.insert(QLatin1String("targetType"), QLatin1String("invalidMapType"));
        QJsonObject submap;
        submap.insert(QLatin1String("com.test.indextest"), QLatin1String("function (o) { jsondb.emit(o); }"));
        map.insert(QLatin1String("join"), submap);

        QJsonDbCreateRequest request(QList<QJsonObject>() << schema << map);
        QVERIFY(mConnection->send(&request));
        QVERIFY(waitForResponse(&request));
        QJsonDbWatcher watcher6;
        watcher6.setQuery(QStringLiteral("[?_type=\"Contact\" | _type = \"invalidMapType\"]"));
        mConnection->addWatcher(&watcher6);
        QVERIFY(waitForError(&watcher6, QJsonDbWatcher::MissingQuery));
        mConnection->removeWatcher(&watcher6);
    }

    // watcher over multiple types in the same object table
    QJsonDbWatcher watcher7;
    watcher7.setQuery(QStringLiteral("[?_type in [\"Contact\", \"AnotherContact\"]]"));
    mConnection->addWatcher(&watcher7);
    QVERIFY(waitForStatus(&watcher7, QJsonDbWatcher::Active));
    mConnection->removeWatcher(&watcher7);
}

void TestQJsonDbWatcher::privatePartition()
{
    // watchers are not supported on private partitions, so it should fail
    QJsonDbWatcher privateWatcher;
    privateWatcher.setQuery("[?_type=\"foo\"]");
    privateWatcher.setPartition(QString::fromLatin1("%1.Private").arg(QJsonDbStandardPaths::currentUser()));
    QVERIFY(!mConnection->addWatcher(&privateWatcher));
}

void TestQJsonDbWatcher::addAndRemove()
{
    QJsonDbWatcher watcher;
    watcher.setQuery("[?_type=\"Foo\"]");
    QVERIFY(mConnection->addWatcher(&watcher));
    QVERIFY(mConnection->removeWatcher(&watcher));

    // a dummy request so that we can safely wait for the (internal) watcher
    // requests to be processed.
    QJsonDbReadRequest read(QStringLiteral("[?_type=\"Foo\"]"));
    QVERIFY(mConnection->send(&read));
    QVERIFY(waitForResponse(&read));
}

void TestQJsonDbWatcher::removeWatcherStatus()
{
    {
        QJsonDbWatcher watcher;
        watcher.setQuery("[?_type=\"Foo\"]");
        QVERIFY(mConnection->addWatcher(&watcher));
        QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));
        QVERIFY(mConnection->removeWatcher(&watcher));
        QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Inactive));
    }

    {
        QJsonDbWatcher watcher;
        watcher.setQuery("[?_type=\"Foo\"]");
        QVERIFY(mConnection->addWatcher(&watcher));
        QVERIFY(mConnection->removeWatcher(&watcher));
        QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Inactive));
    }
}

QTEST_MAIN(TestQJsonDbWatcher)

#include "testqjsondbwatcher.moc"
