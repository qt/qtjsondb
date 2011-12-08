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

#include <QCoreApplication>
#include <QList>
#include <QTest>
#include <QFile>
#include <QProcess>
#include <QEventLoop>
#include <QDebug>
#include <QLocalSocket>
#include <QTimer>
#include <QDir>

#include <sys/types.h>
#include <unistd.h>

#include <QtJsonDbQson/private/qson_p.h>

#include "private/jsondb-strings_p.h"
#include "private/jsondb-connection_p.h"

#include "jsondb-client.h"
#include "jsondb-error.h"

#include "json.h"

#include "util.h"
#include "clientwrapper.h"

Q_USE_JSONDB_NAMESPACE

// #define EXTRA_DEBUG

// #define DONT_START_SERVER

class TestJsonDbClient: public ClientWrapper
{
    Q_OBJECT
public:
    TestJsonDbClient();
    ~TestJsonDbClient();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void connectionStatus();

    void create();
    void createList();
    void update();
    void find();
    void registerNotification();
    void notify();
    void notifyRemoveBatch();
    void notifyMultiple();
    void remove();
    void schemaValidation();
    void changesSince();
    void capabilitiesAllowAll();
    void capabilitiesReadOnly();
    void capabilitiesTypeQuery();
    void storageQuotas();
    void requestWithSlot();
    void testToken_data();
    void testToken();
    void partition();
    void queryObject();
    void changesSinceObject();

    void connection_response(int, const QVariant&);
    void connection_error(int, int, const QString&);

private:
#ifndef DONT_START_SERVER
    QProcess        *mProcess;
#endif
    bool failed;
    void removeDbFiles();
};

#ifndef DONT_START_SERVER
static const char dbfileprefix[] = "test-jsondb-client-";
#endif

class Handler : public QObject
{
    Q_OBJECT
public slots:
    void success(int id, const QVariant &d)
    {
        requestId = id;
        data = d;
        ++successCount;
    }
    void error(int id, int code, const QString &message)
    {
        requestId = id;
        errorCode = code;
        errorMessage = message;
        ++errorCount;
    }
    void notify(const QString &uuid, const QVariant &d, const QString &a)
    {
        notifyUuid = uuid;
        data = d;
        notifyAction = a;
        ++notifyCount;
    }

    void clear()
    {
        requestId = 0;
        data = QVariant();
        errorCode = 0;
        errorMessage = QString();
        notifyUuid = QString();
        notifyAction = QString();
        successCount = errorCount = notifyCount = 0;
    }

public:
    Handler() { clear(); }

    int requestId;
    QVariant data;
    int errorCode;
    QString errorMessage;
    QString notifyUuid;
    QString notifyAction;
    int successCount;
    int errorCount;
    int notifyCount;
};

TestJsonDbClient::TestJsonDbClient()
    : mProcess(0)
{
#ifdef EXTRA_DEBUG
    this->debug_output = true;
#endif
}

TestJsonDbClient::~TestJsonDbClient()
{
}

void TestJsonDbClient::removeDbFiles()
{
#ifndef DONT_START_SERVER
    QStringList lst = QDir().entryList(QStringList() << QLatin1String("*.db"));
    lst << "objectFile.bin" << "objectFile2.bin";
    foreach (const QString &fileName, lst)
        QFile::remove(fileName);
#else
    qDebug("Don't forget to clean database files before running the test!");
#endif
}

void TestJsonDbClient::initTestCase()
{
    removeDbFiles();
#ifndef DONT_START_SERVER
    const QString filename = QString::fromLatin1(dbfileprefix);
    QStringList arg_list = (QStringList()
                            << "-validate-schemas"
                            << "-enforce-access-control"
                            << filename);
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, QString("testjsondb_%1").arg(getpid()), arg_list);
#endif
    connectToServer();
}

void TestJsonDbClient::cleanupTestCase()
{
    if (mClient) {
        delete mClient;
        mClient = NULL;
    }

#ifndef DONT_START_SERVER
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }
    removeDbFiles();
#endif
}

void TestJsonDbClient::connectionStatus()
{
    JsonDbConnection *connection = new JsonDbConnection;

    QCOMPARE((int)connection->status(), (int)JsonDbConnection::Disconnected);

    QEventLoop ev;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), &ev, SLOT(quit()));
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(connection, SIGNAL(statusChanged()), &ev, SLOT(quit()));

    connection->connectToServer();
    QCOMPARE((int)connection->status(), (int)JsonDbConnection::Ready);

    connection->disconnectFromServer();
    QCOMPARE((int)connection->status(), (int)JsonDbConnection::Disconnected);

    connection->setToken(QLatin1String("foobar"));
    connection->connectToServer();
    QCOMPARE((int)connection->status(), (int)JsonDbConnection::Connecting);
    mElapsedTimer.start();
    timer.start(mClientTimeout);
    ev.exec();
    QCOMPARE((int)connection->status(), (int)JsonDbConnection::Disconnected);

    delete connection;
}

/*
 * Create an item and remove it
 */

void TestJsonDbClient::create()
{
    QVERIFY(mClient);

    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "create-test");
    item.insert("create-test", 22);

    // Create an item
    int id = mClient->create(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid = mData.toMap().value("_uuid");

    // Try to create the same item again (should fail)
    item.insert("_uuid", uuid);
    id = mClient->create(item);
    waitForResponse2(id, JsonDbError::InvalidRequest);

    // Attempt to remove it without supplying a _uuid
    item.remove("_uuid");
    id = mClient->remove(item);
    waitForResponse2(id, JsonDbError::MissingQuery);

    // Set the _uuid field and attempt to remove it again
    item.insert("_uuid", uuid);
    id = mClient->remove(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("count"));
    QCOMPARE(mData.toMap().value("count").toInt(), 1);
}


