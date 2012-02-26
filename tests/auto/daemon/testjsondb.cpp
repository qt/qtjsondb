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
#include <QUuid>

#include "json.h"

#include "jsondb.h"

#include "jsondb.h"
#include "jsondbmanagedbtree.h"
#include "jsondbobjecttable.h"
#include "jsondbpartition.h"
#include "jsondbindex.h"
#include "jsondbsettings.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"

#include <qjsonobject.h>

#include "../../shared/util.h"

#ifndef QT_NO_DEBUG_OUTPUT
#define DBG() if (gDebug) qDebug()
#else
#define DBG() if (0) qDebug()
#endif

QT_USE_NAMESPACE_JSONDB

static QString kContactStr = "com.example.unittest.contact";

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

template<class T>
class ScopedAssignment
{
public:
    ScopedAssignment(T &p, T b) : mP(&p) {
        mValue = *mP;
        *mP = b;
    };
    ~ScopedAssignment() {
        *mP = mValue;
    }

private:
    T *mP;
    T mValue;
};

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

    void reopen();

    void computeVersion();
    void updateVersionOptimistic();
    void updateVersionReplicating();

    void create();

    void update();
    void update2();
    void update3();
    void update4();

    void remove();
    void remove2();
    void remove3();
    void remove4();
    void remove5();

    void schemaValidation_data();
    void schemaValidation();
    void schemaValidationExtends_data();
    void schemaValidationExtends();
    void schemaValidationExtendsArray_data();
    void schemaValidationExtendsArray();
    void schemaValidationLazyInit();

    void createList();
    void updateList();

    void mapDefinition();
    void mapDefinitionInvalid();
    void reduceDefinition();
    void reduceDefinitionInvalid();
    void mapInvalidMapFunc();
    void reduceInvalidAddSubtractFuncs();

    void map();
    void mapDuplicateSourceAndTarget();
    void mapRemoval();
    void mapUpdate();
    void mapJoin();
    void mapSelfJoinSourceUuids();
    void mapMapFunctionError();
    void mapSchemaViolation();
    void mapArrayConversion();
    void reduce();
    void reduceRemoval();
    void reduceUpdate();
    void reduceDuplicate();
    void reduceFunctionError();
    void reduceSchemaViolation();
    void reduceSubObjectProp();
    void reduceArray();
    void changesSinceCreate();

    void capabilities();
    void allowAll();

    void testAccessControl();
    void testFindAccessControl();

    void addIndex();
    void addSchema();
    void duplicateSchema();
    void removeSchema();
    void removeViewSchema();
    void updateSchema();
    void orQuery_data();
    void orQuery();
    void unindexedFind();
    void find1();
    void find2();
    void findFields();
    void testNotify1();

    void findLikeRegexp_data();
    void findLikeRegexp();
    void findInContains();

    void wildcardIndex();
    void uuidJoin();

    void orderedFind1_data();
    void orderedFind1();
    void orderedFind2_data();
    void orderedFind2();

    void findByName();
    void findEQ();
    void find10();

    void testPrimaryKey();
    void testStoredProcedure();

    void startsWith();
    void comparison();

    void removedObjects();
    void partition();
    void arrayIndexQuery();
    void deindexError();
    void expectedOrder();
    void indexQueryOnCommonValues();

    void removeIndexes();
    void setOwner();
    void indexPropertyFunction();
    void managedBtree();

    void settings();

public:
    void createContacts();

private:
    void addSchema(const QString &schemaName, JsonDbObject &schemaObject);
    void addIndex(const QString &propertyName, const QString &propertyType=QString(), const QString &objectType=QString());

    QJsonValue readJsonFile(const QString &filename);
    QJsonValue readJson(const QByteArray& json);
    void removeDbFiles();

private:
    JsonDb *mJsonDb;
    JsonDbPartition *mJsonDbPartition;
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
    : mJsonDb(NULL), mJsonDbPartition(0), mOwner(0)
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

void TestJsonDb::reopen()
{
    int counter = 1;
    for (int i = 0; i < 10; ++i, ++counter) {
        JsonDbObject item;
        item.insert(QLatin1String("_type"), QLatin1String("reopentest"));
        item.insert("create-string", QString("string"));
        QJsonObject result = mJsonDb->create(mOwner, item);

        mJsonDb->close();
        delete mJsonDb;

        mJsonDb = new JsonDb(QString (), kFilename, QStringLiteral("com.example.JsonDbTest"), this);
        mJsonDb->open();

        QJsonObject request;
        request.insert("query", QLatin1String("[?_type=\"reopentest\"]"));
        JsonDbQueryResult queryResult = mJsonDb->find(mOwner, request);
        QCOMPARE(queryResult.data.size(), counter);
    }
    mJsonDb->removeIndex("reopentest");
}

void TestJsonDb::createContacts()
{
    if (!mContactList.isEmpty())
        return;

    QFile contactsFile(":/daemon/json/largeContactsTest.json");
    QVERIFY2(contactsFile.exists(), "Err: largeContactsTest.json doesn't exist!");

    contactsFile.open(QIODevice::ReadOnly);
    QByteArray json = contactsFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok)
        qDebug() << parser.errorString();
    QVariantList contactList = parser.result().toList();
    QList<JsonDbObject> newContactList;
    foreach (QVariant v, contactList) {
        JsonDbObject contact(JsonDbObject::fromVariantMap(v.toMap()));
        QString name = contact.value("name").toString();
        QStringList names = name.split(" ");
        QJsonObject nameObject;
        nameObject.insert("first", names[0]);
        nameObject.insert("last", names[names.size()-1]);
        contact.insert("name", nameObject);
        contact.insert(JsonDbString::kTypeStr, QString("contact"));
        verifyGoodResult(mJsonDb->create(mOwner, contact));
        newContactList.append(contact);
    }
    mContactList = newContactList;

    addIndex(QLatin1String("name"));
    addIndex(QLatin1String("name.first"));
    addIndex(QLatin1String("name.last"));
    addIndex(QLatin1String("_type"));

}

void TestJsonDb::addSchema(const QString &schemaName, JsonDbObject &schemaObject)
{
    QJsonValue schema = readJsonFile(QString(":/daemon/schemas/%1.json").arg(schemaName));
    schemaObject = JsonDbObject();
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", schemaName);
    schemaObject.insert("schema", schema);

    QJsonObject result = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(result);
}

void TestJsonDb::addIndex(const QString &propertyName, const QString &propertyType, const QString &objectType)
{
    JsonDbObject index;
    index.insert(JsonDbString::kTypeStr, kIndexTypeStr);
    index.insert(kPropertyNameStr, propertyName);
    if (!propertyType.isEmpty())
        index.insert(kPropertyTypeStr, propertyType);
    if (!objectType.isEmpty())
        index.insert(kObjectTypeStr, objectType);
    QVERIFY(mJsonDb->addIndex(index, mJsonDb->mSystemPartitionName));
}

void TestJsonDb::computeVersion()
{
    JsonDbObject item0;
    QCOMPARE(item0.version(), QString());
    item0.computeVersion();
    QVERIFY(!item0.version().isEmpty());

    // check that _uuid and _conflicts are ignored
    JsonDbObject item1;
    QVERIFY(item1.version().isEmpty());
    item1.insert(JsonDbString::kUuidStr, QString("123"));
    item1.insert(JsonDbString::kConflictsStr, QString("456"));
    item1.computeVersion();
    QVERIFY(item1.version().startsWith("1-"));
    QCOMPARE(item1.version().size(), 12);
    QVERIFY(!item0.isAncestorOf(item1));
    QVERIFY(!item1.isAncestorOf(item0));

    JsonDbObject replay(item1);
    replay.computeVersion();
    QCOMPARE(replay.isAncestorOf(item1), false);
    QCOMPARE(item1.isAncestorOf(replay), false);
    QCOMPARE(replay.version(), item1.version());

    JsonDbObject item2(item1);
    item2.insert(QStringLiteral("key"), QJsonValue::Null);
    item2.computeVersion();
    QVERIFY(item2.version().startsWith("2-"));
    QCOMPARE(item2.version().size(), 12);
    QVERIFY(item2.version().mid(2) != item1.version().mid(2));
    QVERIFY(item1.isAncestorOf(item2));
    QVERIFY(!item2.isAncestorOf(item1));

    JsonDbObject item3(item2);
    item3.insert(QStringLiteral("key"), false);
    item3.computeVersion();
    QVERIFY(item3.version().startsWith("3-"));
    QCOMPARE(item3.version().size(), 12);
    QVERIFY(item3.version().mid(2) != item2.version().mid(2));
    QVERIFY(item1.isAncestorOf(item3));
    QVERIFY(item2.isAncestorOf(item3));
    QVERIFY(!item3.isAncestorOf(item1));
    QVERIFY(!item3.isAncestorOf(item2));

    JsonDbObject item4(item3);
    item4.insert(QStringLiteral("key"), true);
    item4.computeVersion();
    QVERIFY(item4.version().startsWith("4-"));
    QCOMPARE(item4.version().size(), 12);
    QVERIFY(item4.version().mid(2) != item3.version().mid(2));
    QVERIFY(item1.isAncestorOf(item4));
    QVERIFY(item2.isAncestorOf(item4));
    QVERIFY(item3.isAncestorOf(item4));
    QVERIFY(!item4.isAncestorOf(item1));
    QVERIFY(!item4.isAncestorOf(item2));
    QVERIFY(!item4.isAncestorOf(item3));

    JsonDbObject item5(item4);
    item5.insert(QStringLiteral("key"), double(0));
    item5.computeVersion();
    QVERIFY(item5.version().startsWith("5-"));
    QCOMPARE(item5.version().size(), 12);
    QVERIFY(item5.version().mid(2) != item4.version().mid(2));
    QVERIFY(item1.isAncestorOf(item5));
    QVERIFY(item2.isAncestorOf(item5));
    QVERIFY(item3.isAncestorOf(item5));
    QVERIFY(item4.isAncestorOf(item5));
    QVERIFY(!item5.isAncestorOf(item1));
    QVERIFY(!item5.isAncestorOf(item2));
    QVERIFY(!item5.isAncestorOf(item3));
    QVERIFY(!item5.isAncestorOf(item4));

    JsonDbObject item6(item5);
    item6.insert(QStringLiteral("key"), QString());
    item6.computeVersion();
    QVERIFY(item6.version().startsWith("6-"));
    QCOMPARE(item6.version().size(), 12);
    QVERIFY(item6.version().mid(2) != item5.version().mid(2));
    QVERIFY(item1.isAncestorOf(item6));
    QVERIFY(item2.isAncestorOf(item6));
    QVERIFY(item3.isAncestorOf(item6));
    QVERIFY(item4.isAncestorOf(item6));
    QVERIFY(item5.isAncestorOf(item6));
    QVERIFY(!item6.isAncestorOf(item1));
    QVERIFY(!item6.isAncestorOf(item2));
    QVERIFY(!item6.isAncestorOf(item3));
    QVERIFY(!item6.isAncestorOf(item4));
    QVERIFY(!item6.isAncestorOf(item5));

    JsonDbObject item7(item6);
    item7.insert(QStringLiteral("key"), QJsonArray());
    item7.computeVersion();
    QVERIFY(item7.version().startsWith("7-"));
    QCOMPARE(item7.version().size(), 12);
    QVERIFY(item7.version().mid(2) != item6.version().mid(2));
    QVERIFY(item1.isAncestorOf(item7));
    QVERIFY(item2.isAncestorOf(item7));
    QVERIFY(item3.isAncestorOf(item7));
    QVERIFY(item4.isAncestorOf(item7));
    QVERIFY(item5.isAncestorOf(item7));
    QVERIFY(item6.isAncestorOf(item7));
    QVERIFY(!item7.isAncestorOf(item1));
    QVERIFY(!item7.isAncestorOf(item2));
    QVERIFY(!item7.isAncestorOf(item3));
    QVERIFY(!item7.isAncestorOf(item4));
    QVERIFY(!item7.isAncestorOf(item5));
    QVERIFY(!item7.isAncestorOf(item6));

    JsonDbObject item8(item7);
    item8.insert(QStringLiteral("key"), QJsonObject());
    item8.computeVersion();
    QVERIFY(item8.version().startsWith("8-"));
    QCOMPARE(item8.version().size(), 12);
    QVERIFY(item8.version().mid(2) != item7.version().mid(2));
    QVERIFY(item1.isAncestorOf(item8));
    QVERIFY(item2.isAncestorOf(item8));
    QVERIFY(item3.isAncestorOf(item8));
    QVERIFY(item4.isAncestorOf(item8));
    QVERIFY(item5.isAncestorOf(item8));
    QVERIFY(item6.isAncestorOf(item8));
    QVERIFY(item7.isAncestorOf(item8));
    QVERIFY(!item8.isAncestorOf(item1));
    QVERIFY(!item8.isAncestorOf(item2));
    QVERIFY(!item8.isAncestorOf(item3));
    QVERIFY(!item8.isAncestorOf(item4));
    QVERIFY(!item8.isAncestorOf(item5));
    QVERIFY(!item8.isAncestorOf(item6));
    QVERIFY(!item8.isAncestorOf(item7));

    JsonDbObject item9(item8);
    item9.insert(QStringLiteral("key"), QJsonValue::Undefined);
    item9.computeVersion();
    QVERIFY(item9.version().startsWith("9-"));
    QCOMPARE(item9.version().size(), 12);
    QVERIFY(item9.version().mid(2) != item8.version().mid(2));
    QVERIFY(item1.isAncestorOf(item9));
    QVERIFY(item2.isAncestorOf(item9));
    QVERIFY(item3.isAncestorOf(item9));
    QVERIFY(item4.isAncestorOf(item9));
    QVERIFY(item5.isAncestorOf(item9));
    QVERIFY(item6.isAncestorOf(item9));
    QVERIFY(item7.isAncestorOf(item9));
    QVERIFY(item8.isAncestorOf(item9));
    QVERIFY(!item9.isAncestorOf(item1));
    QVERIFY(!item9.isAncestorOf(item2));
    QVERIFY(!item9.isAncestorOf(item3));
    QVERIFY(!item9.isAncestorOf(item4));
    QVERIFY(!item9.isAncestorOf(item5));
    QVERIFY(!item9.isAncestorOf(item6));
    QVERIFY(!item9.isAncestorOf(item7));
    QVERIFY(!item9.isAncestorOf(item8));

    QCOMPARE(item9.version().mid(1), item1.version().mid(1));
}

void TestJsonDb::updateVersionOptimistic()
{
    JsonDbObject master;
    master.generateUuid();
    master.computeVersion();

    JsonDbObject target(master);
    JsonDbObject update;

    QString versionWritten;

    // non-matching uuid should fail
    QVERIFY(!target.updateVersionOptimistic(update, &versionWritten));
    QCOMPARE(target, master);

    // replay should succeed but not do anything
    update = master;
    QCOMPARE(target.updateVersionOptimistic(update, &versionWritten), true);
    QCOMPARE(target, master);
    QCOMPARE(versionWritten, target.version());
    update.computeVersion();
    QCOMPARE(versionWritten, update.version());

    update.remove("_meta");
    update.insert(QStringLiteral("update"), true);
    JsonDbObject lateUpdate(master);
    lateUpdate.remove("_meta");
    lateUpdate.insert(QStringLiteral("update"), false);

    QVERIFY(target.updateVersionOptimistic(update, &versionWritten));
    update.computeVersion();
    QCOMPARE(versionWritten, update.version());
    QCOMPARE(target.value(QStringLiteral("update")).toBool(), true);

    QVERIFY(master.isAncestorOf(target));
    QVERIFY(!target.isAncestorOf(master));

    QVERIFY(!target.updateVersionOptimistic(lateUpdate, &versionWritten));
    QCOMPARE(target.version(), update.version());
    QCOMPARE(target.value(QStringLiteral("update")).toBool(), true);
    QVERIFY(master.isAncestorOf(target));
    QVERIFY(!target.isAncestorOf(master));

    lateUpdate.computeVersion();
    QVERIFY(!lateUpdate.isAncestorOf(target));

}

