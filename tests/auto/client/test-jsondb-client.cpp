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
#include <QFile>
#include <QProcess>
#include <QEventLoop>
#include <QDebug>
#include <QLocalSocket>
#include <QTimer>
#include <QDir>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#endif
#include "private/jsondb-strings_p.h"
#include "private/jsondb-connection_p.h"

#include "jsondb-client.h"
#include "jsondb-object.h"
#include "jsondb-error.h"

#include "json.h"

#include "util.h"
#include "clientwrapper.h"

Q_DECLARE_METATYPE(QUuid)

QT_USE_NAMESPACE_JSONDB

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
    void index();
    void registerNotification();
    void notify();
    void notifyUpdate();
    void notifyViaCreate();
    void notifyRemoveBatch();
    void notifyMultiple();
    void remove();
    void schemaValidation();
    void changesSince();
    void requestWithSlot();
    void queryObject();
    void changesSinceObject();
    void jsondbobject();
    void jsondbobject_uuidFromObject();
#ifdef Q_OS_LINUX
    void sigstop();
#endif

    void connection_response(int, const QVariant&);
    void connection_error(int, int, const QString&);

private:
#ifndef DONT_START_SERVER
    qint64        mProcess;
#endif
    bool wasRoot;
    uid_t uidUsed;
    uid_t uid2Used;
    QList<gid_t> gidsAdded;
    bool failed;
    void removeDbFiles();
    // Temporarily disable quota test
    void storageQuotas();
    // Temporarily disable partition test
    void partition();
};

#ifndef DONT_START_SERVER
static const char dbfileprefix[] = "test-jsondb-client";
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
    void notify(const QString &uuid, const QtAddOn::JsonDb::JsonDbNotification &n)
    {
        notifyUuid = uuid;
        data = n.object();
        notifyAction = n.action();
        ++notifyCount;
    }

    void clear()
    {
        requestId = 0;
        data = QVariant();
        errorCode = 0;
        errorMessage = QString();
        notifyUuid = QString();
        notifyAction = JsonDbClient::NotifyType(0);
        successCount = errorCount = notifyCount = 0;
    }

public:
    Handler() { clear(); }

    int requestId;
    QVariant data;
    int errorCode;
    QString errorMessage;
    QString notifyUuid;
    JsonDbClient::NotifyType notifyAction;
    int successCount;
    int errorCount;
    int notifyCount;
};

TestJsonDbClient::TestJsonDbClient()
    : mProcess(0), wasRoot(false)
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

// the prefix in which /etc/passwd and friends live
#ifndef NSS_PREFIX
#  define NSS_PREFIX
#endif

gid_t nextFreeGid (gid_t start)
{
    struct group *old_grp;
    errno = 0;
    while ((old_grp = ::getgrgid(start)) != NULL) {
        qDebug () << "group" << old_grp->gr_name << "found from" << start;
        start++;
    }
    return start;
}

