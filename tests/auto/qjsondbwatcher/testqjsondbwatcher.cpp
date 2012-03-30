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
#include "qjsondbreadrequest.h"
#include "qjsondbwatcher.h"
#include "qjsondbwriterequest.h"
#include "private/qjsondbstrings_p.h"

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

    void createAndRemove();
    void history();
    void currentState();
    void notificationTriggersView();
    void notificationTriggersMapReduce();
};

static const char dbfileprefix[] = "test-jsondb-watcher";


TestQJsonDbWatcher::TestQJsonDbWatcher()
{
}

TestQJsonDbWatcher::~TestQJsonDbWatcher()
{
}

void TestQJsonDbWatcher::initTestCase()
{
    removeDbFiles();

    QStringList arg_list = QStringList() << "-validate-schemas";
    launchJsonDbDaemon(QString::fromLatin1(dbfileprefix), arg_list);
}

void TestQJsonDbWatcher::cleanupTestCase()
{
    removeDbFiles();
    stopDaemon();
}

void TestQJsonDbWatcher::init()
{
    connectToServer();
}

void TestQJsonDbWatcher::cleanup()
{
    disconnectFromServer();
}

/*
 * Watch for an item creation
 */

void TestQJsonDbWatcher::createAndRemove()
{
    QVERIFY(mConnection);

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));
    mConnection->addWatcher(&watcher);

    QJsonObject item;
    item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
    item.insert("create-test", 22);

    // Create an item
    QJsonDbCreateRequest request(item);
    mConnection->send(&request);
    waitForResponseAndNotifications(&request, &watcher, 1);

    QList<QJsonObject> results = request.takeResults();
    QCOMPARE(results.size(), 1);
    QJsonObject info = results.at(0);
    item.insert(JsonDbStrings::Property::uuid(), info.value(JsonDbStrings::Property::uuid()));
    item.insert(JsonDbStrings::Property::version(), info.value(JsonDbStrings::Property::version()));

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);

    // remove the object
    QJsonDbRemoveRequest remove(item);
    mConnection->send(&remove);
    waitForResponseAndNotifications(&remove, &watcher, 1);

    notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);
    QJsonDbNotification n = notifications[0];
    QJsonObject o = n.object();
    // make sure we got notified on the right object
    QCOMPARE(o.value(JsonDbStrings::Property::uuid()), info.value(JsonDbStrings::Property::uuid()));

    // we do now expect a tombstone
    QVERIFY(o.contains(JsonDbStrings::Property::deleted()));

    mConnection->removeWatcher(&watcher);
}

void TestQJsonDbWatcher::history()
{
    QVERIFY(mConnection);

    QFile dataFile(":/daemon/json/largeContactsTest.json");
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

        // why does QJsonDbCreate request require me to set the Uuid? /me thinks it's a bug
        item.insert(JsonDbStrings::Property::uuid(), QUuid::createUuid().toString());
        item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
        objects.append(item);

        request.setObjects(QList<QJsonObject>() << item);

        // Create the item
        mConnection->send(&request);
        waitForResponse(&request);
        if (!firstStateNumber)
            firstStateNumber = request.stateNumber();
    }
    QVERIFY(firstStateNumber);

    // create a historical watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));

    // set the starting state
    watcher.setInitialStateNumber(firstStateNumber-1);
    mConnection->addWatcher(&watcher);

    // expecting one notification per create and one state change
    waitForResponseAndNotifications(0, &watcher, objects.size()+1);
    waitForStatus(&watcher, QJsonDbWatcher::Active);

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), objects.size()+1);
    // we received one Create notification per object
    foreach (const QJsonDbNotification n, notifications.mid(0, objects.size()))
        QCOMPARE(n.action(), QJsonDbWatcher::Created);
    // we received one StateChanged notification
    QCOMPARE(notifications.at(objects.size()).action(), QJsonDbWatcher::StateChanged);

    mConnection->removeWatcher(&watcher);

    // create a new historical watcher that should retrieve all the changes
    QJsonDbWatcher watcher2;
    watcher2.setWatchedActions(QJsonDbWatcher::All);
    watcher2.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));
    watcher2.setInitialStateNumber(-1);

    mConnection->addWatcher(&watcher2);
    waitForResponseAndNotifications(0, &watcher2, objects.size() + 1);
    waitForStatus(&watcher2, QJsonDbWatcher::Active);

    QList<QJsonDbNotification> notifications2 = watcher2.takeNotifications();
    QCOMPARE(notifications2.size(), objects.size()+1);
    // we received one Create notification per object
    foreach (const QJsonDbNotification n, notifications2.mid(0, objects.size()))
        QCOMPARE(n.action(), QJsonDbWatcher::Created);
    // we received one StateChanged notification
    QCOMPARE(notifications2.at(objects.size()).action(), QJsonDbWatcher::StateChanged);

    mConnection->removeWatcher(&watcher2);

    QJsonDbRemoveRequest remove(objects);
    mConnection->send(&remove);
    waitForResponse(&remove);
}

