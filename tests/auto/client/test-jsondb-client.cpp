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
#include <QJsonArray>

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

#include "qjsondbconnection.h"
#include "qjsondbobject.h"
#include "qjsondbreadrequest.h"
#include "qjsondbwriterequest.h"
#include "testhelper.h"

Q_DECLARE_METATYPE(QUuid)

QT_USE_NAMESPACE_JSONDB

// #define EXTRA_DEBUG

// #define DONT_START_SERVER

class TestJsonDbClient: public TestHelper
{
    Q_OBJECT
public:
    TestJsonDbClient();
    ~TestJsonDbClient();

public slots:
    void statusChanged(QtJsonDb::QJsonDbConnection::Status status);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void connectionStatus();

    void create();
    void createList();
    void update();
    void find();
    void index();
    void multiTypeIndex();

    void notifyCreateUpdateRemove_data();
    void notifyCreateUpdateRemove();
    void notifyRemoveViaUpdate();
    void notifyRemoveBatch();
    void notifyMultiple();
    void mapNotification();

    void remove();
    void schemaValidation();
    void changesSince();
    void partition();
    void jsondbobject();

#ifdef Q_OS_LINUX
    void sigstop();
#endif

private:
    bool wasRoot;
    uid_t uidUsed;
    uid_t uid2Used;
    QList<gid_t> gidsAdded;
    bool failed;
    qint64 pid;

    inline bool sendWaitTake(QJsonDbRequest *request, QList<QJsonObject> *results = 0)
    {
        Q_ASSERT(mConnection && request);
        if (!mConnection->send(request))
            return false;
        if (!waitForResponse(request))
            return false;
        QList<QJsonObject> res = request->takeResults();
        if (results)
            *results = res;
        return true;
    }
};

TestJsonDbClient::TestJsonDbClient()
    : wasRoot(false), pid(0)
{
#ifdef EXTRA_DEBUG
    this->debug_output = true;
#endif
}