void TestJsonDb::updateVersionReplicating()
{
    JsonDbObject master;
    master.generateUuid();
    master.computeVersion();

    JsonDbObject target(master);
    JsonDbObject source;

    // add something that doesn't have matching uuid
    QCOMPARE(target.updateVersionReplicating(source), false);
    QCOMPARE(target, master);

    // merge with replay
    source = master;
    QCOMPARE(target.updateVersionReplicating(source), true);
    QCOMPARE(target, master);

    // create a conflict
    JsonDbObject conflict(master);
    source.insert(QStringLiteral("key"), false);
    source.computeVersion();
    conflict.insert(QStringLiteral("key"), true);
    conflict.computeVersion();

    // sanity check which version will win
    // deterministic assumption: source
    // Note: if this ever fails, you modified the test or the version algorithm
    // Changing the algorithm is very bad!
    // If just changing the test, you need to reverse source and conflict in below QCOMPAREs
    QVERIFY(conflict < source);

    // adding order does NOT matter
    QCOMPARE(target.updateVersionReplicating(conflict), true);
    QCOMPARE(target.updateVersionReplicating(source), true);

    // let's proof that
    {
        JsonDbObject reversedMergeOrder(master);
        QCOMPARE(reversedMergeOrder.updateVersionReplicating(source), true);
        QCOMPARE(reversedMergeOrder.updateVersionReplicating(conflict), true);
        QCOMPARE(reversedMergeOrder, target);
    }

    // let's look at target more closely
    QVERIFY(master.isAncestorOf(target));
    QCOMPARE(target.version(), source.version());
    {
        // the reported conflict should equal the original sans meta

        QJsonArray conflicts = target.value(JsonDbString::kMetaStr).toObject().value(JsonDbString::kConflictsStr).toArray();
        JsonDbObject reportedConflict(conflicts.at(0).toObject());
        QCOMPARE(reportedConflict.contains(JsonDbString::kMetaStr), false);

        JsonDbObject checkConflict(conflict);
        checkConflict.remove(JsonDbString::kMetaStr);

        QCOMPARE(reportedConflict, checkConflict);
    }

    master = target;

    // lets progress
    source.insert(QStringLiteral("update"), true);
    source.computeVersion();
    QCOMPARE(target.updateVersionReplicating(source), true);

    QVERIFY(master.isAncestorOf(target));
    QCOMPARE(target.version(), source.version());
    QCOMPARE(target.value(QStringLiteral("update")).toBool(), true);
    {
        // the reported conflict should equal the original sans meta

        QJsonArray conflicts = target.value(JsonDbString::kMetaStr).toObject().value(JsonDbString::kConflictsStr).toArray();
        JsonDbObject reportedConflict(conflicts.at(0).toObject());
        QCOMPARE(reportedConflict.contains(JsonDbString::kMetaStr), false);

        JsonDbObject checkConflict(conflict);
        checkConflict.remove(JsonDbString::kMetaStr);

        QCOMPARE(reportedConflict, checkConflict);
    }

    master = target;

    // lets kill the conflict
    conflict.insert(JsonDbString::kDeletedStr, true);
    JsonDbObject conflictReplay(conflict);

    conflict.computeVersion();
    QCOMPARE(target.updateVersionReplicating(conflict), true);
    {
        QJsonObject meta = target.value(JsonDbString::kMetaStr).toObject();
        QCOMPARE(meta.contains(JsonDbString::kConflictsStr), false);
    }
    QVERIFY(conflict.isAncestorOf(target));
    QVERIFY(!target.isAncestorOf(conflict));
    QCOMPARE(target.value(QStringLiteral("update")).toBool(), true);
    QCOMPARE(target.version(), source.version());

    // conflict removal should be possible by updateVersionOptimistic()
    JsonDbObject update(master);
    QString versionWritten;
    QVERIFY(update.updateVersionOptimistic(conflictReplay, &versionWritten));
    QCOMPARE(update, target);
    QCOMPARE(versionWritten, conflict.version());
}

/*
 * Create an item
 */
void TestJsonDb::create()
{
    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QString("create-test-type"));
    item.insert("create-test", 22);
    item.insert("create-string", QString("string"));

    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QJsonObject map = result.value(JsonDbString::kResultStr).toObject();
    QVERIFY(map.contains(JsonDbString::kUuidStr));
    QVERIFY(!map.value(JsonDbString::kUuidStr).toString().isEmpty());
    QCOMPARE(map.value(JsonDbString::kUuidStr).type(), QJsonValue::String);

    QVERIFY(map.contains(JsonDbString::kVersionStr));
    QVERIFY(!map.value(JsonDbString::kVersionStr).toString().isEmpty());
    QCOMPARE(map.value(JsonDbString::kVersionStr).type(), QJsonValue::String);

    QJsonObject query;
    QString querystr(QString("[?_uuid=\"%1\"]").arg(map.value(JsonDbString::kUuidStr).toString()));
    query.insert("query", querystr);
    JsonDbQueryResult findResult = mJsonDb->find(mOwner, query);
    QCOMPARE(findResult.data.size(), 1);
    QCOMPARE(findResult.data.at(0).value(JsonDbString::kUuidStr).toString(), map.value(JsonDbString::kUuidStr).toString());
    QCOMPARE(findResult.data.at(0).value(JsonDbString::kVersionStr).toString(), map.value(JsonDbString::kVersionStr).toString());
}


/*
 * Verify translation of capabilities to access control policies.
 */
void TestJsonDb::capabilities()
{
    QJsonArray viewDefinitions(readJsonFile(":/daemon/json/capabilities-test.json").toArray());
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

/*
 * Insert an item and then update it.
 */

void TestJsonDb::update()
{
    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("update-test-type"));
    item.insert("update-test", 100);
    item.insert("update-string", QLatin1String("update-test-100"));

    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    QString uuid = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kUuidStr).toString();

    item.insert(JsonDbString::kUuidStr, uuid);
    item.insert("update-test", 101);

    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr,1);
}

/*
 * Update an item which doesn't exist
 */

void TestJsonDb::update2()
{
    JsonDbObject item;
    item.insert("update2-test", 100);
    item.insert("_type", QString("update-from-null"));
    item.generateUuid();

    QJsonObject result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr,1);
}

/*
 * Update an item which doesn't have a "uuid" field
 */

void TestJsonDb::update3()
{
    JsonDbObject item;
    item.insert("update2-test", 100);

    QJsonObject result = mJsonDb->update(mOwner, item);
    verifyErrorResult(result);
}

/*
 * Update a stale copy of an item
 */

void TestJsonDb::update4()
{
    jsondbSettings->setRejectStaleUpdates(true);

    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("update-test-type"));
    item.insert("update-test", 100);
    item.insert("update-string", QLatin1String("update-test-100"));

    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    QString uuid = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kUuidStr).toString();

    QString version1 = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kVersionStr).toString();

    item.insert(JsonDbString::kUuidStr, uuid);
    JsonDbObject replay(item);

    item.insert(JsonDbString::kVersionStr, version1);


    item.insert("update-test", 101);

    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr,1);
    verifyResultField(result,JsonDbString::kUuidStr, uuid);

    QString version2 = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kVersionStr).toString();
    QVERIFY(version1 != version2);

    QJsonObject query;
    query.insert("query", QString("[?_uuid=\"%1\"]").arg(uuid));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value(JsonDbString::kUuidStr).toString(), uuid);
    QCOMPARE(queryResult.data.at(0).value(JsonDbString::kVersionStr).toString(), version2);


    item.insert("update-test", 202);
    item.insert("_version", version2);
    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr,1);
    verifyResultField(result,JsonDbString::kUuidStr, uuid);
    QString version3 = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kVersionStr).toString();
    QVERIFY(version3 != version2);

    // replay
    result = mJsonDb->update(mOwner, replay);
    verifyGoodResult(result);
    verifyResultField(result, JsonDbString::kCountStr,1);
    verifyResultField(result, JsonDbString::kUuidStr, uuid);
    verifyResultField(result, JsonDbString::kVersionStr, version1);

    // conflict
    item.insert(JsonDbString::kVersionStr, version2);
    item.insert("update-test", 102);
    result = mJsonDb->update(mOwner, item);
    verifyErrorResult(result);

    jsondbSettings->setRejectStaleUpdates(false);
}

/*
 * Create an item and immediately remove it
 */

void TestJsonDb::remove()
{
    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("remove-test-type"));
    item.insert("remove-test", 100);

    QJsonObject response = mJsonDb->create(mOwner, item);
    verifyGoodResult(response);
    QJsonObject result = response.value(JsonDbString::kResultStr).toObject();

    QString uuid = result.value(JsonDbString::kUuidStr).toString();
    QString version = result.value(JsonDbString::kVersionStr).toString();

    item.insert(JsonDbString::kUuidStr, uuid);
    //item.insert(JsonDbString::kVersionStr, version);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr, 1);

    QJsonObject query;
    query.insert("query", QString("[?_uuid=\"%1\"]").arg(uuid));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 0);
}

/*
 * Try to remove an item which doesn't exist
 */

void TestJsonDb::remove2()
{
    JsonDbObject item;
    item.insert("remove2-test", 100);
    item.insert(JsonDbString::kTypeStr, QLatin1String("Remove2Type"));
    item.insert(JsonDbString::kUuidStr, QUuid::createUuid().toString());

    QJsonObject result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);
}

/*
 * Don't include a 'uuid' field
 */

void TestJsonDb::remove3()
{
    JsonDbObject item;
    item.insert("remove3-test", 100);

    QJsonObject result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);
}

/*
 * Try to remove an item which existed before but was removed
 */
void TestJsonDb::remove4()
{
    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("remove-test-type"));
    item.insert("remove-test", 100);

    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    QString uuid = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kUuidStr).toString();
    item.insert(JsonDbString::kUuidStr, uuid);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr, 1);

    result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);
    result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);
}

/*
 * Remove a stale version of the object
 */
void TestJsonDb::remove5()
{
    jsondbSettings->setRejectStaleUpdates(true);

    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("update-test-type"));

    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    qDebug() << result;

    QString version = item.take(JsonDbString::kVersionStr).toString();
    result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);

    item.insert(JsonDbString::kVersionStr, version);
    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);

    jsondbSettings->setRejectStaleUpdates(false);
}

void TestJsonDb::schemaValidation_data()
{
    QTest::addColumn<QByteArray>("schema");
    QTest::addColumn<QByteArray>("object");
    QTest::addColumn<bool>("result"); // is an object valid against a schema?

    QByteArray schema, object;
    bool result;

    const QString dataDir = QString::fromLatin1(":/json-validation/");
    QDir dir(dataDir);
    QStringList schemaNames = dir.entryList(QStringList() << "*-schema.json");

    Q_ASSERT_X(schemaNames.count(), Q_FUNC_INFO, "Tests not found");

    foreach (const QString &schemaFileName, schemaNames) {
        QFile schemeFile(dataDir + schemaFileName);
        schemeFile.open(QIODevice::ReadOnly);
        schema = schemeFile.readAll();
        const QString testName = schemaFileName.mid(0, schemaFileName.length() - 12); // chop "-schema.json"
        QStringList tests = dir.entryList(QStringList() << (testName + "*"));
        foreach (const QString &test, tests) {
            if (test.endsWith("-schema.json"))
                continue;

            QFile objectFile(dataDir + test);
            objectFile.open(QIODevice::ReadOnly);
            object = objectFile.readAll();
            result = !test.endsWith("invalid.json");
            QTest::newRow(test.toLatin1().data()) << schema << object << result;
        }
    }
}

void TestJsonDb::schemaValidation()
{
    jsondbSettings->setValidateSchemas(true);

    QFETCH(QByteArray, schema);
    QFETCH(QByteArray, object);
    QFETCH(bool, result);

    static uint id = 0;
    id++;
    QString schemaName = QLatin1String("schemaValidationSchema") + QString::number(id);

    QJsonObject schemaBody = readJson(schema).toObject();
    JsonDbObject schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", schemaName);
    schemaObject.insert("schema", schemaBody);

    QJsonObject qResult = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(qResult);

    JsonDbObject item = readJson(object).toObject();
    item.insert(JsonDbString::kTypeStr, schemaName);

    // Create an item that matches the schema
    qResult = mJsonDb->create(mOwner, item);
    if (result) {
        verifyGoodResult(qResult);
    } else {
        verifyErrorResult(qResult);
    }
    if (result) {
        qResult = mJsonDb->remove(mOwner, item);
        verifyGoodResult(qResult);
    }
    qResult = mJsonDb->remove(mOwner, schemaObject);
    verifyGoodResult(qResult);

    jsondbSettings->setValidateSchemas(false);
}

void TestJsonDb::schemaValidationExtends_data()
{
    QTest::addColumn<QByteArray>("item");
    QTest::addColumn<bool>("isPerson");
    QTest::addColumn<bool>("isAdult");

    QTest::newRow("empty")
            << QByteArray("{}") << true << true;
    QTest::newRow("40 and 4")
            << QByteArray("{ \"name\":\"40 and 4\" }") << true << true;
    QTest::newRow("Alice")
            << QByteArray("{ \"name\":\"Alice\", \"age\": 5}")  << true << false;
    QTest::newRow("Alice's mother")
            << QByteArray("{ \"name\":\"Alice's mother\", \"age\": 32}") << true << true;
    QTest::newRow("Alice's grandmother")
            << QByteArray("{ \"name\":\"Alice's grandmother\", \"age\": 100}") << true << true;
    QTest::newRow("Alice's great-grandmother")
            << QByteArray("{ \"name\":\"Alice's great-grandmother\", \"age\": 130}") << false << false;
}