void TestJsonDbClient::initTestCase()
{
    removeDbFiles();
    // Test if we are running as root
    if (!geteuid())
        wasRoot = true;

#ifndef DONT_START_SERVER
    QStringList arg_list = (QStringList()
                            << "-validate-schemas");
    if (wasRoot)
        arg_list << "-enforce-access-control";
    arg_list << QString::fromLatin1(dbfileprefix);
    mProcess = launchJsonDbDaemonDetached(JSONDB_DAEMON_BASE, QString("testjsondb_%1").arg(getpid()), arg_list);
#endif
#if !defined(Q_OS_MAC)
    if (wasRoot) {
        // Add the needed Capability objects to database
        JsonDbObject capa_obj;
        capa_obj.insert(QLatin1String("_type"), QLatin1String("Capability"));
        capa_obj.insert(QLatin1String("name"), QLatin1String("User"));
        capa_obj.insert(QLatin1String("partition"), QLatin1String(dbfileprefix) + QLatin1String(".System"));
        QVariantMap access_rules;
        QVariantMap rw_rule;
        rw_rule.insert(QLatin1String("read"), (QStringList() << QLatin1String("[*]")));
        rw_rule.insert(QLatin1String("write"),
                          (QStringList() <<
                           QLatin1String("[?_type startsWith \"public.\"]") <<
                           QLatin1String("[?%owner startsWith %typeDomain]") <<
                           QLatin1String("[?%owner startsWith \"com.my.domain\"][?_type startsWith \"com.my.domain\"]") <<
                           QLatin1String("[?_type = \"_schemaType\"]") <<
                           QLatin1String("[?_type = \"Index\"]") <<
                           QLatin1String("[?_type = \"Map\"]")));
        access_rules.insert(QLatin1String("rw"), rw_rule);
        QVariantMap set_owner_rule;
        set_owner_rule.insert(QLatin1String("setOwner"),
                              (QStringList() << QLatin1String("[*]")));
        set_owner_rule.insert(QLatin1String("read"), (QStringList() << QLatin1String("[*]")));
        set_owner_rule.insert(QLatin1String("write"), (QStringList() << QLatin1String("[*]")));
        access_rules.insert(QLatin1String("setOwner"), set_owner_rule);
        capa_obj.insert(QLatin1String("accessRules"), access_rules);
        connectToServer();
        qDebug() << "Creating: " << capa_obj;
        int id = mClient->create(capa_obj);
        waitForResponse1(id);
        capa_obj.clear();
        capa_obj.insert(QLatin1String("_type"), QLatin1String("Capability"));
        capa_obj.insert(QLatin1String("name"), QLatin1String("User"));
        capa_obj.insert(QLatin1String("partition"), QLatin1String("Ephemeral"));
        access_rules.clear();
        rw_rule.clear();
        rw_rule.insert(QLatin1String("read"), (QStringList() << QLatin1String("[*]")));
        rw_rule.insert(QLatin1String("write"),
                          (QStringList() <<
                           QLatin1String("[?_type=\"notification\"]") <<
                           QLatin1String("[?_type startsWith \"public.\"]") <<
                           QLatin1String("[?%owner startsWith %typeDomain]")));
        access_rules.insert(QLatin1String("rw"), rw_rule);
        capa_obj.insert(QLatin1String("accessRules"), access_rules);
        qDebug() << "Creating: " << capa_obj;
        id = mClient->create(capa_obj);
        waitForResponse1(id);

        // Add a test object
        JsonDbObject foo_obj;
        foo_obj.insert(QLatin1String("_type"), QLatin1String("com.test.bar.FooType"));
        foo_obj.insert(QLatin1String("name"), QLatin1String("foo"));
        qDebug() << "Creating: " << foo_obj;
        id = mClient->create(foo_obj);
        waitForResponse1(id);
        QVERIFY(mData.toMap().contains("_uuid"));

        QVariantMap query;
        query.insert("query", "[?_type=\"com.test.bar.FooType\"]");
        id = mClient->find(query);
        waitForResponse1(id);
        QVariantList resultList = mData.toMap().value("data").toList();
        foo_obj = resultList.at(0).toMap();

        foo_obj[QLatin1String("_owner")] = QLatin1String("com.test.bar.app");
        id = mClient->update(foo_obj);
        waitForResponse1(id);

        // Add a schemaValidation tests object (must be done as root)
        QFile schemaFile(findFile("create-test.json"));
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
        schemaObject.insert("name", "com.test.SchemaTestObject");
        schemaObject.insert("schema", schemaBody);
        //qDebug() << "schemaObject" << schemaObject;
        id = mClient->create(schemaObject);
        waitForResponse1(id);
        QVERIFY(mData.toMap().contains("_uuid"));

        qDebug() << "Disconnect client that was connected as root.";
        mClient->disconnectFromServer();
        delete mClient;

        // Create needed test users & groups
        bool use_shadow = QFile::exists(QLatin1String(NSS_PREFIX "/etc/shadow")) &&
                QFile::exists(QLatin1String(NSS_PREFIX "/etc/gshadow"));
        QString etcipwd, etcigr;
        if (use_shadow) {
            etcipwd = QStringLiteral (NSS_PREFIX "/etc/shadow");
            etcigr = QStringLiteral (NSS_PREFIX "/etc/gshadow");
        } else {
            etcipwd = QStringLiteral (NSS_PREFIX "/etc/passwd");
            etcigr = QStringLiteral (NSS_PREFIX "/etc/group");
        }
        uid_t uid = 1042;
        errno = 0;
        struct passwd *old_pwd;
        while ((old_pwd = getpwuid(uid)) != NULL) {
            qDebug () << "user" << old_pwd->pw_name << "found from" << uid;
            uid++;
        }
        uid_t uid2 = uid+1;
        while ((old_pwd = getpwuid(uid2)) != NULL) {
            qDebug () << "user" << old_pwd->pw_name << "found from" << uid2;
            uid2++;
        }
        gid_t gid = nextFreeGid(1042);
        gid_t gid2 = nextFreeGid(gid+1);
        QString appName = QString("com.test.foo.%1").arg(getpid());
        // Tes app name for app without any supplementary groups
        QString app2Name = QString("com.test.bar.%1").arg(getpid());
        if (!errno) {
            // Add primary groups
            struct group grp;
            grp.gr_name = appName.toLocal8Bit().data();
            grp.gr_passwd = NULL;
            grp.gr_gid = gid;
            grp.gr_mem = (char *[]){NULL};
            struct group grp2;
            grp2.gr_name = app2Name.toLocal8Bit().data();
            grp2.gr_passwd = NULL;
            grp2.gr_gid = gid2;
            grp2.gr_mem = (char *[]){NULL};
            FILE *grfile = ::fopen (etcigr.toLocal8Bit().data(), "a");
            ::putgrent(&grp, grfile);
            ::putgrent(&grp2, grfile);
            ::fclose (grfile);
            gidsAdded.append(gid);
            gidsAdded.append(gid2);

            // Add the user
            struct passwd pwd;
            pwd.pw_name = appName.toLocal8Bit().data();
            pwd.pw_passwd = NULL;
            pwd.pw_uid = uid;
            pwd.pw_gid = gid;
            pwd.pw_gecos = NULL;
            pwd.pw_dir = NULL;
            pwd.pw_shell = NULL;
            struct passwd pwd2;
            pwd2.pw_name = app2Name.toLocal8Bit().data();
            pwd2.pw_passwd = NULL;
            pwd2.pw_uid = uid2;
            pwd2.pw_gid = gid2;
            pwd2.pw_gecos = NULL;
            pwd2.pw_dir = NULL;
            pwd2.pw_shell = NULL;
            FILE *pwdfile = ::fopen (etcipwd.toLocal8Bit().data(), "a");
            ::putpwent(&pwd, pwdfile);
            ::putpwent(&pwd2, pwdfile);
            ::fclose (pwdfile);

            // Add 'User' supplementary group
            gid = nextFreeGid(gid2+1);
            grp.gr_name = const_cast<char *>("User");
            grp.gr_passwd = NULL;
            grp.gr_gid = gid;
            // Add only the first user to it
            grp.gr_mem = (char *[]){appName.toLocal8Bit().data(), NULL};
            grfile = ::fopen (etcigr.toLocal8Bit(), "a");
            ::putgrent(&grp, grfile);
            ::fclose (grfile);
            gidsAdded.append(gid);
            ::seteuid (uid);
            uidUsed = uid;
            uid2Used = uid2;
            qDebug() << "Setting euid to: " << uid;

        }
    }
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
#if !defined(Q_OS_MAC)
        if (wasRoot)
            ::seteuid(0);
#endif
        ::kill(mProcess, SIGTERM);
#if !defined(Q_OS_MAC)
        if (wasRoot) {
            // Clean passwd
            FILE *newpasswd = ::fopen("newpasswd", "w");
            setpwent ();
            struct passwd *pwd;
            while ((pwd = ::getpwent())) {
                if (pwd->pw_uid != uidUsed &&  pwd->pw_uid != uid2Used)
                    ::putpwent (pwd, newpasswd);
            }
            ::fclose(newpasswd);
            ::rename("newpasswd","/etc/passwd");
            // Clean group
            FILE *newgroup = ::fopen("newgroup", "w");
            setgrent ();
            struct group *grp;
            while ((grp = ::getgrent())) {
                bool found = false;
                foreach (gid_t gid, gidsAdded) {
                    if (grp->gr_gid ==  gid) {
                        found = true;
                        break;
                    }
                }

                if (!found)
                    ::putgrent (grp, newgroup);
            }
            ::fclose(newgroup);
            ::rename("newgroup","/etc/group");
        }
#endif
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

    delete connection;
}

