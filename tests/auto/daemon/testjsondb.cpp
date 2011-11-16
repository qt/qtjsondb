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
#include <QtTest/QtTest>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTime>
#include <QUuid>

#include "json.h"

#include "jsondb.h"

#include <QtJsonDbQson/private/qson_p.h>
#include <QtJsonDbQson/private/qsonparser_p.h>
#include <QtJsonDbQson/private/qsonstrings_p.h>

#include "jsondb.h"
#include "aodb.h"
#include "objecttable.h"
#include "jsondbbtreestorage.h"
#include "jsondbindex.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"

#include "../../shared/util.h"

namespace QtAddOn { namespace JsonDb {
extern bool gValidateSchemas;
extern bool gDebug;
extern bool gVerbose;
} } // end namespace QtAddOn::JsonDb

#ifndef QT_NO_DEBUG_OUTPUT
#define DBG() if (gDebug) qDebug()
#else
#define DBG() if (0) qDebug()
#endif

Q_USE_JSONDB_NAMESPACE

static QString kContactStr = "com.noklab.nrcc.jsondb.unittest.contact";

/*
  Ensure that a good result object contains the correct fields
 */

#define verifyGoodResult(result) \
{ \
    QsonMap __result = result; \
    QVERIFY(__result.contains(JsonDbString::kErrorStr)); \
    QVERIFY2(__result.isNull(JsonDbString::kErrorStr), __result.subObject(JsonDbString::kErrorStr).valueString("message").toLocal8Bit()); \
    QVERIFY(__result.contains(JsonDbString::kResultStr)); \
}

// QsonMap result, QString field, QVariant expectedValue
#define verifyResultField(result, field, expectedValue) \
{ \
    QsonMap map = result.subObject(JsonDbString::kResultStr); \
    QVERIFY(map.contains(field)); \
    QCOMPARE(qsonToVariant(map.value<QsonElement>(field)), QVariant(expectedValue)); \
}

/*
  Ensure that a error result contains the correct fields
 */

#define verifyErrorResult(result) \
{\
    QVERIFY(result.contains(JsonDbString::kErrorStr)); \
    if (result.valueType(JsonDbString::kErrorStr) != QsonObject::MapType ) \
        qDebug() << "verifyErrorResult" << result; \
    QCOMPARE(result.valueType(JsonDbString::kErrorStr), QsonObject::MapType ); \
    QVariantMap errormap = qsonToVariant(result.value<QsonMap>(JsonDbString::kErrorStr)).toMap(); \
    QVERIFY(errormap.contains(JsonDbString::kCodeStr)); \
    QCOMPARE(errormap.value(JsonDbString::kCodeStr).type(), QVariant::LongLong ); \
    QVERIFY(errormap.contains(JsonDbString::kMessageStr)); \
    QCOMPARE(errormap.value(JsonDbString::kMessageStr).type(), QVariant::String ); \
    QVERIFY(result.contains(JsonDbString::kResultStr)); \
    QVERIFY(result.subObject(JsonDbString::kResultStr).isEmpty() \
            || (result.subObject(JsonDbString::kResultStr).contains("count") \
                && result.subObject(JsonDbString::kResultStr).valueInt("count", 0) == 0)); \
}

class TestJsonDb: public QObject
{
    Q_OBJECT
public:
    TestJsonDb();

public slots:
    void notified(const QString, QsonMap, const QString);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void reopen();

    void create();

    void update();
    void update2();
    void update3();
    void update4();

    void remove();
    void remove2();
    void remove3();
    void remove4();

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

#if 0
    void testAccessControl();
    void testFindAccessControl();
    void permissionsCleared();
#endif

    void addIndex();
    void addSchema();
    void duplicateSchema();
    void removeSchema();
    void removeViewSchema();
    void updateSchema();
    void parseQuery();
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

public:
    void createContacts();

private:
    void dumpObjects();
    void addSchema(const QString &schemaName, QsonMap &schemaObject);
    void addIndex(const QString &propertyName, const QString &propertyType=QString(), const QString &objectType=QString());

    QsonObject readJsonFile(const QString &filename);
    QsonObject readJson(const QByteArray& json);
    void removeDbFiles();

private:
    JsonDb *mJsonDb;
    JsonDbBtreeStorage *mJsonDbStorage;
    QStringList mNotificationsReceived;
    QList<QsonMap> mContactList;
    JsonDbOwner *mOwner;
};

const char *kFilename = "testdatabase";
//const char *kFilename = ":memory:";
const QString kReplica1Name = QString("replica1");
const QString kReplica2Name = QString("replica2");
const QStringList kReplicaNames = (QStringList() << kReplica1Name << kReplica2Name);

TestJsonDb::TestJsonDb()
    : mJsonDb(NULL), mJsonDbStorage(0), mOwner(0)
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
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setOrganizationDomain("nrcc.noklab.com");
    QCoreApplication::setApplicationName("testjsondb");
    QCoreApplication::setApplicationVersion("1.0");

    removeDbFiles();
    gVerbose = false;
    mJsonDb = new JsonDb(kFilename, this);
    mOwner = new JsonDbOwner(this);
    mOwner->setOwnerId("com.noklab.nrcc.JsonDbTest");

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
//    QCOMPARE(mJsonDb->mStorage->mTransactionDepth, 0);
    //QVERIFY(mJsonDb->checkValidity());
}

void TestJsonDb::reopen()
{
    int counter = 1;
    for (int i = 0; i < 10; ++i, ++counter) {
        QsonMap item;
        item.insert(QLatin1String("_type"), QLatin1String("reopentest"));
        item.insert("create-string", QString("string"));
        QsonMap result = mJsonDb->create(mOwner, item);

        mJsonDb->close();
        delete mJsonDb;

        mJsonDb = new JsonDb(kFilename, this);
        mJsonDb->open();

        QsonMap request;
        request.insert("query", QLatin1String("[?_type=\"reopentest\"]"));
        result = mJsonDb->find(mOwner, request);
        QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(counter));
        QCOMPARE(result.subObject("result").subList("data").size(), counter);
    }
    mJsonDb->removeIndex("reopentest");
}

void TestJsonDb::createContacts()
{
    if (!mContactList.isEmpty())
        return;

    QFile contactsFile(findFile(SRCDIR, "largeContactsTest.json"));
    QVERIFY2(contactsFile.exists(), "Err: largeContactsTest.json doesn't exist!");

    contactsFile.open(QIODevice::ReadOnly);
    QByteArray json = contactsFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok)
        qDebug() << parser.errorString();
    QVariantList contactList = parser.result().toList();
    QList<QsonMap> newContactList;
    foreach (QVariant v, contactList) {
        QsonMap contact(variantToQson(v.toMap()));
        QString name = contact.valueString("name");
        QStringList names = name.split(" ");
        QsonMap nameObject;
        nameObject.insert("first", names[0]);
        nameObject.insert("last", names[names.size()-1]);
        contact.insert("name", nameObject);
        contact.insert(JsonDbString::kTypeStr, QString("contact"));
        verifyGoodResult(mJsonDb->create(mOwner, contact));
        newContactList.append(QsonParser::fromRawData(contact.data()).toMap());
    }
    mContactList = newContactList;

    addIndex(QLatin1String("name"));
    addIndex(QLatin1String("name.first"));
    addIndex(QLatin1String("name.last"));
    addIndex(QLatin1String("_type"));

}

void TestJsonDb::addSchema(const QString &schemaName, QsonMap &schemaObject)
{
    QsonObject schema = readJsonFile(QString("schemas/%1.json").arg(schemaName));
    schemaObject = QsonMap();
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", schemaName);
    schemaObject.insert("schema", schema);

    QsonMap result = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(result);
}

void TestJsonDb::addIndex(const QString &propertyName, const QString &propertyType, const QString &objectType)
{
    QsonMap index;
    index.insert(JsonDbString::kTypeStr, kIndexTypeStr);
    index.insert(kPropertyNameStr, propertyName);
    if (!propertyType.isEmpty())
        index.insert(kPropertyTypeStr, propertyType);
    if (!objectType.isEmpty())
        index.insert(kObjectTypeStr, objectType);
    QVERIFY(mJsonDb->addIndex(index, JsonDbString::kSystemPartitionName));
}

void TestJsonDb::duplicateSchema()
{
    QsonObject schema = readJsonFile("schemas/address.json");
    QsonMap schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", QLatin1String("Address"));
    schemaObject.insert("schema", schema);

    QsonMap result = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(result);

    QsonMap schemaObject2;
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
    QsonObject schema = readJsonFile("schemas/address.json");
    QsonMap schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", QLatin1String("Address"));
    schemaObject.insert("schema", schema);

    QsonMap result = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(result);

    QsonMap address;
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
    QsonList objects = readJsonFile("reduce.json").toList();
    QsonObject schema;
    QsonObject reduce;
    for (int i = 0; i < objects.size(); ++i) {
        QsonMap object = objects.at<QsonMap>(i);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.valueString(JsonDbString::kTypeStr) == "Reduce")
            reduce = object;
        else
            schema = object;
    }

    QsonMap result = mJsonDb->remove(mOwner, schema);
    verifyErrorResult(result);

    result = mJsonDb->remove(mOwner, reduce);
    verifyGoodResult(result);

    result = mJsonDb->remove(mOwner, schema);
    verifyGoodResult(result);
}

void TestJsonDb::updateSchema()
{
    QsonMap schema = readJsonFile("schemas/address.json").toMap();
    QsonMap schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", QLatin1String("Address"));
    schemaObject.insert("schema", schema);

    QsonMap schemaResult = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(schemaResult);

    QsonMap address;
    address.insert(JsonDbString::kTypeStr, QLatin1String("Address"));
    address.insert("street", QLatin1String("Main Street"));
    address.insert("number", 1);

    QsonMap result = mJsonDb->create(mOwner, address);
    verifyGoodResult(result);

    schema.insert("streetNumber", schema.subObject("number"));
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

/*
 * Create an item
 */
void TestJsonDb::create()
{
    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QString("create-test-type"));
    item.insert("create-test", 22);
    item.insert("create-string", QString("string"));

    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    qDebug() << "create result" << result;

    QsonMap map = result.value<QsonMap>(JsonDbString::kResultStr);
    QVERIFY(map.contains(JsonDbString::kUuidStr));
    QVERIFY(!map.valueString(JsonDbString::kUuidStr).isEmpty());
    QCOMPARE(map.valueType(JsonDbString::kUuidStr), QsonObject::StringType);

    QVERIFY(map.contains(JsonDbString::kVersionStr));
    QVERIFY(!map.valueString(JsonDbString::kVersionStr).isEmpty());
    QCOMPARE(map.valueType(JsonDbString::kVersionStr), QsonObject::StringType);

    QsonMap query;
    query.insert("query", QString("[?_uuid=\"%1\"]").arg(map.valueString(JsonDbString::kUuidStr)));
    result = mJsonDb->find(mOwner, query);
    QsonMap findMap = result.subObject("result");
    QCOMPARE(findMap.valueInt("length"), qint64(1));
    QCOMPARE(findMap.subList("data").size(), 1);
    QCOMPARE(findMap.subList("data").objectAt(0).valueString(JsonDbString::kUuidStr), map.valueString(JsonDbString::kUuidStr));
    QCOMPARE(findMap.subList("data").objectAt(0).valueString(JsonDbString::kVersionStr), map.valueString(JsonDbString::kVersionStr));
}

/*
 * Verify translation of capabilities to access control policies.
 */