void TestJsonDb::schemaValidationExtends()
{
    jsondbSettings->setValidateSchemas(true);

    QFETCH(QByteArray, item);
    QFETCH(bool, isPerson);
    QFETCH(bool, isAdult);

    static int id = 0; // schema name id
    ++id;
    const QString personSchemaName = QString::fromLatin1("person") + QString::number(id);
    const QString adultSchemaName = QString::fromLatin1("adult") + QString::number(id);

    // init schemas
    QJsonObject qResult;
    {
        const QByteArray person =
                "{"
                "    \"description\": \"A person\","
                "    \"type\": \"object\","
                "    \"properties\": {"
                "        \"name\": {\"type\": \"string\"},"
                "        \"age\" : {\"type\": \"integer\", \"maximum\": 125 }"
                "    }"
                "}";
        const QByteArray adult = QString::fromLatin1(
                "{"
                "    \"description\": \"An adult\","
                "    \"properties\": {\"age\": {\"minimum\": 18}},"
                "    \"extends\": {\"$ref\":\"person%1\"}"
                "}").arg(QString::number(id)).toLatin1();
        QJsonObject personSchemaBody = readJson(person).toObject();
        QJsonObject adultSchemaBody = readJson(adult).toObject();

        JsonDbObject personSchemaObject;
        personSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        personSchemaObject.insert("name", personSchemaName);
        personSchemaObject.insert("schema", personSchemaBody);

        JsonDbObject adultSchemaObject;
        adultSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        adultSchemaObject.insert("name", adultSchemaName);
        adultSchemaObject.insert("schema", adultSchemaBody);

        qResult = mJsonDb->create(mOwner, personSchemaObject);
        verifyGoodResult(qResult);
        qResult = mJsonDb->create(mOwner, adultSchemaObject);
        verifyGoodResult(qResult);
    }

    {
        JsonDbObject object = readJson(item).toObject();
        object.insert("testingForPerson", isPerson);
        object.insert(JsonDbString::kTypeStr, personSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        if (isPerson) {
            verifyGoodResult(qResult);
        } else {
            verifyErrorResult(qResult);
        }
    }

    {
        JsonDbObject object = readJson(item).toObject();
        object.insert("testingForAdult", isAdult);
        object.insert(JsonDbString::kTypeStr, adultSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        if (isAdult) {
            verifyGoodResult(qResult);
        } else {
            verifyErrorResult(qResult);
        }
    }

    jsondbSettings->setValidateSchemas(false);
}


void TestJsonDb::schemaValidationExtendsArray_data()
{
    QTest::addColumn<QByteArray>("item");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("empty")
            << QByteArray("{}") << false;
    QTest::newRow("fiat 125p")
            << QByteArray("{ \"landSpeed\": 5}")  << false;
    QTest::newRow("bavaria 44")
            << QByteArray("{ \"waterSpeed\": 6}") << false;
    QTest::newRow("orukter amphibolos")
            << QByteArray("{ \"waterSpeed\":10, \"landSpeed\": 10}") << true;
    QTest::newRow("batmans car")
            << QByteArray("{ \"waterSpeed\":100, \"landSpeed\": 100, \"airSpeed\": 100}") << true;
}

void TestJsonDb::schemaValidationExtendsArray()
{
    jsondbSettings->setValidateSchemas(true);

    QFETCH(QByteArray, item);
    QFETCH(bool, isValid);

    static int id = 0; // schema name id
    ++id;
    const QString amphibiousSchemaName = QString::fromLatin1("amphibious") + QString::number(id);
    const QString carSchemaName = QString::fromLatin1("car") + QString::number(id);
    const QString boatSchemaName = QString::fromLatin1("boat") + QString::number(id);

    // init schemas
    QJsonObject qResult;
    {
        const QByteArray car =
                "{"
                "    \"description\": \"A car\","
                "    \"properties\": {\"landSpeed\": {\"required\": true}}"
                "}";

        const QByteArray boat =
                "{"
                "    \"description\": \"A boat\","
                "    \"properties\": {\"waterSpeed\": {\"required\": true}}"
                "}";

        const QByteArray amphibious = QString::fromLatin1(
                "{"
                "    \"description\": \"A amphibious\","
                "    \"extends\": [{\"$ref\":\"car%1\"}, {\"$ref\":\"boat%1\"}]"
                "}").arg(QString::number(id)).toLatin1();

        QJsonObject amphibiousSchemaBody = readJson(amphibious).toObject();
        QJsonObject carSchemaBody = readJson(car).toObject();
        QJsonObject boatSchemaBody = readJson(boat).toObject();


        JsonDbObject carSchemaObject;
        carSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        carSchemaObject.insert("name", carSchemaName);
        carSchemaObject.insert("schema", carSchemaBody);

        JsonDbObject boatSchemaObject;
        boatSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        boatSchemaObject.insert("name", boatSchemaName);
        boatSchemaObject.insert("schema", boatSchemaBody);

        JsonDbObject amphibiousSchemaObject;
        amphibiousSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        amphibiousSchemaObject.insert("name", amphibiousSchemaName);
        amphibiousSchemaObject.insert("schema", amphibiousSchemaBody);

        qResult = mJsonDb->create(mOwner, carSchemaObject);
        verifyGoodResult(qResult);
        qResult = mJsonDb->create(mOwner, boatSchemaObject);
        verifyGoodResult(qResult);
        qResult = mJsonDb->create(mOwner, amphibiousSchemaObject);
        verifyGoodResult(qResult);
    }

    {
        JsonDbObject object = readJson(item).toObject();
        object.insert("testingForAmphibious", isValid);
        object.insert(JsonDbString::kTypeStr, amphibiousSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        if (isValid) {
            verifyGoodResult(qResult);
        } else {
            verifyErrorResult(qResult);
        }
    }

    jsondbSettings->setValidateSchemas(false);
}

void TestJsonDb::schemaValidationLazyInit()
{
    jsondbSettings->setValidateSchemas(true);

    const QByteArray person =
            "{"
            "    \"description\": \"A person\","
            "    \"type\": \"object\","
            "    \"properties\": {"
            "        \"name\": {\"type\": \"string\", \"required\": true},"
            "        \"age\" : {\"type\": \"integer\", \"maximum\": 125 }"
            "    }"
            "}";
    const QByteArray adult =
            "{"
            "    \"description\": \"An adult\","
            "    \"properties\": {\"age\": {\"minimum\": 18}},"
            "    \"extends\": {\"$ref\":\"personLazyInit\"}"
            "}";

    const QString personSchemaName = QString::fromLatin1("personLazyInit");
    const QString adultSchemaName = QString::fromLatin1("adultLazyInit");

    QJsonObject personSchemaBody = readJson(person).toObject();
    QJsonObject adultSchemaBody = readJson(adult).toObject();

    JsonDbObject personSchemaObject;
    personSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    personSchemaObject.insert("name", personSchemaName);
    personSchemaObject.insert("schema", personSchemaBody);

    JsonDbObject adultSchemaObject;
    adultSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    adultSchemaObject.insert("name", adultSchemaName);
    adultSchemaObject.insert("schema", adultSchemaBody);

    // Without lazy compilation this operation fails, because adult schema referece unexisting yet
    // person schema
    QJsonObject qResult;
    qResult = mJsonDb->create(mOwner, adultSchemaObject);
    verifyGoodResult(qResult);
    qResult = mJsonDb->create(mOwner, personSchemaObject);
    verifyGoodResult(qResult);

    // Insert some objects to force full schema compilation
    {
        const QByteArray item = "{ \"name\":\"Nierob\", \"age\":99 }";
        JsonDbObject object = readJson(item).toObject();
        object.insert(JsonDbString::kTypeStr, adultSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyGoodResult(qResult);
    }
    {
        const QByteArray item = "{ \"name\":\"Nierob\", \"age\":12 }";
        JsonDbObject object = readJson(item).toObject();
        object.insert(JsonDbString::kTypeStr, adultSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyErrorResult(qResult);
    }
    {
        const QByteArray item = "{ \"age\":19 }";
        JsonDbObject object = readJson(item).toObject();
        object.insert(JsonDbString::kTypeStr, adultSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyErrorResult(qResult);
    }
    {
        const QByteArray item = "{ \"name\":\"Nierob\", \"age\":12 }";
        JsonDbObject object = readJson(item).toObject();
        object.insert(JsonDbString::kTypeStr, personSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyGoodResult(qResult);
    }
    {
        const QByteArray item = "{ \"age\":12 }";
        JsonDbObject object = readJson(item).toObject();
        object.insert(JsonDbString::kTypeStr, personSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyErrorResult(qResult);
    }

    jsondbSettings->setValidateSchemas(false);
}


/*
 * Create a list of items
 */

#define LIST_TEST_ITEMS 6

void TestJsonDb::createList()
{
    JsonDbObjectList list;
    for (int i = 0 ; i < LIST_TEST_ITEMS ; i++ ) {
        JsonDbObject map;
        map.insert(JsonDbString::kTypeStr, QLatin1String("create-list-type"));
        map.insert("create-list-test", i + 100);
        list.append(map);
    }

    QJsonObject result = mJsonDb->createList(mOwner, list);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr, LIST_TEST_ITEMS);
}
/*
 * Create a list of items and then update them
 */

void TestJsonDb::updateList()
{
    JsonDbObjectList list;
    for (int i = 0 ; i < LIST_TEST_ITEMS ; i++ ) {
        JsonDbObject map;
        map.insert(JsonDbString::kTypeStr, QLatin1String("update-list-type"));
        map.insert("update-list-test", i + 100);
        QJsonObject result = mJsonDb->create(mOwner, map);
        verifyGoodResult(result);
        QString uuid = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kUuidStr).toString();
        map.insert(JsonDbString::kUuidStr, uuid);
        map.insert("fuzzyduck", QLatin1String("Duck test"));
        list.append(map);
    }

    QJsonObject result = mJsonDb->updateList(mOwner, list);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr, LIST_TEST_ITEMS);
}

void TestJsonDb::mapDefinition()
{
    // we need a schema that extends View for our targetType
    JsonDbObject schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QJsonObject schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QJsonObject schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    JsonDbObject mapDefinition;
    mapDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Map"));
    mapDefinition.insert("targetType", QLatin1String("MyViewType"));
    JsonDbObject sourceToMapFunctions;
    sourceToMapFunctions.insert("Contact", QLatin1String("function map (c) { }"));
    mapDefinition.insert("map", sourceToMapFunctions);
    QJsonObject res = mJsonDb->create(mOwner, mapDefinition);
    verifyGoodResult(res);

    verifyGoodResult(mJsonDb->remove(mOwner, mapDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::mapDefinitionInvalid()
{
    // we need a schema that extends View for our targetType
    JsonDbObject schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QJsonObject schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QJsonObject schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    JsonDbObject mapDefinition;
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("sourceType", QLatin1String("Contact"));
    mapDefinition.insert("map", QLatin1String("function map (c) { }"));
    QJsonObject res = mJsonDb->create(mOwner, mapDefinition);
    verifyErrorResult(res);

    mapDefinition = JsonDbObject();
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("targetType", QLatin1String("MyViewType"));
    mapDefinition.insert("map", QLatin1String("function map (c) { }"));
    res = mJsonDb->create(mOwner, mapDefinition);
    verifyErrorResult(res);

    mapDefinition = JsonDbObject();
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("targetType", QLatin1String("MyViewType"));
    mapDefinition.insert("sourceType", QLatin1String("Contact"));
    res = mJsonDb->create(mOwner, mapDefinition);
    verifyErrorResult(res);

    // fail because targetType doesn't extend View
    mapDefinition = JsonDbObject();
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("targetType", QLatin1String("MyViewType2"));
    mapDefinition.insert("sourceType", QLatin1String("Contact"));
    mapDefinition.insert("map", QLatin1String("function map (c) { }"));
    res = mJsonDb->create(mOwner, mapDefinition);
    verifyErrorResult(res);
    QVERIFY(res.value("error").toObject().value(JsonDbString::kMessageStr).toString().contains("View"));

    res = mJsonDb->remove(mOwner, schema);
    verifyGoodResult(res);
}

void TestJsonDb::reduceDefinition()
{
    // we need a schema that extends View for our targetType
    JsonDbObject schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QJsonObject schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QJsonObject schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    JsonDbObject reduceDefinition;
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    QJsonObject res = mJsonDb->create(mOwner, reduceDefinition);
    verifyGoodResult(res);

    verifyGoodResult(mJsonDb->remove(mOwner, reduceDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::reduceDefinitionInvalid()
{
    // we need a schema that extends View for our targetType
    JsonDbObject schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QJsonObject schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QJsonObject schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    JsonDbObject reduceDefinition;
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    QJsonObject res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    reduceDefinition = JsonDbObject();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    reduceDefinition = JsonDbObject();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    reduceDefinition = JsonDbObject();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    reduceDefinition = JsonDbObject();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    // fail because targetType doesn't extend View
    reduceDefinition = JsonDbObject();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType2"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);
    QVERIFY(res.value("error").toObject().value(JsonDbString::kMessageStr).toString().contains("View"));

    //schemaRes.value("result").toObject()
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::mapInvalidMapFunc()
{
    JsonDbObject schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("InvalidMapViewType"));
    QJsonObject schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QJsonObject schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    JsonDbObject mapDefinition;
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("targetType", QLatin1String("InvalidMapViewType"));
    JsonDbObject sourceToMapFunctions;
    sourceToMapFunctions.insert("Contact", QLatin1String("function map (c) { ;")); // non-parsable map function
    mapDefinition.insert("map", sourceToMapFunctions);

    qDebug() << "mapDefinition" << mapDefinition;
    QJsonObject defRes = mJsonDb->create(mOwner, mapDefinition);
    verifyGoodResult(defRes);
    QString uuid = defRes.value("result").toObject().value("_uuid").toString();

    // force the view to be updated
    mJsonDb->updateView("InvalidMapViewType");

    // now check for an error
    GetObjectsResult res = mJsonDb->getObjects("_uuid", uuid, JsonDbString::kMapTypeStr);
    QVERIFY(res.data.size() > 0);
    mapDefinition = res.data.at(0);
    QVERIFY(mapDefinition.contains(JsonDbString::kActiveStr) && !mapDefinition.value(JsonDbString::kActiveStr).toBool());
    QVERIFY(!mapDefinition.value(JsonDbString::kErrorStr).toString().isEmpty());

    verifyGoodResult(mJsonDb->remove(mOwner, mapDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::reduceInvalidAddSubtractFuncs()
{
    JsonDbObject schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QJsonObject schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QJsonObject schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    JsonDbObject reduceDefinition;
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { ;")); // non-parsable add function
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    QJsonObject res = mJsonDb->create(mOwner, reduceDefinition);
    verifyGoodResult(res);

    mJsonDb->updateView("MyViewType");

    GetObjectsResult getObjects = mJsonDb->getObjects("_uuid", res.value("result").toObject().value("_uuid").toString());
    reduceDefinition = getObjects.data.at(0);
    QVERIFY(reduceDefinition.contains(JsonDbString::kActiveStr) && !reduceDefinition.value(JsonDbString::kActiveStr).toBool());
    QVERIFY(!reduceDefinition.value(JsonDbString::kErrorStr).toString().isEmpty());

    verifyGoodResult(mJsonDb->remove(mOwner, reduceDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::map()
{
    addIndex(QLatin1String("phoneNumber"));

    QJsonArray objects(readJsonFile(":/daemon/json/map-reduce.json").toArray());

    JsonDbObjectList mapsReduces;
    JsonDbObjectList schemas;
    QMap<QString, JsonDbObject> toDelete;
    for (int i = 0; i < objects.size(); ++i) {
        QJsonObject object(objects.at(i).toObject());
        JsonDbObject doc(object);
        QJsonObject result = mJsonDb->create(mOwner, doc);
        verifyGoodResult(result);

        if (object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kMapTypeStr ||
            object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kReduceTypeStr)
            mapsReduces.append(doc);
        else if (object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kSchemaTypeStr)
            schemas.append(doc);
        else
            toDelete.insert(doc.value("_uuid").toString(), doc);
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"Phone\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 5);

    // now remove one of the source items
    QJsonObject query2;
    query2.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"Contact\"][?displayName=\"Nancy Doe\"]"));
    queryResult = mJsonDb->find(mOwner, query2);
    JsonDbObject firstItem = queryResult.data.at(0);
    QVERIFY(!firstItem.value("_uuid").toString().isEmpty());
    toDelete.remove(firstItem.value("_uuid").toString());
    QJsonObject result = mJsonDb->remove(mOwner, firstItem);
    verifyGoodResult(result);

    // get results with getObjects()
    GetObjectsResult getObjectsResult = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("Phone"));
    QCOMPARE(getObjectsResult.data.size(), 3);
    if (jsondbSettings->verbose()) {
        JsonDbObjectList vs = getObjectsResult.data;
        for (int i = 0; i < vs.size(); i++)
            qDebug() << "    " << vs[i];
    }

    // query for results
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 3);

    QJsonObject query3;
    query3.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"PhoneCount\"][/key]"));
    queryResult = mJsonDb->find(mOwner, query3);
    verifyGoodQueryResult(queryResult);
    if (jsondbSettings->verbose()) {
        JsonDbObjectList vs = queryResult.data;
        for (int i = 0; i < vs.size(); i++)
            qDebug() << "    " << vs[i];
    }
    QCOMPARE(queryResult.data.size(), 3);

    GetObjectsResult gor = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("PhoneCount"));
    QCOMPARE(gor.data.size(), 3);

    for (int i = 0; i < mapsReduces.size(); ++i) {
        JsonDbObject object = mapsReduces.at(i);
        verifyGoodResult(mJsonDb->remove(mOwner, object));
    }
    for (int i = 0; i < schemas.size(); ++i) {
        JsonDbObject object = schemas.at(i);
        verifyGoodResult(mJsonDb->remove(mOwner, object));
    }
    foreach (JsonDbObject map, toDelete.values())
        verifyGoodResult(mJsonDb->remove(mOwner, map));
    //mJsonDb->removeIndex(QLatin1String("phoneNumber"));
}

void TestJsonDb::mapDuplicateSourceAndTarget()
{
    QJsonArray objects(readJsonFile(":/daemon/json/map-sametarget.json").toArray());
    JsonDbObjectList toDelete;
    JsonDbObjectList maps;

    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.value(JsonDbString::kTypeStr).toString() == "Map")
            maps.append(object);
        else
            toDelete.append(object);
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"ContactView\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 4);

    int firstNameCount = 0;
    JsonDbObjectList data = queryResult.data;
    for (int ii = 0; ii < data.size(); ii++)
        if (!data.at(ii).value("firstName").toString().isEmpty())
            firstNameCount++;
    QCOMPARE(firstNameCount, 2);

    for (int ii = 0; ii < maps.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, maps.at(ii)));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii)));
    mJsonDb->removeIndex("ContactView");
}

void TestJsonDb::mapRemoval()
{
    QJsonArray objects(readJsonFile(":/daemon/json/map-sametarget.json").toArray());

    QList<JsonDbObject> maps;
    QList<JsonDbObject> toDelete;

    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.value(JsonDbString::kTypeStr).toString() == "Map")
            maps.append(object);
        else
            toDelete.append(object);
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"ContactView\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 4);

    // remove a map
    QJsonObject result = mJsonDb->remove(mOwner, maps.takeAt(0));
    verifyGoodResult(result);
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 2);

    // an now the other
    result = mJsonDb->remove(mOwner, maps.takeAt(0));
    verifyGoodResult(result);
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 0);

    for (int ii = 0; ii < maps.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, maps.at(ii)));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii)));
}