void TestQJsonDbWatcher::currentState()
{
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));

    // set the starting state to -1 to get the current state
    watcher.setInitialStateNumber(static_cast<quint32>(-1));
    mConnection->addWatcher(&watcher);

    // expecting one notification per create and one state change
    bool stateChanged = false;
    while (!stateChanged) {
      // wait for a notification
      waitForResponseAndNotifications(0, &watcher, 1);
      QList<QJsonDbNotification> notifications = watcher.takeNotifications();
      foreach (const QJsonDbNotification n, notifications)
        if (n.action() == QJsonDbWatcher::StateChanged)
          stateChanged = true;
    }

    // now create another object
    QJsonObject item;
    item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
    item.insert("create-test", 22);

    // Create an item
    QJsonDbCreateRequest request(item);
    mConnection->send(&request);
    waitForResponseAndNotifications(&request, &watcher, 1);

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);

    mConnection->removeWatcher(&watcher);

    QJsonDbRemoveRequest remove(request.takeResults());
    mConnection->send(&remove);
    waitForResponse(&remove);
}

void TestQJsonDbWatcher::notificationTriggersView()
{
    QVERIFY(mConnection);

    QJsonParseError error;
    QJsonArray array(readJsonFile(":/daemon/json/map-array-conversion.json", &error).array());
    QVERIFY(error.error == QJsonParseError::NoError);
    QList<QJsonObject> objects;
    foreach (const QJsonValue v, array)
        objects.append(v.toObject());
    QJsonObject testObject;
    testObject.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.Test"));
    objects.append(testObject);

    // create the objects
    QJsonDbCreateRequest request(objects);
    mConnection->send(&request);
    waitForResponse(&request);

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.TestView\"]"));
    mConnection->addWatcher(&watcher);
    waitForResponseAndNotifications(0, &watcher, 1);
    waitForStatus(&watcher, QJsonDbWatcher::Active);

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);
    QJsonDbNotification n = notifications[0];
    QJsonObject o = n.object();
    // make sure we got notified on the right object
    //QCOMPARE(o.value(JsonDbStrings::Property::uuid()), info.value(JsonDbStrings::Property::uuid()));

    mConnection->removeWatcher(&watcher);

    QList<QJsonObject> toDelete;
    foreach (const QJsonObject &object, request.takeResults())
        toDelete.prepend(object);

    QJsonDbRemoveRequest remove(toDelete);
    mConnection->send(&remove);
    waitForResponse(&remove);
}

void TestQJsonDbWatcher::notificationTriggersMapReduce()
{
    QVERIFY(mConnection);

    QJsonParseError error;
    QJsonArray array(readJsonFile(":/daemon/json/map-reduce.json", &error).array());
    QVERIFY(error.error == QJsonParseError::NoError);
    QList<QJsonObject> objects;
    foreach (const QJsonValue v, array)
        objects.append(v.toObject());

    // create the objects
    QJsonDbCreateRequest request(objects);
    mConnection->send(&request);
    waitForResponse(&request);

    QString query = QLatin1String("[?_type=\"PhoneCount\"]");

    {
        QJsonDbReadRequest read(query);
        mConnection->send(&read);
        waitForResponse(&read);
        int numObjects = read.takeResults().size();
        QCOMPARE(numObjects, 5);
    }

    {
        const char json[] = "{\"_type\":\"Contact\",\"displayName\":\"Will Robinson\",\"phoneNumbers\":[{\"type\":\"satellite\",\"number\":\"+614159\"}]}";
        QJsonObject object(QJsonDocument::fromJson(json).object());

        QJsonDbCreateRequest write(object);
        mConnection->send(&write);
        waitForResponse(&write);
    }

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(query);
    mConnection->addWatcher(&watcher);

    waitForResponseAndNotifications(0, &watcher, 1);
    int numNotifications = watcher.takeNotifications().size();
    QCOMPARE(numNotifications, 1);

    {
        QJsonDbReadRequest read(query);
        mConnection->send(&read);
        waitForResponse(&read);

        QList<QJsonObject> results = read.takeResults();
        QCOMPARE(results.size(), 6);
    }

    // now write another one
    {
        const char json[] = "{\"_type\":\"Contact\",\"displayName\":\"Jeffrey Goines\",\"phoneNumbers\":[{\"type\":\"satellite\",\"number\":\"+2718281828\"}]}";
        QJsonObject object(QJsonDocument::fromJson(json).object());

        QJsonDbCreateRequest write(object);
        mConnection->send(&write);
        waitForResponseAndNotifications(&write, &watcher, 1);

        int numNotifications = watcher.takeNotifications().size();
        QCOMPARE(numNotifications, 1);
    }


    mConnection->removeWatcher(&watcher);
}

QTEST_MAIN(TestQJsonDbWatcher)

#include "testqjsondbwatcher.moc"