void TestJsonDb::capabilities()
{
    QsonList viewDefinitions(readJsonFile("capabilities-test.json").toList());
    //qDebug() << "viewDefinitions" << viewDefinitions;
    for (int i = 0; i < viewDefinitions.size(); ++i) {
        QsonMap object = viewDefinitions.at<QsonMap>(i);
        //qDebug() << "object" << object;
        if (object.valueString("_type") == "CapabilitiesTest") {
            QScopedPointer<JsonDbOwner> owner(new JsonDbOwner());
            owner->setOwnerId(object.valueString("identifier"));
            QsonMap capabilities(object.subObject("capabilities"));
            owner->setCapabilities(capabilities, mJsonDb);
        } else {
            QsonMap result = mJsonDb->create(mOwner, object);
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
    bool acp = gEnforceAccessControlPolicies;
    gEnforceAccessControlPolicies = true;

    JsonDbOwner *owner = new JsonDbOwner();
    owner->setAllowedObjects("read", QStringList());
    owner->setAllowedObjects("write", QStringList());
    owner->setStorageQuota(-1);

    QsonMap toPut;
    toPut.insert("_type", QLatin1String("TestObject"));

    QsonMap result = mJsonDb->create(owner, toPut);
    verifyErrorResult(result);

    QsonMap toPut2;
    toPut2.insert("_type", QLatin1String("TestObject"));

    owner->setAllowAll(true);
    result =  mJsonDb->create(owner, toPut2);
    verifyGoodResult(result);

    gEnforceAccessControlPolicies = acp;
    mJsonDb->removeIndex("TestObject");
}

/*
 * Create an item and verify access control
 */

#if 0
void TestJsonDb::testAccessControl()
{
    QSet<QString> emptySet;
    QSet<QString> myTypes;
    myTypes.insert("create-test-type");
    QSet<QString> otherTypes;
    otherTypes.insert("other-test-type");
    QSet<QString> myDomains;
    myDomains.insert("my-domain");
    QSet<QString> otherDomains;
    otherDomains.insert("other-domain");

    //qDebug() << "isAllowed" << mOwner->isAllowed("create-test-type", mOwner->ownerId(), "create");

    QStringList ops = (QStringList() /* << "read" */ << "write");
    for (int k = 0; k < ops.size(); k++) {
        QString op = ops[k];
        for (int i = 0; i < 4; i++) {
            mOwner->setAllowedTypes(op, ((i % 2) == 0) ? myTypes : otherTypes);
            mOwner->setProhibitedTypes(op, emptySet);
            if (i >= 2)
                mOwner->setProhibitedTypes(op, ((i % 2) == 0) ? otherTypes : myTypes);

            for (int j = 0; j < 4; j++) {
                mOwner->setAllowedDomains(op, ((j % 2) == 0) ? myDomains : otherDomains);
                mOwner->setProhibitedDomains(op, emptySet);
                if (j >= 2)
                    mOwner->setProhibitedDomains(op, ((j % 2) == 0) ? otherDomains : myDomains);

                //qDebug() << "isAllowed" << op << i << j << mOwner->isAllowed("create-test-type", mOwner->ownerId(), op);
                QsonObject item;
                QString type = "create-test-type";
                QString domain = "my-domain";
                item.insert(JsonDbString::kTypeStr, type);
                item.insert(JsonDbString::kDomainStr, domain);
                item.insert("create-test", 22);

                QsonObject result = mJsonDb->create(mOwner, item);

                if ((mOwner->allowedDomains("write").isEmpty() || mOwner->allowedDomains("write").contains(domain))
                    && !mOwner->prohibitedDomains("write").contains(domain)
                    && (mOwner->allowedTypes("write").isEmpty() || mOwner->allowedTypes("write").contains(type))
                    && !mOwner->prohibitedTypes("write").contains(type))
                    verifyGoodResult(result);
                else
                    verifyErrorResult(result);

                if (result.subObject(JsonDbString::kErrorStr).isEmpty()) {
                    //if (op != "create")
                    //item.insert(JsonDbString::kUuidStr, item.value(JsonDbString::kUuidStr));
                    result = mJsonDb->update(mOwner, item);
                    verifyGoodResult(result);

                    result = mJsonDb->remove(mOwner, item);
                    verifyGoodResult(result);
                }
            }
        }

        mOwner->setAllowedTypes(op, emptySet);
        mOwner->setAllowedDomains(op, emptySet);
        mOwner->setProhibitedTypes(op, emptySet);
        mOwner->setProhibitedDomains(op, emptySet);
    }
}

void TestJsonDb::testFindAccessControl()
{
    QSet<QString> emptySet;
    QSet<QString> myTypes;
    myTypes.insert("create-test-type");
    QSet<QString> otherTypes;
    otherTypes.insert("other-test-type");
    QSet<QString> myDomains;
    myDomains.insert("my-domain");
    QSet<QString> otherDomains;
    otherDomains.insert("other-owner-id");

    QsonObject item;
    item.insert(JsonDbString::kTypeStr, "create-test-type");
    item.insert(JsonDbString::kDomainStr, "my-domain");
    item.insert("create-test", 22);
    QsonObject result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QsonMap request;
    request.insert(JsonDbString::kQueryStr, QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("create-test-type"));

    //qDebug() << "isAllowed" << mOwner->isAllowed("create-test-type", mOwner->ownerId(), "create");

    QStringList ops = (QStringList() << "read");
    for (int k = 0; k < ops.size(); k++) {
        QString op = ops[k];
        for (int i = 0; i < 4; i++) {
            mOwner->setAllowedTypes(op, ((i % 2) == 0) ? myTypes : otherTypes);
            mOwner->setProhibitedTypes(op, emptySet);
            if (i >= 2)
                mOwner->setProhibitedTypes(op, ((i % 2) == 0) ? otherTypes : myTypes);

            for (int j = 0; j < 4; j++) {
                mOwner->setAllowedDomains(op, ((j % 2) == 0) ? myDomains : otherDomains);
                mOwner->setProhibitedDomains(op, emptySet);
                if (j >= 2)
                    mOwner->setProhibitedDomains(op, ((j % 2) == 0) ? otherDomains : myDomains);

                //qDebug() << "isAllowed" << op << i << j << mOwner->isAllowed("create-test-type", mOwner->ownerId(), op);
                QsonObject result = mJsonDb->find(mOwner, request);
                //if (op != "create")
                //item.insert(JsonDbString::kUuidStr, item.value(JsonDbString::kUuidStr));
                if (op == "update")
                    result = mJsonDb->update(mOwner, item);
                if (op == "remove")
                    result = mJsonDb->remove(mOwner, item);
                verifyGoodResult(result);
                QsonObject map = result.value("result").toMap();
                int length = (map.contains("length") ? map.value("length").toInt() : 0);
                if (((i % 2) == 0) && ((j % 2) == 0)) {
//                    if (!length) {
//                        qDebug() << "isAllowed" << op << i << j << mOwner->isAllowed("create-test-type", mOwner->ownerId(), op);
//                        qDebug() << result;
//                    }
                    QVERIFY(map.contains("length"));
                    QVERIFY(map.value("length").toInt() >= 1);
                } else {
//                    if (length) {
//                        qDebug() << "isAllowed" << op << i << j << mOwner->isAllowed("create-test-type", mOwner->ownerId(), op);
//                        qDebug() << result;
//                        qDebug() << "mOwner->allowedTypes(op)" << mOwner->allowedTypes(op);
//                        qDebug() << "mOwner->allowedDomains(op)" << mOwner->allowedDomains(op);
//                        qDebug() << "mOwner->prohibitedTypes(op)" << mOwner->prohibitedTypes(op);
//                        qDebug() << "mOwner->prohibitedDomains(op)" << mOwner->prohibitedDomains(op);
//                    }
                    QVERIFY(map.contains("length"));
                    QCOMPARE(map.value("length").toInt(), 0);
                }
            }
        }

//        qDebug() << __FUNCTION__ << "Clearing permissions for op" << op;
        emptySet = QSet<QString>();
        mOwner->setAllowedTypes(op, emptySet);
        mOwner->setAllowedDomains(op, emptySet);
        mOwner->setProhibitedTypes(op, emptySet);
        mOwner->setProhibitedDomains(op, emptySet);
    }
}

void TestJsonDb::permissionsCleared()
{
    QStringList ops = (QStringList() << "find" << "create" << "update" << "remove");
    for (int k = 0; k < ops.size(); k++) {
        QString op = ops[k];
        QSet<QString> emptySet;
        mOwner->setAllowedTypes(op, emptySet);
        mOwner->setAllowedDomains(op, emptySet);
        mOwner->setProhibitedTypes(op, emptySet);
        mOwner->setProhibitedDomains(op, emptySet);
    }
    for (int k = 0; k < ops.size(); k++) {
        QString op = ops[k];
        //qDebug() << __FUNCTION__ << op;
        //qDebug() << "mOwner->allowedTypes(op)" << mOwner->allowedTypes(op);
        QCOMPARE(mOwner->allowedTypes(op).size(), 0);
        //qDebug() << "mOwner->allowedDomains(op)" << mOwner->allowedDomains(op);
        QCOMPARE(mOwner->allowedDomains(op).size(), 0);
        //qDebug() << "mOwner->prohibitedTypes(op)" << mOwner->prohibitedTypes(op);
        QCOMPARE(mOwner->prohibitedTypes(op).size(), 0);
        //qDebug() << "mOwner->prohibitedDomains(op)" << mOwner->prohibitedDomains(op);
        QCOMPARE(mOwner->prohibitedDomains(op).size(), 0);
    }
}
#endif

/*
 * Insert an item and then update it.
 */

void TestJsonDb::update()
{
    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("update-test-type"));
    item.insert("update-test", 100);
    item.insert("update-string", QLatin1String("update-test-100"));

    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    QString uuid = result.subObject(JsonDbString::kResultStr).valueString(JsonDbString::kUuidStr);

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
    QsonMap item;
    item.insert("update2-test", 100);
    item.insert("_type", QString("update-from-null"));
    item.generateUuid();

    QsonMap result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr,1);
}

/*
 * Update an item which doesn't have a "uuid" field
 */

void TestJsonDb::update3()
{
    QsonMap item;
    item.insert("update2-test", 100);

    QsonMap result = mJsonDb->update(mOwner, item);
    verifyErrorResult(result);
}

/*
 * Update a stale copy of an item
 */

void TestJsonDb::update4()
{
    bool prev = gRejectStaleUpdates;
    gRejectStaleUpdates = true;

    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("update-test-type"));
    item.insert("update-test", 100);
    item.insert("update-string", QLatin1String("update-test-100"));

    //qDebug(">>> CREATE");
    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    QString uuid = result.subObject(JsonDbString::kResultStr).valueString(JsonDbString::kUuidStr);

    QString version1 = result.subObject(JsonDbString::kResultStr).valueString(JsonDbString::kVersionStr);
    //qDebug() << "<<< CREATE, version is" << version1;

    item.insert(JsonDbString::kUuidStr, uuid);
    item.insert(JsonDbString::kVersionStr, version1);
    item.insert("update-test", 101);

    //qDebug(">>> UPDATE");
    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr,1);
    verifyResultField(result,JsonDbString::kUuidStr, uuid);

    QString version2 = result.subObject(JsonDbString::kResultStr).valueString(JsonDbString::kVersionStr);
    QVERIFY(version1 != version2);

    //qDebug() << "<<< UPDATE, version is" << version2;

    //qDebug() << ">>> REREAD" << uuid;
    QsonMap query;
    query.insert("query", QString("[?_uuid=\"%1\"]").arg(uuid));
    result = mJsonDb->find(mOwner, query);
    QsonMap findMap = result.subObject("result");
    //qDebug() << "re-read after update" << findMap;
    QCOMPARE(findMap.valueInt("length"), qint64(1));
    QCOMPARE(findMap.subList("data").size(), 1);
    QCOMPARE(findMap.subList("data").objectAt(0).valueString(JsonDbString::kUuidStr), uuid);
    QCOMPARE(findMap.subList("data").objectAt(0).valueString(JsonDbString::kVersionStr), version2);
    //qDebug() << "<<< REREAD" << uuid;


    // replay
    //qDebug(">>> REPLAY");
    item.insert(JsonDbString::kVersionStr, version1);
    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result, JsonDbString::kCountStr,1);
    verifyResultField(result, JsonDbString::kUuidStr, uuid);
    verifyResultField(result, JsonDbString::kVersionStr, version2);
    //qDebug() << "<<< REPLAY";

    // conflict
    item.insert(JsonDbString::kVersionStr, version1);
    item.insert("update-test", 102);
    //qDebug(">>> CONFLICT");
    result = mJsonDb->update(mOwner, item);
    qDebug() << result;
    verifyErrorResult(result);
    //qDebug("<<< CONFLICT");

    // conflict as replication
    item.insert(JsonDbString::kVersionStr, version1);
    item.insert("update-test", 102);
    //qDebug(">>> REPLICATE");
    result = mJsonDb->update(mOwner, item, QString(), true);
    verifyGoodResult(result);

    query.insert("query", QString("[?_uuid=\"%1\"]").arg(uuid));
    result = mJsonDb->find(mOwner, query);
    findMap = result.subObject("result");
    QCOMPARE(findMap.valueInt("length"), qint64(1));
    QCOMPARE(findMap.subList("data").size(), 1);

    QsonMap conflict = findMap.subList("data").objectAt(0);
    //qDebug() << "Object with conflict embedded" << conflict;
    QCOMPARE(conflict.valueString(JsonDbString::kUuidStr), uuid);
    QCOMPARE(conflict.subObject(QsonStrings::kMetaStr).subList(QsonStrings::kConflictsStr).size(), 1);
    //qDebug("<<< REPLICATE");

    gRejectStaleUpdates = prev;
}

/*
 * Create an item and immediately remove it
 */

void TestJsonDb::remove()
{
    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("remove-test-type"));
    item.insert("remove-test", 100);

    QsonMap response = mJsonDb->create(mOwner, item);
    verifyGoodResult(response);
    QsonMap result = response.value<QsonMap>(JsonDbString::kResultStr);

    QString uuid = result.valueString(JsonDbString::kUuidStr);
    QString version = result.valueString(JsonDbString::kVersionStr);

    item.insert(JsonDbString::kUuidStr, uuid);
    item.insert(JsonDbString::kVersionStr, uuid);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr, 1);

    QsonMap query;
    query.insert("query", QString("[?_uuid=\"%1\"]").arg(uuid));
    result = mJsonDb->find(mOwner, query);
    QsonMap findMap = result.subObject("result");
    QCOMPARE(findMap.valueInt("length"), qint64(0));
    QCOMPARE(findMap.subList("data").size(), 0);
}

/*
 * Try to remove an item which doesn't exist
 */

void TestJsonDb::remove2()
{
    QsonMap item;
    item.insert("remove2-test", 100);
    item.insert(JsonDbString::kTypeStr, QLatin1String("Remove2Type"));
    item.insert(JsonDbString::kUuidStr, QUuid::createUuid().toString());

    QsonMap result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);
}

/*
 * Don't include a 'uuid' field
 */

void TestJsonDb::remove3()
{
    QsonMap item;
    item.insert("remove3-test", 100);

    QsonMap result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);
}

/*
 * Try to remove an item which existed before but was removed
 */