/*
 * Create an item and remove it
 */

void TestJsonDbClient::create()
{
    QVERIFY(mClient);

    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "com.test.create-test");
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
    waitForResponse2(id, JsonDbError::MissingUUID);

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
        item.insert("_type", "com.test.create-list-test");
        item.insert("name", names[count]);
        list << item;
    }
    int id = mClient->create(list);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("count").toInt(), count);

    // Retrieve the _uuids of the items
    QVariantMap query;
    query.insert("query", "[?_type=\"com.test.create-list-test\"]");
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
    item.insert("_type", "com.test.update-test");
    item.insert("name", names[0]);
    id = mClient->create(item);
    waitForResponse1(id);
    QString uuid = mData.toMap().value("_uuid").toString();
    QString version = mData.toMap().value("_version").toString();

    // Check that it's there
    query.insert("query", "[?_type=\"com.test.update-test\"]");
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
    query.insert("query", "[?_type=\"com.test.update-test\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);

    QMap<QString, QVariant> obj = mData.toMap().value("data").toList().first().toMap();
    QCOMPARE(obj.value("name").toString(), QString(names[1]));

    // Check if _version has changed
    QVERIFY(obj.value("_version").toString() != version);

    // Test access control
    if (wasRoot) {
        QVariantMap query1;
        // Find the test item
        query1.insert("query", "[?_type=\"com.test.bar.FooType\"]");
        query1.insert("limit", 1);
        id = mClient->find(query1);
        waitForResponse1(id);
        QVariantList answer = mData.toMap().value("data").toList();
        QVariantMap item1 = answer.at(0).toMap();
        item[QLatin1String("name")] = QLatin1String("fail");

        id = mClient->update(item1);
        // Should fail because of access control (error code 13)
        waitForResponse2(id, 13);

        item1.insert(QLatin1String("_type"), QLatin1String("com.test.FooType"));

        id = mClient->update(item1);
        // Should fail because of access control (error code 13)
        // The new _type is ok, but the old _type is not
        waitForResponse2(id, 13);
    }
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
        item.insert("_type", "com.test.find-test");
        item.insert("name", names[count]);
        id = mClient->create(item);
        waitForResponse1(id);
    }

    query.insert("query", "[?_type=\"com.test.find-test\"]");
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
    query.insert("query", "[?_type=\"com.test.find-test\"]");
    query.insert("limit", 1);
    id = mClient->find(query);
    waitForResponse1(id);
    answer = mData.toMap().value("data").toList();
    QCOMPARE(answer.size(), 1);
    QVERIFY(nameList.contains(answer.at(0).toMap().value("name").toString()));

    // Find one, sorted in reverse alphabetical order
    query = QVariantMap();
    query.insert("query", "[?_type=\"com.test.find-test\"][\\name]");
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
#if !defined(Q_OS_MAC)
    if (wasRoot) {
        // Set user id that has no supplementary groups
        mClient->disconnectFromServer();
        delete mClient;
        mClient = NULL;

        qDebug() << "Setting uid to" << uid2Used;
        ::seteuid(0);
        ::seteuid(uid2Used);
        connectToServer();

        // Read should fail
        query = QVariantMap();
        query.insert("query", "[?_type=\"com.test.find-test\"][\\name]");
        id = mClient->find(query);
        waitForResponse1(id);
        answer = mData.toMap().value("data").toList();
        QCOMPARE(answer.size(), 0);

        // Go back to 'capable' user
        mClient->disconnectFromServer();
        delete mClient;
        mClient = NULL;
        ::seteuid(0);
        ::seteuid(uidUsed);
        connectToServer();
    }
