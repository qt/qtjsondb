/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "json.h"

#include <QtJsonDbQson/private/qson_p.h>
#include <QtJsonDbQson/private/qsonparser_p.h>

#include "jsondb.h"
#include "jsondbbtreestorage.h"
#include "jsondbindex.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"

#include "../../shared/util.h"

QT_USE_NAMESPACE_JSONDB

Q_DECLARE_METATYPE(QsonList)
Q_DECLARE_METATYPE(QsonMap)

class TestJsonDb: public QObject
{
    Q_OBJECT
public:
    TestJsonDb();

private slots:
    void init();
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void contactListChaff();//moved from auto/daemon
    void qsonListCreate();
    void qsonMapCreate();

    void qsonListReadValue_data();
    void qsonListReadValue();
    void qsonMapReadValue_data();
    void qsonMapReadValue();

    void qsonListInsertValue();
    void qsonMapInsertValue();

    void benchmarkCreate();
    void benchmarkFileAppend();
    void benchmarkFileAppend2();
    void benchmarkParseQuery_data();
    void benchmarkParseQuery();
    void benchmarkFieldMatch();
    void benchmarkTokenizer();
    void benchmarkForwardKeyCmp();
    void benchmarkParsedQuery();

    void benchmarkSchemaValidation_data();
    void benchmarkSchemaValidation();

    void benchmarkFind();
    void benchmarkFindByName();
    void benchmarkFindByUuid();
    void benchmarkFindEQ();
    void benchmarkFindLE();
    void benchmarkFirst();
    void benchmarkLast();
    void benchmarkFirst10();
    void benchmarkFind10();
    void benchmarkFind20();
    void benchmarkFirstByUuid();
    void benchmarkLastByUuid();
    void benchmarkFirst10ByUuid();
    void benchmarkFind10ByUuid();
    void benchmarkFindUnindexed();
    void benchmarkFindReindexed();
    void benchmarkFindNames();
    void findNamesMapL();
    void benchmarkFindNamesMapL();
    void findNamesMapO();
    void benchmarkFindNamesMapO();
    void benchmarkCursorCount();
    void benchmarkQueryCount();
    void benchmarkScriptEngineCreation();

private:
    QsonObject readJsonFile(const QString &filename);
    QsonObject readJson(const QByteArray &json);
    void removeDbFiles();
    void addSchema(const QString &schemaName, QsonMap &schemaObject);
    void addIndex(const QString &propertyName, const QString &propertyType=QString(), const QString &objectType=QString());

private:
    JsonDb *mJsonDb;
    QsonList  mContactList;
    QStringList mFirstNames;
    QStringList mUuids;
    JsonDbOwner *mOwner;
};

#define verifyGoodResult(result) \
{ \
    QsonMap __result = result; \
    QVERIFY(__result.contains(JsonDbString::kErrorStr)); \
    QVERIFY2(__result.isNull(JsonDbString::kErrorStr), __result.subObject(JsonDbString::kErrorStr).valueString("message").toLocal8Bit()); \
    QVERIFY(__result.contains(JsonDbString::kResultStr)); \
}

#define verifyGoodQueryResult(result) \
{ \
    QsonMap __result = result; \
    QVERIFY(__result.contains(JsonDbString::kErrorStr)); \
    QVERIFY2(__result.isNull(JsonDbString::kErrorStr), __result.subObject(JsonDbString::kErrorStr).valueString("message").toLocal8Bit()); \
    QVERIFY(__result.contains(JsonDbString::kResultStr)); \
}

const char *kFilename = "testdatabase";

TestJsonDb::TestJsonDb()
    : mJsonDb(NULL), mOwner(0)
{
}

void TestJsonDb::removeDbFiles()
{
    QStringList filters;
    filters << QString::fromLatin1(kFilename)+QLatin1Char('*');
    filters << QString("*.db") << "objectFile.bin" << "objectFile2.bin";
    QStringList lst = QDir().entryList(filters);
    foreach (const QString &fileName, lst)
        QFile::remove(fileName);
}