void TestJsonDb::remove4()
{
    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("remove-test-type"));
    item.insert("remove-test", 100);

    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    QString uuid = result.value<QsonMap>(JsonDbString::kResultStr).valueString(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kUuidStr, uuid);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr, 1);

    result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);
    result = mJsonDb->remove(mOwner, item);
    verifyErrorResult(result);
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
    bool validate = gValidateSchemas;
    gValidateSchemas = true;

    QFETCH(QByteArray, schema);
    QFETCH(QByteArray, object);
    QFETCH(bool, result);

    static uint id = 0;
    id++;
    QString schemaName = QLatin1String("schemaValidationSchema") + QString::number(id);

    QsonMap schemaBody = readJson(schema);
    QsonMap schemaObject;
    schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    schemaObject.insert("name", schemaName);
    schemaObject.insert("schema", schemaBody);

    QsonMap qResult = mJsonDb->create(mOwner, schemaObject);
    verifyGoodResult(qResult);

    QsonMap item = readJson(object);
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


    gValidateSchemas = validate;
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
    bool validate = gValidateSchemas;
    gValidateSchemas = true;

    QFETCH(QByteArray, item);
    QFETCH(bool, isPerson);
    QFETCH(bool, isAdult);

    static int id = 0; // schema name id
    ++id;
    const QString personSchemaName = QString::fromLatin1("person") + QString::number(id);
    const QString adultSchemaName = QString::fromLatin1("adult") + QString::number(id);

    // init schemas
    QsonMap qResult;
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
        QsonMap personSchemaBody = readJson(person);
        QsonMap adultSchemaBody = readJson(adult);

        QsonMap personSchemaObject;
        personSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        personSchemaObject.insert("name", personSchemaName);
        personSchemaObject.insert("schema", personSchemaBody);

        QsonMap adultSchemaObject;
        adultSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        adultSchemaObject.insert("name", adultSchemaName);
        adultSchemaObject.insert("schema", adultSchemaBody);

        qResult = mJsonDb->create(mOwner, personSchemaObject);
        verifyGoodResult(qResult);
        qResult = mJsonDb->create(mOwner, adultSchemaObject);
        verifyGoodResult(qResult);
    }

    {
        QsonMap object = readJson(item);
        object.insert("testingForPerson", isPerson);
        object.insert(JsonDbString::kTypeStr, personSchemaName);
        qResult = mJsonDb->create(mOwner, object);
//        qDebug() << "person: " << qResult << isPerson << isAdult;
        if (isPerson) {
            verifyGoodResult(qResult);
        } else {
            verifyErrorResult(qResult);
        }
    }

    {
        QsonMap object = readJson(item);
        object.insert("testingForAdult", isAdult);
        object.insert(JsonDbString::kTypeStr, adultSchemaName);
        qResult = mJsonDb->create(mOwner, object);
//        qDebug() << "adult: " << qResult << isPerson << isAdult;
        if (isAdult) {
            verifyGoodResult(qResult);
        } else {
            verifyErrorResult(qResult);
        }
    }

    gValidateSchemas = validate;
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
    bool validate = gValidateSchemas;
    gValidateSchemas = true;

    QFETCH(QByteArray, item);
    QFETCH(bool, isValid);

    static int id = 0; // schema name id
    ++id;
    const QString amphibiousSchemaName = QString::fromLatin1("amphibious") + QString::number(id);
    const QString carSchemaName = QString::fromLatin1("car") + QString::number(id);
    const QString boatSchemaName = QString::fromLatin1("boat") + QString::number(id);

    // init schemas
    QsonMap qResult;
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

        QsonMap amphibiousSchemaBody = readJson(amphibious);
        QsonMap carSchemaBody = readJson(car);
        QsonMap boatSchemaBody = readJson(boat);


        QsonMap carSchemaObject;
        carSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        carSchemaObject.insert("name", carSchemaName);
        carSchemaObject.insert("schema", carSchemaBody);

        QsonMap boatSchemaObject;
        boatSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
        boatSchemaObject.insert("name", boatSchemaName);
        boatSchemaObject.insert("schema", boatSchemaBody);

        QsonMap amphibiousSchemaObject;
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
        QsonMap object = readJson(item);
        object.insert("testingForAmphibious", isValid);
        object.insert(JsonDbString::kTypeStr, amphibiousSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        if (isValid) {
            verifyGoodResult(qResult);
        } else {
            verifyErrorResult(qResult);
        }
    }

    gValidateSchemas = validate;
}

void TestJsonDb::schemaValidationLazyInit()
{
    bool validate = gValidateSchemas;
    gValidateSchemas = true;

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

    QsonMap personSchemaBody = readJson(person);
    QsonMap adultSchemaBody = readJson(adult);

    QsonMap personSchemaObject;
    personSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    personSchemaObject.insert("name", personSchemaName);
    personSchemaObject.insert("schema", personSchemaBody);

    QsonMap adultSchemaObject;
    adultSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    adultSchemaObject.insert("name", adultSchemaName);
    adultSchemaObject.insert("schema", adultSchemaBody);

    // Without lazy compilation this operation fails, because adult schema referece unexisting yet
    // person schema
    QsonMap qResult;
    qResult = mJsonDb->create(mOwner, adultSchemaObject);
    verifyGoodResult(qResult);
    qResult = mJsonDb->create(mOwner, personSchemaObject);
    verifyGoodResult(qResult);

    // Insert some objects to force full schema compilation
    {
        const QByteArray item = "{ \"name\":\"Nierob\", \"age\":99 }";
        QsonMap object = readJson(item);
        object.insert(JsonDbString::kTypeStr, adultSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyGoodResult(qResult);
    }
    {
        const QByteArray item = "{ \"name\":\"Nierob\", \"age\":12 }";
        QsonMap object = readJson(item);
        object.insert(JsonDbString::kTypeStr, adultSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyErrorResult(qResult);
    }
    {
        const QByteArray item = "{ \"age\":19 }";
        QsonMap object = readJson(item);
        object.insert(JsonDbString::kTypeStr, adultSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyErrorResult(qResult);
    }
    {
        const QByteArray item = "{ \"name\":\"Nierob\", \"age\":12 }";
        QsonMap object = readJson(item);
        object.insert(JsonDbString::kTypeStr, personSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyGoodResult(qResult);
    }
    {
        const QByteArray item = "{ \"age\":12 }";
        QsonMap object = readJson(item);
        object.insert(JsonDbString::kTypeStr, personSchemaName);
        qResult = mJsonDb->create(mOwner, object);
        verifyErrorResult(qResult);
    }

    gValidateSchemas = validate;
}

/*
 * Create a list of items
 */

#define LIST_TEST_ITEMS 6

void TestJsonDb::createList()
{
    QsonList list;
    for (int i = 0 ; i < LIST_TEST_ITEMS ; i++ ) {
        QsonMap map;
        map.insert(JsonDbString::kTypeStr, QLatin1String("create-list-type"));
        map.insert("create-list-test", i + 100);
        list.append(map);
    }

    QsonMap result = mJsonDb->createList(mOwner, list);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr, LIST_TEST_ITEMS);
}

/*
 * Create a list of items and then update them
 */

void TestJsonDb::updateList()
{
    QsonList list;
    for (int i = 0 ; i < LIST_TEST_ITEMS ; i++ ) {
        QsonMap map;
        map.insert(JsonDbString::kTypeStr, QLatin1String("update-list-type"));
        map.insert("update-list-test", i + 100);
        QsonMap result = mJsonDb->create(mOwner, map);
        verifyGoodResult(result);
        QString uuid = result.value<QsonMap>(JsonDbString::kResultStr).valueString(JsonDbString::kUuidStr);
        map.insert(JsonDbString::kUuidStr, uuid);
        map.insert("fuzzyduck", QLatin1String("Duck test"));
        list.append(map);
    }

    QsonMap result = mJsonDb->updateList(mOwner, list);
    verifyGoodResult(result);
    verifyResultField(result,JsonDbString::kCountStr, LIST_TEST_ITEMS);
}

void TestJsonDb::mapDefinition()
{
    // we need a schema that extends View for our targetType
    QsonMap schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QsonMap schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QsonMap schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    QsonMap mapDefinition;
    mapDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Map"));
    mapDefinition.insert("targetType", QLatin1String("MyViewType"));
    mapDefinition.insert("sourceType", QLatin1String("Contact"));
    mapDefinition.insert("map", QLatin1String("function map (c) { }"));

    QsonMap res = mJsonDb->create(mOwner, mapDefinition);
    verifyGoodResult(res);

    verifyGoodResult(mJsonDb->remove(mOwner, mapDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::mapDefinitionInvalid()
{
    // we need a schema that extends View for our targetType
    QsonMap schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QsonMap schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QsonMap schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    QsonMap mapDefinition;
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("sourceType", QLatin1String("Contact"));
    mapDefinition.insert("map", QLatin1String("function map (c) { }"));
    QsonMap res = mJsonDb->create(mOwner, mapDefinition);
    verifyErrorResult(res);

    mapDefinition = QsonMap();
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("targetType", QLatin1String("MyViewType"));
    mapDefinition.insert("map", QLatin1String("function map (c) { }"));
    res = mJsonDb->create(mOwner, mapDefinition);
    verifyErrorResult(res);

    mapDefinition = QsonMap();
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("targetType", QLatin1String("MyViewType"));
    mapDefinition.insert("sourceType", QLatin1String("Contact"));
    res = mJsonDb->create(mOwner, mapDefinition);
    verifyErrorResult(res);

    // fail because targetType doesn't extend View
    mapDefinition = QsonMap();
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("targetType", QLatin1String("MyViewType2"));
    mapDefinition.insert("sourceType", QLatin1String("Contact"));
    mapDefinition.insert("map", QLatin1String("function map (c) { }"));
    res = mJsonDb->create(mOwner, mapDefinition);
    verifyErrorResult(res);
    QVERIFY(res.subObject("error").valueString(JsonDbString::kMessageStr).contains("View"));

    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::reduceDefinition()
{
    // we need a schema that extends View for our targetType
    QsonMap schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QsonMap schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QsonMap schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    QsonMap reduceDefinition;
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    QsonMap res = mJsonDb->create(mOwner, reduceDefinition);
    verifyGoodResult(res);

    verifyGoodResult(mJsonDb->remove(mOwner, reduceDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::reduceDefinitionInvalid()
{
    // we need a schema that extends View for our targetType
    QsonMap schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QsonMap schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QsonMap schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    QsonMap reduceDefinition;
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    QsonMap res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    reduceDefinition = QsonMap();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    reduceDefinition = QsonMap();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    reduceDefinition = QsonMap();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    reduceDefinition = QsonMap();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);

    // fail because targetType doesn't extend View
    reduceDefinition = QsonMap();
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType2"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { }"));
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    res = mJsonDb->create(mOwner, reduceDefinition);
    verifyErrorResult(res);
    QVERIFY(res.subObject("error").valueString(JsonDbString::kMessageStr).contains("View"));

    //schemaRes.subObject("result")
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::mapInvalidMapFunc()
{
    QsonMap schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QsonMap schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QsonMap schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    QsonMap mapDefinition;
    mapDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kMapTypeStr);
    mapDefinition.insert("targetType", QLatin1String("MyViewType"));
    mapDefinition.insert("sourceType", QLatin1String("Contact"));
    mapDefinition.insert("map", QLatin1String("function map (c) { ;")); // non-parsable map function

    QsonMap defRes = mJsonDb->create(mOwner, mapDefinition);
    verifyGoodResult(defRes);
    QString uuid = defRes.subObject("result").valueString("_uuid");

    // force the view to be updated
    mJsonDb->updateView("MyViewType");

    // now check for an error
    QsonMap res = mJsonDb->getObjects("_uuid", uuid, JsonDbString::kMapTypeStr);
    QVERIFY(res.valueInt("count") > 0);
    mapDefinition = res.subList("result").objectAt(0);
    QVERIFY(!mapDefinition.isNull(JsonDbString::kActiveStr) && !mapDefinition.valueBool(JsonDbString::kActiveStr));
    QVERIFY(!mapDefinition.valueString(JsonDbString::kErrorStr).isEmpty());

    verifyGoodResult(mJsonDb->remove(mOwner, mapDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::reduceInvalidAddSubtractFuncs()
{
    QsonMap schema;
    schema.insert(JsonDbString::kTypeStr, QLatin1String("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QLatin1String("MyViewType"));
    QsonMap schemaSub;
    schemaSub.insert("type", QLatin1String("object"));
    schemaSub.insert("extends", QLatin1String("View"));
    schema.insert("schema", schemaSub);
    QsonMap schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    QsonMap reduceDefinition;
    reduceDefinition.insert(JsonDbString::kTypeStr, QLatin1String("Reduce"));
    reduceDefinition.insert("targetType", QLatin1String("MyViewType"));
    reduceDefinition.insert("sourceType", QLatin1String("Contact"));
    reduceDefinition.insert("sourceKeyName", QLatin1String("phoneNumber"));
    reduceDefinition.insert("add", QLatin1String("function add (k, z, c) { ;")); // non-parsable add function
    reduceDefinition.insert("subtract", QLatin1String("function subtract (k, z, c) { }"));
    QsonMap res = mJsonDb->create(mOwner, reduceDefinition);
    verifyGoodResult(res);

    mJsonDb->updateView("MyViewType");

    res = mJsonDb->getObjects("_uuid", res.subObject("result").valueString("_uuid"));
    reduceDefinition = res.subList("result").objectAt(0);
    QVERIFY(!reduceDefinition.isNull(JsonDbString::kActiveStr) && !reduceDefinition.valueBool(JsonDbString::kActiveStr));
    QVERIFY(!reduceDefinition.valueString(JsonDbString::kErrorStr).isEmpty());

    verifyGoodResult(mJsonDb->remove(mOwner, reduceDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::map()
{
    addIndex(QLatin1String("phoneNumber"));

  //gVerbose = true;
  //gDebug = true;
    QsonList objects(readJsonFile("map-reduce.json").toList());

    //qDebug() << "viewDefinitions" << viewDefinitions;
    QsonList mapsReduces;
    QsonList schemas;
    QMap<QString, QsonMap> toDelete;
    for (int i = 0; i < objects.size(); ++i) {
        QsonMap object = objects.objectAt(i);
        //qDebug() << "object" << object;
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);

        if (object.valueString(JsonDbString::kTypeStr) == JsonDbString::kMapTypeStr ||
            object.valueString(JsonDbString::kTypeStr) == JsonDbString::kReduceTypeStr)
            mapsReduces.append(object);
        else if (object.valueString(JsonDbString::kTypeStr) == JsonDbString::kSchemaTypeStr)
            schemas.append(object);
        else
            toDelete.insert(object.valueString("_uuid"), object);
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"Phone\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 5);

    // now remove one of the source items
    QsonMap query2;
    query2.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"Contact\"][?displayName=\"Nancy Doe\"]"));
    result = mJsonDb->find(mOwner, query2);
    QsonMap firstItem = result.subObject("result").subList("data").at<QsonMap>(0);
    QVERIFY(!firstItem.valueString("_uuid").isEmpty());
    toDelete.remove(firstItem.valueString("_uuid"));
    result = mJsonDb->remove(mOwner, firstItem);
    verifyGoodResult(result);

    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 3);

    QsonMap query3;
    query3.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"PhoneCount\"][/key]"));
    result = mJsonDb->find(mOwner, query3);
    verifyGoodResult(result);
    if (gVerbose)
        foreach (const QVariant v, qsonToVariant(result.subObject("result").subList("data")).toList()) {
            qDebug() << "    " << v;
        }
    QCOMPARE(result.subObject("result").subList("data").size(), 3);

    for (int i = 0; i < mapsReduces.size(); ++i) {
        QsonObject object = mapsReduces.at<QsonMap>(i);
        verifyGoodResult(mJsonDb->remove(mOwner, object));
    }
    for (int i = 0; i < schemas.size(); ++i) {
        QsonObject object = schemas.at<QsonMap>(i);
        verifyGoodResult(mJsonDb->remove(mOwner, object));
    }
    foreach (QsonMap map, toDelete.values())
        verifyGoodResult(mJsonDb->removeList(mOwner, map));
    //mJsonDb->removeIndex(QLatin1String("phoneNumber"));
}

void TestJsonDb::mapDuplicateSourceAndTarget()
{
    QsonList objects(readJsonFile("map-sametarget.json"));
    QsonList toDelete;
    QsonList maps;

    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.valueString(JsonDbString::kTypeStr) == "Map")
            maps.append(object);
        else
            toDelete.append(object);
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"ContactView\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 4);

    int firstNameCount = 0;
    QsonList data = result.subObject("result").subList("data");
    for (int ii = 0; ii < data.size(); ii++)
        if (!data.objectAt(ii).subObject("value").valueString("firstName").isEmpty())
            firstNameCount++;
    QCOMPARE(firstNameCount, 2);

    for (int ii = 0; ii < maps.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, maps.objectAt(ii)));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.objectAt(ii)));
    mJsonDb->removeIndex("ContactView");
}

void TestJsonDb::mapRemoval()
{
    QsonList objects(readJsonFile("map-sametarget.json"));

    QList<QsonMap> maps;
    QList<QsonMap> toDelete;

    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.valueString(JsonDbString::kTypeStr) == "Map")
            maps.append(object);
        else
            toDelete.append(object);
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"ContactView\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 4);

    // remove a map
    result = mJsonDb->remove(mOwner, maps.takeAt(0));
    verifyGoodResult(result);
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);

    // an now the other
    result = mJsonDb->remove(mOwner, maps.takeAt(0));
    verifyGoodResult(result);
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 0);

    for (int ii = 0; ii < maps.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, maps.at(ii)));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at(ii)));
}