#endif
}

void TestJsonDbClient::index()
{
    QFile dataFile(":/json/client/index-test.json");
    dataFile.open(QIODevice::ReadOnly);
    QByteArray json = dataFile.readAll();
    dataFile.close();
    JsonReader parser;
    bool ok = parser.parse(json);
    QVERIFY2(ok, parser.errorString().toLocal8Bit());
    QVariantList data = parser.result().toList();

    int id = mClient->create(data);
    waitForResponse1(id);
    QVariantMap result = mData.toMap();

    QVariantMap query;
    query.insert("query", "[?_type=\"com.test.indextest\"][/test1]");
    id = mClient->find(query);
    waitForResponse1(id);
    QVariantList answer = mData.toMap().value("data").toList();
    QCOMPARE(answer.size(), 2);

    query.insert("query", "[?_type=\"com.test.indextest\"][/test2.nested]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("sortKeys").toList().at(0).toString(), QString::fromLatin1("test2.nested"));
    answer = mData.toMap().value("data").toList();
    QCOMPARE(answer.size(), 2);

    query.insert("query", "[?_type=\"com.test.IndexedView\"][/test3.nested]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("sortKeys").toList().at(0).toString(), QString::fromLatin1("test3.nested"));
    answer = mData.toMap().value("data").toList();
    QCOMPARE(answer.size(), 2);

    id = mClient->remove(result.value("data").toList());
    waitForResponse1(id);
}

void TestJsonDbClient::notify()
{
    int id = 400;

    // Create a notification object
    JsonDbClient::NotifyTypes actions = JsonDbClient::NotifyCreate|JsonDbClient::NotifyUpdate|JsonDbClient::NotifyRemove;
    const QString query = "[?_type=\"com.test.notify-test\"]";
    QString notifyUuid = mClient->registerNotification(actions, query);

    // Create a notify-test object
    QVariantMap object;
    object.insert("_type","com.test.notify-test");
    object.insert("name","test1");
    mNotifications.clear();
    id = mClient->create(object);
    waitForResponse4(-1, -1, notifyUuid, 1);
    QVariant uuid = mData.toMap().value("_uuid");
    QString version = mData.toMap().value("_version").toString();

    QCOMPARE(mNotifications.size(), 1);
    JsonDbTestNotification n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QString("create"));

    // Update the notify-test object
    object.insert("_uuid",uuid);
    object.insert("_version", version);
    object.insert("name","test2");
    id = mClient->update(object);
    waitForResponse4(-1, -1, notifyUuid, 1);

    QCOMPARE(mNotifications.size(), 1);
    n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QLatin1String("update"));

    // Remove the notify-test object
    id = mClient->remove(object);
    waitForResponse4(-1, -1, notifyUuid, 1);

    QCOMPARE(mNotifications.size(), 1);
    n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QLatin1String("remove"));

    // Remove the notification object
    mClient->unregisterNotification(notifyUuid);
}