TestJsonDbClient::~TestJsonDbClient()
{
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

    QStringList arg_list = (QStringList()
                            << "-validate-schemas");
    if (wasRoot)
        arg_list << "-enforce-access-control";
    pid = launchJsonDbDaemonDetached(arg_list, __FILE__);

#if !defined(Q_OS_MAC)
    if (wasRoot) {
        connectToServer();

        // Add the needed Capability objects to database
        QJsonDbObject capa_obj;
        capa_obj.insert(QLatin1String("_type"), QLatin1String("Capability"));
        capa_obj.insert(QLatin1String("name"), QLatin1String("User"));
        capa_obj.insert(QLatin1String("partition"), QLatin1String("test-jsondb-client"));
        QJsonObject access_rules;
        QJsonObject rw_rule;
        rw_rule.insert(QLatin1String("read"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        rw_rule.insert(QLatin1String("write"),
                       QJsonArray::fromStringList(QStringList() <<
                           QLatin1String("[?_type startsWith \"public.\"]") <<
                           QLatin1String("[?%owner startsWith %typeDomain]") <<
                           QLatin1String("[?%owner startsWith \"com.my.domain\"][?_type startsWith \"com.my.domain\"]") <<
                           QLatin1String("[?_type = \"_schemaType\"]") <<
                           QLatin1String("[?_type = \"Index\"]") <<
                           QLatin1String("[?_type = \"Partition\"]") <<
                           QLatin1String("[?_type startsWith \"Phone\"]") <<
                           QLatin1String("[?_type = \"Contact\"]") <<
                           QLatin1String("[?_type = \"Reduce\"]") <<
                           QLatin1String("[?_type = \"Map\"]")));
        access_rules.insert(QLatin1String("rw"), rw_rule);
        QJsonObject set_owner_rule;
        set_owner_rule.insert(QLatin1String("setOwner"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        set_owner_rule.insert(QLatin1String("read"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        set_owner_rule.insert(QLatin1String("write"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        access_rules.insert(QLatin1String("setOwner"), set_owner_rule);
        capa_obj.insert(QLatin1String("accessRules"), access_rules);

        capa_obj.setUuid(QJsonDbObject::createUuid());
        QJsonDbWriteRequest toCreate;

        toCreate.setObjects(QList<QJsonObject>() << capa_obj);
        QVERIFY(sendWaitTake(&toCreate));

        capa_obj = QJsonObject();
        capa_obj.insert(QLatin1String("_type"), QLatin1String("Capability"));
        capa_obj.insert(QLatin1String("name"), QLatin1String("User"));
        capa_obj.insert(QLatin1String("partition"), QLatin1String("Ephemeral"));
        access_rules = QJsonObject();
        rw_rule = QJsonObject();
        rw_rule.insert(QLatin1String("read"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        rw_rule.insert(QLatin1String("write"),
                       QJsonArray::fromStringList(QStringList() <<
                           QLatin1String("[?_type=\"notification\"]") <<
                           QLatin1String("[?_type startsWith \"public.\"]") <<
                           QLatin1String("[?%owner startsWith %typeDomain]")));
        access_rules.insert(QLatin1String("rw"), rw_rule);
        capa_obj.insert(QLatin1String("accessRules"), access_rules);

        capa_obj.setUuid(QJsonDbObject::createUuid());
        toCreate.setObjects(QList<QJsonObject>() << capa_obj);
        QVERIFY(sendWaitTake(&toCreate));

        capa_obj = QJsonObject();
        capa_obj.insert(QLatin1String("_type"), QLatin1String("Capability"));
        capa_obj.insert(QLatin1String("name"), QLatin1String("User"));
        capa_obj.insert(QLatin1String("partition"), QLatin1String("com.example.autotest.Partition1"));
        access_rules = QJsonObject();
        rw_rule = QJsonObject();
        rw_rule.insert(QLatin1String("read"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        rw_rule.insert(QLatin1String("write"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        access_rules.insert(QLatin1String("rw"), rw_rule);
        capa_obj.insert(QLatin1String("accessRules"), access_rules);

        capa_obj.setUuid(QJsonDbObject::createUuid());
        toCreate.setObjects(QList<QJsonObject>() << capa_obj);
        QVERIFY(sendWaitTake(&toCreate));

        capa_obj = QJsonObject();
        capa_obj.insert(QLatin1String("_type"), QLatin1String("Capability"));
        capa_obj.insert(QLatin1String("name"), QLatin1String("User"));
        capa_obj.insert(QLatin1String("partition"), QLatin1String("com.example.autotest.Partition2"));
        access_rules = QJsonObject();
        rw_rule = QJsonObject();
        rw_rule.insert(QLatin1String("read"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        rw_rule.insert(QLatin1String("write"), QJsonArray::fromStringList(QStringList() << QLatin1String("[*]")));
        access_rules.insert(QLatin1String("rw"), rw_rule);
        capa_obj.insert(QLatin1String("accessRules"), access_rules);

        capa_obj.setUuid(QJsonDbObject::createUuid());
        toCreate.setObjects(QList<QJsonObject>() << capa_obj);
        QVERIFY(sendWaitTake(&toCreate));

        // Add a test object
        QJsonDbObject foo_obj;
        foo_obj.insert(QLatin1String("_type"), QLatin1String("com.test.bar.FooType"));
        foo_obj.insert(QLatin1String("name"), QLatin1String("foo"));
        qDebug() << "Creating: " << foo_obj;

        foo_obj.setUuid(QJsonDbObject::createUuid());
        toCreate.setObjects(QList<QJsonObject>() << foo_obj);
        QList<QJsonObject> results;
        QVERIFY(sendWaitTake(&toCreate, &results));

        QVERIFY(!results[0].value(QLatin1String("_uuid")).toString().isEmpty());

        QJsonDbReadRequest query;
        query.setQuery(QLatin1String("[?_type=\"com.test.bar.FooType\"]"));
        QVERIFY(sendWaitTake(&query, &results));

        foo_obj = results[0];
        foo_obj.insert(QLatin1String("_owner"), QLatin1String("com.test.bar.app"));
        QJsonDbUpdateRequest toUpdate(foo_obj);
        QVERIFY(sendWaitTake(&toUpdate));

        // Add a schemaValidation tests object (must be done as root)
        QJsonArray schemaBody = readJsonFile(findFile("create-test.json")).array();
        //qDebug() << "schemaBody" << schemaBody;
        QJsonDbObject schemaObject;
        schemaObject.insert("_type", QLatin1String("schema"));
        schemaObject.insert("name", QLatin1String("com.test.SchemaTestObject"));
        schemaObject.insert("schema", schemaBody);
        schemaObject.setUuid(QJsonDbObject::createUuid());
        //qDebug() << "schemaObject" << schemaObject;
        toCreate.setObjects(QList<QJsonObject>() << schemaObject);
        QVERIFY(sendWaitTake(&toCreate, &results));
        QVERIFY(results[0].contains("_uuid"));

        qDebug() << "Disconnect client that was connected as root.";
        disconnectFromServer();

        // Create needed test users & groups
        QString etcipwd = QStringLiteral (NSS_PREFIX "/etc/passwd");
        QString etcigr = QStringLiteral (NSS_PREFIX "/etc/group");
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
        QByteArray appNameBA = appName.toLocal8Bit();
        // Tes app name for app without any supplementary groups
        QString app2Name = QString("com.test.bar.%1").arg(getpid());
        QByteArray app2NameBA = app2Name.toLocal8Bit();
        if (!errno) {
            // Add primary groups
            char *members[] = { NULL };
            struct group grp;
            grp.gr_name = appNameBA.data();
            grp.gr_passwd = NULL;
            grp.gr_gid = gid;
            grp.gr_mem = members;
            struct group grp2;
            grp2.gr_name = app2NameBA.data();
            grp2.gr_passwd = NULL;
            grp2.gr_gid = gid2;
            grp2.gr_mem = members;
            QByteArray etcigrBA = etcigr.toLocal8Bit();
            FILE *grfile = ::fopen (etcigrBA.data(), "a");
            ::putgrent(&grp, grfile);
            ::putgrent(&grp2, grfile);
            ::fclose (grfile);
            gidsAdded.append(gid);
            gidsAdded.append(gid2);

            // Add the user
            struct passwd pwd;
            pwd.pw_name = appNameBA.data();
            pwd.pw_passwd = NULL;
            pwd.pw_uid = uid;
            pwd.pw_gid = gid;
            pwd.pw_gecos = NULL;
            pwd.pw_dir = NULL;
            pwd.pw_shell = NULL;
            struct passwd pwd2;
            pwd2.pw_name = app2NameBA.data();
            pwd2.pw_passwd = NULL;
            pwd2.pw_uid = uid2;
            pwd2.pw_gid = gid2;
            pwd2.pw_gecos = NULL;
            pwd2.pw_dir = NULL;
            pwd2.pw_shell = NULL;
            QByteArray etcipwdBA = etcipwd.toLocal8Bit();
            FILE *pwdfile = ::fopen (etcipwdBA.data(), "a");
            ::putpwent(&pwd, pwdfile);
            ::putpwent(&pwd2, pwdfile);
            ::fclose (pwdfile);

            char *members2[] = { appNameBA.data(), NULL };
            // Add 'User' supplementary group
            gid = nextFreeGid(gid2+1);
            grp.gr_name = const_cast<char *>("User");
            grp.gr_passwd = NULL;
            grp.gr_gid = gid;
            // Add only the first user to it
            grp.gr_mem = members2;
            grfile = ::fopen (etcigrBA.data(), "a");
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
}

void TestJsonDbClient::cleanupTestCase()
{
    if (pid) {
#if !defined(Q_OS_MAC)
        if (wasRoot)
            ::seteuid(0);
#endif
        ::kill(pid, SIGTERM);
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

    stopDaemon();
}

void TestJsonDbClient::init()
{
    clearHelperData();
    connectToServer();
}

void TestJsonDbClient::cleanup()
{
    disconnectFromServer();
}


void TestJsonDbClient::statusChanged(QJsonDbConnection::Status status)
{
    static int callNum  = 0;
    if (callNum == 0)
        QCOMPARE((int)status, (int)QJsonDbConnection::Connecting);
    if (callNum == 1)
        QCOMPARE((int)status, (int)QJsonDbConnection::Connected);
    if (callNum == 2)
        QCOMPARE((int)status, (int)QJsonDbConnection::Unconnected);

    callNum++;
}

void TestJsonDbClient::connectionStatus()
{
    QJsonDbConnection *connection = new QJsonDbConnection;

    QCOMPARE((int)connection->status(), (int)QJsonDbConnection::Unconnected);

    QObject::connect(connection, SIGNAL(statusChanged(QtJsonDb::QJsonDbConnection::Status)), this, SLOT(statusChanged(QtJsonDb::QJsonDbConnection::Status)));

    connection->connectToServer();
    if (connection->status() != (int)QJsonDbConnection::Connected)
        blockWithTimeout();

    QCOMPARE((int)connection->status(), (int)QJsonDbConnection::Connected);

    connection->disconnectFromServer();
    if (connection->status() != (int)QJsonDbConnection::Unconnected)
        blockWithTimeout();

    QCOMPARE((int)connection->status(), (int)QJsonDbConnection::Unconnected);

    delete connection;
}

/*
 * Create an item and remove it
 */

void TestJsonDbClient::create()
{
    QVERIFY(mConnection);

    QJsonDbWriteRequest request;

    QJsonDbObject item;
    item.setUuid(QJsonDbObject::createUuid());
    item.insert("_type", QLatin1String("com.test.create-test"));
    item.insert("create-test", 22);

    // Create an item
    request.setObjects(QList<QJsonObject>() << item);
    QList<QJsonObject> results;
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), 1);
    QVERIFY(results[0].contains("_uuid"));
    QUuid uuid = results[0].value("_uuid").toString();
    QVERIFY(results[0].contains("_version"));
    item.insert("_version", results[0].value("_version"));

    // Attempt to remove it without supplying a _uuid
    item.remove("_uuid");
    item.insert("_delete", QLatin1String("true"));
    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request, &results));

    // Set the _uuid field and attempt to remove it again
    item.setUuid(uuid);
    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request, &results));
}


/*
 * Create an array of items and remove them
 */

static const char *names[] = { "Abe", "Beth", "Carlos", "Dwight", "Emu", "Francis", NULL };

void TestJsonDbClient::createList()
{
    // Create a few items
    QList<QJsonObject> list;
    int count;
    for (count = 0 ; names[count] ; count++ ) {
        QJsonDbObject item;
        item.insert("_type", QLatin1String("com.test.create-list-test"));
        item.insert("name", QLatin1String(names[count]));
        item.setUuid(QJsonDbObject::createUuid());
        list.append(item);
    }

    QJsonDbWriteRequest request;
    request.setObjects(list);
    QList<QJsonObject> results;
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), count);

    // Retrieve the _uuids of the items
    QJsonDbReadRequest query;
    query.setQuery("[?_type=\"com.test.create-list-test\"]");
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), count);

    // Extract the uuids and put into a separate list
    list.clear();
    for (int i = 0; i < results.size(); i++) {
        const QJsonDbObject& v = results.at(i);
        QJsonDbObject obj;
        obj.setUuid(v.uuid());
        obj.insert("_type", v.value("_type"));
        obj.insert("_version", v.value("_version"));
        obj.insert("_deleted", QJsonValue(true));
        list.append(obj);
    }

    request.setObjects(list);
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), count);
}

///*
// * Update item
// */

void TestJsonDbClient::update()
{
    QVERIFY(mConnection);

    QJsonDbObject item;
    QJsonDbReadRequest query;
    QJsonDbWriteRequest request;
    QList<QJsonObject> results;

    // Create a item
    item.insert("_type", QLatin1String("com.test.update-test"));
    item.insert("name", QLatin1String(names[0]));
    item.setUuid(QJsonDbObject::createUuid());

    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), 1);

    QString uuid = results[0].value("_uuid").toString();
    QString version = results[0].value("_version").toString();

    // Check that it's there
    query.setQuery("[?_type=\"com.test.update-test\"]");
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].value("name").toString(), QString(names[0]));

    // Change the item
    item.insert("name", QLatin1String(names[1]));
    item.insert("_uuid", uuid);
    item.insert("_version", version);

    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), 1);

    // Check that it's really changed
    query.setQuery("[?_type=\"com.test.update-test\"]");
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].value("name").toString(), QString(names[1]));
    QVERIFY(results[0].value("_version").toString() != version);

    // Test access control
