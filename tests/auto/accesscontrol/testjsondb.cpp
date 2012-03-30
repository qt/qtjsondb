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

#include "jsondbpartition.h"
#include "jsondbsettings.h"

#include "../../shared/util.h"

QT_USE_NAMESPACE_JSONDB

/*
  Ensure that a good result object contains the correct fields
 */
#define verifyGoodResult(result) \
{ \
    JsonDbWriteResult __result = result; \
    QVERIFY2(__result.message.isEmpty(), __result.message.toLatin1()); \
    QVERIFY(__result.code == JsonDbError::NoError); \
}

/*
  Ensure that a error result contains the correct fields
 */

#define verifyErrorResult(result) \
{\
    JsonDbWriteResult __result = result; \
    QVERIFY(!__result.message.isEmpty()); \
    QVERIFY(__result.code != JsonDbError::NoError); \
}

#define verifyGoodQueryResult(result) \
{ \
    JsonDbQueryResult __result = result; \
    QVERIFY2(__result.error.type() == QJsonValue::Null,  \
         __result.error.toObject().value("message").toString().toLocal8Bit()); \
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
    void testViewAccessControl();
    void testIndexAccessControl();

private:
    JsonDbQueryResult find(JsonDbOwner *owner, const QString &query, const QJsonObject bindings = QJsonObject());
    JsonDbWriteResult create(JsonDbOwner *owner, JsonDbObject &object, JsonDbPartition::WriteMode mode = JsonDbPartition::OptimisticWrite);
    JsonDbWriteResult update(JsonDbOwner *owner, JsonDbObject &object, JsonDbPartition::WriteMode mode = JsonDbPartition::OptimisticWrite);
    JsonDbWriteResult remove(JsonDbOwner *owner, JsonDbObject &object, JsonDbPartition::WriteMode mode = JsonDbPartition::OptimisticWrite);

    void removeDbFiles();

private:
    JsonDbPartition *mJsonDbPartition;
    QStringList mNotificationsReceived;
    QList<JsonDbObject> mContactList;
    QScopedPointer<JsonDbOwner> mOwner;
};

const char *kFilename = "testdatabase";
const QString kReplica1Name = QString("replica1");
const QString kReplica2Name = QString("replica2");
const QStringList kReplicaNames = (QStringList() << kReplica1Name << kReplica2Name);

TestJsonDb::TestJsonDb() :
    mJsonDbPartition(0)
  , mOwner(0)
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
    mOwner.reset (new JsonDbOwner(this));
    mOwner->setOwnerId(QStringLiteral("com.example.JsonDbTest"));

    mJsonDbPartition = new JsonDbPartition(kFilename, QStringLiteral("com.example.JsonDbTest"), mOwner.data(), this);
    mJsonDbPartition->open();
}

void TestJsonDb::cleanupTestCase()
{
    if (mJsonDbPartition) {
        mJsonDbPartition->close();
        delete mJsonDbPartition;
        mJsonDbPartition = 0;
    }
    removeDbFiles();
}

void TestJsonDb::cleanup()
{
    QCOMPARE(mJsonDbPartition->mTransactionDepth, 0);
}

JsonDbQueryResult TestJsonDb::find(JsonDbOwner *owner, const QString &query, const QJsonObject bindings)
{
    QScopedPointer<JsonDbQuery> q(JsonDbQuery::parse(query, bindings));
    return mJsonDbPartition->queryObjects(owner, q.data());
}

JsonDbWriteResult TestJsonDb::create(JsonDbOwner *owner, JsonDbObject &object, JsonDbPartition::WriteMode mode)
{
    JsonDbWriteResult result =  mJsonDbPartition->updateObject(owner, object, mode);
    if (result.code == JsonDbError::NoError) {
        object.insert(JsonDbString::kUuidStr, result.objectsWritten[0].uuid().toString());
        object.insert(JsonDbString::kVersionStr, result.objectsWritten[0].version());
    }
    return result;
}

JsonDbWriteResult TestJsonDb::update(JsonDbOwner *owner, JsonDbObject &object, JsonDbPartition::WriteMode mode)
{
    JsonDbWriteResult result =  mJsonDbPartition->updateObject(owner, object, mode);
    if (result.code == JsonDbError::NoError)
        object.insert(JsonDbString::kVersionStr, result.objectsWritten[0].version());
    return result;
}

JsonDbWriteResult TestJsonDb::remove(JsonDbOwner *owner, JsonDbObject &object, JsonDbPartition::WriteMode mode)
{
    JsonDbObject toDelete = object;
    toDelete.insert(JsonDbString::kDeletedStr, true);
    JsonDbWriteResult result = mJsonDbPartition->updateObject(owner, toDelete, mode);
    if (result.code == JsonDbError::NoError)
        object = result.objectsWritten[0];
    return result;
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
            owner->setCapabilities(capabilities, mJsonDbPartition);
        } else {
            JsonDbWriteResult result = create(mOwner.data(), object);
            verifyGoodResult(result);
        }
    }
    mJsonDbPartition->removeIndex("CapabilitiesTest");
}

