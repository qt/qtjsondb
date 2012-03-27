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
#include "qjsondbwatcher.h"
#include "qjsondbwriterequest.h"
#include "private/qjsondbstrings_p.h"

#include "util.h"

QT_USE_NAMESPACE_JSONDB

// #define EXTRA_DEBUG

// #define DONT_START_SERVER

class TestQJsonDbWatcher: public QObject
{
    Q_OBJECT
    public:
    TestQJsonDbWatcher();
    ~TestQJsonDbWatcher();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void createAndRemove();
    void history();
    void currentState();
    void notificationTriggersView();

public slots:
    // from mConnection
    void error(QtJsonDb::QJsonDbConnection::ErrorCode c, QString msg);
    void statusChanged(QtJsonDb::QJsonDbConnection::Status status);
    void disconnected();

    // from a request
    void onRequestFinished();
    void onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString);
    void onRequestStatusChanged(QtJsonDb::QJsonDbRequest::Status);
    // from a watcher
    void onWatcherNotificationsAvailable(int);
    void onWatcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status);
    void onWatcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString);
private:
    void removeDbFiles();

private:
#ifndef DONT_START_SERVER
    QProcess *mProcess;
#endif
    QJsonDbConnection *mConnection;
    QEventLoop         mEventLoop;
    // what we're waiting for
    QJsonDbRequest    *mRequest;
    int                mNotificationCount;
    int                mNotificationsReceived;

};

#ifndef DONT_START_SERVER
static const char dbfileprefix[] = "test-jsondb-watcher";
#endif

// this should go into a new version of clientwrapper.h
#define waitForResponseAndNotification(_request, _count)        \
    {                                                           \
        mRequest = _request;                                    \
        mNotificationCount = _count;                            \
        mNotificationsReceived = 0;                             \
        mEventLoop.exec(QEventLoop::AllEvents);                 \
    }

#define waitForWatcherStatus(_watcher, _status)                 \
    {                                                           \
        while (_watcher.status() != _status)                   \
            mEventLoop.processEvents(QEventLoop::AllEvents);    \
    }

TestQJsonDbWatcher::TestQJsonDbWatcher()
    : mProcess(0)
{
}

TestQJsonDbWatcher::~TestQJsonDbWatcher()
{
}

void TestQJsonDbWatcher::error(QtJsonDb::QJsonDbConnection::ErrorCode c, QString msg)
{
    qCritical() << "Error from connection" << c << msg;
}

void TestQJsonDbWatcher::statusChanged(QtJsonDb::QJsonDbConnection::Status status)
{
    Q_UNUSED(status);
}

void TestQJsonDbWatcher::disconnected()
{
    qCritical() << "Disconnected from jsondb";
}

// this should go into a new version of clientwrapper.h
void TestQJsonDbWatcher::onRequestFinished()
{
    mRequest = 0;
    if (mNotificationCount <= mNotificationsReceived)
        mEventLoop.quit();
}

// this should go into a new version of clientwrapper.h
void TestQJsonDbWatcher::onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode code, QString msg)
{
    qCritical() << "onRequestError" << code << msg;
    mEventLoop.quit();
}
void TestQJsonDbWatcher::onRequestStatusChanged(QtJsonDb::QJsonDbRequest::Status status)
{
    Q_UNUSED(status);
    //qDebug() << "onRequestStatusChanged" << status;
}

// this should go into a new version of clientwrapper.h
void TestQJsonDbWatcher::onWatcherNotificationsAvailable(int count)
{
    mNotificationsReceived = count;
    if (mRequest == 0 && mNotificationCount <= mNotificationsReceived)
        mEventLoop.quit();
}

void TestQJsonDbWatcher::onWatcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status status)
{
    Q_UNUSED(status);
}

// this should go into a new version of clientwrapper.h
void TestQJsonDbWatcher::onWatcherError(QtJsonDb::QJsonDbWatcher::ErrorCode code, QString message)
{
    qCritical() << "onWatcherError" << code << message;
    mEventLoop.quit();
}

void TestQJsonDbWatcher::removeDbFiles()
{
#ifndef DONT_START_SERVER
    QStringList lst = QDir().entryList(QStringList() << QLatin1String("*.db*"));
    lst << "objectFile.bin" << "objectFile2.bin";
    foreach (const QString &fileName, lst)
        QFile::remove(fileName);
#else
    qDebug("Don't forget to clean database files before running the test!");
#endif
}