#if !defined(Q_OS_MAC)
    if (wasRoot) {
        // Find the test item
        query.setQuery("[?_type=\"com.test.bar.FooType\"]");
        query.setQueryLimit(1);
        QVERIFY(sendWaitTake(&query, &results));
        QJsonObject item1 = results[0];
        item1[QLatin1String("name")] = QLatin1String("fail");
        request.setObjects(QList<QJsonObject>() << item1);
        QVERIFY(sendWaitTake(&request, &results));
        // Should fail because of access control
        QCOMPARE(request.status(), QJsonDbRequest::Error);
        QVERIFY(mRequestErrors.contains(&request));
        QCOMPARE(mRequestErrors[&request], QJsonDbRequest::OperationNotPermitted);

        item1.insert(QLatin1String("_type"), QLatin1String("com.test.FooType"));

        request.setObjects(QList<QJsonObject>() << item1);
        QVERIFY(sendWaitTake(&request, &results));
        // Should fail because of access control
        // The new _type is ok, but the old _type is not
        QCOMPARE(request.status(), QJsonDbRequest::Error);
        QVERIFY(mRequestErrors.contains(&request));
        QCOMPARE(mRequestErrors[&request], QJsonDbRequest::OperationNotPermitted);
    }
#endif
}

///*
// * Find items
// */