void TestJsonDb::mapUpdate()
{
    QsonList objects(readJsonFile("map-sametarget.json"));

    QsonList maps;
    QsonList toDelete;

    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.valueString(JsonDbString::kTypeStr) == "Map")
            maps.append(object);
        else
            toDelete.append(object);
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"ContactView\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 4);

    // tinker with a map
    QsonMap map = maps.objectAt(0);
    map.insert("targetType", QString("ContactView2"));
    result = mJsonDb->update(mOwner, map);
    verifyGoodResult(result);

    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"ContactView2\"]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);

    for (int ii = 0; ii < maps.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, maps.objectAt(ii)));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.objectAt(ii)));

}

void TestJsonDb::mapJoin()
{
    //gVerbose = true;
    QsonList objects(readJsonFile("map-join.json").toList());

    QsonMap join;
    QsonMap schema;
    QsonList people;

    for (int i = 0; i < objects.size(); ++i) {
        QsonMap object = objects.at<QsonMap>(i);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        object.insert(JsonDbString::kUuidStr, result.subObject("result").valueString(JsonDbString::kUuidStr));

        if (object.valueString(JsonDbString::kTypeStr) == "Map")
            join = object;
        else if (object.valueString(JsonDbString::kTypeStr) == "_schemaType")
            schema = object;
        else
            people.append(object);
    }
    addIndex("friend", "string", "FoafPerson");
    addIndex("foaf", "string", "FoafPerson");
    addIndex("friend", "string", "Person");

    QsonMap result = mJsonDb->getObjects(JsonDbString::kTypeStr, "FoafPerson");
    //QsonMap join1 = mJsonDb->getObject(JsonDbString::kTypeStr, "Join");
    //qDebug() << "join" << join1;
    QCOMPARE(result.value<int>(JsonDbString::kCountStr), 0);

    // set some friends
    QString previous;
    for (int i = 0; i < people.size(); ++i) {
        QsonMap person = people.at<QsonMap>(i);
        if (!previous.isEmpty())
            person.insert("friend", previous);

        previous = person.valueString(JsonDbString::kUuidStr);
        //qDebug() << "person" << person.valueString("name") << person.valueString(JsonDbString::kUuidStr) << "friend" << person.valueString("friend");
        if (person.valueString(JsonDbString::kTypeStr) != "Person")
            qDebug() << "nonperson" << person;
        verifyGoodResult(mJsonDb->update(mOwner, person));
    }

    result = mJsonDb->getObjects(JsonDbString::kTypeStr, "Person");
    QsonList peopleWithFriends = result.value<QsonList>("result");

    QsonMap queryFoafPerson;
    // sort the list by key to make ordering deterministic
    queryFoafPerson.insert("query", QString::fromLatin1("[?_type=\"FoafPerson\"][?foaf exists][/friend]"));
    QsonMap findResult = mJsonDb->find(mOwner, queryFoafPerson).subObject("result");
    QCOMPARE(findResult.value<int>(JsonDbString::kLengthStr), people.count()-2); // one has no friend and one is friends with the one with no friends

    QsonList resultList = findResult.subList("data");
    for (int i = 0; i < resultList.size(); ++i) {
        QsonMap person = resultList.at<QsonMap>(i);
        QStringList sources = person.value<QsonList>("_sourceUuids").toStringList();
        QVERIFY(sources.count() == 1 || sources.contains(person.valueString("friend")));
    }

    // take the last person, find his friend, and remove that friend's friend property
    // then make sure the foaf is updated
    QsonMap p = resultList.at<QsonMap>(resultList.size()-1);
    if (p.isNull("foaf"))
      qDebug() << "p" << p << endl;
    QVERIFY(!p.isNull("foaf"));

    QsonMap fr;
    for (int i = 0; i < peopleWithFriends.size(); ++i) {
        QsonMap f = peopleWithFriends.at<QsonMap>(i);
        if (f.valueString(JsonDbString::kUuidStr) == p.valueString("friend")) {
            fr = f;
            break;
        }
    }

    QVERIFY(fr.valueString(JsonDbString::kUuidStr) == p.valueString("friend"));
    fr.insert("friend", QsonObject::NullValue);
    //qDebug() << "Removing friend from" << fr;
    verifyGoodResult(mJsonDb->update(mOwner, fr));

    QsonMap foafRes = mJsonDb->getObjects(JsonDbString::kTypeStr, "FoafPerson");
    QsonList foafs = foafRes.subList("result");
    if (0) {
        for (int i = 0; i < foafs.size(); i++) {
            QsonMap foaf = foafs.objectAt(i);
            qDebug() << "foaf"
                     << foaf.valueString("key")
                     << foaf.valueString("friend")
                     << foaf.valueString("foaf");
        }
        qDebug() << endl;
    }
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"FoafPerson\"][?name=\"%1\"]").arg(p.valueString("name")));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").value<int>("length"), 1);

    p = result.subObject("result").subList("data").at<QsonMap>(0);
    QVERIFY(!p.valueString("friend").isEmpty());
    QVERIFY(p.isNull("foaf"));

    query = QsonMap();
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"FoafPerson\"][/key]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    if (0) {
        qDebug() << endl << result.subObject("result").value<int>("length");
        for (int i = 0; i < result.subObject("result").value<int>("length"); i++) {
            QsonMap foaf = result.subObject("result").subList("data").objectAt(i);
            qDebug() << "unsorted foaf" << foaf.valueString("key") << foaf;
        }
        qDebug() << endl;
    }
    //QCOMPARE(result.subObject("result").value<int>("length"), 10);

    query = QsonMap();
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"FoafPerson\"][/friend]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    if (0) {
        qDebug() << endl << result.subObject("result").value<int>("length");
        for (int i = 0; i < result.subObject("result").value<int>("length"); i++) {
            qDebug() << "sorted foaf" << result.subObject("result").subList("data").objectAt(i);
        }
        qDebug() << endl;
    }
    QCOMPARE(result.subObject("result").value<int>("length"), 8); // two have no friends

    verifyGoodResult(mJsonDb->remove(mOwner, join));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
    for (int i = 0; i < people.size(); ++i) {
        QsonMap object = people.at<QsonMap>(i);
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

    QsonList objects(readJsonFile("map-join-sourceuuids.json").toList());
    QsonList toDelete;
    QsonMap toUpdate;

    for (int i = 0; i < objects.size(); ++i) {
        QsonMap object = objects.at<QsonMap>(i);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        object.insert(JsonDbString::kUuidStr, result.subObject("result").valueString(JsonDbString::kUuidStr));
        toDelete.append(object);

        if (object.valueString(JsonDbString::kTypeStr) == "Bar")
            toUpdate = object;
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MagicView\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);

    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).subList("_sourceUuids").count(), 2);

    toUpdate.insert("extra", QString("123123"));
    verifyGoodResult(mJsonDb->update(mOwner, toUpdate));

    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);

    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).subList("_sourceUuids").count(), 2);
    for (int i = toDelete.count() - 1; i >= 0; i--)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at<QsonMap>(i)));
    mJsonDb->removeIndex("magic", "string");
}

void TestJsonDb::mapMapFunctionError()
{
    QsonMap schema;
    schema.insert(JsonDbString::kTypeStr, QString("_schemaType"));
    schema.insert(JsonDbString::kNameStr, QString("MyViewType"));
    QsonMap schemaSub;
    schemaSub.insert("type", QString("object"));
    schemaSub.insert("extends", QString("View"));
    schema.insert("schema", schemaSub);
    QsonMap schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    QsonMap mapDefinition;
    mapDefinition.insert(JsonDbString::kTypeStr, QString("Map"));
    mapDefinition.insert("targetType", QString("MyViewType"));
    mapDefinition.insert("sourceType", QString("Contact"));
    mapDefinition.insert("map", QString("function map (c) { invalidobject.fail(); };")); // error in map function

    QsonMap defRes = mJsonDb->create(mOwner, mapDefinition);
    verifyGoodResult(defRes);

    QsonMap contact;
    contact.insert(JsonDbString::kTypeStr, QString("Contact"));
    QsonMap res = mJsonDb->create(mOwner, contact);

    // trigger the view update
    mJsonDb->updateView("MyViewType");

    // see if the map definition is still active
    res = mJsonDb->getObjects("_uuid", defRes.subObject("result").valueString("_uuid"));
    mapDefinition = res.subList("result").objectAt(0);
    QVERIFY(!mapDefinition.isNull(JsonDbString::kActiveStr) && !mapDefinition.valueBool(JsonDbString::kActiveStr));
    QVERIFY(mapDefinition.valueString(JsonDbString::kErrorStr).contains("invalidobject"));

    verifyGoodResult(mJsonDb->remove(mOwner, mapDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::mapSchemaViolation()
{
    bool validate = gValidateSchemas;
    gValidateSchemas = true;

    QsonMap contactsRes = mJsonDb->getObjects(JsonDbString::kTypeStr, "Contact");
    if (contactsRes.valueInt("count") > 0)
        verifyGoodResult(mJsonDb->removeList(mOwner, contactsRes.subList("result")));

    QsonList objects(readJsonFile("map-reduce-schema.json"));
    QsonList toDelete;
    QString workingMap;
    QsonMap map;

    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        if (object.valueString(JsonDbString::kTypeStr) != JsonDbString::kReduceTypeStr) {

            // use the broken Map function that creates schema violations
            if (object.valueString(JsonDbString::kTypeStr) == JsonDbString::kMapTypeStr) {
                workingMap = object.valueString("map");
                object.insert("map", object.valueString("brokenMap"));
            }

            QsonMap result = mJsonDb->create(mOwner, object);
            if (object.valueString(JsonDbString::kTypeStr) == JsonDbString::kMapTypeStr) {
                map = object;
            }

            verifyGoodResult(result);
            if (object.valueString(JsonDbString::kTypeStr) != JsonDbString::kMapTypeStr)
                toDelete.append(object);
        }
    }

    mJsonDb->updateView(map.valueString("targetType"));

    QsonMap mapDefinition = mJsonDb->getObjects("_uuid", map.valueString(JsonDbString::kUuidStr));
    QCOMPARE(mapDefinition.subList("result").size(), 1);
    mapDefinition = mapDefinition.subList("result").objectAt(0);
    QVERIFY(!mapDefinition.isNull(JsonDbString::kActiveStr) && !mapDefinition.valueBool(JsonDbString::kActiveStr));
    QVERIFY(mapDefinition.valueString(JsonDbString::kErrorStr).contains("Schema"));

    QsonMap res = mJsonDb->getObjects(JsonDbString::kTypeStr, "Phone");
    QCOMPARE(res.value<int>(JsonDbString::kCountStr), 0);
    // fix the map function
    map.insert("map", workingMap);
    map.insert(JsonDbString::kActiveStr, true);
    map.insert(JsonDbString::kErrorStr, QsonObject::NullValue);

    verifyGoodResult(mJsonDb->update(mOwner, map));

    mJsonDb->updateView(map.valueString("targetType"));

    mapDefinition = mJsonDb->getObjects("_uuid", map.valueString(JsonDbString::kUuidStr));
    mapDefinition = mapDefinition.subList("result").objectAt(0);
    QVERIFY(mapDefinition.isNull(JsonDbString::kActiveStr)|| mapDefinition.valueBool(JsonDbString::kActiveStr));
    QVERIFY(mapDefinition.isNull(JsonDbString::kErrorStr) || mapDefinition.valueString(JsonDbString::kErrorStr).isEmpty());

    res = mJsonDb->getObjects(JsonDbString::kTypeStr, "Phone");
    QCOMPARE(res.value<int>(JsonDbString::kCountStr), 5);

    verifyGoodResult(mJsonDb->remove(mOwner, map));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.objectAt(ii)));

    gValidateSchemas = validate;
}

void TestJsonDb::reduce()
{
    QsonList objects(readJsonFile("reduce-data.json"));

    QsonList toDelete;
    QsonList reduces;

    QHash<QString, int> firstNameCount;
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        firstNameCount[object.valueString("firstName")]++;
        toDelete.append(object);
    }

    objects = readJsonFile("reduce.json");
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.valueString(JsonDbString::kTypeStr) == "Reduce")
            reduces.append(object);
        else
            toDelete.append(object);
    }

    QsonMap query, result;
    QsonList data;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), firstNameCount.keys().count());

    data = result.subObject("result").subList("data");
    for (int ii = 0; ii < data.size(); ii++) {
        QCOMPARE(data.objectAt(ii).subObject("value").value<int>("count"), firstNameCount[data.objectAt(ii).valueString("key")]);
    }
    for (int ii = 0; ii < reduces.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, reduces.objectAt(ii)));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.objectAt(ii)));
    mJsonDb->removeIndex("MyContactCount");
}

void TestJsonDb::reduceRemoval()
{
    QsonList objects(readJsonFile("reduce-data.json"));

    QsonList toDelete;
    QHash<QString, int> firstNameCount;
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        firstNameCount[object.valueString("firstName")]++;
        toDelete.append(object);
    }

    objects = readJsonFile("reduce.json");
    QsonMap reduce;
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.valueString(JsonDbString::kTypeStr) == "Reduce")
            reduce = object;
        else
            toDelete.append(object);
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), firstNameCount.keys().count());

    result = mJsonDb->remove(mOwner, reduce);
    verifyGoodResult(result);
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 0);

    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.objectAt(ii)));
    mJsonDb->removeIndex("MyContactCount");
}