void TestJsonDb::mapUpdate()
{
    QJsonArray objects(readJsonFile(":/daemon/json/map-sametarget.json").toArray());

    JsonDbObjectList maps;
    JsonDbObjectList toDelete;

    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.value(JsonDbString::kTypeStr).toString() == "Map")
            maps.append(object);
        else
            toDelete.append(object);
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"ContactView\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 4);

    // tinker with a map
    JsonDbObject map = maps.at(0);
    map.insert("targetType", QString("ContactView2"));
    QJsonObject result = mJsonDb->update(mOwner, map);
    verifyGoodResult(result);

    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 2);

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"ContactView2\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 2);

    for (int ii = 0; ii < maps.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, maps.at(ii)));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii)));

}

void TestJsonDb::mapJoin()
{
    QJsonArray objects(readJsonFile(":/daemon/json/map-join.json").toArray());

    JsonDbObject join;
    JsonDbObject schema;
    JsonDbObjectList people;

    for (int i = 0; i < objects.size(); ++i) {
        JsonDbObject object(objects.at(i).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        object.insert(JsonDbString::kUuidStr, result.value("result").toObject().value(JsonDbString::kUuidStr).toString());

        if (object.value(JsonDbString::kTypeStr).toString() == "Map")
            join = object;
        else if (object.value(JsonDbString::kTypeStr).toString() == "_schemaType")
            schema = object;
        else
            people.append(object);
    }
    addIndex("friend", "string", "FoafPerson");
    addIndex("foaf", "string", "FoafPerson");
    addIndex("friend", "string", "Person");

    GetObjectsResult getObjects = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("FoafPerson"));
    QCOMPARE(getObjects.data.size(), 0);

    // set some friends
    QString previous;
    for (int i = 0; i < people.size(); ++i) {
        JsonDbObject person = people.at(i);
        if (!previous.isEmpty())
            person.insert("friend", previous);

        previous = person.value(JsonDbString::kUuidStr).toString();
        QCOMPARE(person.value(JsonDbString::kTypeStr).toString(), QLatin1String("Person"));
        verifyGoodResult(mJsonDb->update(mOwner, person));
    }

    getObjects = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("Person"));
    JsonDbObjectList peopleWithFriends = getObjects.data;

    QJsonObject queryFoafPerson;
    // sort the list by key to make ordering deterministic
    queryFoafPerson.insert("query", QString::fromLatin1("[?_type=\"FoafPerson\"][?foaf exists][/friend]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, queryFoafPerson);
    QCOMPARE(queryResult.data.size(), people.size()-2); // one has no friend and one is friends with the one with no friends

    JsonDbObjectList resultList = queryResult.data;
    for (int i = 0; i < resultList.size(); ++i) {
        JsonDbObject person = resultList.at(i);
        QJsonArray sources = person.value("_sourceUuids").toArray();
        QVERIFY(sources.size() == 2 || sources.contains(person.value("friend")));
    }

    // take the last person, find his friend, and remove that friend's friend property
    // then make sure the foaf is updated
    JsonDbObject p = resultList.at(resultList.size()-1);
    QVERIFY(p.contains("foaf"));

    JsonDbObject fr;
    for (int i = 0; i < peopleWithFriends.size(); ++i) {
        JsonDbObject f = peopleWithFriends.at(i);
        if (f.value(JsonDbString::kUuidStr).toString() == p.value("friend").toString()) {
            fr = f;
            break;
        }
    }

    QVERIFY(fr.value(JsonDbString::kUuidStr).toString() == p.value("friend").toString());
    fr.insert("friend", QJsonValue(QJsonValue::Undefined));
    verifyGoodResult(mJsonDb->update(mOwner, fr));

    GetObjectsResult foafRes = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("FoafPerson"));
    JsonDbObjectList foafs = foafRes.data;
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"FoafPerson\"][?name=\"%1\"]").arg(p.value("name").toString()));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 1);

    p = queryResult.data.at(0);
    QVERIFY(!p.value("friend").toString().isEmpty());
    QVERIFY(!p.contains("foaf"));

    query = QJsonObject();
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"FoafPerson\"][/key]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);

    query = QJsonObject();
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"FoafPerson\"][/friend]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 8); // two have no friends

    verifyGoodResult(mJsonDb->remove(mOwner, join));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
    for (int i = 0; i < people.size(); ++i) {
        JsonDbObject object = people.at(i);
        if (object == join || object == schema)
            continue;
        mJsonDb->remove(mOwner, object);
    }
    mJsonDb->removeIndex("value.friend", "string", "FoafPerson");
    mJsonDb->removeIndex("value.foaf", "string", "FoafPerson");
}

void TestJsonDb::mapSelfJoinSourceUuids()
{
    addIndex("magic", "string");

    QJsonArray objects(readJsonFile(":/daemon/json/map-join-sourceuuids.json").toArray());
    JsonDbObjectList toDelete;
    JsonDbObject toUpdate;

    for (int i = 0; i < objects.size(); ++i) {
        JsonDbObject object(objects.at(i).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        object.insert(JsonDbString::kUuidStr, result.value("result").toObject().value(JsonDbString::kUuidStr).toString());
        toDelete.append(object);

        if (object.value(JsonDbString::kTypeStr).toString() == "Bar")
            toUpdate = object;
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MagicView\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);

    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("_sourceUuids").toArray().size(), 3);

    toUpdate.insert("extra", QString("123123"));
    verifyGoodResult(mJsonDb->update(mOwner, toUpdate));

    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);

    QCOMPARE(queryResult.data.at(0).value("_sourceUuids").toArray().size(), 3);
    for (int i = toDelete.size() - 1; i >= 0; i--)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(i)))
    mJsonDb->removeIndex("magic", "string");
}

void TestJsonDb::mapMapFunctionError()
{
    JsonDbObject schema;
    schema.insert(JsonDbString::kTypeStr, QString("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QString("MapFunctionErrorViewType"));
    QJsonObject schemaSub;
    schemaSub.insert("type", QString("object"));
    schemaSub.insert("extends", QString("View"));
    schema.insert("schema", schemaSub);
    QJsonObject schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    JsonDbObject mapDefinition;
    mapDefinition.insert(JsonDbString::kTypeStr, QString("Map"));
    mapDefinition.insert("targetType", QString("MapFunctionErrorViewType"));
    JsonDbObject sourceToMapFunctions;
    sourceToMapFunctions.insert("Contact", QString("function map (c) { invalidobject.fail(); }")); // error in map function
    mapDefinition.insert("map", sourceToMapFunctions);

    QJsonObject defRes = mJsonDb->create(mOwner, mapDefinition);
    verifyGoodResult(defRes);

    JsonDbObject contact;
    contact.insert(JsonDbString::kTypeStr, QString("Contact"));
    QJsonObject res = mJsonDb->create(mOwner, contact);

    // trigger the view update
    mJsonDb->updateView("MapFunctionErrorViewType");

    // see if the map definition is still active
    GetObjectsResult getObjects = mJsonDb->getObjects("_uuid", defRes.value("result").toObject().value("_uuid").toString());
    mapDefinition = getObjects.data.at(0);
    QVERIFY(mapDefinition.contains(JsonDbString::kActiveStr) && !mapDefinition.value(JsonDbString::kActiveStr).toBool());
    QVERIFY(mapDefinition.value(JsonDbString::kErrorStr).toString().contains("invalidobject"));

    verifyGoodResult(mJsonDb->remove(mOwner, mapDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::mapSchemaViolation()
{
    jsondbSettings->setValidateSchemas(true);

    GetObjectsResult contactsRes = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("Contact"));
    if (contactsRes.data.size() > 0)
      verifyGoodResult(mJsonDb->removeList(mOwner, contactsRes.data));

    QJsonArray objects(readJsonFile(":/daemon/json/map-reduce-schema.json").toArray());
    JsonDbObjectList toDelete;
    QJsonValue workingMap;
    JsonDbObject map;

    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        if (object.value(JsonDbString::kTypeStr).toString() != JsonDbString::kReduceTypeStr) {

            // use the broken Map function that creates schema violations
            if (object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kMapTypeStr) {
                workingMap = object.value("map");
                object.insert("map", object.value("brokenMap"));
            }

            QJsonObject result = mJsonDb->create(mOwner, object);
            if (object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kMapTypeStr) {
                map = object;
            }

            verifyGoodResult(result);
            if (object.value(JsonDbString::kTypeStr).toString() != JsonDbString::kMapTypeStr)
                toDelete.append(object);
        }
    }

    mJsonDb->updateView(map.value("targetType").toString());

    GetObjectsResult getObjects = mJsonDb->getObjects("_uuid", map.value(JsonDbString::kUuidStr).toString());
    QCOMPARE(getObjects.data.size(), 1);
    JsonDbObject mapDefinition = getObjects.data.at(0);
    QVERIFY(mapDefinition.contains(JsonDbString::kActiveStr));
    QVERIFY(!mapDefinition.value(JsonDbString::kActiveStr).toBool());
    QVERIFY(mapDefinition.value(JsonDbString::kErrorStr).toString().contains("Schema"));

    getObjects = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("Phone"));
    QCOMPARE(getObjects.data.size(), 0);
    // fix the map function
    map.insert("map", workingMap);
    map.insert(JsonDbString::kActiveStr, true);
    map.insert(JsonDbString::kErrorStr, QJsonValue(QJsonValue::Null));

    verifyGoodResult(mJsonDb->update(mOwner, map));

    mJsonDb->updateView(map.value("targetType").toString());

    getObjects = mJsonDb->getObjects("_uuid", map.value(JsonDbString::kUuidStr).toString());
    mapDefinition = getObjects.data.at(0);
    QVERIFY(!mapDefinition.contains(JsonDbString::kActiveStr)|| mapDefinition.value(JsonDbString::kActiveStr).toBool());
    QVERIFY(!mapDefinition.contains(JsonDbString::kErrorStr) || mapDefinition.value(JsonDbString::kErrorStr).toString().isEmpty());

    getObjects = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("Phone"));
    QCOMPARE(getObjects.data.size(), 5);

    verifyGoodResult(mJsonDb->remove(mOwner, map));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii)));

    jsondbSettings->setValidateSchemas(false);
}

void TestJsonDb::mapArrayConversion()
{
    QJsonArray objects(readJsonFile(":/daemon/json/map-array-conversion.json").toArray());
    JsonDbObjectList toDelete;
    for (int i = 0; i < objects.size(); i++) {
        JsonDbObject object(objects.at(i).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        toDelete.append(object);
    }

    JsonDbObject testObject;
    testObject.insert(JsonDbString::kTypeStr, QLatin1String("Test"));
    QJsonObject result = mJsonDb->create(mOwner, testObject);
    verifyGoodResult(result);

    QJsonObject request;
    request.insert("query", QLatin1String("[?_type=\"TestView\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 1);
    JsonDbObject o = queryResult.data.at(0);
    QVERIFY(o.value("result").isArray());

}

void TestJsonDb::reduce()
{
    QJsonArray objects(readJsonFile(":/daemon/json/reduce-data.json").toArray());

    JsonDbObjectList toDelete;
    JsonDbObjectList reduces;

    QHash<QString, int> firstNameCount;
    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        firstNameCount[object.value("firstName").toString()]++;
        toDelete.append(object);
    }

    objects = readJsonFile(":/daemon/json/reduce.json").toArray();
    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.value(JsonDbString::kTypeStr).toString() == "Reduce")
            reduces.append(object);
        else
            toDelete.append(object);
    }

    QJsonObject query, result;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), firstNameCount.keys().count());

    JsonDbObjectList data = queryResult.data;
    for (int ii = 0; ii < data.size(); ii++) {
        QCOMPARE((int)data.at(ii).value("count").toDouble(),
                 firstNameCount[data.at(ii).value("firstName").toString()]);
    }
    for (int ii = 0; ii < reduces.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, reduces.at(ii)));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii)));
    mJsonDb->removeIndex("MyContactCount");
}

void TestJsonDb::reduceRemoval()
{
    QJsonArray objects(readJsonFile(":/daemon/json/reduce-data.json").toArray());

    QJsonArray toDelete;
    QHash<QString, int> firstNameCount;
    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        firstNameCount[object.value("firstName").toString()]++;
        toDelete.append(object);
    }

    objects = readJsonFile(":/daemon/json/reduce.json").toArray();
    JsonDbObject reduce;
    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.value(JsonDbString::kTypeStr).toString() == "Reduce")
            reduce = object;
        else
            toDelete.append(object);
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), firstNameCount.keys().count());

    QJsonObject result = mJsonDb->remove(mOwner, reduce);
    verifyGoodResult(result);
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 0);

    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii).toObject(),
                                         mJsonDb->mSystemPartitionName));
    mJsonDb->removeIndex("MyContactCount");
}

