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
#include <QtTest/QtTest>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTime>

#include "json.h"
#include "jsondb.h"
#include "jsondbsettings.h"

#include "../../shared/util.h"

QT_USE_NAMESPACE_JSONDB

/*
  Ensure that a good result object contains the correct fields
 */

#define verifyGoodResult(result) \
{ \
    QJsonObject __result = result; \
    QVERIFY(__result.contains(JsonDbString::kErrorStr)); \
    QVERIFY2(__result.value(JsonDbString::kErrorStr).type() == QJsonValue::Null,  \
             __result.value(JsonDbString::kErrorStr).toObject().value("message").toString().toLocal8Bit()); \
    QVERIFY(__result.contains(JsonDbString::kResultStr)); \
}

// QJsonObject result, QString field, QVariant expectedValue
#define verifyResultField(result, field, expectedValue) \
{ \
    QJsonObject map = result.value(JsonDbString::kResultStr).toObject(); \
    QVERIFY(map.contains(field)); \
    QCOMPARE(map.value(field).toVariant(), QVariant(expectedValue));    \
}

#define verifyGoodQueryResult(result) \
{ \
    JsonDbQueryResult __result = result; \
    QVERIFY2(__result.error.type() == QJsonValue::Null,  \
         __result.error.toObject().value("message").toString().toLocal8Bit()); \
}
/*
  Ensure that a error result contains the correct fields
 */

#define verifyErrorResult(result) \
{\
    QVERIFY(result.contains(JsonDbString::kErrorStr)); \
    if (result.value(JsonDbString::kErrorStr).type() != QJsonValue::Object )   \
        qDebug() << "verifyErrorResult" << result; \
    QCOMPARE(result.value(JsonDbString::kErrorStr).type(), QJsonValue::Object ); \
    QJsonObject errormap = result.value(JsonDbString::kErrorStr).toObject(); \
    QVERIFY(errormap.contains(JsonDbString::kCodeStr)); \
    QCOMPARE(errormap.value(JsonDbString::kCodeStr).type(), QJsonValue::Double ); \
    QVERIFY(errormap.contains(JsonDbString::kMessageStr)); \
    QCOMPARE(errormap.value(JsonDbString::kMessageStr).type(), QJsonValue::String ); \
    QVERIFY(result.contains(JsonDbString::kResultStr)); \
    QVERIFY(result.value(JsonDbString::kResultStr).toObject().isEmpty() \
            || (result.value(JsonDbString::kResultStr).toObject().contains("count") \
                && result.value(JsonDbString::kResultStr).toObject().value("count").toDouble() == 0)); \
}

class TestJsonDb: public QObject
{
    Q_OBJECT
public:
    TestJsonDb();

public slots:
    void notified(const QString, const JsonDbObject &, const QString);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void capabilities();
    void allowAll();

    void testAccessControl();
    void testFindAccessControl();
    void settings();

private:
    void addIndex(const QString &propertyName, const QString &propertyType=QString(), const QString &objectType=QString());

    QJsonValue readJsonFile(const QString &filename);
    QJsonValue readJson(const QByteArray& json);
    void removeDbFiles();

private:
    JsonDb *mJsonDb;
    QStringList mNotificationsReceived;
    QList<JsonDbObject> mContactList;
    JsonDbOwner *mOwner;
};

const char *kFilename = "testdatabase";
//const char *kFilename = ":memory:";
const QString kReplica1Name = QString("replica1");
const QString kReplica2Name = QString("replica2");
const QStringList kReplicaNames = (QStringList() << kReplica1Name << kReplica2Name);

TestJsonDb::TestJsonDb()
    : mJsonDb(NULL), mOwner(0)
{
}

void TestJsonDb::removeDbFiles()
{
    QStringList filters;
    filters << QLatin1String("*.db")
            << "objectFile.bin" << "objectFile2.bin";
    QStringList lst = QDir().entryList(filters);
    foreach (const QString &fileName, lst)
        QFile::remove(fileName);
}