void TestJsonDb::reduceUpdate()
{
    QsonList objects(readJsonFile("reduce-data.json"));

    QsonList toDelete;
    QHash<QString, int> firstNameCount;
    QHash<QString, int> lastNameCount;
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        firstNameCount[object.valueString("firstName")]++;
        lastNameCount[object.valueString("lastName")]++;
        toDelete.append(object);
    }

    objects = readJsonFile("reduce.json");
    QsonMap reduce;
    QsonMap schema;
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.valueString(JsonDbString::kTypeStr) == "Reduce")
            reduce = object;
        else if (object.valueString(JsonDbString::kTypeStr) == JsonDbString::kSchemaTypeStr)
            schema = object;
        else
            toDelete.append(object);
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QsonList data = result.subObject("result").subList("data");
    QCOMPARE(data.size(), firstNameCount.keys().count());

    for (int ii = 0; ii < data.size(); ii++)
        QCOMPARE(data.objectAt(ii).subObject("value").value<int>("count"), firstNameCount[data.objectAt(ii).valueString("key")]);

    reduce.insert("sourceKeyName", QString("lastName"));
    result = mJsonDb->update(mOwner, reduce);

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    data = result.subObject("result").subList("data");

    QCOMPARE(result.subObject("result").subList("data").size(), lastNameCount.keys().count());

    data = result.subObject("result").subList("data");
    for (int ii = 0; ii < data.size(); ii++)
        QCOMPARE(data.objectAt(ii).subObject("value").value<int>("count"), lastNameCount[data.objectAt(ii).valueString("key")]);

    verifyGoodResult(mJsonDb->remove(mOwner, reduce));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.objectAt(ii)));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
    mJsonDb->removeIndex("MyContactCount");
}

void TestJsonDb::reduceDuplicate()
{
    QsonList objects(readJsonFile("reduce-data.json"));

    QsonList toDelete;
    QHash<QString, int> firstNameCount;
    QHash<QString, int> lastNameCount;
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        firstNameCount[object.valueString("firstName")]++;
        lastNameCount[object.valueString("lastName")]++;
        toDelete.append(object);
    }

    objects = readJsonFile("reduce.json");
    QsonMap reduce;
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        if (object.valueString(JsonDbString::kTypeStr) == "Reduce")
            reduce = object;
        else
            toDelete.append(object);
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), firstNameCount.keys().count());

    QsonList data = result.subObject("result").subList("data");
    for (int ii = 0; ii < data.size(); ii++) {
        QsonMap object = data.objectAt(ii);
        QCOMPARE(object.subObject("value").value<int>("count"), firstNameCount[object.valueString("key")]);
    }

    QsonMap reduce2;
    reduce2.insert(JsonDbString::kTypeStr, reduce.valueString(JsonDbString::kTypeStr));
    reduce2.insert("targetType", reduce.valueString("targetType"));
    reduce2.insert("sourceType", reduce.valueString("sourceType"));
    reduce2.insert("sourceKeyName", QString("lastName"));
    reduce2.insert("add", reduce.valueString("add"));
    reduce2.insert("subtract", reduce.valueString("subtract"));
    result = mJsonDb->create(mOwner, reduce2);
    verifyGoodResult(result);

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"MyContactCount\"]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), lastNameCount.keys().count() + firstNameCount.keys().count());

    data = result.subObject("result").subList("data");
    for (int ii = 0; ii < data.size(); ii++) {
        QsonMap object = data.objectAt(ii);
        QVERIFY(object.subObject("value").value<int>("count") == firstNameCount[object.valueString("key")]
                || object.subObject("value").value<int>("count") == lastNameCount[object.valueString("key")]);
    }

    verifyGoodResult(mJsonDb->remove(mOwner, reduce));
    verifyGoodResult(mJsonDb->remove(mOwner, reduce2));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.objectAt(ii)));
    mJsonDb->removeIndex("MyContactCount");
}

void TestJsonDb::reduceFunctionError()
{
    QsonMap schema;
    QString viewTypeStr("ReduceFunctionErrorView");
    schema.insert(JsonDbString::kTypeStr, QString("_schemaType"));
    schema.insert(JsonDbString::kNameStr, viewTypeStr);
    QsonMap schemaSub;
    schemaSub.insert("type", QString("object"));
    schemaSub.insert("extends", QString("View"));
    schema.insert("schema", schemaSub);
    QsonMap schemaRes = mJsonDb->create(mOwner, schema);
    verifyGoodResult(schemaRes);

    QsonMap reduceDefinition;
    reduceDefinition.insert(JsonDbString::kTypeStr, QString("Reduce"));
    reduceDefinition.insert("targetType", viewTypeStr);
    reduceDefinition.insert("sourceType", QString("Contact"));
    reduceDefinition.insert("sourceKeyName", QString("phoneNumber"));
    reduceDefinition.insert("add", QString("function add (k, z, c) { invalidobject.test(); }")); // invalid add function
    reduceDefinition.insert("subtract", QString("function subtract (k, z, c) { }"));
    QsonMap defRes = mJsonDb->create(mOwner, reduceDefinition);
    verifyGoodResult(defRes);

    QsonMap contact;
    contact.insert(JsonDbString::kTypeStr, QString("Contact"));
    contact.insert("phoneNumber", QString("+1234567890"));
    QsonMap res = mJsonDb->create(mOwner, contact);
    verifyGoodResult(res);

    mJsonDb->updateView(viewTypeStr);
    res = mJsonDb->getObjects("_uuid", defRes.subObject("result").valueString("_uuid"), JsonDbString::kReduceTypeStr);
    reduceDefinition = res.subList("result").objectAt(0);
    QVERIFY(!reduceDefinition.valueBool(JsonDbString::kActiveStr, false));
    QVERIFY(reduceDefinition.valueString(JsonDbString::kErrorStr).contains("invalidobject"));

    verifyGoodResult(mJsonDb->remove(mOwner, reduceDefinition));
    verifyGoodResult(mJsonDb->remove(mOwner, schema));
}

void TestJsonDb::reduceSchemaViolation()
{
    bool validate = gValidateSchemas;
    gValidateSchemas = true;

    QsonList objects(readJsonFile("map-reduce-schema.json"));

    QsonList toDelete;
    QsonMap map;
    QsonMap reduce;
    QString workingAdd;

    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap object = objects.objectAt(ii);

        if (object.valueString(JsonDbString::kTypeStr) == JsonDbString::kReduceTypeStr) {
            // use the broken add function that creates schema violations
            workingAdd = object.valueString("add");
            object.insert("add", object.valueString("brokenAdd"));
        }

        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);

        if (object.valueString(JsonDbString::kTypeStr) == JsonDbString::kMapTypeStr) {
            map = object;
        } else if (object.valueString(JsonDbString::kTypeStr) == JsonDbString::kReduceTypeStr) {
            reduce = object;
        } else if (object.valueString(JsonDbString::kTypeStr) != JsonDbString::kMapTypeStr &&
                   object.valueString(JsonDbString::kTypeStr) != JsonDbString::kMapTypeStr)
            toDelete.append(object);

    }

    mJsonDb->updateView(reduce.valueString("targetType"));

    QsonMap reduceDefinition = mJsonDb->getObjects("_uuid", reduce.valueString(JsonDbString::kUuidStr));
    QCOMPARE(reduceDefinition.subList("result").size(), 1);
    reduceDefinition = reduceDefinition.subList("result").objectAt(0);
    QVERIFY(!reduceDefinition.isNull(JsonDbString::kActiveStr) && !reduceDefinition.valueBool(JsonDbString::kActiveStr));
    QVERIFY(reduceDefinition.valueString(JsonDbString::kErrorStr).contains("Schema"));

    QsonMap res = mJsonDb->getObjects(JsonDbString::kTypeStr, "PhoneCount");
    QCOMPARE(res.value<int>(JsonDbString::kCountStr), 0);

    // fix the add function
    reduce.insert("add", workingAdd);
    reduce.insert(JsonDbString::kActiveStr, true);
    reduce.insert(JsonDbString::kErrorStr, QsonObject::NullValue);

    verifyGoodResult(mJsonDb->update(mOwner, reduce));

    mJsonDb->updateView(reduce.valueString("targetType"));

    reduceDefinition = mJsonDb->getObjects("_uuid", reduce.valueString(JsonDbString::kUuidStr));
    reduceDefinition = reduceDefinition.subList("result").objectAt(0);
    QVERIFY(reduceDefinition.isNull(JsonDbString::kActiveStr)|| reduceDefinition.valueBool(JsonDbString::kActiveStr));
    QVERIFY(reduceDefinition.isNull(JsonDbString::kErrorStr) || reduceDefinition.valueString(JsonDbString::kErrorStr).isEmpty());

    res = mJsonDb->getObjects(JsonDbString::kTypeStr, "PhoneCount");
    QCOMPARE(res.value<int>(JsonDbString::kCountStr), 4);

    verifyGoodResult(mJsonDb->remove(mOwner, reduce));
    verifyGoodResult(mJsonDb->remove(mOwner, map));
    for (int ii = 0; ii < toDelete.size(); ii++)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.objectAt(ii)));

    gValidateSchemas = validate;
}

void TestJsonDb::reduceSubObjectProp()
{
    QsonList objects(readJsonFile("reduce-subprop.json").toList());

    QsonList toDelete;
    QsonObject reduce;

    QHash<QString, int> firstNameCount;
    for (int i = 0; i < objects.size(); ++i) {
        QsonMap object = objects.at<QsonMap>(i);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);

        if (object.valueString(JsonDbString::kTypeStr) == "Reduce") {
            reduce = object;
        } else {
            if (object.valueString(JsonDbString::kTypeStr) == "Contact")
                firstNameCount[object.subObject("name").valueString("firstName")]++;
            toDelete.append(object);
        }
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"NameCount\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), firstNameCount.keys().count());

    QsonList data = result.subObject("result").subList("data");
    for (int i = 0; i < data.size(); ++i) {
        QsonMap object = data.at<QsonMap>(i);
        QCOMPARE(object.subObject("value").value<int>("count"), firstNameCount[object.valueString("key")]);
    }

    verifyGoodResult(mJsonDb->remove(mOwner, reduce));
    for (int i = 0; i < toDelete.size(); ++i) {
        QsonMap object = toDelete.at<QsonMap>(i);
        verifyGoodResult(mJsonDb->remove(mOwner, object));
    }
    mJsonDb->removeIndex("NameCount");
}

void TestJsonDb::reduceArray()
{
    QsonList objects(readJsonFile("reduce-array.json").toList());
    QsonList toDelete;

    QsonMap human;

    for (int i = 0; i < objects.count(); i++) {
        QsonMap object = objects.at<QsonMap>(i);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        object.insert(JsonDbString::kUuidStr, result.subObject("result").valueString(JsonDbString::kUuidStr));
        toDelete.append(object);

        if (object.valueString("firstName") == "Julio")
            human = object;
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ArrayView\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);

    QsonList results = result.subObject("result").subList("data");
    QCOMPARE(results.count(), 2);

    for (int i = 0; i < results.count(); i++) {
        QsonList firstNames = results.at<QsonMap>(i).subObject("value").subList("firstNames");
        QCOMPARE(firstNames.count(), 2);

        for (int j = 0; j < firstNames.count(); j++)
            QVERIFY(!firstNames.at<QString>(j).isEmpty());
    }

    human.insert("lastName", QString("Johnson"));
    verifyGoodResult(mJsonDb->update(mOwner, human));

    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ArrayView\"][?key=\"Jones\"]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);

    results = result.subObject("result").subList("data");
    QCOMPARE(results.at<QsonMap>(0).subObject("value").subList("firstNames").count(), 1);

    for (int i = toDelete.count() - 1; i >= 0; i--)
        verifyGoodResult(mJsonDb->remove(mOwner, toDelete.at<QsonMap>(i)));
    mJsonDb->removeIndex("ArrayView");
}

void TestJsonDb::changesSinceCreate()
{
    QsonMap csReq;
    csReq.insert("stateNumber", 0);
    QsonMap csRes = mJsonDb->changesSince(mOwner, csReq);
    verifyGoodResult(csRes);
    int state = csRes.subObject("result").value<int>("currentStateNumber");
    QVERIFY(state >= 0);

    QsonMap toCreate;
    toCreate.insert("_type", QString("TestContact"));
    toCreate.insert("firstName", QString("John"));
    toCreate.insert("lastName", QString("Doe"));

    QsonMap crRes = mJsonDb->create(mOwner, toCreate);
    verifyGoodResult(crRes);

    csReq.insert("stateNumber", state);
    csRes = mJsonDb->changesSince(mOwner, csReq);
    verifyGoodResult(csRes);

    QVERIFY(csRes.subObject("result").value<int>("currentStateNumber") > state);
    QCOMPARE(csRes.subObject("result").value<int>("count"), 1);

    QsonMap after = csRes.subObject("result").subList("changes").objectAt(0).subObject("after");
    QCOMPARE(after.valueString("_type"), toCreate.valueString("_type"));
    QCOMPARE(after.valueString("firstName"), toCreate.valueString("firstName"));
    QCOMPARE(after.valueString("lastName"), toCreate.valueString("lastName"));
}

void TestJsonDb::addIndex()
{
    addIndex(QLatin1String("subject"));

    QsonMap indexObject;
    indexObject.insert(JsonDbString::kTypeStr, QLatin1String("Index"));
    indexObject.insert("propertyName", QLatin1String("predicate"));
    indexObject.insert("propertyType", QLatin1String("string"));

    QsonMap result = mJsonDb->create(mOwner, indexObject);
    verifyGoodResult(result);
    QVERIFY(mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->findObjectTable(JsonDbString::kSchemaTypeStr)->indexSpec("predicate") != 0);
    mJsonDb->remove(mOwner, indexObject);
}

void TestJsonDb::addSchema()
{
    QsonMap s;
    addSchema("contact", s);
    verifyGoodResult(mJsonDb->remove(mOwner, s));
}