void TestJsonDbClient::find()
{
    QVERIFY(mConnection);

    QJsonDbObject item;
    QJsonDbReadRequest query;
    QJsonDbWriteRequest request;
    QList<QJsonObject> results;
    int count;

    // create an index on the name property of com.test.NameIndex objects
    QJsonDbObject index;
    index.insert("_type", QLatin1String("Index"));
    index.insert("name", QLatin1String("com.test.NameIndex"));
    index.insert("propertyName", QLatin1String("name"));
    index.insert("objectType", QLatin1String("com.test.find-test"));
    index.insert("propertyType", QLatin1String("string"));
    request.setObjects(QList<QJsonObject>() << index);
    QVERIFY(sendWaitTake(&request));

    QStringList nameList;
    // Create a few items
    for (count = 0 ; names[count] ; count++ ) {
        nameList << names[count];
        item.insert("_type", QLatin1String("com.test.find-test"));
        item.insert("name", QLatin1String(names[count]));
        item.setUuid(QJsonDbObject::createUuid());

        request.setObjects(QList<QJsonObject>() << item);
        QVERIFY(sendWaitTake(&request));
    }

    query.setQuery("[?_type=\"com.test.find-test\"]");
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), count);
    for (int i = 0; i < count; i++) {
        QVERIFY(nameList.contains(results.at(i).value("name").toString()));
    }

    // Find them, but limit it to just one
    query.setQuery("[?_type=\"com.test.find-test\"][\\com.test.NameIndex]");
    query.setQueryLimit(1);
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 1);
    QVERIFY(nameList.contains(results.at(0).value("name").toString()));

    // Find one, sorted in reverse alphabetical order
    query.setQuery("[?_type=\"com.test.find-test\"][\\com.test.NameIndex]");
    query.setQueryLimit(-1);
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 6);
    QCOMPARE(results.at(0).value("name").toString(), QString(names[count-1]));

    nameList.clear();
    QStringList answerNames;
    for (int i = 0; i < count; i++) {
        answerNames << results.at(i).value("name").toString();
        nameList << names[count - i - 1];
    }
    QCOMPARE(answerNames, nameList);
#if !defined(Q_OS_MAC)
    if (wasRoot) {
        // Set user id that has no supplementary groups
        disconnectFromServer();

        qDebug() << "Setting uid to" << uid2Used;
        ::seteuid(0);
        ::seteuid(uid2Used);

        connectToServer();
        if (mConnection->status() != QJsonDbConnection::Connected)
            blockWithTimeout();
        QCOMPARE((int)mConnection->status(), (int)QJsonDbConnection::Connected);

        // Read should fail
        query.setQuery("[?_type=\"com.test.find-test\"][\\com.test.NameIndex]");
        QVERIFY(sendWaitTake(&query, &results));
        QCOMPARE(results.size(), 0);

        // Go back to 'capable' user
        disconnectFromServer();

        ::seteuid(0);
        ::seteuid(uidUsed);
        connectToServer();
        connectToServer();
        if (mConnection->status() != QJsonDbConnection::Connected)
            blockWithTimeout();
        QCOMPARE((int)mConnection->status(), (int)QJsonDbConnection::Connected);
    }
#endif
}

void TestJsonDbClient::index()
{
    QJsonArray data = readJsonFile(":/json/client/index-test.json").array();
    QJsonDbWriteRequest request;
    QList<QJsonObject> results;
    QList<QJsonObject> objects;

    foreach (const QJsonValue v, data) {
        QJsonDbObject o = v.toObject();
        o.setUuid(QJsonDbObject::createUuid());
        objects.append(o);
    }

    request.setObjects(objects);
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), 7);

    for (int i = 0; i < objects.size(); ++i) {
        objects[i].insert("_version", results[i].value("_version"));
    }

    QJsonDbReadRequest query;
    query.setQuery("[?_type=\"com.test.indextest\"][/test1]");
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 2);

    query.setQuery("[?_type=\"com.test.indextest\"][/test2.nested]");
    QVERIFY(sendWaitTake(&query, &results));

    QCOMPARE(query.sortKey(), QString::fromLatin1("test2.nested"));
    QCOMPARE(results.size(), 2);

    query.setQuery("[?_type=\"com.test.IndexedView\"][/test3.nested]");
    QVERIFY(sendWaitTake(&query, &results));

    QCOMPARE(query.sortKey(), QString::fromLatin1("test3.nested"));
    QCOMPARE(results.size(), 2);

    // schemas need to be deleted last or else we'll get an error because the maps still exist
    QList<QJsonObject> toDelete;
    QList<QJsonObject> schemasToDelete;

    foreach (const QJsonObject &d, objects) {
        QJsonObject m = d;
        m.insert("_deleted", QJsonValue(true));
        QString type = d.value("_type").toString();
        if (type == "_schemaType")
            schemasToDelete.append(m);
        else
            toDelete.append(m);
    }

    request.setObjects(toDelete);
    QVERIFY(sendWaitTake(&request));

    request.setObjects(schemasToDelete);
    QVERIFY(sendWaitTake(&request));
}