void TestJsonDb::initTestCase()
{
    qsrand(QDateTime::currentDateTime().toTime_t());
    QCoreApplication::setOrganizationName("Example");
    QCoreApplication::setOrganizationDomain("example.com");
    QCoreApplication::setApplicationName("testjsondb");
    QCoreApplication::setApplicationVersion("1.0");

    removeDbFiles();
    mJsonDb = new JsonDb(QString (), kFilename, QStringLiteral("com.example.JsonDbTest"), this);
    mOwner = new JsonDbOwner(this);
    mOwner->setOwnerId(QStringLiteral("com.example.JsonDbTest"));

    mJsonDb->open();
}

void TestJsonDb::cleanupTestCase()
{
    if (mJsonDb) {
        mJsonDb->close();
        delete mJsonDb;
        mJsonDb = 0;
    }
    removeDbFiles();
}

void TestJsonDb::cleanup()
{
    foreach (JsonDbPartition *partition, mJsonDb->mPartitions)
        QCOMPARE(partition->mTransactionDepth, 0);
}

/*
 * Verify translation of capabilities to access control policies.
 */
void TestJsonDb::capabilities()
{
    QJsonArray viewDefinitions(readJsonFile(":/security/json/capabilities-test.json").toArray());
    for (int i = 0; i < viewDefinitions.size(); ++i) {
        JsonDbObject object(viewDefinitions.at(i).toObject());
        if (object.value("_type").toString() == "CapabilitiesTest") {
            QScopedPointer<JsonDbOwner> owner(new JsonDbOwner());
            owner->setOwnerId(object.value("identifier").toString());
            QJsonObject capabilities(object.value("capabilities").toObject());
            owner->setCapabilities(capabilities, mJsonDb);
        } else {
            QJsonObject result = mJsonDb->create(mOwner, object);
            verifyGoodResult(result);
        }
    }
    mJsonDb->removeIndex("CapabilitiesTest");
}

/*
 * Verify the allowAll flag on owner
 */
void TestJsonDb::allowAll()
{
    // can delete me when this goes away
    jsondbSettings->setEnforceAccessControl(true);

    JsonDbOwner *owner = new JsonDbOwner();
    owner->setAllowedObjects(QLatin1String("all"), QLatin1String("read"), QStringList());
    owner->setAllowedObjects(QLatin1String("all"), QLatin1String("write"), QStringList());
    owner->setStorageQuota(-1);

    JsonDbObject toPut;
    toPut.insert("_type", QLatin1String("TestObject"));

    QJsonObject result = mJsonDb->create(owner, toPut);
    verifyErrorResult(result);

    JsonDbObject toPut2;
    toPut2.insert("_type", QLatin1String("TestObject"));

    owner->setAllowAll(true);
    result =  mJsonDb->create(owner, toPut2);
    verifyGoodResult(result);

    mJsonDb->removeIndex("TestObject");

    jsondbSettings->setEnforceAccessControl(false);
}

/*
 * Create an item and verify access control
 */