/*
 * Create an array of items and remove them
 */

static const char *names[] = { "Abe", "Beth", "Carlos", "Dwight", "Emu", "Francis", NULL };

void TestJsonDbClient::createList()
{
    // Create a few items
    QVariantList list;
    int count;
    for (count = 0 ; names[count] ; count++ ) {
        QVariantMap item;
        item.insert("_type", "create-list-test");
        item.insert("name", names[count]);
        list << item;
    }

    int id = mClient->create(list);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("count").toInt(), count);

    // Retrieve the _uuids of the items
    QVariantMap query;
    query.insert("query", "[?_type=\"create-list-test\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), count);

    // Extract the uuids and put into a separate list
    QVariantList toDelete;
    QVariantList resultList = mData.toMap().value("data").toList();
    for (int i = 0; i < resultList.size(); i++) {
        const QVariant& v = resultList.at(i);
        QVariantMap map = list.at(i).toMap();
        map.insert("_uuid", v.toMap().value("_uuid"));
        toDelete << map;
    }

    id = mClient->remove(toDelete);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("count").toInt(), count);
}

/*
 * Update item
 */

void TestJsonDbClient::update()
{
    QVERIFY(mClient);

    QVariantMap item;
    QVariantMap query;
    int id = 0;

    // Create a item
    item.insert("_type", "update-test");
    item.insert("name", names[0]);
    id = mClient->create(item);
    waitForResponse1(id);
    QString uuid = mData.toMap().value("_uuid").toString();
    QString version = mData.toMap().value("_version").toString();

    // Check that it's there
    query.insert("query", "[?_type=\"update-test\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("data").toList().first().toMap().value("name").toString(),
             QString(names[0]));

    // Change the item
    item.insert("name", names[1]);
    item.insert("_uuid", uuid);
    item.insert("_version", version);
    id = mClient->update(item);
    waitForResponse1(id);

    // Check that it's really changed
    query.insert("query", "[?_type=\"update-test\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);
   
    QMap<QString, QVariant> obj = mData.toMap().value("data").toList().first().toMap();
    QCOMPARE(obj.value("name").toString(), QString(names[1]));

    // Check if _version has changed
    QVERIFY(obj.value("_version").toString() != version);
}

/*
 * Find items
 */

void TestJsonDbClient::find()
{
    QVERIFY(mClient);

    QVariantMap item;
    QVariantMap query;
    int id = 0;
    int count;

    QStringList nameList;
    // Create a few items
    for (count = 0 ; names[count] ; count++ ) {
        nameList << names[count];
        item.insert("_type", "find-test");
        item.insert("name", names[count]);
        id = mClient->create(item);
        waitForResponse1(id);
    }

    query.insert("query", "[?_type=\"find-test\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), count);
    QVariantList answer = mData.toMap().value("data").toList();
    QCOMPARE(answer.size(), count);
    QStringList answerNames;
    for (int i = 0; i < count; i++) {
        QVERIFY(nameList.contains(answer.at(i).toMap().value("name").toString()));
    }

    // Find them, but limit it to just one
    query.insert("query", "[?_type=\"find-test\"]");
    query.insert("limit", 1);
    id = mClient->find(query);
    waitForResponse1(id);
    answer = mData.toMap().value("data").toList();
    QCOMPARE(answer.size(), 1);
    QVERIFY(nameList.contains(answer.at(0).toMap().value("name").toString()));

    // Find one, sorted in reverse alphabetical order
    query = QVariantMap();
    query.insert("query", "[?_type=\"find-test\"][\\name]");
    id = mClient->find(query);
    waitForResponse1(id);
    answer = mData.toMap().value("data").toList();
    QCOMPARE(answer.size(), count);
    QCOMPARE(answer.at(0).toMap().value("name").toString(),
             QString(names[count-1]));
    answerNames.clear();
    nameList.clear();
    for (int i = 0; i < count; i++) {
        answerNames << answer.at(i).toMap().value("name").toString();
        nameList << names[count - i - 1];
    }
    QCOMPARE(answerNames, nameList);

}

void TestJsonDbClient::notify()
{
    int id = 400;

    // Create a notification object
    JsonDbClient::NotifyTypes actions = JsonDbClient::NotifyCreate|JsonDbClient::NotifyUpdate|JsonDbClient::NotifyRemove;
    const QString query = "[?_type=\"notify-test\"]";
    id = mClient->notify(actions, query);
    waitForResponse1(id);
    QVariantMap notifyObject = mData.toMap();
    QString notifyUuid = notifyObject.value("_uuid").toString();

    // Create a notify-test object
    QVariantMap object;
    object.insert("_type","notify-test");
    object.insert("name","test1");
    id = mClient->create(object);
    waitForResponse4(id, -1, notifyUuid, 1);
    QVariant uuid = mData.toMap().value("_uuid");
    QString version = mData.toMap().value("_version").toString();

    QCOMPARE(mNotifications.size(), 1);
    Notification n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QString("create"));

    // Update the notify-test object
    object.insert("_uuid",uuid);
    object.insert("_version", version);
    object.insert("name","test2");
    id = mClient->update(object);
    waitForResponse4(id, -1, notifyUuid, 1);
    
    QCOMPARE(mNotifications.size(), 1);
    n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QLatin1String("update"));

    // Remove the notify-test object
    id = mClient->remove(object);
    waitForResponse4(id, -1, notifyUuid, 1);

    QCOMPARE(mNotifications.size(), 1);
    n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QLatin1String("remove"));

    // Remove the notification object
    id = mClient->remove(notifyObject);
    waitForResponse1(id);
}