void TestJsonDb::unindexedFind()
{
    QsonMap item;
    item.insert("_type", QLatin1String("unindexedFind"));
    item.insert("subject", QString("Programming Languages"));
    item.insert("bar", 10);
    QsonMap createResult = mJsonDb->create(mOwner, item);
    verifyGoodResult(createResult);

    QsonMap request;
    request.insert("query", QString("[?bar=10]"));
    QsonMap result = mJsonDb->find(mOwner, request);
    int extraneous = 0;
    QsonList data = result.subList("data");
    for (int i = 0; i < data.size(); ++i) {
        QsonMap map = data.at<QsonMap>(i);
        if (!map.contains("bar") || (map.value<int>("bar") != 10)) {
            extraneous++;
        }
    }

    verifyGoodResult(result);
    QsonMap map = result.subObject(JsonDbString::kResultStr);
    QVERIFY(map.contains("length"));
    QVERIFY((map.value<int>("length") >= 1) && !extraneous);
    mJsonDb->removeIndex("bar");
    mJsonDb->remove(mOwner, item);
}


void TestJsonDb::find1()
{
    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QString("Find1Type"));
    item.insert("find1", QString("Foobar!"));
    mJsonDb->create(mOwner, item);

    QsonMap query;
    query.insert("query", QString(".*"));
    QsonMap result = mJsonDb->find(mOwner, query);

    verifyGoodResult(result);
    QsonMap map = result.subObject(JsonDbString::kResultStr);
    QVERIFY(map.contains("length"));
    QVERIFY(map.value<int>("length") >=  1);
    QVERIFY(map.contains("data"));
    QVERIFY(map.subList("data").size() >= 1);
}

void TestJsonDb::find2()
{
    addIndex(QLatin1String("name"));
    addIndex(QLatin1String("_type"));

    QsonList toDelete;

    QsonMap item;
    item.insert("name", QString("Wilma"));
    item.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    toDelete.append(item);

    item = QsonObject();
    item.insert("name", QString("Betty"));
    item.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);
    toDelete.append(item);

    int extraneous;

    QsonMap query;
    query.insert("query", QString("[?name=\"Wilma\"][?_type=%type]"));
    QsonMap bindings;
    bindings.insert("type", QString(__FUNCTION__));
    query.insert("bindings", bindings);
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));

    extraneous = 0;
    foreach (QVariant item, qsonToVariant(result.subList("data")).toList()) {
        QsonMap map = QsonMap(variantToQson(item));
        if (!map.contains("name") || (map.valueString("name") != QLatin1String("Wilma")))
            extraneous++;
    }
    verifyGoodResult(result);
    QsonMap map = result.subObject(JsonDbString::kResultStr);
    QVERIFY(map.contains("length"));
    QVERIFY(map.value<int>("length") >= 1);
    QVERIFY(!extraneous);


    query.insert("query", QString("[?_type=%type]"));
    result = mJsonDb->find(mOwner, query);

    extraneous = 0;
    foreach (QVariant item, qsonToVariant(result.subList("data")).toList()) {
        QsonMap map = QsonMap(variantToQson(item));
        if (!map.contains(JsonDbString::kTypeStr) || (map.valueString(JsonDbString::kTypeStr) != kContactStr))
            extraneous++;
    }
    verifyGoodResult(result);
    map = result.subObject(JsonDbString::kResultStr);
    QVERIFY(map.contains("length"));
    QVERIFY(map.value<int>("length") >= 1);
    QVERIFY(!extraneous);

    query.insert("query", QString("[?name=\"Wilma\"][?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg(__FUNCTION__));
    result = mJsonDb->find(mOwner, query);

    extraneous = 0;
    foreach (QVariant item, qsonToVariant(result.subList("data")).toList()) {
        QsonMap map = QsonMap(variantToQson(item.toMap()));
        if (!map.contains("name") || (map.valueString("name") != QLatin1String("Wilma"))
            || !map.contains(JsonDbString::kTypeStr) || (map.valueString(JsonDbString::kTypeStr) != kContactStr)
                )
            extraneous++;
    }

    verifyGoodResult(result);
    map = result.subObject(JsonDbString::kResultStr);
    QVERIFY(map.contains("length"));
    QVERIFY(map.value<int>("length") >= 1);
    QVERIFY(!extraneous);

    for (int ii = 0; ii < toDelete.size(); ii++) {
        mJsonDb->remove(mOwner, toDelete.objectAt(ii));
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
        QsonMap item;
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

    QsonMap query;
    query.insert("query", q);
    QsonMap result = mJsonDb->find(mOwner, query);
    int length = result.subObject("result").value<int>(QLatin1String("length"));
    verifyGoodResult(result);

    QStringList matches;
    foreach (QVariant v, qsonToVariant(result.subObject("result").subList("data")).toList()) {
        QVariantMap m = v.toMap();
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
        QsonMap item;
        item.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
        item.insert(__FUNCTION__, QString("Find Me!"));
        item.insert("stringlist", variantToQson(stringLists[i]));
        item.insert("intlist", variantToQson(intLists[i]));
        item.insert("str", variantToQson(stringLists[i][0]));
        item.insert("i", variantToQson(intLists[i].toList().at(0)));
        mJsonDb->create(mOwner, item);
    }

    QStringList queries = (QStringList()
                           << "[?stringlist contains \"fred\"]"
                           << "[?intlist contains 22]"
                           << "[?str in [\"fred\", \"barney\"]]"
                           << "[?i in [\"1\", \"22\"]]"
        );

    foreach (QString q, queries) {
        QsonMap query;
        query.insert("query", q);
        QsonMap result = mJsonDb->find(mOwner, query);
        //qDebug() << "result.length" << result.value("result").toMap().value(QLatin1String("length")).toInt();

        verifyGoodResult(result);
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

    QsonMap item;
    item.insert("firstName", QString("Wilma"));
    item.insert("lastName", QString("Flintstone"));
    item.insert(JsonDbString::kTypeStr, kContactStr);
    mJsonDb->create(mOwner, item);

    item = QsonObject();
    item.insert("firstName", QString("Betty"));
    item.insert("lastName", QString("Rubble"));
    item.insert(JsonDbString::kTypeStr, kContactStr);
    mJsonDb->create(mOwner, item);

    QsonMap query, result, map;

    query.insert("query", QString("[?firstName=\"Wilma\"][=firstName]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    result = result.subObject("result");
    QCOMPARE(result.value<int>("length"), 1);
    QCOMPARE(result.subList("data").stringAt(0), QString("Wilma"));

    query.insert("query", QString("[?firstName=\"Wilma\"][= [firstName,lastName]]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    result = result.subObject("result");
    QCOMPARE(result.value<int>("length"), 1);
    QCOMPARE(result.subList("data").size(), 1);
    QsonList data = result.subList("data").listAt(0);
    QCOMPARE(result.subList("data").size(), 1);
    QCOMPARE(data.stringAt(0), QString("Wilma"));
    QCOMPARE(data.stringAt(1), QString("Flintstone"));
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

    QsonMap item1;
    item1.insert("orderedFindName", QString("Wilma"));
    item1.insert(JsonDbString::kTypeStr, QLatin1String("orderedFind1"));
    mJsonDb->create(mOwner, item1);

    QsonMap item2;
    item2.insert("orderedFindName", QString("BamBam"));
    item2.insert(JsonDbString::kTypeStr, QLatin1String("orderedFind1"));
    mJsonDb->create(mOwner, item2);

    QsonMap item3;
    item3.insert("orderedFindName", QString("Betty"));
    item3.insert(JsonDbString::kTypeStr, QLatin1String("orderedFind1"));
    mJsonDb->create(mOwner, item3);
}

void TestJsonDb::orderedFind1()
{
    QFETCH(QString, order);

    QsonMap query;
    query.insert("query", QString("[?_type=\"orderedFind1\"][%3orderedFindName]").arg(order));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QVERIFY(result.subObject("result").value<int>(QLatin1String("length")) > 0);

    QStringList names;
    QsonList data = result.subObject("result").subList("data");
    for (int ii = 0; ii < data.size(); ii++) {
        names.append(data.objectAt(ii).valueString("orderedFindName"));
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
        QsonMap item;
        item.insert(JsonDbString::kTypeStr, QLatin1String("orderedFind2"));
        item.insert(QLatin1String("foobar"), QString("%1_orderedFind2").arg(prefix));
        QsonMap r = mJsonDb->create(mOwner, item);
    }
}

void TestJsonDb::orderedFind2()
{
    QFETCH(QString, order);
    QFETCH(QString, field);

    //mJsonDb->checkValidity();

    QsonMap query;
    query.insert("query", QString("[?_type=\"orderedFind2\"][%1%2]").arg(order).arg(field));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QVERIFY(result.subObject("result").value<int>(QLatin1String("length")) > 0);

    QStringList names;
    QsonList data = result.subObject("result").subList("data");
    for (int ii = 0; ii < data.size(); ii++) {
        names.append(data.objectAt(ii).valueString(field));
    }
    QStringList orderedNames = names;
    qSort(orderedNames.begin(), orderedNames.end());
    QStringList disorderedNames = names;
    qSort(disorderedNames.begin(), disorderedNames.end(), qGreater<QString>());
    if (order == "/") {
        if (!(names == orderedNames)
                || !(names != disorderedNames))
            mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->checkIndex(field);
        QVERIFY(names == orderedNames);
        QVERIFY(names != disorderedNames);
    } else {
        if (!(names != orderedNames)
                || !(names == disorderedNames))
            mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->checkIndex(field);
        QVERIFY(names != orderedNames);
        QVERIFY(names == disorderedNames);
    }
}

void TestJsonDb::wildcardIndex()
{
    addIndex("telephoneNumbers.*.number");
    QsonMap item;
    item.insert(JsonDbString::kTypeStr, kContactStr);
    item.insert("name", QString("BamBam"));

    QsonMap mobileNumber;
    QString mobileNumberString = "+15515512323";
    mobileNumber.insert("type", QString("mobile"));
    mobileNumber.insert("number", mobileNumberString);
    QsonList telephoneNumbers;
    telephoneNumbers.append(mobileNumber);
    item.insert("telephoneNumbers", telephoneNumbers);

    mJsonDb->create(mOwner, item);

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?telephoneNumbers.*.number=\"%1\"]").arg(mobileNumberString));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);

    query.insert(JsonDbString::kQueryStr, QString("[?%1=\"%2\"][= .telephoneNumbers[*].number]").arg(JsonDbString::kTypeStr).arg(kContactStr));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    mJsonDb->removeIndex("telephoneNumbers.*.number");
}

void TestJsonDb::uuidJoin()
{
    addIndex("name");
    addIndex("thumbnailUuid");
    addIndex("url");
    QString thumbnailUrl = "file:thumbnail.png";
    QsonMap thumbnail;
    thumbnail.insert(JsonDbString::kTypeStr, QString("com.noklab.nrcc.jsondb.thumbnail"));
    thumbnail.insert("url", thumbnailUrl);
    mJsonDb->create(mOwner, thumbnail);
    QString thumbnailUuid = thumbnail.valueString("_uuid");

    QsonMap item;
    item.insert("_type", QString(__FUNCTION__));
    item.insert("name", QString("Pebbles"));
    item.insert(JsonDbString::kTypeStr, kContactStr);
    //qDebug() << item;
    mJsonDb->create(mOwner, item);

    QsonMap item2;
    item2.insert("_type", QString(__FUNCTION__));
    item2.insert("name", QString("Wilma"));
    item2.insert("thumbnailUuid", thumbnailUuid);
    item2.insert(JsonDbString::kTypeStr, kContactStr);
    //qDebug() << item2;
    mJsonDb->create(mOwner, item2);

    QsonMap betty;
    betty.insert("_type", QString(__FUNCTION__));
    betty.insert("name", QString("Betty"));
    betty.insert("thumbnailUuid", thumbnailUuid);
    betty.insert(JsonDbString::kTypeStr, kContactStr);
    //qDebug() << betty;
    QsonMap r = mJsonDb->create(mOwner, betty);
    QString bettyUuid = r.subObject("result").valueString("_uuid");

    QsonMap bettyRef;
    bettyRef.insert("_type", QString(__FUNCTION__));
    bettyRef.insert("bettyUuid", bettyUuid);
    bettyRef.insert("thumbnailUuid", thumbnailUuid);
    r = mJsonDb->create(mOwner, bettyRef);

    QsonMap query, result;
    query.insert(JsonDbString::kQueryStr, QString("[?thumbnailUuid->url=\"%1\"]").arg(thumbnailUrl));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QVERIFY(result.subObject("result").subList("data").size() > 0);
    QCOMPARE(result.subObject("result").subList("data").objectAt(0).valueString("thumbnailUuid"), thumbnailUuid);

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"%1\"][?thumbnailUuid->url=\"%2\"]").arg(__FUNCTION__).arg(thumbnailUrl));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QVERIFY(result.subObject("result").subList("data").size() > 0);
    QCOMPARE(result.subObject("result").subList("data").objectAt(0).valueString("thumbnailUuid"), thumbnailUuid);

    QString queryString = QString("[?name=\"Betty\"][= [ name, thumbnailUuid->url ]]");
    query.insert(JsonDbString::kQueryStr, queryString);
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").listAt(0).stringAt(1), thumbnailUrl);

    queryString = QString("[?_type=\"%1\"][= [ name,thumbnailUuid->url ]]").arg(__FUNCTION__);
    query.insert(JsonDbString::kQueryStr, queryString);
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QsonList data = result.subObject("Result").subList("data");
    for (int ii = 0; ii < data.size(); ii++) {
        QsonList item = data.listAt(ii);
        QString name = item.stringAt(0);
        QString url = item.stringAt(1);
        if (name == "Pebbles")
            QVERIFY(url.isEmpty());
        else
            QCOMPARE(url, thumbnailUrl);
    }

    queryString = QString("[?_type=\"%1\"][= { name: name, url: thumbnailUuid->url } ]").arg(__FUNCTION__);
    query.insert(JsonDbString::kQueryStr, queryString);
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);

    data = result.subObject("result").subList("da2ta");
    for (int ii = 0; ii < data.size(); ii++) {
        QsonMap item = data.objectAt(ii);
        QString name = item.valueString("name");
        QString url = item.valueString("url");
        if (name == "Pebbles")
            QVERIFY(url.isEmpty());
        else
            QCOMPARE(url, thumbnailUrl);
    }

    query.insert(JsonDbString::kQueryStr, QString("[?bettyUuid exists][= bettyUuid->thumbnailUuid]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").stringAt(0),
             thumbnailUuid);

    query.insert(JsonDbString::kQueryStr, QString("[?bettyUuid exists][= bettyUuid->thumbnailUuid->url]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").stringAt(0), thumbnail.valueString("url"));
    //qDebug() << result;
    mJsonDb->removeIndex("name");
    mJsonDb->removeIndex("thumbnailUuid");
    mJsonDb->removeIndex("url");
    mJsonDb->removeIndex("bettyUuid");
}


void TestJsonDb::testNotify1()
{
    QString query = QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg(kContactStr);

    QsonList actions;
    actions.append(QLatin1String("create"));

    QsonMap notification;
    notification.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    notification.insert(QLatin1String("query"), query);
    notification.insert(QLatin1String("actions"), actions);

    QsonMap result = mJsonDb->create(mOwner, notification);
    QVERIFY(result.contains(JsonDbString::kResultStr));
    QVERIFY(result.subObject(JsonDbString::kResultStr).contains(JsonDbString::kUuidStr));
    QString uuid = result.subObject(JsonDbString::kResultStr).valueString(JsonDbString::kUuidStr);

    if (!connect(mJsonDb, SIGNAL(notified(const QString, QsonMap, const QString)),
                 this, SLOT(notified(const QString, QsonMap, const QString))))
        qDebug() << __FUNCTION__ << "failed to connect SIGNAL(notified)";

    mNotificationsReceived.clear();

    QsonMap item;
    item.insert("name", QString("Wilma"));
    item.insert(JsonDbString::kTypeStr, kContactStr);
    mJsonDb->create(mOwner, item);

    QVERIFY(mNotificationsReceived.contains(uuid));

    result = mJsonDb->remove(mOwner, notification);
    verifyGoodResult(result);
}

void TestJsonDb::notified(const QString nid, QsonMap o, const QString action)
{
//    static int c = 0;
//    if (c++ < 1)
//        qDebug() << "TestJsonDb::notified" << nid << o << action;
    Q_UNUSED(o);
    Q_UNUSED(action);
    mNotificationsReceived.append(nid);
}

QStringList sTestQueries = (QStringList()
                            << "[?foo exists]"
                            << "[?foo->bar exists]"
                            << "[?foo->bar->baz exists]"
                            << "[?foo=\"bar\"]"
                            << "[?foo= %bar ]"
                            << "[?foo= %bar]"
                            << "[?foo=%bar]"
                            << "[?foo=\"bar\" | foo=\"baz\"]"
                            << "[?foo=\"bar\"][/foo]"
                            << "[?foo=\"bar\"][= a ]"
                            << "[?foo =~ \"/a\\//\"]"
                            << "[?foo=\"bar\"][= a,b,c]"
                            << "[?foo=\"bar\"][= a->foreign,b,c]"
                            << "[?foo=\"bar\"][=[ a,b,c]]"
                            << "[?foo=\"bar\"][={ a:x, b:y, c:z}]"
                            << "[?foo=\"bar\"][={ a:x->foreign, b:y, c:z}]"
                            << "[?foo=\"bar\"][= _uuid, name.first, name.last ]"
                            << "[?_type=\"contact\"][= { uuid: _uuid, first: name.first, last: name.last } ]"
                            << "[?telephoneNumbers.*.number=\"6175551212\"]"
                            << "[?_type=\"contact\"][= .telephoneNumbers[*].number]"
    );

void TestJsonDb::parseQuery()
{
#if 1
    QSKIP("This is manual test, skipping", SkipAll);
#else
    foreach (QString query, sTestQueries) {
        qDebug() << endl << endl << "query" << query;
        QsonMap bindings;
        bindings.insert("bar", QString("barValue"));
        JsonDbQuery result = JsonDbQuery::parse(query, bindings);
        const QList<OrQueryTerm> &orQueryTerms = result.queryTerms;
        for (int i = 0; i < orQueryTerms.size(); i++) {
            const OrQueryTerm orQueryTerm = orQueryTerms[i];
            const QList<QueryTerm> &terms = orQueryTerm.terms();
            QString sep = "";
            if (terms.size() > 1) {
                qDebug() << "    (";
                sep = "  ";
            }
            foreach (const QueryTerm &queryTerm, terms) {
                qDebug() << QString("    %6%5%4%1 %2 %3    ").arg(queryTerm.propertyName()).arg(queryTerm.op()).arg(JsonWriter().toString(queryTerm.value()))
                    .arg(queryTerm.joinField().size() ? " -> " : "").arg(queryTerm.joinField())
                    .arg(sep);
                sep = "| ";
            }
            if (terms.size() > 1)
                qDebug() << "    )";
        }
        QList<OrderTerm> &orderTerms = result.orderTerms;
        for (int i = 0; i < orderTerms.size(); i++) {
            const OrderTerm &orderTerm = orderTerms[i];
            qDebug() << QString("    %1 %2    ").arg(orderTerm.propertyName).arg(orderTerm.ascending);
        }
        qDebug() << "mapKeys" << result.mapKeyList.join(", ");
        qDebug() << "mapExprs" << result.mapExpressionList.join(", ");
        qDebug() << "explanation" << result.queryExplanation;
    }
#endif
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
            QsonMap item;
            item.insert(JsonDbString::kTypeStr, QString("OrQueryTestType"));
            item.insert("key1", key1);
            item.insert("key2", key2);
            mJsonDb->create(mOwner, item);

            key1[0] = key1[0].toUpper();
            key2[0] = key2[0].toUpper();
            item = QsonMap();
            item.insert(JsonDbString::kTypeStr, QString("%1Type").arg(key1));
            item.insert("notUsed1", key1);
            item.insert("notUsed2", key2);
            mJsonDb->create(mOwner, item);

            item = QsonMap();
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
    QsonMap request;
    QString typeQuery = "[?_type=\"OrQueryTestType\"]";
    QString queryString = (QString("%6[? %1 = \"%2\" | %3 = \"%4\" ]%5")
                           .arg(field1).arg(value1)
                           .arg(field2).arg(value2)
                           .arg(ordering).arg(((field1 != "_type") && (field2 != "_type")) ? typeQuery : ""));
    request.insert("query", queryString);
    QsonMap result = mJsonDb->find(mOwner, request);
    verifyGoodResult(result);
    QsonList objects = result.subObject("result").subList("data");
    int count = 0;
    for (int ii = 0; ii < objects.size(); ii++) {
        QsonMap o = objects.objectAt(ii);
        QVERIFY((o.valueString(field1) == value1) || (o.valueString(field2) == value2));
        count++;
    }
    QVERIFY(count > 0);
    //qDebug() << result;
    //qDebug() << "verified objects" << count << endl;
    mJsonDb->removeIndex("key1");
    mJsonDb->removeIndex("key2");
}

void TestJsonDb::findByName()
{
    createContacts();
    int count = mContactList.size();
    if (!count)
        return;

    QsonMap request;

    //int itemNumber = (int)((double)qrand() * count / RAND_MAX);
    int itemNumber = 0;
    QsonMap item(mContactList.at(itemNumber).toMap());;
    request.insert("query",
                   QString("[?name=\"%1\"]")
                   .arg(item.valueString("name")));
    if (!item.contains("name"))
        qDebug() << "no name in item" << item;
    QsonMap result = mJsonDb->find(mOwner, request);
    verifyGoodResult(result);
}

void TestJsonDb::findEQ()
{
    createContacts();
    int count = mContactList.size();
    if (!count)
        return;

    QsonMap request;

    int itemNumber = (int)((double)qrand() * count / RAND_MAX);
    QsonMap item(mContactList.at(itemNumber).toMap());
    request.insert("query",
                   QString("[?name.first=\"%1\"][?name.last=\"%2\"][?_type=\"contact\"]")
                   .arg(JsonDb::propertyLookup(item, "name.first").toString())
                   .arg(JsonDb::propertyLookup(item, "name.last").toString()));
    QsonMap result = mJsonDb->find(mOwner, request);
    verifyGoodResult(result);
    QVERIFY(result.subObject("result").contains("length"));
    QCOMPARE(result.subObject("result").value<int>("length"), 1);
    QVERIFY(result.subObject("result").contains("data"));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    mJsonDb->removeIndex("name.first");
    mJsonDb->removeIndex("name.last");
}

void TestJsonDb::find10()
{
    createContacts();
    int count = mContactList.size();
    if (!count)
        return;

    QsonMap request;
    QsonMap result;

    int itemNumber = count / 2;
    QsonMap item(mContactList.at(itemNumber).toMap());
    request.insert("limit", 10);
    request.insert("query",
                   QString("[?name.first<=\"%1\"][?_type=\"contact\"]")
                   .arg(JsonDb::propertyLookup(item, "name.first").toString()));
    result = mJsonDb->find(mOwner, request);
    verifyGoodResult(result);
    QsonMap map = result.subObject(JsonDbString::kResultStr);
    QVERIFY(map.contains("length"));
    QCOMPARE(map.value<int>("length"), 10);
    QVERIFY(map.contains("data"));
    QCOMPARE(map.subList("data").size(), 10);
    mJsonDb->removeIndex("name.first");
    mJsonDb->removeIndex("contact");
}

QsonObject TestJsonDb::readJsonFile(const QString& filename)
{
    QString filepath = findFile(SRCDIR, filename);
    QFile jsonFile(filepath);
    jsonFile.open(QIODevice::ReadOnly);
    QByteArray json = jsonFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok) {
      qDebug() << filepath << parser.errorString();
    }
    QVariant v = parser.result();
    return variantToQson(v);
}