void TestJsonDb::reduceUpdate()
{
    QJsonArray objects(readJsonFile(":/daemon/json/reduce-data.json").toArray());

    QJsonArray toDelete;
    QHash<QString, int> firstNameCount;
    QHash<QString, int> lastNameCount;
    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        firstNameCount[object.value("firstName").toString()]++;
        lastNameCount[object.value("lastName").toString()]++;
        toDelete.append(object);
    }

    objects = readJsonFile(":/daemon/json/reduce.json").toArray();
    JsonDbObject reduce;
    JsonDbObject schema;
    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.value(JsonDbString::kTypeStr).toString() == "Reduce")
            reduce = object;
        else if (object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kSchemaTypeStr)
            schema = object;
        else
            toDelete.append(object);
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    JsonDbObjectList data = queryResult.data;
    QCOMPARE(data.size(), firstNameCount.keys().count());

    for (int ii = 0; ii < data.size(); ii++)
        QCOMPARE((int)data.at(ii).value("value").toObject().value("count").toDouble(),
                 firstNameCount[data.at(ii).value("key").toString()]);

    reduce.insert("sourceKeyName", QString("lastName"));
    QJsonObject result = mJsonDb->update(mOwner, reduce);

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    data = queryResult.data;

    QCOMPARE(queryResult.data.size(), lastNameCount.keys().count());

    data = queryResult.data;
    for (int ii = 0; ii < data.size(); ii++)
        QCOMPARE((int)data.at(ii).value("value").toObject().value("count").toDouble(),
                 lastNameCount[data.at(ii).value("key").toString()]);

    verifyGoodResult(mJsonDb->remove(mOwner, reduce));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii).toObject(),
                                         mJsonDb->mSystemPartitionName));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
    mJsonDb->removeIndex("MyContactCount");
}

void TestJsonDb::reduceDuplicate()
{
    QJsonArray objects(readJsonFile(":/daemon/json/reduce-data.json").toArray());

    JsonDbObjectList toDelete;
    QHash<QString, int> firstNameCount;
    QHash<QString, int> lastNameCount;
    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        firstNameCount[object.value("firstName").toString()]++;
        lastNameCount[object.value("lastName").toString()]++;
        toDelete.append(object);
    }

    objects = readJsonFile(":/daemon/json/reduce.json").toArray();
    JsonDbObject reduce;
    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());
        if (object.value(JsonDbString::kTypeStr).toString() == "Reduce")
            object.insert("targetKeyName", QString("key"));
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.value(JsonDbString::kTypeStr).toString() == "Reduce")
            reduce = object;
        else
            toDelete.append(object);
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), firstNameCount.keys().count());

    JsonDbObjectList data = queryResult.data;
    for (int ii = 0; ii < data.size(); ii++) {
        JsonDbObject object(data.at(ii));
        QCOMPARE((int)object.value("count").toDouble(),
                 firstNameCount[object.value("key").toString()]);
    }

    JsonDbObject reduce2;
    reduce2.insert(JsonDbString::kTypeStr, reduce.value(JsonDbString::kTypeStr).toString());
    reduce2.insert("targetType", reduce.value("targetType").toString());
    reduce2.insert("sourceType", reduce.value("sourceType").toString());
    reduce2.insert("sourceKeyName", QString("lastName"));
    reduce2.insert("targetKeyName", QString("key"));
    reduce2.insert("targetValueName", QString("count"));
    reduce2.insert("add", reduce.value("add").toString());
    reduce2.insert("subtract", reduce.value("subtract").toString());
    QJsonObject result = mJsonDb->create(mOwner, reduce2);
    verifyGoodResult(result);

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), lastNameCount.keys().count() + firstNameCount.keys().count());

    data = queryResult.data;
    for (int ii = 0; ii < data.size(); ii++) {
        JsonDbObject object = data.at(ii);
        QVERIFY(object.value("count").toDouble() == firstNameCount[object.value("key").toString()]
                || object.value("count").toDouble() == lastNameCount[object.value("key").toString()]);
    }

    verifyGoodResult(mJsonDb->remove(mOwner, reduce));
    verifyGoodResult(mJsonDb->remove(mOwner, reduce2));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii)));
    mJsonDb->removeIndex("MyContactCount");
}

void TestJsonDb::reduceFunctionError()
{
    JsonDbObject schema;
    QString viewTypeStr("ReduceFunctionErrorView");
    schema.insert(JsonDbString::kTypeStr, QString("_schemaType"));
    schema.insert(JsonDbString::kNameStr, viewTypeStr);
    QJsonObject schemaSub;
    schemaSub.insert("type", QString("object"));
    schemaSub.insert("extends", QString("View"));
    schema.insert("schema", schemaSub);
    QJsonObject schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    JsonDbObject reduceDefinition;
    reduceDefinition.insert(JsonDbString::kTypeStr, QString("Reduce"));
    reduceDefinition.insert("targetType", viewTypeStr);
    reduceDefinition.insert("sourceType", QString("Contact"));
    reduceDefinition.insert("sourceKeyName", QString("phoneNumber"));
    reduceDefinition.insert("add", QString("function add (k, z, c) { invalidobject.test(); }")); // invalid add function
    reduceDefinition.insert("subtract", QString("function subtract (k, z, c) { }"));
    QJsonObject defRes = mJsonDb->create(mOwner, reduceDefinition);
    verifyGoodResult(defRes);

    JsonDbObject contact;
    contact.insert(JsonDbString::kTypeStr, QString("Contact"));
    contact.insert("phoneNumber", QString("+1234567890"));
    QJsonObject res = mJsonDb->create(mOwner, contact);
    verifyGoodResult(res);

    mJsonDb->updateView(viewTypeStr);
    GetObjectsResult getObjects = mJsonDb->getObjects("_uuid", defRes.value("result").toObject().value("_uuid").toString(), JsonDbString::kReduceTypeStr);
    reduceDefinition = getObjects.data.at(0);
    QVERIFY(!reduceDefinition.value(JsonDbString::kActiveStr).toBool());
    QVERIFY(reduceDefinition.value(JsonDbString::kErrorStr).toString().contains("invalidobject"));

    verifyGoodResult(mJsonDb->remove(mOwner, reduceDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::reduceSchemaViolation()
{
    jsondbSettings->setValidateSchemas(true);

    QJsonArray objects(readJsonFile(":/daemon/json/map-reduce-schema.json").toArray());

    QJsonArray toDelete;
    JsonDbObject map;
    JsonDbObject reduce;
    QString workingAdd;

    for (int ii = 0; ii < objects.size(); ii++) {
        JsonDbObject object(objects.at(ii).toObject());

        if (object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kReduceTypeStr) {
            // use the broken add function that creates schema violations
            workingAdd = object.value("add").toString();
            object.insert("add", object.value("brokenAdd").toString());
        }

        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);

        if (object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kMapTypeStr) {
            map = object;
        } else if (object.value(JsonDbString::kTypeStr).toString() == JsonDbString::kReduceTypeStr) {
            reduce = object;
        } else if (object.value(JsonDbString::kTypeStr).toString() != JsonDbString::kMapTypeStr &&
                   object.value(JsonDbString::kTypeStr).toString() != JsonDbString::kMapTypeStr)
            toDelete.append(object);

    }

    mJsonDb->updateView(reduce.value("targetType").toString());

    GetObjectsResult getObjects = mJsonDb->getObjects("_uuid", reduce.value(JsonDbString::kUuidStr).toString());
    QCOMPARE(getObjects.data.size(), 1);
    JsonDbObject reduceDefinition = getObjects.data.at(0);
    QVERIFY(reduceDefinition.contains(JsonDbString::kActiveStr) && !reduceDefinition.value(JsonDbString::kActiveStr).toBool());
    QVERIFY(reduceDefinition.value(JsonDbString::kErrorStr).toString().contains("Schema"));


    getObjects = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("PhoneCount"));
    QCOMPARE(getObjects.data.size(), 0);

    // fix the add function
    reduce.insert("add", workingAdd);
    reduce.insert(JsonDbString::kActiveStr, true);
    reduce.insert(JsonDbString::kErrorStr, QJsonValue(QJsonValue::Null));

    verifyGoodResult(mJsonDb->update(mOwner, reduce));

    mJsonDb->updateView(reduce.value("targetType").toString());

    getObjects = mJsonDb->getObjects("_uuid", reduce.value(JsonDbString::kUuidStr).toString());
    reduceDefinition = getObjects.data.at(0);
    QVERIFY(!reduceDefinition.contains(JsonDbString::kActiveStr)|| reduceDefinition.value(JsonDbString::kActiveStr).toBool());
    QVERIFY(!reduceDefinition.contains(JsonDbString::kErrorStr) || reduceDefinition.value(JsonDbString::kErrorStr).toString().isEmpty());

    getObjects = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("PhoneCount"));
    QCOMPARE(getObjects.data.size(), 4);

    verifyGoodResult(mJsonDb->remove(mOwner, reduce));
    verifyGoodResult(mJsonDb->remove(mOwner, map));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii).toObject()));

    jsondbSettings->setValidateSchemas(false);
}

void TestJsonDb::reduceSubObjectProp()
{
    QJsonArray objects(readJsonFile(":/daemon/json/reduce-subprop.json").toArray());

    QJsonArray toDelete;
    JsonDbObject reduce;

    QHash<QString, int> firstNameCount;
    for (int i = 0; i < objects.size(); ++i) {
        JsonDbObject object(objects.at(i).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);

        if (object.value(JsonDbString::kTypeStr).toString() == "Reduce") {
            reduce = object;
        } else {
            if (object.value(JsonDbString::kTypeStr).toString() == "Contact")
                firstNameCount[object.value("name").toObject().value("firstName").toString()]++;
            toDelete.append(object);
        }
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"NameCount\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), firstNameCount.keys().count());

    JsonDbObjectList data = queryResult.data;
    for (int i = 0; i < data.size(); ++i) {
        JsonDbObject object(data.at(i));
        QCOMPARE((int)object.value("value").toObject().value("count").toDouble(), firstNameCount[object.value("key").toString()]);
    }

    verifyGoodResult(mJsonDb->remove(mOwner, reduce));
    for (int i = 0; i < toDelete.size(); ++i) {
        JsonDbObject object(toDelete.at(i).toObject());
        verifyGoodResult(mJsonDb->remove(mOwner, object));
    }
    mJsonDb->removeIndex("NameCount");
}

void TestJsonDb::reduceArray()
{
    QJsonArray objects(readJsonFile(":/daemon/json/reduce-array.json").toArray());
    QJsonArray toDelete;

    JsonDbObject human;

    for (int i = 0; i < objects.size(); i++) {
        JsonDbObject object(objects.at(i).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        object.insert(JsonDbString::kUuidStr, result.value("result").toObject().value(JsonDbString::kUuidStr).toString());
        toDelete.append(object);

        if (object.value("firstName").toString() == "Julio")
            human = object;
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ArrayView\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);

    JsonDbObjectList results = queryResult.data;
    QCOMPARE(results.size(), 2);

    for (int i = 0; i < results.size(); i++) {
        QJsonArray firstNames = results.at(i).value("value").toObject().value("firstNames").toArray();
        QCOMPARE(firstNames.size(), 2);

        for (int j = 0; j < firstNames.size(); j++)
            QVERIFY(!firstNames.at(j).toString().isEmpty());
    }

    human.insert("lastName", QString("Johnson"));
    verifyGoodResult(mJsonDb->update(mOwner, human));

    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ArrayView\"][?key=\"Jones\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);

    results = queryResult.data;
    QCOMPARE(results.at(0).value("value").toObject().value("firstNames").toArray().size(), 1);

    for (int i = toDelete.size() - 1; i >= 0; i--)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(i).toObject()));
    mJsonDb->removeIndex("ArrayView");
}

void TestJsonDb::changesSinceCreate()
{
    QJsonObject csReq;
    csReq.insert("stateNumber", 0);
    QJsonObject csRes = mJsonDb->changesSince(mOwner, csReq);
    verifyGoodResult(csRes);
    int state = csRes.value("result").toObject().value("currentStateNumber").toDouble();
    QVERIFY(state >= 0);

    JsonDbObject toCreate;
    toCreate.insert("_type", QString("TestContact"));
    toCreate.insert("firstName", QString("John"));
    toCreate.insert("lastName", QString("Doe"));

    QJsonObject crRes = mJsonDb->create(mOwner, toCreate);
    verifyGoodResult(crRes);

    csReq.insert("stateNumber", state);
    csRes = mJsonDb->changesSince(mOwner, csReq);
    verifyGoodResult(csRes);

    QVERIFY(csRes.value("result").toObject().value("currentStateNumber").toDouble() > state);
    QCOMPARE(csRes.value("result").toObject().value("count").toDouble(), (double)1);

    QJsonObject after = csRes.value("result").toObject().value("changes").toArray().at(0).toObject().value("after").toObject();
    QCOMPARE(after.value("_type").toString(), toCreate.value("_type").toString());
    QCOMPARE(after.value("firstName").toString(), toCreate.value("firstName").toString());
    QCOMPARE(after.value("lastName").toString(), toCreate.value("lastName").toString());
}

void TestJsonDb::addIndex()
{
    addIndex(QLatin1String("subject"));

    JsonDbObject indexObject;
    indexObject.insert(JsonDbString::kTypeStr, QLatin1String("Index"));
    indexObject.insert("propertyName", QLatin1String("predicate"));
    indexObject.insert("propertyType", QLatin1String("string"));

    QJsonObject result = mJsonDb->create(mOwner, indexObject);
    verifyGoodResult(result);
    QVERIFY(mJsonDb->findPartition(mJsonDb->mSystemPartitionName)->findObjectTable(JsonDbString::kSchemaTypeStr)->indexSpec("predicate") != 0);
    mJsonDb->remove(mOwner, indexObject);
}

void TestJsonDb::addSchema()
{
    JsonDbObject s;
    addSchema("contact", s);
    verifyGoodResult(mJsonDb->remove(mOwner, s));
}

void TestJsonDb::duplicateSchema()
{
    QJsonValue schema = readJsonFile(":/daemon/schemas/address.json");
    JsonDbObject schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", QLatin1String("Address"));
    schemaObject.insert("schema", schema);

    QJsonObject result = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(result);

    JsonDbObject schemaObject2;
    schemaObject2.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject2.insert("name", QLatin1String("Address"));
    schemaObject2.insert("schema", schema);
    result = mJsonDb->create(mOwner, schemaObject2);
    verifyErrorResult(result);

    result = mJsonDb->remove(mOwner, schemaObject);
    verifyGoodResult(result);
}

void TestJsonDb::removeSchema()
{
    QJsonValue schema = readJsonFile(":/daemon/schemas/address.json");
    JsonDbObject schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", QLatin1String("Address"));
    schemaObject.insert("schema", schema);

    QJsonObject result = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(result);

    JsonDbObject address;
    address.insert(JsonDbString::kTypeStr, QLatin1String("Address"));
    address.insert("street", QLatin1String("Main Street"));
    address.insert("number", 1);

    result = mJsonDb->create(mOwner, address);
    verifyGoodResult(result);

    result = mJsonDb->remove(mOwner, schemaObject);
    verifyErrorResult(result);

    result = mJsonDb->remove(mOwner, address);
    verifyGoodResult(result);

    result = mJsonDb->remove(mOwner, schemaObject);
    verifyGoodResult(result);
}

void TestJsonDb::removeViewSchema()
{
    QJsonArray objects = readJsonFile(":/daemon/json/reduce.json").toArray();
    JsonDbObject schema;
    JsonDbObject reduce;
    for (int i = 0; i < objects.size(); ++i) {
        JsonDbObject object(objects.at(i).toObject());
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.value(JsonDbString::kTypeStr).toString() == "Reduce")
            reduce = object;
        else
            schema = object;
    }

    QJsonObject result = mJsonDb->remove(mOwner, schema);
    verifyErrorResult(result);

    result = mJsonDb->remove(mOwner, reduce);
    verifyGoodResult(result);

    result = mJsonDb->remove(mOwner, schema);
    verifyGoodResult(result);
}