void TestJsonDbClient::notifyUpdate()
{
    int id = 400;

    // Create a notification object
    JsonDbClient::NotifyTypes actions = JsonDbClient::NotifyCreate|JsonDbClient::NotifyUpdate|JsonDbClient::NotifyRemove;
    const QString query = "[?_type=\"com.test.notify-test\"][?filter=\"match\"]";
    QString notifyUuid = mClient->registerNotification(actions, query);

    // Create a notify-test object
    QVariantMap object;
    object.insert("_type","com.test.notify-test");
    object.insert("name","test1");
    object.insert("filter","match");
    id = mClient->create(object);
    waitForResponse4(id, -1, notifyUuid, 1);
    QVariant uuid = mData.toMap().value("_uuid");
    QString version = mData.toMap().value("_version").toString();

    QCOMPARE(mNotifications.size(), 1);
    JsonDbTestNotification n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QString("create"));

    // Update the notify-test object
    // query no longer matches, so we should receive a "remove" notification even though it is an update
    object.insert("_uuid",uuid);
    object.insert("_version", version);
    object.insert("filter","nomatch");
    id = mClient->update(object);
    waitForResponse4(id, -1, notifyUuid, 1);

    QCOMPARE(mNotifications.size(), 1);
    n = mNotifications.takeFirst();
    QCOMPARE(n.mNotifyUuid, notifyUuid);
    QCOMPARE(n.mAction, QLatin1String("remove"));

    // Remove the notify-test object
    id = mClient->remove(object);
    waitForResponse1(id);

    QCOMPARE(mNotifications.size(), 0);

    // Remove the notification object
    mClient->unregisterNotification(notifyUuid);
}

void TestJsonDbClient::notifyViaCreate()
{
    int id = 400;

    // Create a notification object
    QVariantMap notificationObject;
    QStringList actions;
    actions << "create" << "update" << "remove";
    const QString query = "[?_type=\"com.test.notify-test\"]";
    notificationObject.insert("_type", "notification");
    notificationObject.insert("query", query);
    notificationObject.insert("actions", actions);
    id = mClient->create(notificationObject, QLatin1String("Ephemeral"));
    waitForResponse1(id);
    QVariantMap notifyObject = mData.toMap();
    QString notifyUuid = notifyObject.value("_uuid").toString();

    // Create a notify-test object
    QVariantMap object;
    object.insert("_type","com.test.notify-test");
    object.insert("name","test1");
    id = mClient->create(object);
    waitForResponse4(id, -1, notifyUuid, 1);
    QVariant uuid = mData.toMap().value("_uuid");
    QString version = mData.toMap().value("_version").toString();

    QCOMPARE(mNotifications.size(), 1);
    JsonDbTestNotification n = mNotifications.takeFirst();
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
    id = mClient->remove(notifyObject, QLatin1String("Ephemeral"));
    waitForResponse1(id);
}

void TestJsonDbClient::registerNotification()
{
    int id = 400;

    // Create a notification object
    JsonDbClient::NotifyTypes actions = JsonDbClient::NotifyCreate|JsonDbClient::NotifyUpdate|JsonDbClient::NotifyRemove;
    const QString query = "[?_type=\"com.test.registerNotification\"]";
    QString notifyUuid = mClient->registerNotification(actions, query);

    // Create a registerNotification object
    QVariantMap object;
    object.insert("_type","com.test.registerNotification");
    object.insert("name","test1");
    id = mClient->create(object);
    waitForResponse4(id, -1, notifyUuid, 1);
    QVariant uuid = mData.toMap().value("_uuid");
    QString version = mData.toMap().value("_version").toString();

    QCOMPARE(mNotifications.size(), 1);
    JsonDbTestNotification n = mNotifications.takeFirst();
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
    const QString nquery = "[?_type=\"com.test.notify-test-remove-batch\"]";
    QString notifyUuid = mClient->registerNotification(actions, nquery);
    id = -1;
    waitForResponse1(id);

    // Create notify-test-remove-batch object
    QVariantList list;
    int count;
    for (count = 0 ; rbnames[count] ; count++) {
        QVariantMap object;
        object.insert("_type","com.test.notify-test-remove-batch");
        object.insert("name",rbnames[count]);
        list << object;
    }

    id = mClient->create(list);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("count").toInt(), count);

    // Retrieve the _uuids of the items
    QVariantMap query;
    query.insert("query", "[?_type=\"com.test.notify-test-remove-batch\"]");
    id = mClient->find(query);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), count);
    list = mData.toMap().value("data").toList();

    // Make a list of the uuids returned
    QVariantList uuidList;
    foreach (const QVariant& v, mData.toMap().value("data").toList())
        uuidList << v.toMap().value("_uuid");

    // Remove the objects
    mNotifications.clear();
    id = mClient->remove(list);
    waitForResponse4(id, -1, notifyUuid, count);

    QCOMPARE(mNotifications.size(), count);
    while (mNotifications.length()) {
        JsonDbTestNotification n = mNotifications.takeFirst();
        QCOMPARE(n.mNotifyUuid, notifyUuid);
        QCOMPARE(n.mAction, QLatin1String("remove"));
        QVariant uuid = n.mObject.toMap().value("_uuid");
        QVERIFY(uuidList.contains(uuid));
        uuidList.removeOne(uuid);
    }

    // Remove the notification object
    mClient->unregisterNotification(notifyUuid);
}