void TestQJsonDbWatcher::initTestCase()
{
    removeDbFiles();

#ifndef DONT_START_SERVER
    QStringList arg_list = (QStringList()
                            << "-validate-schemas");
    arg_list << "-base-name";
    arg_list << QString::fromLatin1(dbfileprefix);
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, QString("testjsondb_%1").arg(getpid()), arg_list);
#endif
    mConnection = new QJsonDbConnection(this);
    connect(mConnection, SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(mConnection, SIGNAL(statusChanged(QtJsonDb::QJsonDbConnection::Status)),
            this, SLOT(statusChanged(QtJsonDb::QJsonDbConnection::Status)));

    connect(mConnection, SIGNAL(error(QtJsonDb::QJsonDbConnection::ErrorCode,QString)),
            this, SLOT(error(QtJsonDb::QJsonDbConnection::ErrorCode,QString)));

    mConnection->connectToServer();

}

void TestQJsonDbWatcher::cleanupTestCase()
{
    if (mConnection) {
        delete mConnection;
        mConnection = NULL;
    }

#ifndef DONT_START_SERVER
    if (mProcess) {
        mProcess->close();
        delete mProcess;
    }
    removeDbFiles();
#endif
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
    connect(&watcher, SIGNAL(notificationsAvailable(int)),
            this, SLOT(onWatcherNotificationsAvailable(int)));
    connect(&watcher, SIGNAL(statusChanged(QtJsonDb::QJsonDbWatcher::Status)),
            this, SLOT(onWatcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status)));
    connect(&watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)), this, SLOT(onWatcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    mConnection->addWatcher(&watcher);

    QJsonObject item;
    item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
    item.insert("create-test", 22);

    // Create an item
    QJsonDbCreateRequest request(item);
    connect(&request, SIGNAL(finished()), this, SLOT(onRequestFinished()));
    connect(&request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    mConnection->send(&request);
    waitForResponseAndNotification(&request, 1);

    QList<QJsonObject> results = request.takeResults();
    QCOMPARE(results.size(), 1);
    QJsonObject info = results.at(0);
    item.insert(JsonDbStrings::Property::uuid(), info.value(JsonDbStrings::Property::uuid()));
    item.insert(JsonDbStrings::Property::version(), info.value(JsonDbStrings::Property::version()));

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);

    // remove the object
    QJsonDbRemoveRequest remove(item);
    connect(&remove, SIGNAL(finished()), this, SLOT(onRequestFinished()));
    connect(&remove, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    mConnection->send(&remove);
    waitForResponseAndNotification(&remove, 1);

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
    quint32 lastStateNumber = 0;

    // pass the empty object list to make the constructor happy
    QList<QJsonObject> objects;
    QJsonDbCreateRequest request(objects);
    connect(&request, SIGNAL(finished()), this, SLOT(onRequestFinished()));
    connect(&request, SIGNAL(statusChanged(QtJsonDb::QJsonDbRequest::Status)),
            this, SLOT(onRequestStatusChanged(QtJsonDb::QJsonDbRequest::Status)));
    connect(&request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));

    for (int i = 0; i < qMin(100, array.size()); i++) {
        QJsonObject item = array.at(i).toObject();

        // why does QJsonDbCreate request require me to set the Uuid? /me thinks it's a bug
        item.insert(JsonDbStrings::Property::uuid(), QUuid::createUuid().toString());
        item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
        objects.append(item);

        request.setObjects(QList<QJsonObject>() << item);

        // Create the item
        mConnection->send(&request);
        waitForResponseAndNotification(&request, 0);
        if (!firstStateNumber)
            firstStateNumber = request.stateNumber();
        lastStateNumber = request.stateNumber();
    }
    QVERIFY(firstStateNumber);

    // create a historical watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));
    connect(&watcher, SIGNAL(notificationsAvailable(int)),
            this, SLOT(onWatcherNotificationsAvailable(int)));
    connect(&watcher, SIGNAL(statusChanged(QtJsonDb::QJsonDbWatcher::Status)),
            this, SLOT(onWatcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status)));
    connect(&watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)), this, SLOT(onWatcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));

    // set the starting state
    watcher.setInitialStateNumber(firstStateNumber-1);
    mConnection->addWatcher(&watcher);

    // expecting one notification per create and one state change
    waitForResponseAndNotification(0, objects.size()+1);
    waitForWatcherStatus(watcher, QJsonDbWatcher::Active);

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
    connect(&watcher2, SIGNAL(notificationsAvailable(int)),
            this, SLOT(onWatcherNotificationsAvailable(int)));
    connect(&watcher2, SIGNAL(statusChanged(QtJsonDb::QJsonDbWatcher::Status)),
            this, SLOT(onWatcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status)));
    connect(&watcher2, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)), this, SLOT(onWatcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    watcher2.setInitialStateNumber(-1);

    mConnection->addWatcher(&watcher2);
    waitForResponseAndNotification(0, objects.size() + 1);
    waitForWatcherStatus(watcher2, QJsonDbWatcher::Active);

    QList<QJsonDbNotification> notifications2 = watcher2.takeNotifications();
    QCOMPARE(notifications2.size(), objects.size()+1);
    // we received one Create notification per object
    foreach (const QJsonDbNotification n, notifications2.mid(0, objects.size()))
        QCOMPARE(n.action(), QJsonDbWatcher::Created);
    // we received one StateChanged notification
    QCOMPARE(notifications2.at(objects.size()).action(), QJsonDbWatcher::StateChanged);

    mConnection->removeWatcher(&watcher2);
}