void TestJsonDb::updateSchema()
{
    QJsonObject schema = readJsonFile(":/daemon/schemas/address.json").toObject();
    QVERIFY(!schema.isEmpty());
    JsonDbObject schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", QLatin1String("Address"));
    schemaObject.insert("schema", schema);

    QJsonObject schemaResult = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(schemaResult);

    JsonDbObject address;
    address.insert(JsonDbString::kTypeStr, QLatin1String("Address"));
    address.insert("street", QLatin1String("Main Street"));
    address.insert("number", 1);

    QJsonObject result = mJsonDb->create(mOwner, address);
    verifyGoodResult(result);

    schema.insert("streetNumber", schema.value("number").toObject());
    schemaObject.insert("schema", schema);
    result = mJsonDb->update(mOwner, schemaObject);
    verifyErrorResult(result);

    result = mJsonDb->remove(mOwner, address);
    verifyGoodResult(result);

    result = mJsonDb->update(mOwner, schemaObject);
    verifyGoodResult(result);

    result = mJsonDb->remove(mOwner, schemaObject);
    verifyGoodResult(result);
}

void TestJsonDb::unindexedFind()
{
    JsonDbObject item;
    item.insert("_type", QLatin1String("unindexedFind"));
    item.insert("subject", QString("Programming Languages"));
    item.insert("bar", 10);
    QJsonObject createResult = mJsonDb->create(mOwner, item);
    verifyGoodResult(createResult);

    QJsonObject request;
    // need to pass a string value for bar in the query because auto-
    // generated indexes are always of type "string"
    request.insert("query", QString("[?bar=\"10\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, request);
    int extraneous = 0;
    JsonDbObjectList data = queryResult.data;
    for (int i = 0; i < data.size(); ++i) {
        QJsonObject map = data.at(i);
        if (!map.contains("bar") || (map.value("bar").toDouble() != 10)) {
            extraneous++;
        }
    }

    verifyGoodQueryResult(queryResult);
    QVERIFY((queryResult.data.size() >= 1) && !extraneous);
    mJsonDb->removeIndex("bar");
    mJsonDb->remove(mOwner, item);
}

void TestJsonDb::find1()
{
    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QString("Find1Type"));
    item.insert("find1", QString("Foobar!"));
    mJsonDb->create(mOwner, item);

    QJsonObject query;
    query.insert("query", QString(".*"));
    JsonDbQueryResult queryResult= mJsonDb->find(mOwner, query);

    verifyGoodQueryResult(queryResult);
    QVERIFY(queryResult.data.size() >= 1);
}

void TestJsonDb::find2()
{
    addIndex(QLatin1String("name"));
    addIndex(QLatin1String("_type"));

    JsonDbObjectList toDelete;

    JsonDbObject item;
    item.insert("name", QString("Wilma"));
    item.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    toDelete.append(item);

    item = JsonDbObject();
    item.insert("name", QString("Betty"));
    item.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    toDelete.append(item);

    int extraneous;

    QJsonObject query;
    query.insert("query", QString("[?name=\"Wilma\"][?_type=%type]"));
    QJsonObject bindings;
    bindings.insert("type", QString(__FUNCTION__));
    query.insert("bindings", bindings);
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), qint32(1));

    extraneous = 0;
    foreach (JsonDbObject item, queryResult.data) {
        if (!item.contains("name") || (item.value("name").toString() != QLatin1String("Wilma")))
            extraneous++;
    }
    verifyGoodQueryResult(queryResult);
    QVERIFY(!extraneous);

    query.insert("query", QString("[?_type=%type]"));
    queryResult = mJsonDb->find(mOwner, query);

    extraneous = 0;
    foreach (QVariant item, result.value("data").toArray().toVariantList()) {
        QJsonObject map = QJsonObject(QJsonObject::fromVariantMap(item.toMap()));
        if (!map.contains(JsonDbString::kTypeStr) || (map.value(JsonDbString::kTypeStr).toString() != QString(__FUNCTION__)))
            extraneous++;
    }
    verifyGoodQueryResult(queryResult);
    QVERIFY(queryResult.data.size() >= 1);
    QVERIFY(!extraneous);

    query.insert("query", QString("[?name=\"Wilma\"][?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg(__FUNCTION__));
    queryResult = mJsonDb->find(mOwner, query);

    extraneous = 0;
    for (int i = 0; i < queryResult.data.size(); i++) {
        QJsonObject map = queryResult.data.at(i);
        if (!map.contains("name")
                || (map.value("name").toString() != QString("Wilma"))
                || !map.contains(JsonDbString::kTypeStr)
                || (map.value(JsonDbString::kTypeStr).toString() != QString(__FUNCTION__))
                )
            extraneous++;
    }

    verifyGoodQueryResult(queryResult);
    QVERIFY(queryResult.data.size() >= 1);
    QVERIFY(!extraneous);

    for (int ii = 0; ii < toDelete.size(); ii++) {
        mJsonDb->remove(mOwner, toDelete.at(ii));
    }
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

void TestJsonDb::findLikeRegexp_data()
{
    QTest::addColumn<QString>("pattern");
    QTest::addColumn<QString>("modifiers");
    QTest::newRow("/de.*/") << "de.*" << "";
    QTest::newRow("/.*a.*/") << ".*a.*" << "";
    QTest::newRow("/de*/wi") << "de*" << "wi";
    QTest::newRow("/*a*/w") << "*a*" << "w";
    QTest::newRow("/de*/wi") << "de*" << "wi";
    QTest::newRow("/*a*/wi") << "*a*" << "wi";
    QTest::newRow("/*a*/wi") << "*a*" << "wi";
    QTest::newRow("/*a/wi") << "*a" << "wi";
    QTest::newRow("/a*/wi") << "a*" << "wi";
    QTest::newRow("/c*/wi") << "c*" << "wi";
    QTest::newRow("/c*/wi") << "c*" << "wi";
    QTest::newRow("/*a*/w") << "*a*" << "w";
    QTest::newRow("/*a/w") << "*a" << "w";
    QTest::newRow("/a*/w") << "a*" << "w";
    QTest::newRow("/a+/w") << "a+" << "w";
    QTest::newRow("/c*/w") << "c*" << "w";
    QTest::newRow("/c*/w") << "c*" << "w";
    QTest::newRow("/.*foo.*\\/.*/i") << ".*foo.*\\/.*" << "i";
    QTest::newRow("|.*foo.*/.*|i") << ".*foo.*\\/.*" << "i";

    foreach (QString s, strings) {
        JsonDbObject item;
        item.insert(JsonDbString::kTypeStr, QString("FindLikeRegexpData"));
        item.insert(__FUNCTION__, QString("Find Me!"));
        item.insert("key", s);
        mJsonDb->create(mOwner, item);
    }
}

void TestJsonDb::findLikeRegexp()
{
    QFETCH(QString, pattern);
    QFETCH(QString, modifiers);
    QString q = QString("[?_type=\"%1\"][?key =~ \"/%2/%3\"]").arg("FindLikeRegexpData").arg(pattern).arg(modifiers);
    Qt::CaseSensitivity cs = (modifiers.contains("i") ? Qt::CaseInsensitive : Qt::CaseSensitive);
    QRegExp::PatternSyntax ps = (modifiers.contains("w") ? QRegExp::Wildcard : QRegExp::RegExp);
    QRegExp re(pattern, cs, ps);
    QStringList expectedMatches;
    foreach (QString s, strings) {
        if (re.exactMatch(s))
            expectedMatches << s;
    }
    expectedMatches.sort();

    QJsonObject query;
    query.insert("query", q);
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    int length = queryResult.data.size();
    verifyGoodQueryResult(queryResult);

    QStringList matches;
    foreach (JsonDbObject v, queryResult.data) {
        QVariantMap m = v.toVariantMap();
        matches << m.value("key").toString();
    }
    matches.sort();
    if (matches != expectedMatches) {
        qDebug() << "query" << q;
        qDebug() << "expectedMatches" << expectedMatches;
        qDebug() << "matches" << matches;
    }
    QCOMPARE(matches, expectedMatches);
    QCOMPARE(length, expectedMatches.size());
}

void TestJsonDb::findInContains()
{
    QList<QStringList> stringLists;
    stringLists << (QStringList() << "fred" << "barney");
    stringLists << (QStringList() << "wilma" << "betty");
    QVariantList intLists;
    intLists << QVariant(QVariantList() << 1 << 22);
    intLists << QVariant(QVariantList() << 42 << 17);

    for (int i = 0; i < qMin(stringLists.size(), intLists.size()); i++) {
        JsonDbObject item;
        item.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
        item.insert(__FUNCTION__, QString("Find Me!"));
        item.insert("stringlist", QJsonValue::fromVariant(stringLists[i]));
        item.insert("intlist", QJsonValue::fromVariant(intLists[i]));
        item.insert("str", QJsonValue::fromVariant(stringLists[i][0]));
        item.insert("i", QJsonValue::fromVariant(intLists[i].toList().at(0)));
        mJsonDb->create(mOwner, item);
    }

    QStringList queries = (QStringList()
                           << "[?stringlist contains \"fred\"]"
                           << "[?intlist contains 22]"
                           << "[?str in [\"fred\", \"barney\"]]"
                           << "[?i in [\"1\", \"22\"]]"
        );

    foreach (QString q, queries) {
        QJsonObject query;
        query.insert("query", q);
        JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);

        verifyGoodQueryResult(queryResult);
    }
    mJsonDb->removeIndex("stringlist");
    mJsonDb->removeIndex("intlist");
    mJsonDb->removeIndex("str");
    mJsonDb->removeIndex("i");
}

void TestJsonDb::findFields()
{
    addIndex(QLatin1String("name"));
    addIndex(QLatin1String("_type"));

    JsonDbObject item;
    item.insert("firstName", QString("Wilma"));
    item.insert("lastName", QString("Flintstone"));
    item.insert(JsonDbString::kTypeStr, kContactStr);
    mJsonDb->create(mOwner, item);

    item = JsonDbObject();
    item.insert("firstName", QString("Betty"));
    item.insert("lastName", QString("Rubble"));
    item.insert(JsonDbString::kTypeStr, kContactStr);
    mJsonDb->create(mOwner, item);

    QJsonObject query, result, map;

    query.insert("query", QString("[?firstName=\"Wilma\"][=firstName]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.values.at(0).toString(), QString("Wilma"));

    query.insert("query", QString("[?firstName=\"Wilma\"][= [firstName,lastName]]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.values.size(), 1);
    QJsonArray data = queryResult.values.at(0).toArray();
    QCOMPARE(data.at(0).toString(), QString("Wilma"));
    QCOMPARE(data.at(1).toString(), QString("Flintstone"));
    mJsonDb->removeIndex(QLatin1String("firstName"));
    //mJsonDb->removeIndex(QLatin1String("name"));
    //mJsonDb->removeIndex(QLatin1String("_type")); //crash here
}

void TestJsonDb::orderedFind1_data()
{
    QTest::addColumn<QString>("order");
    QTest::newRow("asc") << "/";
    QTest::newRow("desc") << "\\";

    addIndex(QLatin1String("orderedFindName"));
    addIndex(QLatin1String("_type"));

    JsonDbObject item1;
    item1.insert("orderedFindName", QString("Wilma"));
    item1.insert(JsonDbString::kTypeStr, QLatin1String("orderedFind1"));
    mJsonDb->create(mOwner, item1);

    JsonDbObject item2;
    item2.insert("orderedFindName", QString("BamBam"));
    item2.insert(JsonDbString::kTypeStr, QLatin1String("orderedFind1"));
    mJsonDb->create(mOwner, item2);

    JsonDbObject item3;
    item3.insert("orderedFindName", QString("Betty"));
    item3.insert(JsonDbString::kTypeStr, QLatin1String("orderedFind1"));
    mJsonDb->create(mOwner, item3);
}

void TestJsonDb::orderedFind1()
{
    QFETCH(QString, order);

    QJsonObject query;
    query.insert("query", QString("[?_type=\"orderedFind1\"][%3orderedFindName]").arg(order));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QVERIFY(queryResult.data.size() > 0);

    QStringList names;
    JsonDbObjectList data = queryResult.data;
    for (int ii = 0; ii < data.size(); ii++) {
        names.append(data.at(ii).value("orderedFindName").toString());
    }
    QStringList orderedNames = names;
    qSort(orderedNames.begin(), orderedNames.end());
    QStringList disorderedNames = names;
    qSort(disorderedNames.begin(), disorderedNames.end(), qGreater<QString>());
    if (order == "/") {
        QVERIFY(names == orderedNames);
        QVERIFY(names != disorderedNames);
    } else {
        QVERIFY(names != orderedNames);
        QVERIFY(names == disorderedNames);
    }
    mJsonDb->removeIndex(QLatin1String("orderedFind1"));
    mJsonDb->removeIndex(QLatin1String("orderedFindName"));
    mJsonDb->removeIndex(QLatin1String("_type"));
}

void TestJsonDb::orderedFind2_data()
{
    QTest::addColumn<QString>("order");
    QTest::addColumn<QString>("field");
    QTest::newRow("asc uuid")   << "/"  << "_uuid";
    QTest::newRow("asc foobar")   << "/"  << "foobar";
    QTest::newRow("desc uuid")  << "\\" << "_uuid";
    QTest::newRow("desc foobar")  << "\\" << "foobar";

    for (char prefix = 'z'; prefix >= 'a'; prefix--) {
        JsonDbObject item;
        item.insert(JsonDbString::kTypeStr, QLatin1String("orderedFind2"));
        item.insert(QLatin1String("foobar"), QString("%1_orderedFind2").arg(prefix));
        QJsonObject r = mJsonDb->create(mOwner, item);
    }
}

void TestJsonDb::orderedFind2()
{
    QFETCH(QString, order);
    QFETCH(QString, field);

    QJsonObject query;
    query.insert("query", QString("[?_type=\"orderedFind2\"][%1%2]").arg(order).arg(field));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QVERIFY(queryResult.data.size() > 0);

    QStringList names;
    JsonDbObjectList data = queryResult.data;
    for (int ii = 0; ii < data.size(); ii++) {
        names.append(data.at(ii).value(field).toString());
    }
    QStringList orderedNames = names;
    qSort(orderedNames.begin(), orderedNames.end());
    QStringList disorderedNames = names;
    qSort(disorderedNames.begin(), disorderedNames.end(), qGreater<QString>());
    if (order == "/") {
        if (!(names == orderedNames)
                || !(names != disorderedNames))
            mJsonDb->findPartition(mJsonDb->mSystemPartitionName)->checkIndex(field);
        QVERIFY(names == orderedNames);
        QVERIFY(names != disorderedNames);
    } else {
        if (!(names != orderedNames)
                || !(names == disorderedNames))
            mJsonDb->findPartition(mJsonDb->mSystemPartitionName)->checkIndex(field);
        QVERIFY(names != orderedNames);
        QVERIFY(names == disorderedNames);
    }
}

void TestJsonDb::wildcardIndex()
{
    addIndex("telephoneNumbers.*.number");
    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, kContactStr);
    item.insert("name", QString("BamBam"));

    QJsonObject mobileNumber;
    QString mobileNumberString = "+15515512323";
    mobileNumber.insert("type", QString("mobile"));
    mobileNumber.insert("number", mobileNumberString);
    QJsonArray telephoneNumbers;
    telephoneNumbers.append(mobileNumber);
    item.insert("telephoneNumbers", telephoneNumbers);

    mJsonDb->create(mOwner, item);

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?telephoneNumbers.*.number=\"%1\"]").arg(mobileNumberString));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);

    query.insert(JsonDbString::kQueryStr, QString("[?%1=\"%2\"][= .telephoneNumbers[*].number]").arg(JsonDbString::kTypeStr).arg(kContactStr));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    mJsonDb->removeIndex("telephoneNumbers.*.number");
}

void TestJsonDb::uuidJoin()
{
    addIndex("name");
    addIndex("thumbnailUuid");
    addIndex("url");
    QString thumbnailUrl = "file:thumbnail.png";
    JsonDbObject thumbnail;
    thumbnail.insert(JsonDbString::kTypeStr, QString("com.example.thumbnail"));
    thumbnail.insert("url", thumbnailUrl);
    mJsonDb->create(mOwner, thumbnail);
    QString thumbnailUuid = thumbnail.value("_uuid").toString();

    JsonDbObject item;
    item.insert("_type", QString(__FUNCTION__));
    item.insert("name", QString("Pebbles"));
    item.insert(JsonDbString::kTypeStr, kContactStr);
    mJsonDb->create(mOwner, item);

    JsonDbObject item2;
    item2.insert("_type", QString(__FUNCTION__));
    item2.insert("name", QString("Wilma"));
    item2.insert("thumbnailUuid", thumbnailUuid);
    item2.insert(JsonDbString::kTypeStr, kContactStr);
    mJsonDb->create(mOwner, item2);

    JsonDbObject betty;
    betty.insert("_type", QString(__FUNCTION__));
    betty.insert("name", QString("Betty"));
    betty.insert("thumbnailUuid", thumbnailUuid);
    betty.insert(JsonDbString::kTypeStr, kContactStr);
    QJsonObject r = mJsonDb->create(mOwner, betty);
    QString bettyUuid = r.value("result").toObject().value("_uuid").toString();

    JsonDbObject bettyRef;
    bettyRef.insert("_type", QString(__FUNCTION__));
    bettyRef.insert("bettyUuid", bettyUuid);
    bettyRef.insert("thumbnailUuid", thumbnailUuid);
    r = mJsonDb->create(mOwner, bettyRef);

    QJsonObject query, result;
    query.insert(JsonDbString::kQueryStr, QString("[?thumbnailUuid->url=\"%1\"]").arg(thumbnailUrl));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QVERIFY(queryResult.data.size() > 0);
    QCOMPARE(queryResult.data.at(0).value("thumbnailUuid").toString(), thumbnailUuid);

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"%1\"][?thumbnailUuid->url=\"%2\"]").arg(__FUNCTION__).arg(thumbnailUrl));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QVERIFY(queryResult.data.size() > 0);
    QCOMPARE(queryResult.data.at(0).value("thumbnailUuid").toString(), thumbnailUuid);

    QString queryString = QString("[?name=\"Betty\"][= [ name, thumbnailUuid->url ]]");
    query.insert(JsonDbString::kQueryStr, queryString);
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.values.at(0).toArray().at(1).toString(), thumbnailUrl);

    queryString = QString("[?_type=\"%1\"][= [ name, thumbnailUuid->url ]]").arg(__FUNCTION__);
    query.insert(JsonDbString::kQueryStr, queryString);
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QJsonArray values = queryResult.values;
    for (int ii = 0; ii < values.size(); ii++) {
        QJsonArray item = values.at(ii).toArray();
        QString name = item.at(0).toString();
        QString url = item.at(1).toString();
        if (name == "Pebbles")
            QVERIFY(url.isEmpty());
        else
            QCOMPARE(url, thumbnailUrl);
    }

    queryString = QString("[?_type=\"%1\"][= { name: name, url: thumbnailUuid->url } ]").arg(__FUNCTION__);
    query.insert(JsonDbString::kQueryStr, queryString);
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);

    QList<JsonDbObject> data = queryResult.data;
    for (int ii = 0; ii < data.size(); ii++) {
        QJsonObject item = data.at(ii);
        QString name = item.value("name").toString();
        QString url = item.value("url").toString();
        if (name == "Pebbles")
            QVERIFY(url.isEmpty());
        else
            QCOMPARE(url, thumbnailUrl);
    }

    query.insert(JsonDbString::kQueryStr, QString("[?bettyUuid exists][= bettyUuid->thumbnailUuid]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.values.at(0).toString(),
             thumbnailUuid);

    query.insert(JsonDbString::kQueryStr, QString("[?bettyUuid exists][= bettyUuid->thumbnailUuid->url]"));
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.values.at(0).toString(), thumbnail.value("url").toString());
    mJsonDb->removeIndex("name");
    mJsonDb->removeIndex("thumbnailUuid");
    mJsonDb->removeIndex("url");
    mJsonDb->removeIndex("bettyUuid");
}