void TestJsonDbClient::schemaValidation()
{
    int id;
    if (!wasRoot) {
        QFile schemaFile(findFile("create-test.json"));
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
        schemaObject.insert("name", "com.test.SchemaTestObject");
        schemaObject.insert("schema", schemaBody);
        //qDebug() << "schemaObject" << schemaObject;
        id = mClient->create(schemaObject);
        waitForResponse1(id);
        QVERIFY(mData.toMap().contains("_uuid"));
    }

    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "com.test.SchemaTestObject");
    item.insert("create-test", 22);
    item.insert("another-field", "a string");

    // Create an item that matches the schema
    id = mClient->create(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("_uuid"));
    QVariant uuid = mData.toMap().value("_uuid");

    // Create an item that does not match the schema

    QVariantMap noncompliant;
    noncompliant.insert(JsonDbString::kTypeStr, "com.test.SchemaTestObject");
    noncompliant.insert("create-test", 22);
    id = mClient->create(noncompliant);
    waitForResponse2(id, JsonDbError::FailedSchemaValidation);
}

void TestJsonDbClient::remove()
{
    QVERIFY(mClient);

    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "com.test.remove-test");

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
    id = mClient->query(QLatin1String("[?_type=\"com.test.remove-test\"]"));
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 4);

    // Set the _uuid field and attempt to remove first item
    item.insert("_uuid", uuid);
    id = mClient->remove(item);
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("count"));
    QCOMPARE(mData.toMap().value("count").toInt(), 1);

    // query and make sure there are only three items left
    id = mClient->query(QLatin1String("[?_type=\"com.test.remove-test\"]"));
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("length"));
    QCOMPARE(mData.toMap().value("length").toInt(), 3);

    // Remove two items using query
    id = mClient->remove(QString::fromLatin1("[?_type=\"com.test.remove-test\"][?foo >= 63]"));
    waitForResponse1(id);
    QVERIFY(mData.toMap().contains("count"));
    QCOMPARE(mData.toMap().value("count").toInt(), 2);

    // query and make sure there are only one item left
    id = mClient->query(QLatin1String("[?_type=\"com.test.remove-test\"]"));
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
    const QString query = "[?_type=\"com.test.notify-test\"][?identifier=\"w1-identifier\"]";

    ClientWrapper w1; w1.connectToServer();
    ClientWrapper w2; w2.connectToServer();

    QString notifyUuid = w1.mClient->registerNotification(actions, query);

    // Create a notify-test object
    QVariantMap object;
    object.insert("_type","com.test.notify-test");
    object.insert("identifier","w1-identifier");
    int id = w1.mClient->create(object);
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
    w1.mClient->unregisterNotification(notifyUuid);
}

void TestJsonDbClient::changesSince()
{
    QVERIFY(mClient);

    int id = mClient->changesSince(0);
    waitForResponse1(id);

    int state = mData.toMap()["currentStateNumber"].toInt();

    QVariantMap c1, c2, c3;
    c1["_type"] = c2["_type"] = c3["_type"] = "com.test.TestContact";
    c1["firstName"] = "John";
    c1["lastName"] = "Doe";
    c2["firstName"] = "George";
    c2["lastName"] = "Washington";
    c3["firstName"] = "Betsy";
    c3["lastName"] = "Ross";

    id = mClient->create(c1);
    waitForResponse1(id);
    c1["_uuid"] = mData.toMap().value("_uuid");
    c1["_version"] = mData.toMap().value("_version");

    id = mClient->create(c2);
    waitForResponse1(id);
    c2["_uuid"] = mData.toMap().value("_uuid");
    c2["_version"] = mData.toMap().value("_version");

    id = mClient->create(c3);
    waitForResponse1(id);
    c3["_uuid"] = mData.toMap().value("_uuid");
    c3["_version"] = mData.toMap().value("_version");

    id = mClient->remove(c1);
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
    //qDebug() << writer.toByteArray(results[0]);
    //qDebug() << writer.toByteArray(results[1]);
    QVERIFY(results[0].toMap()["before"].toMap().isEmpty());
    QVERIFY(results[1].toMap()["before"].toMap().isEmpty());

    QVariantMap r1(results[0].toMap()["after"].toMap());
    QVariantMap r2(results[1].toMap()["after"].toMap());

    QMap<QString,QVariantMap> objectsByUuid;
    objectsByUuid[c2["_uuid"].toString()] = c2;
    objectsByUuid[c3["_uuid"].toString()] = c3;
    for (int i = 0; i < results.size(); i++) {
        QVariantMap after = results[i].toMap()["after"].toMap();
        QVariantMap original = objectsByUuid[after["_uuid"].toString()];
        QMapIterator<QString, QVariant> j(original);
        while (j.hasNext()) {
            j.next();
            QCOMPARE(j.value().toString(), after[j.key()].toString());
        }
    }
}