void TestJsonDb::initTestCase()
{
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setOrganizationDomain("nrcc.noklab.com");
    QCoreApplication::setApplicationName("TestJsonDb");
    QCoreApplication::setApplicationVersion("1.0");

    removeDbFiles();
    gVerbose = false;
    mJsonDb = new JsonDb(kFilename, this);
    mJsonDb->open();
    mOwner = new JsonDbOwner(this);
    mOwner->setOwnerId("com.noklab.nrcc.JsonDbTest");

    QFile contactsFile(findFile(SRCDIR, "largeContactsTest.json"));
    if (!contactsFile.exists()) {
        qDebug() << "Err: largeContactsTest.json doesn't exist!";
        return;
    }
    contactsFile.open(QIODevice::ReadOnly);
    QByteArray json = contactsFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok)
        qDebug() << parser.errorString();
    QVariantList contactList = parser.result().toList();
    QsonList newContactList;
    QSet<QString> firstNames;
    foreach (QVariant v, contactList) {
        QsonMap contact = QsonMap(variantToQson(v.toMap()));
        QString name = contact.valueString("name");
        QStringList names = name.split(" ");
        QsonMap nameObject;
        nameObject.insert("first", names[0]);
        nameObject.insert("last", names[names.size()-1]);
        contact.insert("name", nameObject);
        contact.insert(JsonDbString::kTypeStr, QString("contact"));
        newContactList.append(QsonParser::fromRawData(contact.data()).toMap());
        firstNames.insert(names[0]);
    }
    mContactList = newContactList;
    mFirstNames = firstNames.toList();
    mFirstNames.sort();

    addIndex(QLatin1String("name"));
    addIndex(QLatin1String("name.first"));
    addIndex(QLatin1String("name.last"));
    addIndex(QLatin1String("_type"));

    qDebug() << "Creating" << mContactList.size() << "contacts...";

    QElapsedTimer time;
    time.start();
    int count = 0;
    int chunksize = 100;
    for (count = 0; count < mContactList.size(); count += chunksize) {
        QsonList chunk;
        for (int i = 0; i < chunksize; i++)
            chunk.append(mContactList.objectAt(count+i));
        QsonMap result = mJsonDb->createList(mOwner, chunk);
        QsonList data = result.subObject("result").subList("data");
        for (int i = 0; i < data.size(); i++)
            mUuids.append(data.objectAt(i).valueString(JsonDbString::kUuidStr));
    }
    long elapsed = time.elapsed();
    mUuids.sort();
    qDebug() << "done. Time per item (ms):" << (double)elapsed / count << "count" << count << "elapsed" << elapsed << "ms";
}

void TestJsonDb::init()
{
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
    foreach(JsonDbBtreeStorage *storage, mJsonDb->mStorages)
        QCOMPARE(storage->mTransactionDepth, 0);
    //QVERIFY(mJsonDb->checkValidity());
}