void TestJsonDb::testNotify1()
{
    QString query = QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg(kContactStr);

    QJsonArray actions;
    actions.append(QLatin1String("create"));

    JsonDbObject notification;
    notification.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    notification.insert(QLatin1String("query"), query);
    notification.insert(QLatin1String("actions"), actions);

    QJsonObject result = mJsonDb->create(mOwner, notification);
    QVERIFY(result.contains(JsonDbString::kResultStr));
    QVERIFY(result.value(JsonDbString::kResultStr).toObject().contains(JsonDbString::kUuidStr));
    QString uuid = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kUuidStr).toString();

    connect(mJsonDb, SIGNAL(notified(QString,JsonDbObject,QString)),
            this, SLOT(notified(QString,JsonDbObject,QString)));

    mNotificationsReceived.clear();

    JsonDbObject item;
    item.insert("name", QString("Wilma"));
    item.insert(JsonDbString::kTypeStr, kContactStr);
    mJsonDb->create(mOwner, item);

    QVERIFY(mNotificationsReceived.contains(uuid));

    result = mJsonDb->remove(mOwner, notification);
    verifyGoodResult(result);
}

void TestJsonDb::notified(const QString nid, const JsonDbObject &o, const QString action)
{
    Q_UNUSED(o);
    Q_UNUSED(action);
    mNotificationsReceived.append(nid);
}

void TestJsonDb::orQuery_data()
{
    QTest::addColumn<QString>("field1");
    QTest::addColumn<QString>("value1");
    QTest::addColumn<QString>("field2");
    QTest::addColumn<QString>("value2");
    QTest::addColumn<QString>("ordering");
    QTest::newRow("[? key1 = \"red\" | key1 = \"green\" ]")
        << "key1" << "red" << "key1" << "green" << "";
    QTest::newRow("[? key1 = \"red\" | key2 = \"bar\" ]")
        << "key1" << "red" << "key2" << "bar" << "";
    QTest::newRow("[? key1 = \"red\" | key1 = \"green\"][/key1]")
        << "key1" << "red" << "key1" << "green" << "[/key1]";
    QTest::newRow("[? key1 = \"red\" | key2 = \"bar\" ][/key1]")
        << "key1" << "red" << "key2" << "bar" << "[/key1]";
    QTest::newRow("[? key1 = \"red\" | key1 = \"green\"][/key2]")
        << "key1" << "red" << "key1" << "green" << "[/key2]";
    QTest::newRow("[? key1 = \"red\" | key2 = \"bar\" ][/key2]")
        << "key1" << "red" << "key2" << "bar" << "[/key2]";
    QTest::newRow("[? _type = \"RedType\" | _type = \"BarType\" ]")
        << "_type" << "RedType" << "_type" << "BarType" << "";

    addIndex(QLatin1String("key1"));

    QStringList keys1 = QStringList() << "red" << "green" << "blue";
    QStringList keys2 = QStringList() << "foo" << "bar" << "baz";
    foreach (QString key1, keys1) {
        foreach (QString key2, keys2) {
            JsonDbObject item;
            item.insert(JsonDbString::kTypeStr, QString("OrQueryTestType"));
            item.insert("key1", key1);
            item.insert("key2", key2);
            mJsonDb->create(mOwner, item);

            key1[0] = key1[0].toUpper();
            key2[0] = key2[0].toUpper();
            item = JsonDbObject();
            item.insert(JsonDbString::kTypeStr, QString("%1Type").arg(key1));
            item.insert("notUsed1", key1);
            item.insert("notUsed2", key2);
            mJsonDb->create(mOwner, item);

            item = JsonDbObject();
            item.insert(JsonDbString::kTypeStr, QString("%1Type").arg(key2));
            item.insert("notUsed1", key1);
            item.insert("notUsed2", key2);
            mJsonDb->create(mOwner, item);
        }
    }
    mJsonDb->removeIndex(QLatin1String("key1"));
    mJsonDb->removeIndex(QLatin1String("key2"));
}

void TestJsonDb::orQuery()
{
    QFETCH(QString, field1);
    QFETCH(QString, value1);
    QFETCH(QString, field2);
    QFETCH(QString, value2);
    QFETCH(QString, ordering);
    QJsonObject request;
    QString typeQuery = "[?_type=\"OrQueryTestType\"]";
    QString queryString = (QString("%6[? %1 = \"%2\" | %3 = \"%4\" ]%5")
                           .arg(field1).arg(value1)
                           .arg(field2).arg(value2)
                           .arg(ordering).arg(((field1 != "_type") && (field2 != "_type")) ? typeQuery : ""));
    request.insert("query", queryString);
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);
    QList<JsonDbObject> objects = queryResult.data;
    int count = 0;
    for (int ii = 0; ii < objects.size(); ii++) {
        QJsonObject o = objects.at(ii);
        QVERIFY((o.value(field1).toString() == value1) || (o.value(field2).toString() == value2));
        count++;
    }
    QVERIFY(count > 0);
    mJsonDb->removeIndex("key1");
    mJsonDb->removeIndex("key2");
}

void TestJsonDb::findByName()
{
    createContacts();
    int count = mContactList.size();
    if (!count)
        return;

    QJsonObject request;

    //int itemNumber = (int)((double)qrand() * count / RAND_MAX);
    int itemNumber = 0;
    JsonDbObject item(mContactList.at(itemNumber));;
    request.insert("query",
                   QString("[?name=\"%1\"]")
                   .arg(item.value("name").toString()));
    if (!item.contains("name"))
        qDebug() << "no name in item" << item;
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);
}

void TestJsonDb::findEQ()
{
    createContacts();
    int count = mContactList.size();
    if (!count)
        return;

    QJsonObject request;

    int itemNumber = (int)((double)qrand() * count / RAND_MAX);
    JsonDbObject item(mContactList.at(itemNumber));
    request.insert("query",
                   QString("[?name.first=\"%1\"][?name.last=\"%2\"][?_type=\"contact\"]")
                   .arg(JsonDb::propertyLookup(item, "name.first").toString())
                   .arg(JsonDb::propertyLookup(item, "name.last").toString()));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 1);
    mJsonDb->removeIndex("name.first");
    mJsonDb->removeIndex("name.last");
}

void TestJsonDb::find10()
{
    createContacts();
    int count = mContactList.size();
    if (!count)
        return;

    QJsonObject request;
    QJsonObject result;

    int itemNumber = count / 2;
    JsonDbObject item(mContactList.at(itemNumber));
    request.insert("limit", 10);
    request.insert("query",
                   QString("[?name.first<=\"%1\"][?_type=\"contact\"]")
                   .arg(JsonDb::propertyLookup(item, "name.first").toString()));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 10);
    mJsonDb->removeIndex("name.first");
    mJsonDb->removeIndex("contact");
}

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

void TestJsonDb::testPrimaryKey()
{
    QJsonArray objects = readJsonFile(":/daemon/json/pk-capability.json").toArray();
    JsonDbObject schema = objects.at(0).toObject();
    JsonDbObject capability = objects.at(1).toObject();
    JsonDbObject replay(capability);

    QJsonObject result0 = mJsonDb->create(mOwner, schema);
    verifyGoodResult(result0);
    QJsonObject result1 = mJsonDb->create(mOwner, capability);
    verifyGoodResult(result1);

    QJsonObject result2 = mJsonDb->create(mOwner, replay);
    verifyGoodResult(result2);

    if (jsondbSettings->verbose())
        qDebug() << 1 << result1;
    if (jsondbSettings->verbose())
        qDebug() << 2 << result2;

    QCOMPARE(result1.value("result").toObject().value("_uuid"),
             result2.value("result").toObject().value("_uuid"));
    QCOMPARE(result1.value("result").toObject().value("_version"),
             result2.value("result").toObject().value("_version"));
    QCOMPARE(result1.value("result").toObject().value("count"),
             result2.value("result").toObject().value("count"));
}

void TestJsonDb::testStoredProcedure()
{
    JsonDbObject notification;
    notification.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    QString query = QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg(__FUNCTION__);
    QVariantList actions;
    actions.append(QLatin1String("create"));
    notification.insert(JsonDbString::kQueryStr, query);
    notification.insert(JsonDbString::kActionsStr, QJsonValue::fromVariant(actions));
    notification.insert("script", QLatin1String("function foo (v) { return \"hello world\";}"));
    QJsonValue result = mJsonDb->create(mOwner, notification);


    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
    result = mJsonDb->create(mOwner, item);

    notification.insert("script", QString("function foo (v) { return jsondb.find({'query':'[?_type=\"%1\"]'}); }").arg(__FUNCTION__));
    result = mJsonDb->update(mOwner, notification);

    JsonDbObject item2;
    item2.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
    result = mJsonDb->create(mOwner, item2);
}