static QStringList stringList(QString s)
{
    return (QStringList() << s);
}

void TestJsonDbClient::storageQuotas()
{
    QVERIFY(mClient);

    // Create Security Object with storage quota
    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, "Quota");
    struct passwd *pwd = getpwent ();
    item.insert(JsonDbString::kTokenStr, QString::fromAscii(pwd->pw_name));
    QVariantMap capability;
    QVariantMap quotas;
    quotas.insert("storage", int(512));
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
    connection.connectToServer();
    JsonDbClient tokenClient(&connection);
    connect( &tokenClient, SIGNAL(notified(QString,QtAddOn::JsonDb::JsonDbNotification)),
             this, SLOT(notified(QString,QtAddOn::JsonDb::JsonDbNotification)));
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
    QString notifyUuid =mClient->registerNotification(JsonDbClient::NotifyCreate, "[?_type=\"com.test.requestWithSlot\"]", QString(),
                                                       &handler, SLOT(notify(QString,QtAddOn::JsonDb::JsonDbNotification)),
                                                       &handler, SLOT(success(int,QVariant)), SLOT(error(int,int,QString)));
    qDebug() << "notifyUuid" << notifyUuid;
    int id;
    handler.clear();

    QVariantMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.test.requestWithSlot"));
    item.insert("create-test", 42);
    id = mClient->create(item, QString(), &handler, SLOT(success(int,QVariant)), SLOT(error(int,int,QString)));
    waitForResponse4(id, -1, notifyUuid, 1);
    QVERIFY(mData.toMap().contains("_uuid"));
    QString uuid = mData.toMap().value("_uuid").toString();

    QCOMPARE(handler.requestId, id);
    QVERIFY(handler.data.toMap().contains("_uuid"));
    QCOMPARE(handler.data.toMap().value("_uuid").toString(), uuid);

    QCOMPARE(handler.errorCount, 0);
    QCOMPARE(handler.successCount, 2);
    QCOMPARE(handler.notifyCount, 1);
    QCOMPARE(handler.notifyUuid, notifyUuid);

    // Remove the notification object
    mClient->unregisterNotification(notifyUuid);
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

void TestJsonDbClient::partition()
{
    int id;
    const QString firstPartitionName = "com.example.autotest.Partition1";
    const QString secondPartitionName = "com.example.autotest.Partition2";

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
    id = mClient->create(item, firstPartitionName);
    waitForResponse1(id);

    item = QVariantMap();
    item.insert(JsonDbString::kTypeStr, "Foobar");
    item.insert("one", "two");
    id = mClient->create(item, secondPartitionName);
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
    { resultsReadyCalls++; resultsReadyCount = count; }
    void finished()
    { finishedCalls++; }
    void error(QtAddOn::JsonDb::JsonDbError::ErrorCode code, const QString &message)
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
        item.insert("_type", QLatin1String("com.test.queryObject"));
        item.insert("foo", i);
        list.append(item);
    }

    int id = mClient->create(list);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("count").toInt(), 10);

    JsonDbQuery *r = mClient->query();
    r->setQuery(QLatin1String("[?_type=\"com.test.queryObject\"]"));

    QueryHandler handler;
    connect(r, SIGNAL(started()), &handler, SLOT(started()));
    connect(r, SIGNAL(resultsReady(int)), &handler, SLOT(resultsReady(int)));
    connect(r, SIGNAL(finished()), &handler, SLOT(finished()));
    connect(r, SIGNAL(error(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)),
            &handler, SLOT(error(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)));

    QEventLoop loop;
    connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(r, SIGNAL(error(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)), &loop, SLOT(quit()));

    r->start();
    loop.exec();

    QCOMPARE(handler.startedCalls, 1);
    QCOMPARE(handler.resultsReadyCalls, 2);
    QCOMPARE(handler.resultsReadyCount, 10);
    QCOMPARE(r->resultsAvailable(), 10);
    QCOMPARE(handler.finishedCalls, 1);
    QCOMPARE(handler.errorCalls, 0);
    QVariantList results = r->takeResults();
    QCOMPARE(results.size(), 10);
    QCOMPARE(r->resultsAvailable(), 0);
}