void TestJsonDbClient::multiTypeIndex()
{
    QVERIFY(mConnection);

    QJsonDbWriteRequest request;
    QJsonDbReadRequest query;
    QList<QJsonObject> results;
    int count;

    QStringList indexedTypes = (QStringList()
                                << QLatin1String("com.test.find-test1")
                                << QLatin1String("com.test.find-test2"));
    // create an index on the name property of com.test.NameIndex objects
    QJsonObject index;
    index.insert("_type", QLatin1String("Index"));
    index.insert("name", QLatin1String("com.test.MultiTypeIndex"));
    index.insert("propertyName", QLatin1String("name"));
    index.insert("objectType", QJsonArray::fromStringList(indexedTypes));
    request.setObjects(QList<QJsonObject>() << index);
    QVERIFY(sendWaitTake(&request));

    QStringList objectTypes = (QStringList()
                               << QLatin1String("com.test.find-test1")
                               << QLatin1String("com.test.find-test2")
                               << QLatin1String("com.test.find-test3"));
    QStringList nameList;
    // Create a few items
    for (count = 0 ; names[count] ; count++ ) {
        nameList << names[count];
        foreach (const QString objectType, objectTypes) {
            QJsonDbObject item;
            item.insert("_type", objectType);
            item.insert("name", QLatin1String(names[count]));
            item.setUuid(QJsonDbObject::createUuid());
            request.setObjects(QList<QJsonObject>() << item);
            QVERIFY(sendWaitTake(&request));
        }
    }

    // Find all in the index
    query.setQuery("[?name exists][/com.test.MultiTypeIndex]");
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), nameList.size() * 2);
    foreach (const QJsonObject &v, results)
        QVERIFY(indexedTypes.contains(v.value("_type").toString()));
}

void TestJsonDbClient::notifyCreateUpdateRemove_data()
{
    QTest::addColumn<QString>("partition");

    QTest::newRow("persistent") << "";
    QTest::newRow("ephemeral") << "Ephemeral";
}

void TestJsonDbClient::notifyCreateUpdateRemove()
{
    QFETCH(QString, partition);

    QList<QJsonObject> results;
    QJsonDbWriteRequest request;
    QList<QJsonDbNotification> notifications;

    // Create a notification object
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.notify-test\"]"));
    watcher.setPartition(partition);
    QVERIFY(mConnection->addWatcher(&watcher));
    QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));

    // Create a notify-test object
    QJsonDbObject object;
    object.insert("_type", QLatin1String("com.test.notify-test"));
    object.insert("name", QLatin1String("test1"));
    object.setUuid(QJsonDbObject::createUuid());
    request.setObjects(QList<QJsonObject>() << object);
    request.setPartition(partition);

    QVERIFY(mConnection->send(&request));
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

    results = request.takeResults();
    notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), 1);
    QCOMPARE(notifications.size(), 1);

    QString uuid = results[0].value("_uuid").toString();
    QString version = results[0].value("_version").toString();

    QCOMPARE(notifications[0].action(), QJsonDbWatcher::Created);
    QCOMPARE(notifications[0].object().value("_uuid").toString(), uuid);

    // Update the notify-test object
    object.insert("_uuid", uuid);
    object.insert("_version", version);
    object.insert("name", QLatin1String("test2"));
    request.setObjects(QList<QJsonObject>() << object);
    QVERIFY(mConnection->send(&request));
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

    results = request.takeResults();
    notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), 1);
    QCOMPARE(notifications.size(), 1);

    QCOMPARE(notifications[0].action(), QJsonDbWatcher::Updated);
    QVERIFY(notifications[0].object().value("_uuid").toString() == uuid);
    QVERIFY(notifications[0].object().value("_version").toString() != version);
    QVERIFY(notifications[0].object().value("name").toString() == QLatin1String("test2"));

    // Remove the notify-test object
    version = results[0].value("_version").toString();
    object.insert("_version", version);
    object.insert("_deleted", true);
    request.setObjects(QList<QJsonObject>() << object);
    QVERIFY(mConnection->send(&request));
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

    results = request.takeResults();
    notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), 1);
    QCOMPARE(notifications.size(), 1);

    QCOMPARE(notifications[0].action(), QJsonDbWatcher::Removed);
    QVERIFY(notifications[0].object().value("_uuid").toString() == uuid);
    QVERIFY(notifications[0].object().value("_version").toString() == version);
    QVERIFY(notifications[0].object().value("_deleted").toBool() == true);
    QVERIFY(notifications[0].object().value("name").toString() == QLatin1String("test2"));

    QVERIFY(mConnection->removeWatcher(&watcher));
}

void TestJsonDbClient::notifyRemoveViaUpdate()
{

    QList<QJsonObject> results;
    QJsonDbWriteRequest request;
    QList<QJsonDbNotification> notifications;

    // Create a notification object
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.notify-test\"][?filter=\"match\"]"));
    QVERIFY(mConnection->addWatcher(&watcher));
    QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));

    // Create a notify-test object
    QJsonDbObject object;
    object.insert("_type", QLatin1String("com.test.notify-test"));
    object.insert("filter", QLatin1String("match"));
    object.setUuid(QJsonDbObject::createUuid());
    request.setObjects(QList<QJsonObject>() << object);

    QVERIFY(mConnection->send(&request));
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

    results = request.takeResults();
    notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), 1);
    QCOMPARE(notifications.size(), 1);

    QString uuid = results[0].value("_uuid").toString();
    QString version = results[0].value("_version").toString();

    QCOMPARE(notifications[0].action(), QJsonDbWatcher::Created);
    QCOMPARE(notifications[0].object().value("_uuid").toString(), uuid);

    // Update the notify-test object
    object.insert("_uuid", uuid);
    object.insert("_version", version);
    object.insert("filter", QLatin1String("nomatch"));
    request.setObjects(QList<QJsonObject>() << object);
    QVERIFY(mConnection->send(&request));
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 1));

    results = request.takeResults();
    notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), 1);
    QCOMPARE(notifications.size(), 1);

    QCOMPARE(notifications[0].action(), QJsonDbWatcher::Removed);
    QVERIFY(notifications[0].object().value("_uuid").toString() == uuid);
    QVERIFY(notifications[0].object().value("_version").toString() == version);
    QVERIFY(notifications[0].object().value("filter").toString() == QLatin1String("match"));
    QVERIFY(!notifications[0].object().contains("_deleted"));

    // Remove the notify-test object
    version = results[0].value("_version").toString();
    object.insert("_version", version);
    object.insert("_deleted", true);
    request.setObjects(QList<QJsonObject>() << object);
    QVERIFY(mConnection->send(&request));
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, 0));

    results = request.takeResults();
    notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), 1);
    QCOMPARE(notifications.size(), 0);

    QVERIFY(mConnection->removeWatcher(&watcher));
}