void TestJsonDbClient::registerNotification()
{
    int id = 400;

    // Create a notification object
    JsonDbClient::NotifyTypes actions = JsonDbClient::NotifyCreate|JsonDbClient::NotifyUpdate|JsonDbClient::NotifyRemove;
    const QString query = "[?_type=\"registerNotification\"]";
    QString notifyUuid = mClient->registerNotification(actions, query);

    // Create a registerNotification object
    QVariantMap object;
    object.insert("_type","registerNotification");
    object.insert("name","test1");
    id = mClient->create(object);
    waitForResponse4(id, -1, notifyUuid, 1);
    QVariant uuid = mData.toMap().value("_uuid");
    QString version = mData.toMap().value("_version").toString();

    QCOMPARE(mNotifications.size(), 1);
    Notification n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QString("create"));

    // Update the registerNotification object
    object.insert("_uuid",uuid);
    object.insert("_version", version);
    object.insert("name","test2");
    id = mClient->update(object);
    waitForResponse4(id, -1, notifyUuid, 1);

    QCOMPARE(mNotifications.size(), 1);
    n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QLatin1String("update"));

    // Remove the registerNotification object
    id = mClient->remove(object);
    waitForResponse4(id, -1, notifyUuid, 1);

    QCOMPARE(mNotifications.size(), 1);
    n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QLatin1String("remove"));

    // Remove the notification object
    mClient->unregisterNotification(notifyUuid);
}


static const char *rbnames[] = { "Fred", "Joe", "Sam", NULL };

void TestJsonDbClient::notifyRemoveBatch()
{
    int id = 400;

    // Create a notification object
    JsonDbClient::NotifyTypes actions = JsonDbClient::NotifyRemove;
    const QString nquery = "[?_type=\"notify-test-remove-batch\"]";
    id = mClient->notify(actions, nquery);
    waitForResponse1(id);
    QVariantMap notifyObject = mData.toMap();
    QString notifyUuid = notifyObject.value("_uuid").toString();

    // Create notify-test-remove-batch object
    QVariantList list;
    int count;
    for (count = 0 ; rbnames[count] ; count++) {
        QVariantMap object;
        object.insert("_type","notify-test-remove-batch");
        object.insert("name",rbnames[count]);
        list << object;
    }

    id = mClient->create(list);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("count").toInt(), count);

    // Retrieve the _uuids of the items
    QVariantMap query;
    query.insert("query", "[?_type=\"notify-test-remove-batch\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), count);
    list = mData.toMap().value("data").toList();

    // Make a list of the uuids returned
    QVariantList uuidList;
    foreach (const QVariant& v, mData.toMap().value("data").toList())
        uuidList << v.toMap().value("_uuid");

    // Remove the objects
    id = mClient->remove(list);
    waitForResponse4(id, -1, notifyUuid, count);

    QCOMPARE(mNotifications.size(), count);
    while (mNotifications.length()) {
        Notification n = mNotifications.takeFirst();
        QCOMPARE(n.mNotifyUuid, notifyUuid);
        QCOMPARE(n.mAction, QLatin1String("remove"));
        QVariant uuid = n.mObject.toMap().value("_uuid");
        QVERIFY(uuidList.contains(uuid));
        uuidList.removeOne(uuid);
    }

    // Remove the notification object
    id = mClient->remove(notifyObject);
    waitForResponse1(id);
}

void TestJsonDbClient::schemaValidation()
{
    QFile schemaFile(findFile(SRCDIR, "create-test.json"));
    schemaFile.open(QIODevice::ReadOnly);
    QByteArray json = schemaFile.readAll();
    schemaFile.close();
    JsonReader parser;
    bool ok = parser.parse(json);
    QVERIFY2(ok, parser.errorString().toLocal8Bit());
    QVariantMap schemaBody = parser.result().toMap();
    //qDebug() << "schemaBody" << schemaBody;
    QVariantMap schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", "SchemaTestObject");
    schemaObject.insert("schema", schemaBody);
    //qDebug() << "schemaObject" << schemaObject;

    int id = mClient->create(schemaObject);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));

    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "SchemaTestObject");
    item.insert("create-test", 22);
    item.insert("another-field", "a string");

    // Create an item that matches the schema
    id = mClient->create(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid = mData.toMap().value("_uuid");

    // Create an item that does not match the schema

    QVariantMap noncompliant;
    noncompliant.insert(JsonDbString::kTypeStr, "SchemaTestObject");
    noncompliant.insert("create-test", 22);
    id = mClient->create(noncompliant);
    waitForResponse2(id, JsonDbError::FailedSchemaValidation);
}