void TestJsonDb::startsWith()
{
    addIndex(QLatin1String("name"));

    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("startsWithTest"));
    item.insert("name", QLatin1String("Wilma"));
    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("startsWithTest"));
    item.insert("name", QLatin1String("Betty"));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("startsWithTest"));
    item.insert("name", QLatin1String("Bettina"));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("startsWithTest"));
    item.insert("name", QLatin1String("Benny"));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QJsonObject query;
    query.insert("query", QString("[?_type=\"startsWithTest\"][?name startsWith \"Zelda\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 0);

    query = QJsonObject();
    query.insert("query", QString("[?_type=\"startsWithTest\"][?name startsWith \"Wilma\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);

    query = QJsonObject();
    query.insert("query", QString("[?_type=\"startsWithTest\"][?name startsWith \"B\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 3);

    query = QJsonObject();
    query.insert("query", QString("[?_type=\"startsWithTest\"][?name startsWith \"Bett\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 2);

    query = QJsonObject();
    query.insert("query", QString("[?_type startsWith \"startsWith\"][/name]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 4);

    query = QJsonObject();
    query.insert("query", QString("[?_type startsWith \"startsWith\"][= _type ]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.values.size(), 4);
}

void TestJsonDb::comparison()
{
    addIndex(QLatin1String("latitude"), QLatin1String("number"));

    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("comparison"));
    item.insert("latitude", qint32(10));
    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("comparison"));
    item.insert("latitude", qint32(42));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("comparison"));
    item.insert("latitude", qint32(0));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("comparison"));
    item.insert("latitude", qint32(-64));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QJsonObject query;
    query.insert("query", QString("[?_type=\"comparison\"][?latitude > 10]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("latitude").toDouble(), (double)(42));

    query.insert("query", QString("[?_type=\"comparison\"][?latitude >= 10]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 2);
    QCOMPARE(queryResult.data.at(0).value("latitude").toDouble(), (double)(10));
    QCOMPARE(queryResult.data.at(1).value("latitude").toDouble(), (double)(42));

    query.insert("query", QString("[?_type=\"comparison\"][?latitude < 0]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("latitude").toDouble(), (double)(-64));
    mJsonDb->removeIndex(QLatin1String("latitude"));
}

void TestJsonDb::removedObjects()
{
    addIndex(QLatin1String("foo"));
    addIndex(QLatin1String("name"));
    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("removedObjects"));
    item.insert("foo", QLatin1String("bar"));
    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QJsonObject object = result.value("result").toObject();
    object.insert(JsonDbString::kTypeStr, QLatin1String("removedObjects"));

    QJsonObject query;
    query.insert("query", QString("[?_type=\"removedObjects\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("foo").toString(), QLatin1String("bar"));

    // update the object
    item = object;
    item.insert("name", QLatin1String("anna"));
    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);

    query = QJsonObject();
    query.insert("query", QString("[?_type=\"removedObjects\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);
    QVERIFY(!queryResult.data.at(0).contains("foo"));
    QCOMPARE(queryResult.data.at(0).value("name").toString(), QLatin1String("anna"));

    query = QJsonObject();
    query.insert("query", QString("[?_type=\"removedObjects\"][/name]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);
    QVERIFY(!queryResult.data.at(0).contains("foo"));
    QCOMPARE(queryResult.data.at(0).value("name").toString(), QLatin1String("anna"));

    query = QJsonObject();
    query.insert("query", QString("[?_type=\"removedObjects\"][/foo]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 0);

    // remove the object
    result = mJsonDb->remove(mOwner, object);
    verifyGoodResult(result);

    query = QJsonObject();
    query.insert("query", QString("[?_type=\"removedObjects\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 0);

    query = QJsonObject();
    query.insert("query", QString("[?_type=\"removedObjects\"][/foo]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 0);
    mJsonDb->removeIndex(QLatin1String("foo"));
    mJsonDb->removeIndex(QLatin1String("name"));
}

void TestJsonDb::partition()
{
    JsonDbObject map;
    map.insert(QLatin1String("_type"), QLatin1String("Partition"));
    map.insert(QLatin1String("name"), QLatin1String("private"));
    QJsonObject result = mJsonDb->create(mOwner, map);
    verifyGoodResult(result);

    map = JsonDbObject();
    map.insert(QLatin1String("_type"), QLatin1String("partitiontest"));
    map.insert(QLatin1String("foo"), QLatin1String("bar"));
    result = mJsonDb->create(mOwner, map);
    verifyGoodResult(result);

    map = JsonDbObject();
    map.insert(QLatin1String("_type"), QLatin1String("partitiontest"));
    map.insert(QLatin1String("foo"), QLatin1String("asd"));
    result = mJsonDb->create(mOwner, map, "private");
    verifyGoodResult(result);

    QJsonObject query;
    query.insert("query", QString("[?_type=\"partitiontest\"]"));

    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query, mJsonDb->mSystemPartitionName);
    verifyGoodResult(result);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("foo").toString(), QLatin1String("bar"));

    queryResult = mJsonDb->find(mOwner, query, "private");
    verifyGoodResult(result);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("foo").toString(), QLatin1String("asd"));

    // defaults to system partition
    queryResult = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("foo").toString(), QLatin1String("bar"));
}

void TestJsonDb::arrayIndexQuery()
{
    addIndex(QLatin1String("phoneNumber"));

    QJsonArray objects(readJsonFile(":/daemon/json/array.json").toArray());
    QMap<QString, JsonDbObject> toDelete;
    for (int i = 0; i < objects.size(); ++i) {
        JsonDbObject object = objects.at(i).toObject();
        QJsonObject result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        toDelete.insert(object.value("_uuid").toString(), object);
    }

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 2);

    QJsonObject queryListMember;
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.number =~\"/*789*/wi\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 1);

    queryListMember = QJsonObject();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.validTime.0.timeFrom =\"09:00\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 1);

    queryListMember = QJsonObject();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.validTime.0.timeFrom =\"10:00\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 1);

    queryListMember = QJsonObject();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.validTime.0.timeTo =\"13:00\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 2);

    queryListMember = QJsonObject();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.validTime.10.timeTo =\"13:00\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 0);

    queryListMember = QJsonObject();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.10.validTime.10.timeTo =\"13:00\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 0);

    queryListMember = QJsonObject();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?foo.0.0.bar =\"val00\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 2);

    queryListMember = QJsonObject();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?test.45 =\"joe\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 1);

    queryListMember = QJsonObject();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?test2.56.firstName =\"joe\"]"));
    queryResult = mJsonDb->find(mOwner, queryListMember);
    verifyGoodQueryResult(queryResult);
    QCOMPARE(queryResult.data.size(), 1);

    foreach (JsonDbObject object, toDelete.values()) {
        verifyGoodResult(mJsonDb->remove(mOwner, object));
    }

}

void TestJsonDb::deindexError()
{
    QJsonObject result;

    // create document with a property "name"
    JsonDbObject foo;
    foo.insert("_type", QLatin1String("Foo"));
    foo.insert("name", QLatin1String("fooo"));
    result = mJsonDb->create(mOwner, foo);
    verifyGoodResult(result);

    // create a schema object (has 'name' property)
    JsonDbObject schema;
    schema.insert("_type", QLatin1String("_schemaType"));
    schema.insert("name", QLatin1String("ArrayObject"));
    QJsonObject s;
    s.insert("type", QLatin1String("object"));
    //s.insert("extends", QLatin1String("View"));
    schema.insert("schema", s);
    result = mJsonDb->create(mOwner, schema);
    verifyGoodResult(result);

    // create instance of ArrayView (defined by the schema)
    JsonDbObject arrayView;
    arrayView.insert("_type", QLatin1String("ArrayView"));
    arrayView.insert("name", QLatin1String("fooo"));
    result = mJsonDb->create(mOwner, arrayView);
    verifyGoodResult(result);

    addIndex(QLatin1String("name"));

    // now remove some objects that have "name" property
    QJsonObject query;
    query.insert("query", QString("[?_type=\"Foo\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    JsonDbObjectList objs = queryResult.data;
    QVERIFY(!objs.isEmpty());
    result = mJsonDb->removeList(mOwner, objs);
    verifyGoodResult(result);
}

void TestJsonDb::expectedOrder()
{
    QStringList list;
    QStringList uuids;

    // Bug occurs when there is a string A and multiple other strings that are longer
    // than A but the first A.length characters are exactly the same as A's.
    const int count = 100;
    for (int i = 0; i < count; ++i) {
        int r = qrand();
        QString str0 = QString::number(r);
        QString str1 = str0 + QString(str0[0]);
        QString str2 = str0 + QString(str0[0]) + QString(str0[0]);
        list.append(str0);
        list.append(str1);
        list.append(str2);
    }

    foreach (const QString &str, list) {
        JsonDbObject item;
        item.insert("_type", QLatin1String(__FUNCTION__));
        item.insert("order", str);
        QJsonObject result = mJsonDb->create(mOwner, item);
        QVERIFY(result.value("error").toObject().isEmpty());
        uuids << result.value("result").toObject().value("_uuid").toString();
    }

    // This is the order we expect from the query
    list.sort();

    QJsonObject query;
    query.insert("query", QString("[?_type=\"%1\"][/order]").arg(__FUNCTION__));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    verifyGoodQueryResult(queryResult);
    JsonDbObjectList dataList = queryResult.data;

    QCOMPARE(dataList.size(), list.size());

    for (int i = 0; i < dataList.size(); ++i) {
        JsonDbObject obj = dataList.at(i);
        QVERIFY(obj.contains("order"));
        QCOMPARE(obj.value("order").toString(), list.at(i));
    }
}

void TestJsonDb::indexQueryOnCommonValues()
{
    // Specific indexing bug when you have records inserted that only differ
    // by their _type
    createContacts();

    for (int ii = 0; ii < mContactList.size(); ii++) {
        JsonDbObject data(mContactList.at(ii));
        JsonDbObject chaff;
        chaff.insert(JsonDbString::kTypeStr, QString("com.example.ContactChaff"));
        QStringList skipKeys = (QStringList() << JsonDbString::kUuidStr << JsonDbString::kVersionStr << JsonDbString::kTypeStr);
        foreach (QString key, data.keys()) {
            if (!skipKeys.contains(key))
                chaff.insert(key, data.value(key));
        }

        QJsonObject result = mJsonDb->create(mOwner, chaff);
        verifyGoodResult(result);
    }

    int count = mContactList.size();

    QCOMPARE(count > 0, true);

    QJsonObject request;

    int itemNumber = (int)((double)qrand() * count / RAND_MAX);

    JsonDbObject item(mContactList.at(itemNumber));
    QString first = JsonDb::propertyLookup(item, "name.first").toString();
    QString last = JsonDb::propertyLookup(item, "name.last").toString();
    request.insert("query",
                   QString("[?name.first=\"%1\"][?name.last=\"%2\"][?_type=\"contact\"]")
                   .arg(first)
                   .arg(last) );
    JsonDbQueryResult queryResult= mJsonDb->find(mOwner, request);
    verifyGoodQueryResult(queryResult);

    QCOMPARE(queryResult.data.size(), 1);
}

void TestJsonDb::removeIndexes()
{
    addIndex("wacky_index");
    QVERIFY(mJsonDb->findPartition(mJsonDb->mSystemPartitionName)->findObjectTable(JsonDbString::kSchemaTypeStr)->indexSpec("wacky_index") != 0);

    QVERIFY(mJsonDb->removeIndex("wacky_index"));
    QVERIFY(mJsonDb->findPartition(mJsonDb->mSystemPartitionName)->findObjectTable(JsonDbString::kSchemaTypeStr)->indexSpec("wacky_index") == 0);

    JsonDbObject indexObject;
    indexObject.insert(JsonDbString::kTypeStr, QLatin1String("Index"));
    indexObject.insert("propertyName", QLatin1String("predicate"));
    indexObject.insert("propertyType", QLatin1String("string"));
    JsonDbObject indexObject2 = indexObject;

    QJsonObject result = mJsonDb->create(mOwner, indexObject);
    verifyGoodResult(result);
    QVERIFY(mJsonDb->findPartition(mJsonDb->mSystemPartitionName)->findObjectTable("Index")->indexSpec("predicate") != 0);

    indexObject2.insert(JsonDbString::kUuidStr, indexObject.value(JsonDbString::kUuidStr).toString());

    indexObject.insert("propertyType", QLatin1String("integer"));
    result = mJsonDb->update(mOwner, indexObject);
    verifyErrorResult(result);

    result = mJsonDb->remove(mOwner, indexObject2);
    verifyGoodResult(result);
    QVERIFY(mJsonDb->findPartition(mJsonDb->mSystemPartitionName)->findObjectTable("Index")->indexSpec("predicate") == 0);
}

void TestJsonDb::setOwner()
{
    jsondbSettings->setEnforceAccessControl(true);

    mOwner->setAllowAll(true);
    QLatin1String fooOwnerStr("com.foo.owner");

    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("SetOwnerType"));
    item.insert(JsonDbString::kOwnerStr, fooOwnerStr);
    QJsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    GetObjectsResult getObjects = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("SetOwnerType"));
    QCOMPARE(getObjects.data.at(0).value(JsonDbString::kOwnerStr).toString(), fooOwnerStr);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);

    JsonDbOwner *unauthOwner = new JsonDbOwner(this);
    unauthOwner->setOwnerId("com.example.OtherOwner");
    unauthOwner->setAllowAll(false);
    unauthOwner->setAllowedObjects(QLatin1String("all"), "write", (QStringList() << QLatin1String("[*]")));
    unauthOwner->setAllowedObjects(QLatin1String("all"), "read", (QStringList() << QLatin1String("[*]")));

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("SetOwnerType2"));
    item.insert(JsonDbString::kOwnerStr, fooOwnerStr);
    result = mJsonDb->create(unauthOwner, item);
    verifyGoodResult(result);

    getObjects = mJsonDb->getObjects(JsonDbString::kTypeStr, QLatin1String("SetOwnerType2"));
    QVERIFY(getObjects.data.at(0).value(JsonDbString::kOwnerStr).toString()
            != fooOwnerStr);
    result = mJsonDb->remove(unauthOwner, item);
    verifyGoodResult(result);

    jsondbSettings->setEnforceAccessControl(false);
}

void TestJsonDb::indexPropertyFunction()
{
    JsonDbObject index;
    index.insert(JsonDbString::kTypeStr, QLatin1String("Index"));
    index.insert(QLatin1String("name"), QLatin1String("propertyFunctionIndex"));
    index.insert(QLatin1String("propertyType"), QLatin1String("number"));
    index.insert(QLatin1String("propertyFunction"), QLatin1String("function (o) { if (o.from !== undefined) jsondb.emit(o.from); else jsondb.emit(o.to); }"));
    QJsonObject result = mJsonDb->create(mOwner, index);
    verifyGoodResult(result);

    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("IndexPropertyFunction"));
    item.insert("from", 10);
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("IndexPropertyFunction"));
    item.insert("to", 42);
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("IndexPropertyFunction"));
    item.insert("from", 0);
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = JsonDbObject();
    item.insert(JsonDbString::kTypeStr, QLatin1String("IndexPropertyFunction"));
    item.insert("to", -64);
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QJsonObject query;
    query.insert("query", QString("[?_type=\"IndexPropertyFunction\"][?propertyFunctionIndex > 10][/propertyFunctionIndex]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("to").toDouble(), (double)42);

    query.insert("query", QString("[?_type=\"IndexPropertyFunction\"][?propertyFunctionIndex >= 10][/propertyFunctionIndex]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 2);
    QCOMPARE(queryResult.data.at(0).value("from").toDouble(), (double)10);
    QCOMPARE(queryResult.data.at(1).value("to").toDouble(), (double)42);

    query.insert("query", QString("[?_type=\"IndexPropertyFunction\"][?propertyFunctionIndex < 0]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), 1);
    QCOMPARE(queryResult.data.at(0).value("to").toDouble(), (double)-64);
}

void TestJsonDb::managedBtree()
{
    const char mdbname[] = "tst_qmanagedbtree.db";
    const int numtags = 3;
    const QByteArray k1("foo");
    const QByteArray k2("bar");
    const QByteArray k3("baz");

    QFile::remove(mdbname);
    JsonDbManagedBtree *mdb = new JsonDbManagedBtree;
    if (!mdb->open(mdbname, QBtree::NoSync))
        Q_ASSERT(false);

    for (int i = 0; i < numtags; ++i) {
        char c = 'a' + i;
        QByteArray value(&c, 1);

        JsonDbManagedBtreeTxn txn1 = mdb->beginWrite();
        JsonDbManagedBtreeTxn txn2 = mdb->beginWrite();
        JsonDbManagedBtreeTxn txn3 = mdb->beginWrite();
        QVERIFY(txn1 && txn2 && txn3);

        QVERIFY(txn1.put(k1, value));
        QVERIFY(txn2.put(k2, value));
        QVERIFY(txn3.put(k3, value));

        QVERIFY(txn1.commit(i));
        QVERIFY(!txn1 && !txn2 && !txn3);
    }

    for (int i = 0; i < numtags; ++i) {
        QByteArray value;

        JsonDbManagedBtreeTxn txn1 = mdb->beginRead(i);
        JsonDbManagedBtreeTxn txn2 = mdb->beginRead(i);
        JsonDbManagedBtreeTxn txn3 = mdb->beginRead(i);
        QVERIFY(txn1 && txn2 && txn3);

        QVERIFY(txn1.get(k1, &value));
        QCOMPARE(*(char*)value.constData(), char('a' + i));
        QVERIFY(txn2.get(k2, &value));
        QCOMPARE(*(char*)value.constData(), char('a' + i));
        QVERIFY(txn3.get(k3, &value));
        QCOMPARE(*(char*)value.constData(), char('a' + i));

        QCOMPARE(txn1.txn(), txn2.txn());
        QCOMPARE(txn2.txn(), txn3.txn());

        txn1.abort();
        QVERIFY(!txn1 && txn2 && txn3);
        txn2.abort();
        QVERIFY(!txn1 && !txn2 && txn3);
        txn3.abort();
        QVERIFY(!txn1 && !txn2 && !txn3);
    }

    mdb->close();
    delete mdb;
    mdb = 0;
    QFile::remove(mdbname);
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

QTEST_MAIN(TestJsonDb)
#include "testjsondb.moc"