QsonObject TestJsonDb::readJson(const QByteArray& json)
{
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok) {
      qDebug() << parser.errorString();
    }
    QVariant v = parser.result();
    return variantToQson(v);
}

void TestJsonDb::testPrimaryKey()
{
    QsonMap capability = readJsonFile("pk-capability.json").toMap();
    QsonMap replay(capability);

    QsonMap result1 = mJsonDb->create(mOwner, capability).toMap();
    verifyGoodResult(result1);

    QsonMap result2 = mJsonDb->create(mOwner, replay).toMap();
    verifyGoodResult(result2);

    if (gVerbose) qDebug() << 1 << result1;
    if (gVerbose) qDebug() << 2 << result2;

    QCOMPARE(result1, result2);
}

void TestJsonDb::testStoredProcedure()
{
    QsonMap notification;
    notification.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    QString query = QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg(__FUNCTION__);
    QVariantList actions;
    actions.append(QLatin1String("create"));
    notification.insert(JsonDbString::kQueryStr, query);
    notification.insert(JsonDbString::kActionsStr, variantToQson(actions));
    notification.insert("script", QLatin1String("function foo (v) { return \"hello world\";}"));
    QsonObject result = mJsonDb->create(mOwner, notification);


    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
    result = mJsonDb->create(mOwner, item);

    notification.insert("script", QString("function foo (v) { return jsondb.find({'query':'[?_type=\"%1\"]'}); }").arg(__FUNCTION__));
    result = mJsonDb->update(mOwner, notification);

    QsonMap item2;
    item2.insert(JsonDbString::kTypeStr, QString(__FUNCTION__));
    result = mJsonDb->create(mOwner, item2);
}

void TestJsonDb::dumpObjects()
{
//    HdbCursor cursor(mJsonDbStorage->mHdb);
//    bool debug = gDebug;
//    gDebug = true;
//    quint32 lastObjectKey = 0;
//    QString lastUuid;
//    QByteArray baKey, baValue;
//    while (cursor.next(baKey, baValue)) {
//      quint32 objectKey = qFromLittleEndian<quint32>((const uchar *)baKey.data());
//      QsonObject object = mJsonDbStorage->deserialize(baValue).toMap();
//      QString uuid = object.value(JsonDbString::kUuidStr).toString();
//      DBG() << baKey.toHex() << objectKey;
//      DBG() << object;
//      QVERIFY(objectKey > lastObjectKey);
//      QVERIFY(uuid > lastUuid);
//      lastObjectKey = objectKey;
//      lastUuid = uuid;
//    }
//    gDebug = debug;
}