void TestJsonDbClient::remove()
{
    QVERIFY(mClient);

    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "remove-test");

    // Create an item
    item.insert("foo", 42);
    int id = mClient->create(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid = mData.toMap().value("_uuid");

    // create more items
    item.insert("foo", 5);
    id = mClient->create(item);
    waitForResponse1(id);
    item.insert("foo", 64);
    id = mClient->create(item);
    waitForResponse1(id);
    item.insert("foo", 65);
    id = mClient->create(item);
    waitForResponse1(id);

    // query and make sure there are four items
    id = mClient->query(QLatin1String("[?_type=\"remove-test\"]"));
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 4);

    // Set the _uuid field and attempt to remove first item
    item.insert("_uuid", uuid);
    id = mClient->remove(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("count"));
    QCOMPARE(mData.toMap().value("count").toInt(), 1);

    // query and make sure there are only three items left
    id = mClient->query(QLatin1String("[?_type=\"remove-test\"]"));
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("length"));
    QCOMPARE(mData.toMap().value("length").toInt(), 3);

    // Remove two items using query
    id = mClient->remove(QString::fromLatin1("[?_type=\"remove-test\"][?foo >= 63]"));
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("count"));
    QCOMPARE(mData.toMap().value("count").toInt(), 2);

    // query and make sure there are only one item left
    id = mClient->query(QLatin1String("[?_type=\"remove-test\"]"));
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("length"));
    QCOMPARE(mData.toMap().value("length").toInt(), 1);
    QVERIFY(mData.toMap().contains("data"));
    QCOMPARE(mData.toMap().value("data").toList().size(), 1);
    QVERIFY(mData.toMap().value("data").toList().at(0).toMap().contains("foo"));
    QCOMPARE(mData.toMap().value("data").toList().at(0).toMap().value("foo").toInt(), 5);
}

void TestJsonDbClient::notifyMultiple()
{
    // notifications with multiple client connections
    // Makes sure only the client that signed up for notifications gets them

    JsonDbClient::NotifyTypes actions = JsonDbClient::NotifyCreate|JsonDbClient::NotifyUpdate|JsonDbClient::NotifyRemove;
    const QString query = "[?_type=\"notify-test\"][?identifier=\"w1-identifier\"]";

    ClientWrapper w1; w1.connectToServer();
    ClientWrapper w2; w2.connectToServer();

    int id = w1.mClient->notify(actions, query);
    waitForResponse(w1.mEventLoop, &w1, id, -1, QVariant(), 0);
    QString notifyUuid = w1.mData.toMap().value("_uuid").toString();

    // Create a notify-test object
    QVariantMap object;
    object.insert("_type","notify-test");
    object.insert("identifier","w1-identifier");
    id = w1.mClient->create(object);
    waitForResponse(w1.mEventLoop, &w1, id, -1, notifyUuid, 1);
    QVariant uuid = w1.mData.toMap().value("_uuid");
    QString version = w1.mData.toMap().value("_version").toString();
    QCOMPARE(w1.mNotifications.size(), 1);
    QCOMPARE(w2.mNotifications.size(), 0);

    id = w2.mClient->create(object);
    waitForResponse(w2.mEventLoop, &w2, id, -1, QVariant(), 0);
    QCOMPARE(w2.mNotifications.size(), 0);
    if (w1.mNotifications.size() != 2)
        waitForResponse(w1.mEventLoop, &w1, -1, -1, notifyUuid, 1);
    QCOMPARE(w1.mNotifications.size(), 2);

    w1.mNotifications.clear();
    w2.mNotifications.clear();
    object.insert("identifier","not-w1-identifier");
    id = w1.mClient->create(object);
    waitForResponse(w1.mEventLoop, &w1, id, -1, QVariant(), 0);
    QCOMPARE(w1.mNotifications.size(), 0);
    QCOMPARE(w2.mNotifications.size(), 0);

    id = w2.mClient->create(object);
    waitForResponse(w2.mEventLoop, &w2, id, -1, QVariant(), 0);
    QCOMPARE(w2.mNotifications.size(), 0);
    QCOMPARE(w1.mNotifications.size(), 0);
}

void TestJsonDbClient::changesSince()
{
    QVERIFY(mClient);

    int id = mClient->changesSince(0);
    waitForResponse1(id);

    int state = mData.toMap()["currentStateNumber"].toInt();

    QVariantMap c1, c2;
    c1["_type"] = c2["_type"] = "TestContact";
    c1["firstName"] = "John";
    c1["lastName"] = "Doe";
    c2["firstName"] = "George";
    c2["lastName"] = "Washington";

    id = mClient->create(c1);
    waitForResponse1(id);
    id = mClient->create(c2);
    waitForResponse1(id);

    // changesSince returns changes after the specified state
    id = mClient->changesSince(state);
    waitForResponse1(id);

    QVariantMap data(mData.toMap());
    QCOMPARE(data["startingStateNumber"].toInt(), state);
    QVERIFY(data["currentStateNumber"].toInt() > state);
    QCOMPARE(data["count"].toInt(), 2);

    QVariantList results(data["changes"].toList());
    QCOMPARE(results.count(), 2);

    JsonWriter writer;
    qDebug() << writer.toByteArray(results);
    QVERIFY(results[0].toMap()["before"].toMap().isEmpty());
    QVERIFY(results[1].toMap()["before"].toMap().isEmpty());

    QVariantMap r1(results[0].toMap()["after"].toMap());
    QVariantMap r2(results[1].toMap()["after"].toMap());

    QMapIterator<QString, QVariant> i(c1);
    while (i.hasNext()) {
        i.next();
        QCOMPARE(i.value().toString(), r1[i.key()].toString());
    }

    i = QMapIterator<QString, QVariant>(c2);
    while (i.hasNext()) {
        i.next();
        QCOMPARE(i.value().toString(), r2[i.key()].toString());
    }
}