void TestJsonDb::testAccessControl()
{
    jsondbSettings->setEnforceAccessControl(true);
    QJsonObject contactsCapabilities;
    QJsonArray value;
    value.append (QLatin1String("rw"));
    contactsCapabilities.insert (QLatin1String("contacts"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(contactsCapabilities, mJsonDb);

    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("create-test-type"));
    item.insert("access-control-test", 22);

    QJsonObject result = mJsonDb->create(mOwner, item);

    verifyErrorResult(result);

    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("Contact"));
    item.insert("access-control-test", 23);

    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item.insert("access-control-test", 24);
    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);

    // Test some %owner and %typeDomain horror
    // ---------------------------------------
    mOwner->setAllowAll(true);

    // Create an object for testing (failing) update & delete
    mOwner->setOwnerId(QStringLiteral("test"));

    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.bar.FooType"));
    item.insert("access-control-test", 25);

    result = mJsonDb->create(mOwner, item);

    QJsonValue uuid = item.value(JsonDbString::kUuidStr);

    mOwner->setOwnerId(QStringLiteral("com.example.foo.App"));
    QJsonObject ownDomainCapabilities;
    while (!value.isEmpty())
        value.removeLast();
    value.append (QLatin1String("rw"));
    ownDomainCapabilities.insert (QStringLiteral("own_domain"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(ownDomainCapabilities, mJsonDb);

    // Test that we can not create
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.bar.FooType"));
    item.insert("access-control-test", 26);

    result = mJsonDb->create(mOwner, item);

    verifyErrorResult(result);

    // Test that we can not update
    item.insert(JsonDbString::kUuidStr, uuid);
    item.insert("access-control-test", 27);

    result = mJsonDb->update(mOwner, item);
    verifyErrorResult(result);

    // .. or remove
    item.insert(JsonDbString::kUuidStr, uuid);
    result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);

    // Positive tests
    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.FooType"));
    item.insert("access-control-test", 28);

    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);

    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.FooType"));
    item.insert("access-control-test", 29);

    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item.insert("access-control-test", 30);
    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);
    jsondbSettings->setEnforceAccessControl(false);
}

void TestJsonDb::testFindAccessControl()
{
    jsondbSettings->setEnforceAccessControl(true);
    mOwner->setAllowAll(true);
    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("find-access-control-test-type"));
    item.insert("find-access-control-test", 50);
    JsonDbObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("Contact"));
    item.insert("find-access-control-test", 51);
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QJsonObject contactsCapabilities;
    QJsonArray value;
    value.append (QLatin1String("rw"));
    contactsCapabilities.insert (QStringLiteral("contacts"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(contactsCapabilities, mJsonDb);

    QJsonObject request;
    request.insert(JsonDbString::kQueryStr, QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("find-access-control-test-type"));

    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);

    QVERIFY(queryResult.length.toDouble() < 1);

    request.insert(JsonDbString::kQueryStr, QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("Contact"));

    queryResult= mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);

    QVERIFY(queryResult.length.toDouble() > 0);

    mOwner->setAllowAll(true);

    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.bar.FooType"));
    item.insert("find-access-control-test", 55);
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.FooType"));
    item.insert("find-access-control-test", 56);
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    mOwner->setOwnerId(QStringLiteral("com.example.foo.App"));
    QJsonObject ownDomainCapabilities;
    while (!value.isEmpty())
        value.removeLast();
    value.append (QLatin1String("rw"));
    ownDomainCapabilities.insert (QLatin1String("own_domain"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(ownDomainCapabilities, mJsonDb);

    request.insert(JsonDbString::kQueryStr, QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("com.example.foo.bar.FooType"));

    queryResult = mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);

    QVERIFY(queryResult.length.toDouble() < 1);

    request.insert(JsonDbString::kQueryStr, QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("com.example.foo.FooType"));

    queryResult= mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);

    QVERIFY(queryResult.length.toDouble() > 0);
    jsondbSettings->setEnforceAccessControl(false);
}

QStringList strings = (QStringList()
                       << "abc"
                       << "def"
                       << "deaf"
                       << "leaf"
                       << "DEAF"
                       << "LEAF"
                       << "ghi"
                       << "foo/bar");

QStringList patterns = (QStringList()
        );

QJsonValue TestJsonDb::readJsonFile(const QString& filename)
{
    QString filepath = filename;
    QFile jsonFile(filepath);
    jsonFile.open(QIODevice::ReadOnly);
    QByteArray json = jsonFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok) {
      qDebug() << filepath << parser.errorString();
    }
    QVariant v = parser.result();
    return QJsonValue::fromVariant(v);
}

QJsonValue TestJsonDb::readJson(const QByteArray& json)
{
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok) {
      qDebug() << parser.errorString();
    }
    QVariant v = parser.result();
    return QJsonObject::fromVariantMap(v.toMap());
}