static const char *rbnames[] = { "Fred", "Joe", "Sam", NULL };

void TestJsonDbClient::notifyRemoveBatch()
{
    QJsonDbWriteRequest request;
    QList<QJsonObject> results;

    // Create a notification object
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::Removed);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.notify-test-remove-batch\"]"));
    QVERIFY(mConnection->addWatcher(&watcher));
    QVERIFY(waitForStatus(&watcher, QJsonDbWatcher::Active));

    // Create notify-test-remove-batch object
    QList<QJsonObject> list;
    int count;
    for (count = 0 ; rbnames[count] ; count++) {
        QJsonDbObject object;
        object.insert("_type", QLatin1String("com.test.notify-test-remove-batch"));
        object.insert("name", QLatin1String(rbnames[count]));
        object.setUuid(QJsonDbObject::createUuid());
        list << object;
    }
    request.setObjects(list);
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), count);

    // Retrieve the _uuids of the items
    QJsonDbReadRequest query;
    query.setQuery("[?_type=\"com.test.notify-test-remove-batch\"]");
    QVERIFY(sendWaitTake(&query, &results));

    QCOMPARE(results.size(), count);

    // Make a list of the uuids returned
    QStringList uuidList;
    foreach (const QJsonObject& v, results) {
        uuidList << v.value("_uuid").toString();
    }

    list.clear();
    for (int i = 0; i < results.size(); ++i) {
        const QJsonObject obj = results[i];
        QJsonObject copy;
        QVERIFY(uuidList.contains(obj.value("_uuid").toString()));
        copy.insert("_deleted", true);
        copy.insert("_version", obj.value("_version"));
        copy.insert("_uuid", obj.value("_uuid"));
        list.append(copy);
    }

    // Remove the objects
    request.setObjects(list);
    QVERIFY(mConnection->send(&request));
    QVERIFY(waitForResponseAndNotifications(&request, &watcher, count));

    results = request.takeResults();
    QList<QJsonDbNotification> notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), count);
    QCOMPARE(notifications.size(), count);

    foreach (const QJsonDbNotification &n, notifications) {
        QCOMPARE(n.action(), QJsonDbWatcher::Removed);
        QString uuid = n.object().value("_uuid").toString();
        QVERIFY(uuidList.contains(uuid));
        uuidList.removeOne(uuid);
    }

    // Remove the notification object
    QVERIFY(mConnection->removeWatcher(&watcher));
}

void TestJsonDbClient::schemaValidation()
{
    QJsonDbWriteRequest request;
    QList<QJsonObject> results;

    if (!wasRoot) {
        QJsonObject schemaBody = readJsonFile(findFile("create-test.json")).object();
        //qDebug() << "schemaBody" << schemaBody;
        QJsonObject schemaObject;
        schemaObject.insert("_type", QLatin1String("_schemaType"));
        schemaObject.insert("name", QLatin1String("com.test.SchemaTestObject"));
        schemaObject.insert("schema", schemaBody);
        //qDebug() << "schemaObject" << schemaObject;
        request.setObjects(QList<QJsonObject>() << schemaObject);
        QVERIFY(sendWaitTake(&request, &results));
        QCOMPARE(results.size(), 1);
        QVERIFY(results[0].contains("_uuid"));
    }

    QJsonDbObject item;

    item.insert("_type", QLatin1String("com.test.SchemaTestObject"));
    item.insert("create-test", 22);
    item.insert("another-field", QLatin1String("a string"));
    item.setUuid(QJsonDbObject::createUuid());

    // Create an item that matches the schema
    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request, &results));
    QVERIFY(results.size() == 1);
    QVERIFY(results[0].contains("_uuid"));

    if (!wasRoot) {
        // Create an item that does not match the schema
        QJsonDbObject noncompliant;
        noncompliant.insert("_type", QLatin1String("com.test.SchemaTestObject"));
        noncompliant.insert("create-test", 22);
        noncompliant.setUuid(QJsonDbObject::createUuid());
        request.setObjects(QList<QJsonObject>() << noncompliant);
        QVERIFY(sendWaitTake(&request, &results));

        QCOMPARE(mRequestErrors[&request], QJsonDbRequest::FailedSchemaValidation);
    }
}

void TestJsonDbClient::remove()
{
    QVERIFY(mConnection);

    QJsonDbWriteRequest request;
    QList<QJsonObject> results;

    QJsonDbObject item;
    item.insert("_type", QLatin1String("com.test.remove-test"));

    // Create an item
    item.insert("foo", 42);
    item.setUuid(QJsonDbObject::createUuid());
    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request, &results));

    QCOMPARE(results.size(), 1);
    QVERIFY(results[0].contains("_uuid"));
    QString uuid = results[0].value("_uuid").toString();
    QVERIFY(results[0].contains("_version"));
    QString version = results[0].value("_version").toString();

    // create more items
    item.insert("foo", 5);
    item.setUuid(QJsonDbObject::createUuid());
    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request));
    item.insert("foo", 64);
    item.setUuid(QJsonDbObject::createUuid());
    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request));
    item.insert("foo", 45);
    item.setUuid(QJsonDbObject::createUuid());
    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request));

    // query and make sure there are four items
    QJsonDbReadRequest query;
    query.setQuery(QLatin1String("[?_type=\"com.test.remove-test\"]"));
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 4);

    // Set the _uuid field and attempt to remove first item
    item.insert("_uuid", uuid);
    item.insert("_version", version);
    item.insert("_deleted", true);
    request.setObjects(QList<QJsonObject>() << item);
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), 1);

    // query and make sure there are only three items left
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 3);
}