void TestJsonDb::addSchema(const QString &schemaName, QsonMap &schemaObject)
{
    QsonObject schema = readJsonFile(QString("../../auto/daemon/schemas/%1.json").arg(schemaName));
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

void TestJsonDb::qsonListCreate()
{
    QBENCHMARK {
        QsonList list;
    }
}

void TestJsonDb::qsonMapCreate()
{
    QBENCHMARK {
        QsonMap map;
    }
}

void TestJsonDb::qsonListReadValue_data()
{
    QTest::addColumn<QsonList>("list");
    QTest::addColumn<int>("index");
    QTest::addColumn<int>("value");

    QsonList data2;
    data2.append(123);
    data2.append(133);
    data2.append(323);
    QTest::newRow("small list") << data2 << 2 << 323;

    QsonList data3;
    for (int i = 0; i < 256; ++i)
        data3.append(i);
    QTest::newRow("large list") << data3 << 12 << 12;
}

void TestJsonDb::qsonListReadValue()
{
    QFETCH(QsonList, list);
    QFETCH(int, index);
    QFETCH(int, value);

    QBENCHMARK {
        QCOMPARE(list.at<int>(index), value);
    }
}

void TestJsonDb::qsonMapReadValue_data()
{
    QTest::addColumn<QsonMap>("map");
    QTest::addColumn<QString>("property");
    QTest::addColumn<int>("value");

    QsonMap data1;
    data1.insert(QString::number(1), 123);
    data1.insert(QString::number(12), 133);
    data1.insert(QString::number(123), 323);
    QTest::newRow("small map") << data1 << QString::number(123) << 323;

    QsonMap data2;
    for (int i = 0; i < 256; ++i)
        data2.insert(QString::number(i), i);
    QTest::newRow("large map") << data2 << QString::number(12) << 12;
}

void TestJsonDb::qsonMapReadValue()
{
    QFETCH(QsonMap, map);
    QFETCH(QString, property);
    QFETCH(int, value);

    QBENCHMARK {
        QCOMPARE(map.value<int>(property), value);
    }
}

void TestJsonDb::qsonListInsertValue()
{
    QBENCHMARK {
        QsonList list;
        for (int i = 0; i < 1024; ++i)
            list.append(i);
    }
}

void TestJsonDb::qsonMapInsertValue()
{
    const int iterations = 1024;
    QVarLengthArray<QString, iterations> names;

    for (int i = 0; i < iterations; ++i)
        names.append(QString::number(i));

    QBENCHMARK {
        QsonMap map;
        for (int i = 0; i < iterations; ++i)
            map.insert(names[i], i);
    }
}

void TestJsonDb::benchmarkCreate()
{
    QsonList contacts(readJsonFile("../../auto/daemon/largeContactsTest.json"));
    QBENCHMARK {
        QsonMap contact = contacts.objectAt(0);
            mJsonDb->create(mOwner, contact);
    }
}

void TestJsonDb::benchmarkFileAppend()
{
    QsonList contacts(readJsonFile("../../auto/daemon/largeContactsTest.json"));
    QFile objectFile("objectFile.bin");
    objectFile.open(QIODevice::ReadWrite);

    QBENCHMARK {
            objectFile.write(contacts.objectAt(0).data());
            objectFile.flush();
            fsync(objectFile.handle());
    }
}

void TestJsonDb::benchmarkFileAppend2()
{
    QsonObject bson(readJsonFile("../../auto/daemon/largeContactsTest.json"));
    QsonList contacts(bson);
    QFile objectFile("objectFile.bin");
    objectFile.open(QIODevice::ReadWrite);
    QFile objectFile2("objectFile2.bin");
    objectFile2.open(QIODevice::ReadWrite);

    QBENCHMARK {
            objectFile.write(contacts.objectAt(0).data());
            objectFile.flush();
            objectFile2.write(contacts.objectAt(0).data());
            objectFile2.flush();
            fsync(objectFile2.handle());
            fsync(objectFile.handle());
    }
}

void TestJsonDb::benchmarkParseQuery_data()
{
    QTest::addColumn<QString>("query");
    QTest::newRow("1")  << "[?foo exists]";
    QTest::newRow("2")  << "[?foo->bar exists]";
    QTest::newRow("3")  << "[?foo->bar->baz exists]";
    QTest::newRow("4")  << "[?foo=\"bar\"]";
    QTest::newRow("5")  << "[?foo= %bar ]";
    QTest::newRow("6")  << "[?foo= %bar]";
    QTest::newRow("7")  << "[?foo=%bar]";
    QTest::newRow("8")  << "[?foo=\"bar\" | foo=\"baz\"]";
    QTest::newRow("9")  << "[?foo=\"bar\"][/foo]";
    QTest::newRow("10") << "[?foo=\"bar\"][= a ]";
    QTest::newRow("11") << "[?foo =~ \"/a\\//\"]";
    QTest::newRow("12") << "[?foo=\"bar\"][= a,b,c]";
    QTest::newRow("13") << "[?foo=\"bar\"][= a->foreign,b,c]";
    QTest::newRow("14") << "[?foo=\"bar\"][=[ a,b,c]]";
    QTest::newRow("15") << "[?foo=\"bar\"][={ a:x, b:y, c:z}]";
    QTest::newRow("16") << "[?foo=\"bar\"][={ a:x->foreign, b:y, c:z}]";
    QTest::newRow("17") << "[?foo=\"bar\"][= _uuid, name.first, name.last ]";
    QTest::newRow("18") << "[?_type=\"contact\"][= { uuid: _uuid, first: name.first, last: name.last } ]";
    QTest::newRow("19") << "[?telephoneNumbers.*.number=\"6175551212\"]";
    QTest::newRow("20") << "[?_type=\"contact\"][= .telephoneNumbers[*].number]";
    QTest::newRow("21") << "[?_type=\"contact\"][?foo startsWith \"bar\"]";
}

void TestJsonDb::benchmarkParseQuery()
{
    QFETCH(QString, query);
    QsonMap bindings;
    bindings.insert("bar", QString("barValue"));
    QBENCHMARK {
        JsonDbQuery::parse(query, bindings);
    }
}

void TestJsonDb::benchmarkFieldMatch()
{
    int count = mContactList.size();
    if (!count)
        return;
    int itemNumber = (int)((double)qrand() * count / RAND_MAX);
    QsonMap item = mContactList.objectAt(itemNumber);
    QString query =
        QString("[?%1=\"%2\"][?name<=\"%3\"]")
        .arg(JsonDbString::kTypeStr)
        .arg("contact")
        .arg(item.valueString("name"));
    QRegExp fieldMatch("^\\[\\?\\s*([^\\[\\]]+)\\s*\\](.*)");
    QBENCHMARK {
        fieldMatch.exactMatch(query);
    }
}

void TestJsonDb::benchmarkTokenizer()
{
    QStringList queries = (QStringList()
                           << "[?abc=\"def\"]"
                           << "[ ? abc = \"def\" \t]"
                           << "[?abc.def=\"ghi\"]"
                           << "[?abc->def=\"ghi\"]"
                           << "[?abc.def=\"ghi\"][/abc.def]"
                           << "[?abc.def=\"ghi\"][\\foo]"
                           << "[?abc.def=\"ghi\"][=foo]"
                           << "[?abc.def=\"ghi\"][=foo,bar]"
                           << "[?abc.def=\"ghi\"][=.foo,.bar]"
                           << "[?abc.def=\"ghi\"][=[.foo,.bar]]"
                           << "[?abc.def=\"ghi\"][={foo:Foo,bar:Bar}][/foo]"
        );
    foreach (QString query, queries) {
        QBENCHMARK {
            JsonDbQueryTokenizer tokenizer(query);
            //qDebug() << "query" << query;
            //int i = 0;
            QString token;
            while (!(token = tokenizer.pop()).isEmpty()) {
                //qDebug() << QString("    token[%1] %2    ").arg(i++).arg(token);
            }
        }
    }
}

namespace QtAddOn { namespace JsonDb {
extern QByteArray makeForwardKey(const QVariant &fieldValue,  const ObjectKey &objectKey);
extern int forwardKeyCmp(const char *aptr, size_t asiz, const char *bptr, size_t bsiz, void *op);
} } // end namespace QtAddOn::JsonDb

void TestJsonDb::benchmarkForwardKeyCmp()
{
    int count = mContactList.size();

    QVector<QByteArray> keys;
    for (int ii = 0; ii < mContactList.size(); ii++) {
        QsonMap object = mContactList.objectAt(ii);
        QString typeName = object.valueString(JsonDbString::kTypeStr);
    //int typeNumber = ((JsonDbBtreeStorage *)mJsonDb->mStorage)->getTypeNumber(typeName);
        QVariant fullname = QVariant(object.valueString("fullname"));
        QByteArray key = makeForwardKey(fullname, ObjectKey());
        keys.append(key);
    }

    QBENCHMARK {
        for (int j = 0; j < count; j++) {
            QByteArray key1 = keys[j];
            for (int i = 0; i < count; i++) {
                QByteArray key2 = keys[i];
                int cmp = forwardKeyCmp(key1.constData(), key1.size(), key2.constData(), key2.size(), 0);
                if (i == j)
                    QVERIFY(cmp == 0);
                /* Note: this fails horrible but I guess it shouldn't
                else
                    QVERIFY(cmp != 0);
                    */
            }
        }
    }
}

void TestJsonDb::benchmarkParsedQuery()
{
#warning skipping
  return;

    int count = mContactList.size();
    if (!count)
        return;

    int itemNumber = (int)((double)qrand() * count / RAND_MAX);
    QsonMap item = mContactList.objectAt(itemNumber);

    QString query =
        QString("[?%1=\"%2\"][?name<=\"%3\"]")
        .arg(JsonDbString::kTypeStr)
        .arg("contact")
        .arg(item.valueString("name"));
    QsonMap bindings;
    JsonDbQuery parsedQuery = JsonDbQuery::parse(query, bindings);

    QBENCHMARK {
        QsonMap request;
        //QVariantList queryTerms = parseResult.value("queryTerms").toList();
        //QVariantList orderTerms = parseResult.value("orderTerms").toList();
        int limit = 1;
        int offset = 0;
        QsonMap result = mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->queryPersistentObjects(mOwner, parsedQuery, limit, offset);
        if (result.value<int>("length") != 1) {
          qDebug() << "result length" << result.value<int>("length");
          qDebug() << "item" << item;
          qDebug() << "itemNumber" << itemNumber;
        }
        QCOMPARE(result.value<int>("length"), 1);
        //      qDebug() << "result.keys()" << result.keys();
    }
}

void TestJsonDb::benchmarkSchemaValidation_data()
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

void TestJsonDb::benchmarkSchemaValidation()
{
    bool validate = gValidateSchemas;
    gValidateSchemas = true;

    QFETCH(QByteArray, item);
    QFETCH(bool, isPerson);
    QFETCH(bool, isAdult);
    Q_UNUSED(isAdult);

    const QByteArray person =
            "{"
            "    \"description\": \"A person\","
            "    \"type\": \"object\","
            "    \"properties\": {"
            "        \"name\": {\"type\": \"string\"},"
            "        \"age\" : {\"type\": \"integer\", \"maximum\": 125 }"
            "    }"
            "}";
    static int schemaId = 0;
    const QString personSchemaName = QString::fromLatin1("personBenchmark") + QString::number(++schemaId);

    QsonMap qResult;
    QsonMap personSchemaBody = readJson(person);
    QsonMap personSchemaObject;
    personSchemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
    personSchemaObject.insert("name", personSchemaName);
    personSchemaObject.insert("schema", personSchemaBody);
    qResult = mJsonDb->create(mOwner, personSchemaObject);
    verifyGoodResult(qResult);


    // Prepare items
    const uint numberOfIterations = 1000;
    QList<QsonMap> objects;
    objects.reserve(numberOfIterations);
    for (uint i = 0; i < numberOfIterations; ++i) {
        QsonMap object = readJson(item);
        object.insert("testingForAdult", i);
        object.insert(JsonDbString::kTypeStr, personSchemaName);
        objects.append(object);
    }

    QBENCHMARK_ONCE {
        foreach (QsonMap object, objects) {
            qResult = mJsonDb->validateSchema(personSchemaName, object);

            if (isPerson) {
                QVERIFY(qResult.isEmpty());
            } else {
                QVERIFY(!qResult.isEmpty());
            }
        }
    }

    gValidateSchemas = validate;
}

void TestJsonDb::benchmarkFind()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        int itemNumber = (int)((double)qrand() * count / RAND_MAX);
        QsonMap item = mContactList.objectAt(itemNumber);
        request.insert("query",
                       QString("[?name=\"%3\"][?%1=\"%2\"]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact")
                       .arg(item.valueString("name")));
        request.insert("limit", 1);
        QsonMap result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFindByName()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        int itemNumber = (int)((double)qrand() * count / RAND_MAX);
        QsonMap item = mContactList.objectAt(itemNumber);
        if (!item.contains("name"))
            qDebug() << "no name in item" << item;
        request.insert("query",
                       QString("[?name=\"%1\"]")
                       .arg(item.valueString("name")));
        request.insert("limit", 1);
        QsonMap result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFindByUuid()
{
    int count = mContactList.size();
    if (!count)
        return;

    QString uuid = mUuids[mUuids.size() / 2];
    QBENCHMARK {
        QsonMap request;
        request.insert("query",
                       QString("[?%1=\"%2\"]")
                       .arg(JsonDbString::kUuidStr)
                       .arg(uuid));
        QsonMap queryResult = mJsonDb->find(mOwner, request);
        verifyGoodQueryResult(queryResult);
    }
}

void TestJsonDb::benchmarkFindEQ()
{
  int count = mContactList.size();
  if (!count)
    return;

  QsonMap request;

  int itemNumber = (int)((double)qrand() * count / RAND_MAX);
  QsonMap item = mContactList.objectAt(itemNumber);
  request.insert("query",
                 QString("[?name.first=\"%3\"][?%1=\"%2\"]")
                 .arg(JsonDbString::kTypeStr)
                 .arg("contact")
                 .arg(JsonDb::propertyLookup(item, "name.first").toString()));
  QBENCHMARK {
    QsonMap result = mJsonDb->find(mOwner, request);
    verifyGoodResult(result);
  }
}

void TestJsonDb::benchmarkFindLE()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        int itemNumber = (int)((double)qrand() * count / RAND_MAX);
        QsonMap item = mContactList.objectAt(itemNumber);
        request.insert("query",
                       QString("[?name.first<=\"%3\"][?%1=\"%2\"]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact")
                       .arg(JsonDb::propertyLookup(item, "name.first").toString()));
        request.insert("limit", 1);
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFirst()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        request.insert("limit", 1);
        request.insert("query",
                       QString("[?%1=\"%2\"][/name.first][count]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}
void TestJsonDb::benchmarkLast()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        request.insert("limit", 1);
        request.insert("query",
                       QString("[?%1=\"%2\"][\\name.first][count]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFirst10()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        request.insert("limit", 10);
        request.insert("query",
                       QString("[?%1=\"%2\"][/name.first][count]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFind10()
{
    int count = mContactList.size();
    if (!count)
        return;

    int itemNumber = qMax(0, mFirstNames.size()-10);
    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        QsonMap item = mContactList.objectAt(itemNumber);
        request.insert("limit", 10);
        request.insert("query",
                       QString("[?%1=\"%2\"][?name.first>=\"%3\"][?name.last exists][/name.first]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact")
                       .arg(mFirstNames[itemNumber]));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFind20()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        int itemNumber = (int)((double)qrand() * count / RAND_MAX);
        QsonMap item = mContactList.objectAt(itemNumber);
        request.insert("limit", 20);
        request.insert("query",
                       QString("[?name.first<=\"%3\"][?%1=\"%2\"]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact")
                       .arg(JsonDb::propertyLookup(item, "name.first").toString()));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFirstByUuid()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        request.insert("limit", 1);
        request.insert("query",
                       QString("[?%1=\"%2\"][/_uuid][count]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkLastByUuid()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        request.insert("limit", 1);
        request.insert("query",
                       QString("[?%1=\"%2\"][\\_uuid][count]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFirst10ByUuid()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        request.insert("limit", 10);
        request.insert("query",
                       QString("[?%1=\"%2\"][/_uuid][count]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFind10ByUuid()
{
    int count = mContactList.size();
    if (!count)
        return;

    int itemNumber = qMax(0, mFirstNames.size()-10);
    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        QsonMap item = mContactList.objectAt(itemNumber);
        request.insert("limit", 10);
        request.insert("query",
                       QString("[?%1=\"%2\"][?_uuid<=\"%3\"][/_uuid][count]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact")
                       .arg(mUuids[itemNumber]));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFindUnindexed()
{
    int count = mContactList.size();
    if (!count)
        return;

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        int itemNumber = (int)((double)qrand() * count / RAND_MAX);
        QsonMap item = mContactList.objectAt(itemNumber);
        //qDebug() << item.value("firstName").toString();
        request.insert("query",
                       QString("[?%1=\"%2\"][?firstName=\"%3\"]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact")
                       .arg(item.valueString("firstName")));
        //request.insert("limit", 1);
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFindReindexed()
{
    int count = mContactList.size();
    if (!count)
        return;

    //qDebug() << "Adding index for lastName";
    addIndex("lastName");
    //qDebug() << "Done adding index for lastName";

    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        int itemNumber = (int)((double)qrand() * count / RAND_MAX);
        QsonMap item = mContactList.objectAt(itemNumber);
        //qDebug() << item.value("firstName").toString();
        request.insert("query",
                       QString("[?%1=\"%2\"][?lastName=\"%3\"]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact")
                       .arg(item.valueString("lastName")));
        //request.insert("limit", 1);
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
    }
}

void TestJsonDb::benchmarkFindNames()
{
    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        QVariantList items;
        request.insert("query",
                       QString("[?%1=\"%2\"]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        request.insert("limit", 1);
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
        result = result.subObject("result");
        int length = result.value<int>("length");
        if (false && (length > 0)) {
          QsonList data = result.subList("data");
          qDebug() << JsonWriter().toString(data.stringAt(0));
        }
    }
}

void TestJsonDb::findNamesMapL()
{
    QBENCHMARK_ONCE {
        QsonMap request;
        QsonMap result;

        request.insert("query",
                       QString("[?%1=\"%2\"][= [_uuid, name.first, name.last] ]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
        QCOMPARE(result.subObject("result").value<int>("length"), 1000);
        QCOMPARE(result.subObject("result").subList("data").size(), 1000);
    }
}

void TestJsonDb::benchmarkFindNamesMapL()
{
    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        request.insert("query",
                       QString("[?%1=\"%2\"][= [_uuid, name.first, name.last] ]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        request.insert("limit", 1);
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
        QCOMPARE(result.subObject("result").value<int>("length"), 1);
        QCOMPARE(result.subObject("result").subList("data").size(), 1);
    }
}


void TestJsonDb::findNamesMapO()
{
    QBENCHMARK_ONCE {
        QsonMap request;
        QsonMap result;
        request.insert("query",
                       QString("[?%1=\"%2\"][= { uuid: _uuid, first: name.first, last: name.last } ]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
        QCOMPARE(result.subObject("result").value<int>("length"), 1000);
        QCOMPARE(result.subObject("result").subList("data").size(), 1000);
    }
}

void TestJsonDb::benchmarkFindNamesMapO()
{
    QBENCHMARK {
        QsonMap request;
        QsonMap result;
        request.insert("query",
                       QString("[?%1=\"%2\"][= { uuid: _uuid, first: name.first, last: name.last } ]")
                       .arg(JsonDbString::kTypeStr)
                       .arg("contact"));
        request.insert("limit", 1);
        result = mJsonDb->find(mOwner, request);
        verifyGoodResult(result);
        QCOMPARE(result.subObject("result").value<int>("length"), 1);
        QCOMPARE(result.subObject("result").subList("data").size(), 1);
    }
}

void TestJsonDb::benchmarkCursorCount()
{
    QStringList queries = (QStringList()
                           << "[/name.first]"
                           << "[/name.first][?_type=\"contact\"]"
        );
    QsonMap bindings;
    foreach (QString query, queries) {
    JsonDbQuery parsedQuery = JsonDbQuery::parse(query, bindings);
        IndexQuery *indexQuery = mJsonDb->findPartition(JsonDbString::kSystemPartitionName)->compileIndexQuery(mOwner, parsedQuery);
        int count = 0;
        //qDebug() << "query" << query;
        QBENCHMARK {
            int mycount = 0;
            for (QsonMap object = indexQuery->first();
                 object.size() > 0;
                 object = indexQuery->next()) {
                mycount++;
            }
            count = mycount;
        }
    }
}

void TestJsonDb::benchmarkQueryCount()
{
    QStringList queries = (QStringList()
                           << "[/name.first]"
                           << "[/name.first][?_type=\"contact\"]"
        );
    foreach (QString query, queries) {
        QsonMap request;
        request.insert("query", QString("%1[count]").arg(query));
        QBENCHMARK {
            QsonMap result = mJsonDb->find(mOwner, request);
        }
    }
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

void TestJsonDb::benchmarkScriptEngineCreation()
{
    QJSValue result;
    QBENCHMARK {
        QJSEngine *engine = new QJSEngine();
        engine->setParent(0);
        QJSValue globalObject = engine->globalObject();
        result =
            engine->evaluate(QString("var reduce = function(k, v, s) { s.count = s.count + v.count; return s; };"));
        engine = 0;
    }
}

void TestJsonDb::contactListChaff()
{
    QBENCHMARK {
        for (int ii = 0; ii < mContactList.size(); ii++) {
        QsonMap data = mContactList.objectAt(ii);
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
    }
}

QTEST_MAIN(TestJsonDb)
#include "bench_daemon.moc"