void TestQJsonDbWatcher::currentState()
{
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.qjsondbwatcher-test\"]"));
    connect(&watcher, SIGNAL(notificationsAvailable(int)),
            this, SLOT(onWatcherNotificationsAvailable(int)));
    connect(&watcher, SIGNAL(statusChanged(QtJsonDb::QJsonDbWatcher::Status)),
            this, SLOT(onWatcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status)));
    connect(&watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)), this, SLOT(onWatcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));

    // set the starting state to -1 to get the current state
    watcher.setInitialStateNumber(static_cast<quint32>(-1));
    mConnection->addWatcher(&watcher);

    // expecting one notification per create and one state change
    bool stateChanged = false;
    while (!stateChanged) {
      // wait for a notification
      waitForResponseAndNotification(0, 1);
      QList<QJsonDbNotification> notifications = watcher.takeNotifications();
      foreach (const QJsonDbNotification n, notifications)
        if (n.action() == QJsonDbWatcher::StateChanged)
          stateChanged = true;
    }

    // now create another object
    {
        QJsonObject item;
        item.insert(JsonDbStrings::Property::type(), QLatin1String("com.test.qjsondbwatcher-test"));
        item.insert("create-test", 22);

        // Create an item
        QJsonDbCreateRequest request(item);
        connect(&request, SIGNAL(finished()), this, SLOT(onRequestFinished()));
        connect(&request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        mConnection->send(&request);
        waitForResponseAndNotification(&request, 1);
    }
    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);

    mConnection->removeWatcher(&watcher);
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
    connect(&request, SIGNAL(finished()), this, SLOT(onRequestFinished()));
    connect(&request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    mConnection->send(&request);
    waitForResponseAndNotification(&request, 0);

    // create a watcher
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.TestView\"]"));
    connect(&watcher, SIGNAL(notificationsAvailable(int)),
            this, SLOT(onWatcherNotificationsAvailable(int)));
    connect(&watcher, SIGNAL(statusChanged(QtJsonDb::QJsonDbWatcher::Status)),
            this, SLOT(onWatcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status)));
    connect(&watcher, SIGNAL(error(int,QString)), this, SLOT(onWatcherError(int,QString)));
    mConnection->addWatcher(&watcher);
    waitForResponseAndNotification(0, 1);

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 1);
    QJsonDbNotification n = notifications[0];
    QJsonObject o = n.object();
    // make sure we got notified on the right object
    //QCOMPARE(o.value(JsonDbStrings::Property::uuid()), info.value(JsonDbStrings::Property::uuid()));

    mConnection->removeWatcher(&watcher);
}

QTEST_MAIN(TestQJsonDbWatcher)

#include "testqjsondbwatcher.moc"