/*
 * Verify the allowAll flag on owner
 */
void TestJsonDb::allowAll()
{
    // can delete me when this goes away
    jsondbSettings->setEnforceAccessControl(true);

    QScopedPointer<JsonDbOwner> owner(new JsonDbOwner());
    owner->setAllowedObjects(QLatin1String("all"), QLatin1String("read"), QStringList());
    owner->setAllowedObjects(QLatin1String("all"), QLatin1String("write"), QStringList());
    owner->setStorageQuota(-1);

    JsonDbObject toPut;
    toPut.insert("_type", QLatin1String("TestObject"));

    JsonDbWriteResult result = create(owner.data(), toPut);
    verifyErrorResult(result);

    JsonDbObject toPut2;
    toPut2.insert("_type", QLatin1String("TestObject"));

    owner.data()->setAllowAll(true);
    result =  create(owner.data(), toPut2);
    verifyGoodResult(result);

    mJsonDbPartition->removeIndex("TestObject");

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
    mOwner->setCapabilities(contactsCapabilities, mJsonDbPartition);

    JsonDbObject item;
    item.insert(JsonDbString::kTypeStr, QLatin1String("create-test-type"));
    item.insert("access-control-test", 22);

    JsonDbWriteResult result = create(mOwner.data(), item);

    verifyErrorResult(result);

    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("Contact"));
    item.insert("access-control-test", 23);

    result = create(mOwner.data(), item);
    verifyGoodResult(result);

    item.insert("access-control-test", 24);
    result = update(mOwner.data(), item);
    verifyGoodResult(result);

    result = remove(mOwner.data(), item);
    verifyGoodResult(result);

    // Test some %owner and %typeDomain horror
    // ---------------------------------------
    mOwner->setAllowAll(true);

    // Create an object for testing (failing) update & delete
    mOwner->setOwnerId(QStringLiteral("test"));

    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.bar.FooType"));
    item.insert("access-control-test", 25);

    result = create(mOwner.data(), item);

    QJsonValue uuid = item.value(JsonDbString::kUuidStr);

    mOwner->setOwnerId(QStringLiteral("com.example.foo.App"));
    QJsonObject ownDomainCapabilities;
    while (!value.isEmpty())
        value.removeLast();
    value.append (QLatin1String("rw"));
    ownDomainCapabilities.insert (QStringLiteral("own_domain"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(ownDomainCapabilities, mJsonDbPartition);

    // Test that we can not create
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.bar.FooType"));
    item.insert("access-control-test", 26);

    result = create(mOwner.data(), item);

    verifyErrorResult(result);

    // Test that we can not update
    item.insert(JsonDbString::kUuidStr, uuid);
    item.insert("access-control-test", 27);

    result = update(mOwner.data(), item);
    verifyErrorResult(result);

    // .. or remove
    item.insert(JsonDbString::kUuidStr, uuid);
    result = remove(mOwner.data(), item);
    verifyErrorResult(result);

    // Positive tests
    item.remove(JsonDbString::kUuidStr);
    item.remove(JsonDbString::kVersionStr);
    item.remove(JsonDbString::kDeletedStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.FooType"));
    item.insert("access-control-test", 28);

    result = create(mOwner.data(), item);
    verifyGoodResult(result);

    result = remove(mOwner.data(), item);
    verifyGoodResult(result);

    item.remove(JsonDbString::kUuidStr);
    item.remove(JsonDbString::kVersionStr);
    item.remove(JsonDbString::kDeletedStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.FooType"));
    item.insert("access-control-test", 29);

    result = create(mOwner.data(), item);
    verifyGoodResult(result);

    item.insert("access-control-test", 30);
    result = update(mOwner.data(), item);
    verifyGoodResult(result);

    result = remove(mOwner.data(), item);
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
    JsonDbWriteResult result = create(mOwner.data(), item);
    verifyGoodResult(result);
    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("Contact"));
    item.insert("find-access-control-test", 51);
    result = create(mOwner.data(), item);
    verifyGoodResult(result);

    QJsonObject contactsCapabilities;
    QJsonArray value;
    value.append (QLatin1String("rw"));
    contactsCapabilities.insert (QStringLiteral("contacts"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(contactsCapabilities, mJsonDbPartition);

    JsonDbQueryResult queryResult = find(mOwner.data(), QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("find-access-control-test-type"));
    verifyGoodQueryResult(queryResult);

    QVERIFY(queryResult.length.toDouble() < 1);

    queryResult= find(mOwner.data(), QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("Contact"));
    verifyGoodQueryResult(queryResult);

    QVERIFY(queryResult.length.toDouble() > 0);

    mOwner->setAllowAll(true);

    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.bar.FooType"));
    item.insert("find-access-control-test", 55);
    result = create(mOwner.data(), item);
    verifyGoodResult(result);

    item.remove(JsonDbString::kUuidStr);
    item.insert(JsonDbString::kTypeStr, QLatin1String("com.example.foo.FooType"));
    item.insert("find-access-control-test", 56);
    result = create(mOwner.data(), item);
    verifyGoodResult(result);

    mOwner->setOwnerId(QStringLiteral("com.example.foo.App"));
    QJsonObject ownDomainCapabilities;
    while (!value.isEmpty())
        value.removeLast();
    value.append (QLatin1String("rw"));
    ownDomainCapabilities.insert (QLatin1String("own_domain"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(ownDomainCapabilities, mJsonDbPartition);

    queryResult = find(mOwner.data(), QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("com.example.foo.bar.FooType"));
    verifyGoodQueryResult(queryResult);

    QVERIFY(queryResult.length.toDouble() < 1);

    queryResult= find(mOwner.data(), QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("com.example.foo.FooType"));
    verifyGoodQueryResult(queryResult);

    QVERIFY(queryResult.length.toDouble() > 0);
    jsondbSettings->setEnforceAccessControl(false);
}

/*
 * Create Map and Reduce objects and check access control
 */

void TestJsonDb::testViewAccessControl()
{
    jsondbSettings->setEnforceAccessControl(false);
    QJsonArray defs(readJsonFile(":/security/json/capabilities-view.json").toArray());
    for (int i = 0; i < defs.size(); ++i) {
        JsonDbObject object(defs.at(i).toObject());
        JsonDbWriteResult result = create(mOwner.data(), object);
        verifyGoodResult(result);
    }

    jsondbSettings->setEnforceAccessControl(true);
    QJsonObject viewCapabilities;
    QJsonArray value;
    value.append (QLatin1String("rw"));
    viewCapabilities.insert (QLatin1String("views"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(viewCapabilities, mJsonDbPartition);

    defs = readJsonFile(":/security/json/view-test.json").toArray();
    for (int i = 0; i < defs.size(); ++i) {
        JsonDbObject object(defs.at(i).toObject());
        JsonDbWriteResult result = create(mOwner.data(), object);
        verifyGoodResult(result);
    }

    QJsonObject novwCapabilities;
    QJsonArray novwValue;
    novwValue.append (QLatin1String("rw"));
    novwCapabilities.insert (QLatin1String("noviews"), novwValue);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(novwCapabilities, mJsonDbPartition);

    defs = readJsonFile(":/security/json/view-test2.json").toArray();
    for (int i = 0; i < defs.size(); ++i) {
        JsonDbObject object(defs.at(i).toObject());
        JsonDbWriteResult result = create(mOwner.data(), object);
        verifyErrorResult(result);
    }
}

/*
 * Create Index objects and check access control
 */

void TestJsonDb::testIndexAccessControl()
{
    jsondbSettings->setEnforceAccessControl(false);
    QJsonArray defs(readJsonFile(":/security/json/capabilities-indexes.json").toArray());
    for (int i = 0; i < defs.size(); ++i) {
        JsonDbObject object(defs.at(i).toObject());
        JsonDbWriteResult result = create(mOwner.data(), object);
        verifyGoodResult(result);
    }

    JsonDbWriteResult result;

    defs = readJsonFile(":/security/json/index-test.json").toArray();
    for (int i = 0; i < defs.size(); ++i) {
        JsonDbObject object(defs.at(i).toObject());
        result = create(mOwner.data(), object);
        verifyGoodResult(result);
        result = remove(mOwner.data(), object);
    }

    jsondbSettings->setEnforceAccessControl(true);
    QJsonObject indexCapabilities;
    QJsonArray value;
    value.append (QLatin1String("rw"));
    indexCapabilities.insert (QLatin1String("indexes"), value);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(indexCapabilities, mJsonDbPartition);

    for (int i = 0; i < defs.size(); ++i) {
        JsonDbObject object(defs.at(i).toObject());
        result = create(mOwner.data(), object);
        if (i<2){
            verifyGoodResult(result);
            result = remove(mOwner.data(), object);
        } else {
            // Third one should fail as there is no objectType given
            verifyErrorResult(result);
        }
    }

    QJsonObject indexCapabilities2;
    QJsonArray value2;
    value2.append (QLatin1String("rw"));
    indexCapabilities2.insert (QLatin1String("noindexes"), value2);
    mOwner->setAllowAll(false);
    mOwner->setCapabilities(indexCapabilities2, mJsonDbPartition);

    for (int i = 0; i < defs.size(); ++i) {
        JsonDbObject object(defs.at(i).toObject());
        result = create(mOwner.data(), object);
        if (i<1){
            verifyGoodResult(result);
            result = remove(mOwner.data(), object);
        } else {
            // Second & Third should fail as there is no objectType given
            verifyErrorResult(result);
        }
    }
}

void TestJsonDb::notified(const QString nid, const JsonDbObject &o, const QString action)
{
    Q_UNUSED(o);
    Q_UNUSED(action);
    mNotificationsReceived.append(nid);
}

QTEST_MAIN(TestJsonDb)
#include "testjsondb.moc"