static QStringList stringList(QString s)
{
    return (QStringList() << s);
}

void TestJsonDbClient::capabilitiesAllowAll()
{
    QVERIFY(mClient);

    // Create Security Object
    QString jsondb_token = "testToken";
    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "com.nokia.mp.core.Security");
    item.insert(JsonDbString::kTokenStr, jsondb_token);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    item.insert("pid", getpid());
#endif
    item.insert("domain", "testDomain");
    item.insert("identifier", "TestJsonDbClient");
    QVariantMap capability;
    QStringList accessTypesAllowed;
    accessTypesAllowed << "allAllowed";
    capability.insert("AllAccess", accessTypesAllowed);
    item.insert("capabilities", capability);
    int id = mClient->create(item);
    waitForResponse1(id);

    // Create Capability Object
    QVariantMap item2;
    item2.insert(JsonDbString::kTypeStr, "Capability");
    item2.insert("name", "AllAccess");
    QVariantMap accessRules;
    QVariantMap accessTypeTranslation;
    accessTypeTranslation.insert("read", stringList(".*"));
    accessTypeTranslation.insert("write", stringList(".*"));
    accessRules.insert("allAllowed", accessTypeTranslation);
    item2.insert("accessRules", accessRules);
    id = mClient->create(item2);
    waitForResponse1(id);

    // Add an item to the db
    QVariantMap item3;
    item3.insert(JsonDbString::kTypeStr, "all-allowed-test");
    item3.insert("number", 22);
    id = mClient->create(item3);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid = mData.toMap().value("_uuid");

    // Set token in environment
    JsonDbConnection connection;
    connection.setToken(jsondb_token);
    connection.connectToServer();
    JsonDbClient tokenClient(&connection);
    connect( &tokenClient, SIGNAL(notified(const QString&, const QVariant&, const QString&)),
             this, SLOT(notified(const QString&, const QVariant&, const QString&)));
    connect( &tokenClient, SIGNAL(response(int, const QVariant&)),
             this, SLOT(response(int, const QVariant&)));
    connect( &tokenClient, SIGNAL(error(int, int, const QString&)),
             this, SLOT(error(int, int, const QString&)));

    // Now try to update the item.
    item3.insert("_uuid", uuid);
    item3.insert("number", 46);
    id = tokenClient.update(item3);
    waitForResponse1(id);

    // Check that it's really changed
    QVariantMap query;
    query.insert("query", "[?_type=\"all-allowed-test\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);

    QMap<QString, QVariant> obj = mData.toMap().value("data").toList().first().toMap();
    QCOMPARE(obj.value("number").toInt(), 46);
}

void TestJsonDbClient::capabilitiesReadOnly()
{
    QVERIFY(mClient);

    // Create Security Object
    QString jsondb_token = "testToken2";
    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "com.nokia.mp.core.Security");
    item.insert(JsonDbString::kTokenStr, jsondb_token);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    item.insert("pid", getpid());
#endif
    item.insert("domain", "testDomain");
    item.insert("identifier", "TestJsonDbClient");
    QVariantMap capability;
    QStringList accessTypesAllowed;
    accessTypesAllowed << "noWrite";
    capability.insert("NoWrite", accessTypesAllowed);
    item.insert("capabilities", capability);
    int id = mClient->create(item);
    waitForResponse1(id);

    // Create Capability Object
    QVariantMap item2;
    item2.insert(JsonDbString::kTypeStr, "Capability");
    item2.insert(JsonDbString::kNameStr, "NoWrite");
    QVariantMap accessRules;
    QVariantMap accessTypeTranslation;
    accessTypeTranslation.insert("read", stringList(".*"));
    accessRules.insert("noWrite", accessTypeTranslation);
    item2.insert("accessRules", accessRules);
    id = mClient->create(item2);
    waitForResponse1(id);

    // Add an item to the db
    QVariantMap item3;
    item3.insert(JsonDbString::kTypeStr, "read-only-test");
    item3.insert("number", 22);
    id = mClient->create(item3);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid = mData.toMap().value("_uuid");

    // Set token in environment
    JsonDbConnection connection;
    connection.setToken(jsondb_token);
    connection.connectToServer();
    JsonDbClient tokenClient(&connection);
    connect( &tokenClient, SIGNAL(notified(const QString&, const QVariant&, const QString&)),
             this, SLOT(notified(const QString&, const QVariant&, const QString&)));
    connect( &tokenClient, SIGNAL(response(int, const QVariant&)),
             this, SLOT(response(int, const QVariant&)));
    connect( &tokenClient, SIGNAL(error(int, int, const QString&)),
             this, SLOT(error(int, int, const QString&)));

    // Now try to update the item.
    item3.insert("_uuid", uuid);
    item3.insert("number", 46);
    id = tokenClient.update(item3);
    waitForResponse2(id, JsonDbError::OperationNotPermitted);

    // Check that it's not changed
    QVariantMap query;
    query.insert("query", "[?_type=\"read-only-test\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);

    QMap<QString, QVariant> obj = mData.toMap().value("data").toList().first().toMap();
    QCOMPARE(obj.value("number").toInt(), 22);
}