void TestJsonDb::startsWith()
{
    addIndex(QLatin1String("name"));

    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("startsWithTest"));
    item.insert("name", QLatin1String("Wilma"));
    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("startsWithTest"));
    item.insert("name", QLatin1String("Betty"));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("startsWithTest"));
    item.insert("name", QLatin1String("Bettina"));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("startsWithTest"));
    item.insert("name", QLatin1String("Benny"));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QsonMap query;
    query.insert("query", QString("[?_type=\"startsWithTest\"][?name startsWith \"Zelda\"]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(0));
    QCOMPARE(result.subObject("result").subList("data").size(), 0);

    query = QsonMap();
    query.insert("query", QString("[?_type=\"startsWithTest\"][?name startsWith \"Wilma\"]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);

    query = QsonMap();
    query.insert("query", QString("[?_type=\"startsWithTest\"][?name startsWith \"B\"]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(3));
    QCOMPARE(result.subObject("result").subList("data").size(), 3);

    query = QsonMap();
    query.insert("query", QString("[?_type=\"startsWithTest\"][?name startsWith \"Bett\"]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(2));
    QCOMPARE(result.subObject("result").subList("data").size(), 2);

    query = QsonMap();
    query.insert("query", QString("[?_type startsWith \"startsWith\"][/name]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(4));
    QCOMPARE(result.subObject("result").subList("data").size(), 4);

    query = QsonMap();
    query.insert("query", QString("[?_type startsWith \"startsWith\"][= _type ]"));
    result = mJsonDb->find(mOwner, query);
    qDebug() << "sortKeys" << result.subObject("result").subList("sortKeys");
    qDebug() << result.subObject("result").subList("data");
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(4));
    QCOMPARE(result.subObject("result").subList("data").size(), 4);
}

void TestJsonDb::comparison()
{
    addIndex(QLatin1String("latitude"));

    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("comparison"));
    item.insert("latitude", qint64(10));
    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("comparison"));
    item.insert("latitude", qint64(42));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("comparison"));
    item.insert("latitude", qint64(0));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("comparison"));
    item.insert("latitude", qint64(-64));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QsonMap query;
    query.insert("query", QString("[?_type=\"comparison\"][?latitude > 10]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueInt("latitude"), qint64(42));

    query.insert("query", QString("[?_type=\"comparison\"][?latitude >= 10]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(2));
    QCOMPARE(result.subObject("result").subList("data").size(), 2);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueInt("latitude"), qint64(10));
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(1).valueInt("latitude"), qint64(42));

    query.insert("query", QString("[?_type=\"comparison\"][?latitude < 0]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueInt("latitude"), qint64(-64));
    mJsonDb->removeIndex(QLatin1String("latitude"));
}

void TestJsonDb::removedObjects()
{
    addIndex(QLatin1String("foo"));
    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("removedObjects"));
    item.insert("foo", QLatin1String("bar"));
    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QsonMap object = result.subObject("result");
    object.insert(JsonDbString::kTypeStr, QLatin1String("removedObjects"));

    QsonMap query;
    query.insert("query", QString("[?_type=\"removedObjects\"]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueString("foo"), QLatin1String("bar"));

    // update the object
    item = object;
    item.insert("name", QLatin1String("anna"));
    result = mJsonDb->update(mOwner, item);
    verifyGoodResult(result);

    query = QsonMap();
    query.insert("query", QString("[?_type=\"removedObjects\"]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QVERIFY(!result.subObject("result").subList("data").at<QsonMap>(0).contains("foo"));
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueString("name"), QLatin1String("anna"));

    query = QsonMap();
    query.insert("query", QString("[?_type=\"removedObjects\"][/name]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QVERIFY(!result.subObject("result").subList("data").at<QsonMap>(0).contains("foo"));
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueString("name"), QLatin1String("anna"));

    query = QsonMap();
    query.insert("query", QString("[?_type=\"removedObjects\"][/foo]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(0));
    QCOMPARE(result.subObject("result").subList("data").size(), 0);

    // remove the object
    result = mJsonDb->remove(mOwner, object);
    verifyGoodResult(result);

    query = QsonMap();
    query.insert("query", QString("[?_type=\"removedObjects\"]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(0));
    QCOMPARE(result.subObject("result").subList("data").size(), 0);

    query = QsonMap();
    query.insert("query", QString("[?_type=\"removedObjects\"][/foo]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(0));
    QCOMPARE(result.subObject("result").subList("data").size(), 0);
    mJsonDb->removeIndex(QLatin1String("foo"));
}

void TestJsonDb::partition()
{
    QsonMap map;
    map.insert(QLatin1String("_type"), QLatin1String("Partition"));
    map.insert(QLatin1String("name"), QLatin1String("private"));
    QsonMap result = mJsonDb->create(mOwner, map);
    verifyGoodResult(result);

    map = QsonMap();
    map.insert(QLatin1String("_type"), QLatin1String("partitiontest"));
    map.insert(QLatin1String("foo"), QLatin1String("bar"));
    result = mJsonDb->create(mOwner, map);
    verifyGoodResult(result);

    map = QsonMap();
    map.insert(QLatin1String("_type"), QLatin1String("partitiontest"));
    map.insert(QLatin1String("foo"), QLatin1String("asd"));
    result = mJsonDb->create(mOwner, map, "private");
    verifyGoodResult(result);

    QsonMap query;
    query.insert("query", QString("[?_type=\"partitiontest\"]"));

    result = mJsonDb->find(mOwner, query, JsonDbString::kSystemPartitionName);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).value<QString>("foo"), QLatin1String("bar"));

    result = mJsonDb->find(mOwner, query, "private");
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).value<QString>("foo"), QLatin1String("asd"));

    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);
    QStringList values = (QStringList() << QLatin1String("asd") << QLatin1String("bar"));
    QVERIFY(values.contains(result.subObject("result").subList("data").at<QsonMap>(0).value<QString>("foo")));
    QVERIFY(values.contains(result.subObject("result").subList("data").at<QsonMap>(1).value<QString>("foo")));

    query.insert("query", QString("[?_type=\"partitiontest\"][/foo]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);
    QVERIFY(result.subObject("result").subList("data").at<QsonMap>(0).value<QString>("foo")
            < result.subObject("result").subList("data").at<QsonMap>(1).value<QString>("foo"));

    query.insert("query", QString("[?_type=\"partitiontest\"][\\foo]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);
    QVERIFY(result.subObject("result").subList("data").at<QsonMap>(0).value<QString>("foo")
            > result.subObject("result").subList("data").at<QsonMap>(1).value<QString>("foo"));

}

void TestJsonDb::arrayIndexQuery()
{
    addIndex(QLatin1String("phoneNumber"));

    QsonList objects(readJsonFile("array.json").toList());
    QMap<QString, QsonMap> toDelete;
    for (int i = 0; i < objects.size(); ++i) {
        QsonMap object = objects.objectAt(i);
        QsonMap result = mJsonDb->create(mOwner, object);
        verifyGoodResult(result);
        toDelete.insert(object.valueString("_uuid"), object);
    }

    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);

    QsonMap queryListMember;
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.number =~\"/*789*/wi\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 1);

    queryListMember = QsonMap();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.validTime.0.timeFrom =\"09:00\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 1);

    queryListMember = QsonMap();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.validTime.0.timeFrom =\"10:00\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 1);

    queryListMember = QsonMap();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.validTime.0.timeTo =\"13:00\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);

    queryListMember = QsonMap();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.0.validTime.10.timeTo =\"13:00\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 0);

    queryListMember = QsonMap();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?phoneNumbers.10.validTime.10.timeTo =\"13:00\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 0);

    queryListMember = QsonMap();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?foo.0.0.bar =\"val00\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 2);

    queryListMember = QsonMap();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?test.45 =\"joe\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 1);

    queryListMember = QsonMap();
    queryListMember.insert(JsonDbString::kQueryStr, QLatin1String("[?_type=\"ContactInArray\"][?test2.56.firstName =\"joe\"]"));
    result = mJsonDb->find(mOwner, queryListMember);
    verifyGoodResult(result);
    QCOMPARE(result.subObject("result").subList("data").size(), 1);

    foreach(QsonMap object, toDelete.values()) {
        verifyGoodResult(mJsonDb->remove(mOwner, object));
    }

}

void TestJsonDb::deindexError()
{
    QsonMap result;

    // create document with a property "name"
    QsonMap foo;
    foo.insert("_type", QLatin1String("Foo"));
    foo.insert("name", QLatin1String("fooo"));
    result = mJsonDb->create(mOwner, foo);
    verifyGoodResult(result);

    // create a schema object (has 'name' property)
    QsonMap schema;
    schema.insert("_type", QLatin1String("_schemaType"));
    schema.insert("name", QLatin1String("ArrayObject"));
    QsonMap s;
    s.insert("type", QLatin1String("object"));
    //s.insert("extends", QLatin1String("View"));
    schema.insert("schema", s);
    result = mJsonDb->create(mOwner, schema);
    verifyGoodResult(result);

    // create instance of ArrayView (defined by the schema)
    QsonMap arrayView;
    arrayView.insert("_type", QLatin1String("ArrayView"));
    arrayView.insert("name", QLatin1String("fooo"));
    result = mJsonDb->create(mOwner, arrayView);
    verifyGoodResult(result);

    addIndex(QLatin1String("name"));

    // now remove some objects that have "name" property
    QsonMap query;
    query.insert("query", QString("[?_type=\"Foo\"]"));
    result = mJsonDb->find(mOwner, query);
    verifyGoodResult(result);
    QsonList objs = result.value<QsonMap>("result").value<QsonList>("data");
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
        QsonMap item;
        item.insert("_type", QLatin1String(__FUNCTION__));
        item.insert("order", str);
        QsonMap result = mJsonDb->create(mOwner, item);
        QVERIFY(result.value<QsonMap>("error").isEmpty());
        uuids << result.value<QsonMap>("result").valueString("_uuid");
    }

    // This is the order we expect from the query
    list.sort();

    QsonMap query;
    query.insert("query", QString("[?_type=\"%1\"][/order]").arg(__FUNCTION__));
    QsonMap findResult = mJsonDb->find(mOwner, query);

    QVERIFY(findResult.contains("result"));
    QsonMap resultMap = findResult.subObject("result");

    QVERIFY(resultMap.contains("data"));
    QsonList dataList = resultMap.subList("data");

    QCOMPARE(dataList.size(), list.size());

    for (int i = 0; i < dataList.size(); ++i) {
        QCOMPARE(dataList.typeAt(i), QsonMap::MapType);
        QsonMap obj = dataList.at<QsonMap>(i);
        QVERIFY(obj.contains("order"));
        QCOMPARE(obj.valueString("order"), list.at(i));
    }
}

void TestJsonDb::indexQueryOnCommonValues()
{
    // Specific indexing bug when you have records inserted that only differ
    // by their _type
    createContacts();

    for (int ii = 0; ii < mContactList.size(); ii++) {
        QsonMap data(mContactList.at(ii).toMap());
        QsonMap chaff;
        chaff.insert(JsonDbString::kTypeStr, QString("com.noklab.nrcc.ContactChaff"));
        QStringList skipKeys = (QStringList() << JsonDbString::kUuidStr << JsonDbString::kVersionStr << JsonDbString::kTypeStr);
        foreach (QString key, data.keys()) {
            if (!skipKeys.contains(key))
                chaff.insert(key, data.value<QsonElement>(key));
        }

        QsonMap result = mJsonDb->create(mOwner, chaff);
        verifyGoodResult(result);
    }

    int count = mContactList.size();

    QCOMPARE(count > 0, true);

    QsonMap request;

    int itemNumber = (int)((double)qrand() * count / RAND_MAX);

    QsonMap item(mContactList.at(itemNumber).toMap());
    QString first = JsonDb::propertyLookup(item, "name.first").toString();
    QString last = JsonDb::propertyLookup(item, "name.last").toString();
    request.insert("query",
                   QString("[?name.first=\"%1\"][?name.last=\"%2\"][?_type=\"contact\"]")
                   .arg(first)
                   .arg(last) );
    QsonMap result = mJsonDb->find(mOwner, request);
    verifyGoodResult(result);

    QVERIFY(result.subObject("result").contains("length"));
    QCOMPARE(result.subObject("result").value<int>("length"), 1);
    QVERIFY(result.subObject("result").contains("data"));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
}

void TestJsonDb::removeIndexes()
{
    addIndex("wacky_index");
    QVERIFY(mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->findObjectTable(JsonDbString::kSchemaTypeStr)->indexSpec("wacky_index") != 0);

    QVERIFY(mJsonDb->removeIndex("wacky_index"));
    QVERIFY(mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->findObjectTable(JsonDbString::kSchemaTypeStr)->indexSpec("wacky_index") == 0);

    QsonMap indexObject;
    indexObject.insert(JsonDbString::kTypeStr, QLatin1String("Index"));
    indexObject.insert("propertyName", QLatin1String("predicate"));
    indexObject.insert("propertyType", QLatin1String("string"));
    QsonMap indexObject2 = indexObject;

    QsonMap result = mJsonDb->create(mOwner, indexObject);
    verifyGoodResult(result);
    QVERIFY(mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->findObjectTable("Index")->indexSpec("predicate") != 0);

    indexObject2.insert(JsonDbString::kUuidStr, indexObject.valueString(JsonDbString::kUuidStr));

    indexObject.insert("propertyType", QLatin1String("integer"));
    result = mJsonDb->update(mOwner, indexObject);
    verifyErrorResult(result);

    result = mJsonDb->remove(mOwner, indexObject2);
    verifyGoodResult(result);
    QVERIFY(mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->findObjectTable("Index")->indexSpec("predicate") == 0);
}

void TestJsonDb::setOwner()
{
    gEnforceAccessControlPolicies = true;
    mOwner->setAllowAll(true);
    QLatin1String fooOwnerStr("com.foo.owner");

    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("SetOwnerType"));
    item.insert(JsonDbString::kOwnerStr, fooOwnerStr);
    QsonMap result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    result = mJsonDb->getObjects(JsonDbString::kTypeStr, "SetOwnerType");
    QCOMPARE(result.subList("result").objectAt(0).valueString(JsonDbString::kOwnerStr),
             fooOwnerStr);

    result = mJsonDb->remove(mOwner, item);
    verifyGoodResult(result);

    JsonDbOwner *unauthOwner = new JsonDbOwner(this);
    unauthOwner->setOwnerId("com.noklab.nrcc.OtherOwner");
    unauthOwner->setAllowAll(false);
    unauthOwner->setAllowedObjects("write", (QStringList() << QLatin1String("[*]")));
    unauthOwner->setAllowedObjects("read", (QStringList() << QLatin1String("[*]")));

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("SetOwnerType2"));
    item.insert(JsonDbString::kOwnerStr, fooOwnerStr);
    result = mJsonDb->create(unauthOwner, item);
    verifyGoodResult(result);

    result = mJsonDb->getObjects(JsonDbString::kTypeStr, "SetOwnerType2");
    QVERIFY(result.subList("result").objectAt(0).valueString(JsonDbString::kOwnerStr)
            != fooOwnerStr);
    result = mJsonDb->remove(unauthOwner, item);
    verifyGoodResult(result);
}

void TestJsonDb::indexPropertyFunction()
{
    QsonMap index;
    index.insert(JsonDbString::kTypeStr, QLatin1String("Index"));
    index.insert(QLatin1String("name"), QLatin1String("propertyFunctionIndex"));
    index.insert(QLatin1String("propertyType"), QLatin1String("string"));
    index.insert(QLatin1String("propertyFunction"), QLatin1String("function (o) { if (o.from) jsondb.emit(o.from); else jsondb.emit(o.to); }"));
    QsonMap result = mJsonDb->create(mOwner, index);
    verifyGoodResult(result);

    QsonMap item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("IndexPropertyFunction"));
    item.insert("from", qint64(10));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("IndexPropertyFunction"));
    item.insert("to", qint64(42));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("IndexPropertyFunction"));
    item.insert("from", qint64(0));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    item = QsonMap();
    item.insert(JsonDbString::kTypeStr, QLatin1String("IndexPropertyFunction"));
    item.insert("to", qint64(-64));
    result = mJsonDb->create(mOwner, item);
    verifyGoodResult(result);

    QsonMap query;
    query.insert("query", QString("[?_type=\"IndexPropertyFunction\"][?propertyFunctionIndex > 10][/propertyFunctionIndex]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueInt("to"), qint64(42));

    query.insert("query", QString("[?_type=\"IndexPropertyFunction\"][?propertyFunctionIndex >= 10][/propertyFunctionIndex]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(2));
    QCOMPARE(result.subObject("result").subList("data").size(), 2);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueInt("from"), qint64(10));
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(1).valueInt("to"), qint64(42));

    query.insert("query", QString("[?_type=\"IndexPropertyFunction\"][?propertyFunctionIndex < 0]"));
    result = mJsonDb->find(mOwner, query);
    QCOMPARE(result.subObject("result").valueInt("length", 0), qint64(1));
    QCOMPARE(result.subObject("result").subList("data").size(), 1);
    QCOMPARE(result.subObject("result").subList("data").at<QsonMap>(0).valueInt("to"), qint64(-64));
}

QTEST_MAIN(TestJsonDb)
#include "testjsondb.moc"