void TestJsonDb::settings()
{
    // first explicitly set the values
    jsondbSettings->setRejectStaleUpdates(true);
    jsondbSettings->setDebug(true);
    jsondbSettings->setVerbose(true);
    jsondbSettings->setPerformanceLog(true);
    jsondbSettings->setCacheSize(64);
    jsondbSettings->setCompactRate(2000);
    jsondbSettings->setEnforceAccessControl(true);
    jsondbSettings->setTransactionSize(50);
    jsondbSettings->setValidateSchemas(true);
    jsondbSettings->setSyncInterval(10000);
    jsondbSettings->setIndexSyncInterval(20000);
    jsondbSettings->setDebugQuery(true);

    QVERIFY(jsondbSettings->rejectStaleUpdates());
    QVERIFY(jsondbSettings->debug());
    QVERIFY(jsondbSettings->verbose());
    QVERIFY(jsondbSettings->performanceLog());
    QCOMPARE(jsondbSettings->cacheSize(), 64);
    QCOMPARE(jsondbSettings->compactRate(), 2000);
    QVERIFY(jsondbSettings->enforceAccessControl());
    QCOMPARE(jsondbSettings->transactionSize(), 50);
    QVERIFY(jsondbSettings->validateSchemas());
    QCOMPARE(jsondbSettings->syncInterval(), 10000);
    QCOMPARE(jsondbSettings->indexSyncInterval(), 20000);
    QVERIFY(jsondbSettings->debugQuery());

    jsondbSettings->setRejectStaleUpdates(false);
    jsondbSettings->setDebug(false);
    jsondbSettings->setVerbose(false);
    jsondbSettings->setPerformanceLog(false);
    jsondbSettings->setEnforceAccessControl(false);
    jsondbSettings->setValidateSchemas(false);
    jsondbSettings->setDebugQuery(false);

    // then with environment variables
    ::setenv("JSONDB_REJECT_STALE_UPDATES", "true", true);
    ::setenv("JSONDB_DEBUG", "true", true);
    ::setenv("JSONDB_VERBOSE", "true", true);
    ::setenv("JSONDB_PERFORMANCE_LOG", "true", true);
    ::setenv("JSONDB_CACHE_SIZE", "256", true);
    ::setenv("JSONDB_COMPACT_RATE", "1500", true);
    ::setenv("JSONDB_ENFORCE_ACCESS_CONTROL", "true", true);
    ::setenv("JSONDB_TRANSACTION_SIZE", "75", true);
    ::setenv("JSONDB_VALIDATE_SCHEMAS", "true", true);
    ::setenv("JSONDB_SYNC_INTERVAL", "6000", true);
    ::setenv("JSONDB_INDEX_SYNC_INTERVAL", "17000", true);
    ::setenv("JSONDB_DEBUG_QUERY", "true", true);
    jsondbSettings->reload();

    QVERIFY(jsondbSettings->rejectStaleUpdates());
    QVERIFY(jsondbSettings->debug());
    QVERIFY(jsondbSettings->verbose());
    QVERIFY(jsondbSettings->performanceLog());
    QCOMPARE(jsondbSettings->cacheSize(), 256);
    QCOMPARE(jsondbSettings->compactRate(), 1500);
    QVERIFY(jsondbSettings->enforceAccessControl());
    QCOMPARE(jsondbSettings->transactionSize(), 75);
    QVERIFY(jsondbSettings->validateSchemas());
    QCOMPARE(jsondbSettings->syncInterval(), 6000);
    QCOMPARE(jsondbSettings->indexSyncInterval(), 17000);
    QVERIFY(jsondbSettings->debugQuery());
}

void TestJsonDb::notified(const QString nid, const JsonDbObject &o, const QString action)
{
    Q_UNUSED(o);
    Q_UNUSED(action);
    mNotificationsReceived.append(nid);
}

QTEST_MAIN(TestJsonDb)
#include "testjsondb.moc"