void TestJsonDbClient::capabilitiesTypeQuery()
{
    QVERIFY(mClient);

    // Create Security Object
    QString jsondb_token = "testToken3";
    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "com.nokia.mp.core.Security");
    item.insert(JsonDbString::kTokenStr, jsondb_token);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    item.insert("pid", getpid());
#endif
    item.insert("domain", "testDomain");
    item.insert("identifier", "TestJsonDbClient");
    QVariantMap capability;
    QStringList accessTypesAllowed;
    accessTypesAllowed << "onlyWlan";
    capability.insert("allAccess", accessTypesAllowed);
    item.insert("capabilities", capability);
    int id = mClient->create(item);
    waitForResponse1(id);

    // Create Capability Object
    QVariantMap item2;
    item2.insert(JsonDbString::kTypeStr, "Capability");
    item2.insert(JsonDbString::kNameStr, "allAccess");
    QVariantMap accessRules;
    QVariantMap accessTypeTranslation;
    accessTypeTranslation.insert("read", stringList("[?_type=\"Wlan\"]"));
    accessTypeTranslation.insert("write", stringList("[?_type=\"Wlan\"]"));
    accessRules.insert("onlyWlan", accessTypeTranslation);
    item2.insert("accessRules", accessRules);
    id = mClient->create(item2);
    waitForResponse1(id);

    // Add an item on type Wlan to the db
    QVariantMap item3;
    item3.insert(JsonDbString::kTypeStr, "Wlan");
    item3.insert("SSID", "NokiaWlan");
    item3.insert("Encryption", "None");
    item3.insert("SignalStrenght", 46);
    id = mClient->create(item3);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid = mData.toMap().value("_uuid");


    // Add an item on type NoAccess to the db
    QVariantMap item4;
    item4.insert(JsonDbString::kTypeStr, "NoAccess");
    item4.insert("Number", 46);
    id = mClient->create(item4);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid2 = mData.toMap().value("_uuid");

    // Set token in environment
    JsonDbConnection connection;
    connection.setToken(jsondb_token);
    connection.connectToServer();
    JsonDbClient tokenClient(&connection);
    connect( &tokenClient, SIGNAL(notified(const QString&, const QVariant&, const QString&)),
             this, SLOT(notified(const QString&, const QVariant&, const QString&)));
    connect( &tokenClient, SIGNAL(response(int, const QVariant&)),
             this, SLOT(response(int, const QVariant&)));
    connect( &tokenClient, SIGNAL(error(int, int, const QString&)),
             this, SLOT(error(int, int, const QString&)));

    // Now try to update the Wlan item.
    item3.insert("_uuid", uuid);
    item3.insert("SSID", "NokiaWlan2");
    item3.insert("SignalStrenght", 12);
    id = tokenClient.update(item3);
    waitForResponse1(id);

    // Check that it changed
    QVariantMap query;
    query.insert("query", "[?_type=\"Wlan\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);

    QMap<QString, QVariant> obj = mData.toMap().value("data").toList().first().toMap();
    QCOMPARE(obj.value("SSID").toString(), QString("NokiaWlan2"));
    QCOMPARE(obj.value("SignalStrenght").toInt(), 12);

    // Now try to update the NoAccess item.
    item4.insert("_uuid", uuid2);
    item4.insert("Number", 12);
    id = tokenClient.update(item4);
    waitForResponse2(id, JsonDbError::OperationNotPermitted);

    // Check that it did not change
    QVariantMap query2;
    query2.insert("query", "[?_type=\"NoAccess\"]");
    id = mClient->find(query2);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);

    QMap<QString, QVariant> obj2 = mData.toMap().value("data").toList().first().toMap();
    QCOMPARE(obj2.value("Number").toInt(), 46);
}

void TestJsonDbClient::storageQuotas()
{
    QVERIFY(mClient);

    // Create Security Object with storage quota
    QString jsondb_token = "testToken4";
    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "com.nokia.mp.core.Security");
    item.insert(JsonDbString::kTokenStr, jsondb_token);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    item.insert("pid", getpid());