void TestJsonDbClient::notifyMultiple()
{
    // notifications with multiple client connections
    // Makes sure only the client that signed up for notifications gets them
    QList<QJsonObject> results;
    QList<QJsonDbNotification> notifications;
    const QString query = "[?_type=\"com.test.notify-test\"][?identifier=\"w1-identifier\"]";

    TestHelper w1;
    w1.connectToServer();
    TestHelper w2;
    w2.connectToServer();

    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(query);
    QVERIFY(w1.connection()->addWatcher(&watcher));
    QVERIFY(w1.waitForStatus(&watcher, QJsonDbWatcher::Active));

    // Create a notify-test object
    QJsonDbObject object;
    object.insert("_type", QLatin1String("com.test.notify-test"));
    object.insert("identifier", QLatin1String("w1-identifier"));
    object.setUuid(QJsonDbObject::createUuid());
    QJsonDbCreateRequest toCreate1(object);
    QVERIFY(w2.connection()->send(&toCreate1));
    QVERIFY(w2.waitForResponseAndNotifications(&toCreate1, &watcher, 1));

    results = toCreate1.takeResults();
    notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), 1);
    QCOMPARE(notifications.size(), 1);
    QCOMPARE(notifications[0].object().value("_uuid"), object.value("_uuid"));
    QCOMPARE((int)notifications[0].action(), (int)QJsonDbWatcher::Created);

    object.setUuid(QJsonDbObject::createUuid());
    QJsonDbCreateRequest toCreate2(object);
    QVERIFY(w1.connection()->send(&toCreate2));
    QVERIFY(w1.waitForResponseAndNotifications(&toCreate2, &watcher, 1));

    results = toCreate2.takeResults();
    notifications = watcher.takeNotifications();

    QCOMPARE(results.size(), 1);
    QCOMPARE(notifications.size(), 1);
    QCOMPARE(notifications[0].object().value("_uuid"), object.value("_uuid"));
    QCOMPARE((int)notifications[0].action(), (int)QJsonDbWatcher::Created);

    object.insert("identifier", QLatin1String("w2-identifier"));
    object.setUuid(QJsonDbObject::createUuid());
    QJsonDbCreateRequest toCreate3(object);
    QVERIFY(w1.connection()->send(&toCreate3));
    QVERIFY(w1.waitForResponseAndNotifications(&toCreate3, &watcher, 0));

    object.insert("identifier", QLatin1String("w2-identifier"));
    object.setUuid(QJsonDbObject::createUuid());
    QJsonDbCreateRequest toCreate4(object);
    QVERIFY(w2.connection()->send(&toCreate4));
    QVERIFY(w2.waitForResponseAndNotifications(&toCreate4, &watcher, 0));

    w1.connection()->removeWatcher(&watcher);
}

void TestJsonDbClient::mapNotification()
{
    QJsonArray jsonArray = readJsonFile(":/json/auto/partition/json/map-reduce.json").array();
    QList<QJsonObject> list, results;

    foreach (const QJsonValue v, jsonArray) {
        QJsonDbObject o = v.toObject();
        o.setUuid(QJsonDbObject::createUuid());
        list.append(o);
    }

    QJsonDbCreateRequest toCreate(list);
    QVERIFY(sendWaitTake(&toCreate, &results));
    QCOMPARE(results.size(), list.size());
    QList<QJsonObject> views;
    QList<QJsonObject> schemas;
    QMap<QString, QJsonObject> toDelete;

    for (int i = 0; i < list.size(); ++i) {
        QString t = list[i].value("_type").toString();
        QString v = results[i].value("_version").toString();
        QString uuid = results[i].value("_uuid").toString();
        list[i].insert("_version", v);
        list[i].insert("_uuid", uuid);
        if (t == QLatin1String("Map") || t == QLatin1String("Reduce"))
            views.append(list[i]);
        else if (t == QLatin1String("_schemaType"))
            schemas.append(list[i]);
        else
            toDelete.insert(uuid, list[i]);
    }

    // Create a notification object
    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"Phone\"]"));
    QVERIFY(mConnection->addWatcher(&watcher));
    QVERIFY(waitForStatusAndNotifications(&watcher, QJsonDbWatcher::Active, 5));
    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 5);

    QJsonDbReadRequest query;
    query.setQuery(QLatin1String("[?_type=\"Contact\"][?displayName=\"Nancy Doe\"]"));

    QVERIFY(sendWaitTake(&query, &results));

    QCOMPARE(results.size(), 1);
    QJsonObject firstItem = results[0];
    QVERIFY(!firstItem.value("_uuid").toString().isEmpty());
    toDelete.remove(firstItem.value("_uuid").toString());

    QJsonDbRemoveRequest toRemove(firstItem);
    QVERIFY(mConnection->send(&toRemove));
    QVERIFY(waitForResponseAndNotifications(&toRemove, &watcher, 2));

    notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 2);

    mConnection->removeWatcher(&watcher);

    QJsonDbRemoveRequest viewRemove(views);
    QVERIFY(sendWaitTake(&viewRemove, &results));
    QCOMPARE(results.size(), 2);

    QJsonDbRemoveRequest schemaRemove(schemas);
    QVERIFY(sendWaitTake(&schemaRemove, &results));
    QCOMPARE(results.size(), 2);

    foreach (const QJsonObject &obj, toDelete.values()) {
        QJsonDbRemoveRequest objRemove(obj);
        QVERIFY(sendWaitTake(&objRemove, &results));
        QCOMPARE(results.size(), 1);
    }
}