void TestJsonDbClient::changesSinceObject()
{
    QVariantMap item;
    item.insert("_type", QLatin1String("com.test.queryObject"));
    item.insert("foo", -1);
    int id = mClient->create(item);
    waitForResponse1(id);

    id = mClient->query(QLatin1String("[?_type=\"com.test.queryObject\"]"), 0, 1);
    waitForResponse1(id);
    QCOMPARE(mData.toMap().value("length").toInt(), 1);
    quint32 stateNumber = mData.toMap().value("state").value<quint32>();

    // Create a few items
    for (int i = 0; i < 2; ++i) {
        QVariantMap item;
        item.insert("_type", QLatin1String("com.test.queryObject"));
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
    connect(r, SIGNAL(error(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)), &handler, SLOT(error(int,QString)));

    QEventLoop loop;
    connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(r, SIGNAL(error(QtAddOn::JsonDb::JsonDbError::ErrorCode,QString)), &loop, SLOT(quit()));

    r->start();
    loop.exec();

    QCOMPARE(handler.startedCalls, 1);
    QCOMPARE(handler.resultsReadyCalls, 2);
//    QCOMPARE(handler.resultsReadyCount, 16); // there is something fishy with jsondb state handling
    QCOMPARE(handler.finishedCalls, 1);
    QCOMPARE(handler.errorCalls, 0);
}

void TestJsonDbClient::jsondbobject()
{
    JsonDbObject o;
    QVERIFY(o.isEmpty());
    QUuid uuid = o.uuid();
    QVERIFY(uuid.isNull());
    QVERIFY(!o.contains(QLatin1String("_uuid")));

    uuid = QUuid::createUuid();
    o.setUuid(uuid);
    QCOMPARE(o.uuid(), uuid);
    QVariant v = o.value(QLatin1String("_uuid"));
    QVERIFY(!v.isNull());
    QVERIFY(v.canConvert<QUuid>());
    QCOMPARE(v.value<QUuid>(), uuid);

    o = JsonDbObject();
    QVERIFY(o.uuid().isNull());
    o.insert(QLatin1String("_uuid"), QVariant::fromValue(uuid));
    QCOMPARE(o.uuid(), uuid);
    v = o.value(QLatin1String("_uuid"));
    QVERIFY(!v.isNull());
    QVERIFY(v.canConvert<QUuid>());
    QCOMPARE(v.value<QUuid>(), uuid);
}

void TestJsonDbClient::jsondbobject_uuidFromObject()
{
    {
        // empty object
        JsonDbObject o;
        QUuid uuid = JsonDbObject::uuidFromObject(o);
        QVERIFY(!uuid.isNull());
    }

    {
        // object without _id
        JsonDbObject o;
        o.insert(QLatin1String("foo"), QLatin1String("bar"));
        QUuid uuid = JsonDbObject::uuidFromObject(o);
        QVERIFY(!uuid.isNull());
        QUuid uuid2 = JsonDbObject::uuidFromObject(o);
        QVERIFY(!uuid2.isNull());
        QVERIFY(uuid != uuid2);
    }

    {
        // object _id
        JsonDbObject o;
        o.insert(QLatin1String("_id"), QLatin1String("Venus"));
        o.insert(QLatin1String("foo"), QLatin1String("bar"));
        QUuid uuid = JsonDbObject::uuidFromObject(o);
        QVERIFY(!uuid.isNull());
        QUuid uuid2 = JsonDbObject::uuidFromObject(o);
        QVERIFY(!uuid2.isNull());
        QCOMPARE(uuid, uuid2);

        o.insert(QLatin1String("foo"), QLatin1String("ZZZZZAAAAAPPPP"));
        o.insert(QLatin1String("a"), QLatin1String("b"));
        QUuid uuid3 = JsonDbObject::uuidFromObject(o);
        QVERIFY(!uuid3.isNull());
        QCOMPARE(uuid, uuid3);
    }
}

#ifdef Q_OS_LINUX
void TestJsonDbClient::sigstop()
{
#ifdef DONT_START_SERVER
    QSKIP("cannot test sigstop without starting server");
#endif
    QStringList argList = QStringList() << "-sigstop";
    argList << QString::fromLatin1("sigstop.db");

    QProcess *jsondb = launchJsonDbDaemon(JSONDB_DAEMON_BASE, QString("testjsondb_sigstop%1").arg(getpid()), argList);
    int status;
    ::waitpid(jsondb->pid(), &status, WUNTRACED);
    QVERIFY(WIFSTOPPED(status));
    ::kill(jsondb->pid(), SIGCONT);
    ::waitpid(jsondb->pid(), &status, WCONTINUED);
    QVERIFY(WIFCONTINUED(status));
    ::kill(jsondb->pid(), SIGTERM);
    ::waitpid(jsondb->pid(), &status, 0);
}
#endif

QTEST_MAIN(TestJsonDbClient)

#include "test-jsondb-client.moc"