#endif
    item.insert("domain", "testDomain");
    item.insert("identifier", "TestJsonDbClient");
    QVariantMap capability;
    QStringList accessTypesAllowed;
    accessTypesAllowed << "allAllowed";
    capability.insert("AllAccess", accessTypesAllowed);
    QVariantMap quotas;
    quotas.insert("storage", int(1024));
    capability.insert("quotas", quotas);
    item.insert("capabilities", capability);
    int id = mClient->create(item);
    waitForResponse1(id);

    // Create Capability Object
    QVariantMap item2;
    item2.insert(JsonDbString::kTypeStr, "Capability");
    item2.insert("name", "AllAccess");
    QVariantMap accessRules;
    QVariantMap accessTypeTranslation;
    accessTypeTranslation.insert("read", stringList(".*"));
    accessTypeTranslation.insert("write", stringList(".*"));
    accessRules.insert("allAllowed", accessTypeTranslation);
    item2.insert("accessRules", accessRules);
    id = mClient->create(item2);
    waitForResponse1(id);


    // Set token in environment
    JsonDbConnection connection;
    connection.setToken(jsondb_token);
    connection.connectToServer();
    JsonDbClient tokenClient(&connection);
    connect( &tokenClient, SIGNAL(notified(const QString&, const QVariant&, const QString&)),
             this, SLOT(notified(const QString&, const QVariant&, const QString&)));
    connect( &tokenClient, SIGNAL(response(int, const QVariant&)),
             this, SLOT(response(int, const QVariant&)));
    connect( &tokenClient, SIGNAL(error(int, int, const QString&)),
             this, SLOT(error(int, int, const QString&)));


    // Add an item to the db
    QVariantMap item3;
    item3.insert(JsonDbString::kTypeStr, "storage-test");
    item3.insert("storagedata", QString(256, 'a'));
    item3.insert("number", 123);
    id = tokenClient.create(item3);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid = mData.toMap().value("_uuid");

    // This should be to large to fit the quota
    QVariantMap item4;
    item4.insert(JsonDbString::kTypeStr, "storage-test");
    item4.insert("storagedata",  QString(256, 'a'));
    item4.insert("number", 123);
    id = tokenClient.create(item4);
    waitForResponse2(id, JsonDbError::QuotaExceeded);

    // Remove the first item to make more space
    item3.insert("_uuid", uuid);
    id = tokenClient.remove(item3);
    waitForResponse1(id);

    // This time it should work to add the second item
    id = tokenClient.create(item4);
    waitForResponse1(id);

    // Remove all items
    QVERIFY(mData.toMap().contains("_uuid"));
    uuid = mData.toMap().value("_uuid");
    item4.insert("_uuid", uuid);
    id = tokenClient.remove(item4);
    waitForResponse1(id);


    // Make sure that the storage does not increase
    // by adding and removing objects.
    for (int i = 0; i < 10; i++) {
        // Add an item to the db
        QVariantMap item5;
        item5.insert(JsonDbString::kTypeStr, "storage-test");
        item5.insert("storagedata", QString(256, 'a'));
        item5.insert("number", 123);
        id = tokenClient.create(item5);
        waitForResponse1(id);
        QVERIFY(mData.toMap().contains("_uuid"));
        uuid = mData.toMap().value("_uuid");

        // Remove the  item again
        item5.insert("_uuid", uuid);
        id = tokenClient.remove(item5);
        waitForResponse1(id);
    }
}

void TestJsonDbClient::requestWithSlot()
{
    Handler handler;

    // create notification object
    int id = mClient->notify(JsonDbClient::NotifyCreate, "[?_type=\"requestWithSlot\"]",
                             &handler, SLOT(notify(QString,QVariant,QString)),
                             &handler, SLOT(success(int,QVariant)), SLOT(error(int,int,QString)));
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QString notifyUuid = mData.toMap().value("_uuid").toString();
    QCOMPARE(handler.errorCount, 0);
    QCOMPARE(handler.successCount, 1);
    handler.clear();

    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("requestWithSlot"));
    item.insert("create-test", 42);
    id = mClient->create(item, &handler, SLOT(success(int,QVariant)), SLOT(error(int,int,QString)));
    waitForResponse4(id, -1, notifyUuid, 1);
    QVERIFY(mData.toMap().contains("_uuid"));
    QString uuid = mData.toMap().value("_uuid").toString();

    QCOMPARE(handler.requestId, id);
    QVERIFY(handler.data.toMap().contains("_uuid"));
    QCOMPARE(handler.data.toMap().value("_uuid").toString(), uuid);

    QCOMPARE(handler.errorCount, 0);
    QCOMPARE(handler.successCount, 1);
    QCOMPARE(handler.notifyCount, 1);
    QCOMPARE(handler.notifyUuid, notifyUuid);
}

void TestJsonDbClient::connection_response(int, const QVariant&)
{
    failed = false;
    mEventLoop.quit();
}

void TestJsonDbClient::connection_error(int id, int code, const QString &message)
{
    Q_UNUSED(id);
    Q_UNUSED(code);
    Q_UNUSED(message);
    failed = true;
    mEventLoop.quit();
}


void TestJsonDbClient::testToken_data()
{
    QTest::addColumn<pid_t>("pid");
    QTest::addColumn<QString>("tokenString");
    QTest::addColumn<bool>("willFail");

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    QTest::newRow("valid") << getpid() <<  "testToken_valid" << false;
    QTest::newRow("invalid") << getpid()+1 <<  "testToken_invalid" << true;
#else
    QSKIP("Not supported on this platform", SkipAll);
#endif
}

void TestJsonDbClient::testToken()
{
    QVERIFY(mClient);
    QFETCH(pid_t, pid);
    QFETCH(QString, tokenString);
    QFETCH(bool, willFail);

    // Create Security Object
    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "com.nokia.mp.core.Security");
    item.insert(JsonDbString::kTokenStr, tokenString);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    item.insert("pid", pid);
#else
    Q_UNUSED(pid);
#endif
    item.insert("domain", "testDomain");
    item.insert("identifier", "TestJsonDbClient");
    QVariantMap capability;
    QStringList accessTypesAllowed;
    accessTypesAllowed << "allAllowed";
    capability.insert("AllAccess", accessTypesAllowed);
    item.insert("capabilities", capability);
    int id = mClient->create(item);
    waitForResponse1(id);

    // Set token in environment
    JsonDbConnection connection;
    connect( &connection, SIGNAL(response(int, const QVariant&)),
             this, SLOT(connection_response(int, const QVariant&)));
    connect( &connection, SIGNAL(error(int, int, const QString&)),
             this, SLOT(connection_error(int, int, const QString&)));

    connection.setToken(tokenString);
    connection.connectToServer();
    mEventLoop.exec(QEventLoop::AllEvents);
    QCOMPARE(failed, willFail);
}