void TestJsonDbClient::changesSince()
{
    QVERIFY(mConnection);

    QList<QJsonObject> results;
    QJsonDbReadRequest query;
    query.setQuery(QLatin1String("[?_type=\"com.test.TestContact\"]"));
    query.setQueryLimit(1);
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 0);

    int state = query.stateNumber();

    QList<QJsonObject> list;
    QString type = QLatin1String("com.test.TestContact");
    QJsonObject c1, c2, c3;
    c1["_type"] = type;
    c2["_type"] = type;
    c3["_type"] = type;
    c1["firstName"] = QLatin1String("John");
    c1["lastName"] = QLatin1String("Doe");
    c1["_uuid"] = QJsonDbObject::createUuid().toString();
    c2["firstName"] = QLatin1String("George");
    c2["lastName"] = QLatin1String("Washington");
    c2["_uuid"] = QJsonDbObject::createUuid().toString();
    c3["firstName"] = QLatin1String("Betsy");
    c3["lastName"] = QLatin1String("Ross");
    c3["_uuid"] = QJsonDbObject::createUuid().toString();
    list << c1 << c2 << c3;

    QJsonDbCreateRequest toCreate(list);
    QVERIFY(sendWaitTake(&toCreate, &results));

    c3.insert("_version", results[2].value("_version"));
    c3.insert("_deleted", true);

    QJsonDbRemoveRequest toRemove(c3);
    QVERIFY(sendWaitTake(&toRemove, &results));

    QJsonDbWatcher watcher;
    watcher.setWatchedActions(QJsonDbWatcher::All);
    watcher.setQuery(QLatin1String("[?_type=\"com.test.TestContact\"]"));
    watcher.setInitialStateNumber(state);
    QVERIFY(mConnection->addWatcher(&watcher));
    QVERIFY(waitForStatusAndNotifications(&watcher, QJsonDbWatcher::Active, 2));

    QList<QJsonDbNotification> notifications = watcher.takeNotifications();
    QCOMPARE(notifications.size(), 2);

    QCOMPARE((int)notifications[0].action(), (int)QJsonDbWatcher::Created);
    QCOMPARE((int)notifications[1].action(), (int)QJsonDbWatcher::Created);
    QVERIFY(notifications[0].object().value("_uuid").toString() == c1.value("_uuid").toString() ||
            notifications[0].object().value("_uuid").toString() == c2.value("_uuid").toString());
    QVERIFY(notifications[1].object().value("_uuid").toString() == c1.value("_uuid").toString() ||
            notifications[1].object().value("_uuid").toString() == c2.value("_uuid").toString());
}

void TestJsonDbClient::partition()
{
    const QString firstPartitionName = "com.example.autotest.Partition1";
    const QString secondPartitionName = "com.example.autotest.Partition2";

    QJsonDbWriteRequest request;
    QList<QJsonObject> results;

    QJsonDbObject item;
    item.insert("_type", QLatin1String("Foobar"));
    item.insert("one", QLatin1String("one"));
    item.setUuid(QJsonDbObject::createUuid());

    request.setObjects(QList<QJsonObject>() << item);
    request.setPartition(firstPartitionName);
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), 1);

    item.insert("_type", QLatin1String("Foobar"));
    item.insert("one", QLatin1String("two"));
    item.setUuid(QJsonDbObject::createUuid());

    request.setObjects(QList<QJsonObject>() << item);
    request.setPartition(secondPartitionName);
    QVERIFY(sendWaitTake(&request, &results));
    QCOMPARE(results.size(), 1);

    // now query
    QJsonDbReadRequest query;
    query.setQuery(QLatin1String("[?_type=\"Foobar\"]"));
    query.setPartition(firstPartitionName);
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].value("one"), QJsonValue(QLatin1String("one")));

    query.setPartition(secondPartitionName);
    QVERIFY(sendWaitTake(&query, &results));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].value("one"), QJsonValue(QLatin1String("two")));
}

void TestJsonDbClient::jsondbobject()
{
    QJsonDbObject o;
    QVERIFY(o.isEmpty());
    QUuid uuid = o.uuid();
    QVERIFY(uuid.isNull());
    QVERIFY(!o.contains(QLatin1String("_uuid")));

    uuid = QUuid::createUuid();
    o.setUuid(uuid);
    QCOMPARE(o.uuid(), uuid);
    QVariant v = o.value(QLatin1String("_uuid")).toVariant();
    QVERIFY(!v.isNull());
    QVERIFY(v.canConvert<QUuid>());
    QCOMPARE(v.value<QUuid>(), uuid);

    o = QJsonDbObject();
    QVERIFY(o.uuid().isNull());
    o.insert(QLatin1String("_uuid"), uuid.toString());
    QCOMPARE(o.uuid(), uuid);
    v = o.value(QLatin1String("_uuid")).toVariant();
    QVERIFY(!v.isNull());
    QVERIFY(v.canConvert<QUuid>());
    QCOMPARE(v.value<QUuid>(), uuid);
}

#ifdef Q_OS_LINUX
void TestJsonDbClient::sigstop()
{
    QStringList argList = QStringList() << "-sigstop";
    argList << QString::fromLatin1("sigstop.db");

    qint64 pid = launchJsonDbDaemonDetached(argList, __FILE__);
    int status;
    ::waitpid(pid, &status, WUNTRACED);
    QVERIFY(WIFSTOPPED(status));
    ::kill(pid, SIGCONT);
    ::waitpid(pid, &status, WCONTINUED);
    QVERIFY(WIFCONTINUED(status));
    ::kill(pid, SIGTERM);
    ::waitpid(pid, &status, 0);
}
#endif

QTEST_MAIN(TestJsonDbClient)

#include "test-jsondb-client.moc"