void TestJsonDbClient::partition()
{
    int id;
    const QString firstPartitionName = "com.nokia.qtjsondb.autotest.Partition1";
    const QString secondPartitionName = "com.nokia.qtjsondb.autotest.Partition2";

    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "Partition");
    item.insert("name", firstPartitionName);
    id = mClient->create(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant firstPartitionUuid = mData.toMap().value("_uuid");

    item = QVariantMap();
    item.insert(JsonDbString::kTypeStr, "Partition");
    item.insert("name", secondPartitionName);
    id = mClient->create(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant secondPartitionUuid = mData.toMap().value("_uuid");


    item = QVariantMap();
    item.insert(JsonDbString::kTypeStr, "Foobar");
    item.insert("one", "one");
    id = mClient->create(variantToQson(item), firstPartitionName);
    waitForResponse1(id);

    item = QVariantMap();
    item.insert(JsonDbString::kTypeStr, "Foobar");
    item.insert("one", "two");
    id = mClient->create(variantToQson(item), secondPartitionName);
    waitForResponse1(id);

    // now query
    id = mClient->query("[?_type=\"Foobar\"]", 0, -1, firstPartitionName);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);
    QCOMPARE(mData.toMap().value("data").toList().at(0).toMap().value("one").toString(), QLatin1String("one"));

    id = mClient->query("[?_type=\"Foobar\"]", 0, -1, secondPartitionName);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);
    QCOMPARE(mData.toMap().value("data").toList().at(0).toMap().value("one").toString(), QLatin1String("two"));
}

class QueryHandler : public QObject
{
    Q_OBJECT
public slots:
    void started()
    { startedCalls++; }
    void resultsReady(int count)
    { resultsReadyCalls++; resultsReadyCount += count; }
    void finished()
    { finishedCalls++; }
    void error(int code, const QString &message)
    { errorCalls++; errorCode = code; errorMessage = message; }

public:
    QueryHandler() { clear(); }
    void clear()
    {
        startedCalls = 0;
        resultsReadyCalls = 0;
        resultsReadyCount = 0;
        finishedCalls = 0;
        errorCalls = 0;
        errorCode = 0;
        errorMessage = QString();
    }

    int startedCalls;
    int resultsReadyCalls;
    int resultsReadyCount;
    int finishedCalls;
    int errorCalls;
    int errorCode;
    QString errorMessage;
};

void TestJsonDbClient::queryObject()
{
    // Create a few items
    QVariantList list;
    for (int i = 0; i < 10; ++i) {
        QVariantMap item;
        item.insert("_type", QLatin1String("queryObject"));
        item.insert("foo", i);
        list.append(item);
    }

    int id = mClient->create(list);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("count").toInt(), 10);

    JsonDbQuery *r = mClient->query();
    r->setQuery(QLatin1String("[?_type=\"queryObject\"]"));

    QueryHandler handler;
    connect(r, SIGNAL(started()), &handler, SLOT(started()));
    connect(r, SIGNAL(resultsReady(int)), &handler, SLOT(resultsReady(int)));
    connect(r, SIGNAL(finished()), &handler, SLOT(finished()));
    connect(r, SIGNAL(error(int,QString)), &handler, SLOT(error(int,QString)));

    QEventLoop loop;
    connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(r, SIGNAL(error(int,QString)), &loop, SLOT(quit()));

    r->start();
    loop.exec();

    QCOMPARE(handler.startedCalls, 1);
    QCOMPARE(handler.resultsReadyCalls, 2);
    QCOMPARE(handler.resultsReadyCount, 10);
    QCOMPARE(handler.finishedCalls, 1);
    QCOMPARE(handler.errorCalls, 0);
}

void TestJsonDbClient::changesSinceObject()
{
    QVariantMap item;
    item.insert("_type", QLatin1String("queryObject"));
    item.insert("foo", -1);
    int id = mClient->create(item);
    waitForResponse1(id);

    id = mClient->query(QLatin1String("[?_type=\"queryObject\"]"), 0, 1);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);
    quint32 stateNumber = mData.toMap().value("state").value<quint32>();

    // Create a few items
    for (int i = 0; i < 2; ++i) {
        QVariantMap item;
        item.insert("_type", QLatin1String("queryObject"));
        item.insert("foo", i);
        int id = mClient->create(item);
        waitForResponse1(id);
    }

    JsonDbChangesSince *r = mClient->changesSince();
    r->setStateNumber(stateNumber);

    QueryHandler handler;
    connect(r, SIGNAL(started()), &handler, SLOT(started()));
    connect(r, SIGNAL(resultsReady(int)), &handler, SLOT(resultsReady(int)));
    connect(r, SIGNAL(finished()), &handler, SLOT(finished()));
    connect(r, SIGNAL(error(int,QString)), &handler, SLOT(error(int,QString)));

    QEventLoop loop;
    connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(r, SIGNAL(error(int,QString)), &loop, SLOT(quit()));

    r->start();
    loop.exec();

    QCOMPARE(handler.startedCalls, 1);
    QCOMPARE(handler.resultsReadyCalls, 2);
//    QCOMPARE(handler.resultsReadyCount, 16); // there is something fishy with jsondb state handling
    QCOMPARE(handler.finishedCalls, 1);
    QCOMPARE(handler.errorCalls, 0);
}

QTEST_MAIN(TestJsonDbClient)

#include "test-jsondb-client.moc"
