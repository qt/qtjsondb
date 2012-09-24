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
#include <QtTest/QtTest>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTime>
#include <QUuid>

#include "jsondbowner.h"
#include "jsondbpartition.h"
#include "jsondbquery.h"
#include "jsondbqueryparser.h"
#include "jsondbstrings.h"
#include "jsondberrors.h"

#include "../../shared/util.h"

#define verifyGoodWriteResult(result) \
{ \
    JsonDbWriteResult __result = result; \
    QVERIFY2(__result.message.isEmpty(), __result.message.toLocal8Bit()); \
    QVERIFY(__result.code == JsonDbError::NoError); \
}

#define verifyGoodQueryResult(result) \
{ \
    JsonDbQueryResult __result = result; \
    QVERIFY2(__result.error.type() == QJsonValue::Null,  \
         __result.error.toObject().value("message").toString().toLocal8Bit()); \
}

QT_USE_NAMESPACE_JSONDB_PARTITION

class TestJsonDbQueries: public QObject
{
    Q_OBJECT
public:
    TestJsonDbQueries();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void queryAll();
    void queryOneType();
    void queryOneOrOtherType();
    void queryTypesIn();
    void queryUnion();
    void queryFieldExists();
    void queryFieldNotExists();
    void queryLessThan();
    void queryLessThanOrEqual();
    void queryGreaterThan();
    void queryGreaterThanOrEqual();
    void queryNotEqual();
    void queryQuotedProperties();
    void querySortedByIndexName();
    void queryContains();
    void queryInvalid();
    void queryRegExp();
    void queryExtract();
    void queryExtractLink();
    void queryJoinedObject();

private:
    void removeDbFiles();
    JsonDbQueryResult find(JsonDbOwner *owner, const QString &query, const QJsonObject bindings = QJsonObject());

    template <class CheckType>
    bool confirmEachObject(const JsonDbObjectList &data, CheckType checker)
    {
        for (int i = 0; i < data.size(); ++i)
            if (!checker(data.at(i)))
                return false;
        return true;
    }

private:
    JsonDbPartition *mJsonDbPartition;
    JsonDbOwner *mOwner;
    QVariantMap mDataStats;
    int mTotalObjects;
};

const char *kFilename = "test_queries";

// Passed to confirmEachObject to verify if field in data results matches either a single value
// or matches any value in a list of values.
template<class T>
struct CheckObjectFieldEqualTo
{
    CheckObjectFieldEqualTo(QString fld, T value)
        : field(fld), singlevalue(value)
    {}
    CheckObjectFieldEqualTo(QString fld, QList<T> values)
        : field(fld), listOfValues(values)
    {}
    bool operator() (const JsonDbObject &obj) const {
        if (listOfValues.isEmpty())
            return (obj[field] == singlevalue);
        bool ok = false;
        foreach (T str, listOfValues) {
            ok |= (obj[field] == str);
        }
        return ok;
    }
    QString field;
    T singlevalue;
    QList<T> listOfValues;
};


// Passed to confirmEachObject to verify that field in data results doesnt match either a single value
// or doesnt matches any value in a list of values.
template <class T>
struct CheckObjectFieldNotEqualTo : public CheckObjectFieldEqualTo<T>
{
    CheckObjectFieldNotEqualTo(QString fld, T value)
        : CheckObjectFieldEqualTo<T>(fld, value)
    {}
    CheckObjectFieldNotEqualTo(QString fld, QList<T> values)
        : CheckObjectFieldEqualTo<T>(fld, values)
    {}
    bool operator() (const JsonDbObject &obj) const {
        return !CheckObjectFieldEqualTo<T>::operator ()(obj);
    }
};


// Passed to confirmEachObject to verify that the order of the returned results matches the
// order of the list of values that are passed in
template<class T>
struct CheckSortOrder
{
    CheckSortOrder(QString fld, QList<T> values)
        : index(0), field(fld), listOfValues(values)
    {}
    bool operator() (const JsonDbObject &obj) {
        return obj[field].toVariant().template value<T>() == listOfValues.at(index++);
    }
    int index;
    QString field;
    QList<T> listOfValues;
};

TestJsonDbQueries::TestJsonDbQueries() :
    mJsonDbPartition(0)
  , mOwner(0)
{
}

void TestJsonDbQueries::removeDbFiles()
{
    QStringList filters;
    filters << QLatin1String("*.db");
    QStringList lst = QDir().entryList(filters);
    foreach (const QString &fileName, lst)
        QFile::remove(fileName);
}

JsonDbQueryResult TestJsonDbQueries::find(JsonDbOwner *owner, const QString &query, const QJsonObject bindings)
{
    JsonDbQueryParser parser;
    parser.setQuery(query);
    parser.setBindings(bindings);
    parser.parse();
    return mJsonDbPartition->queryObjects(owner, parser.result());
}

void TestJsonDbQueries::initTestCase()
{
    qsrand(QDateTime::currentDateTime().toTime_t());
    QCoreApplication::setOrganizationName("Example");
    QCoreApplication::setOrganizationDomain("example.com");
    QCoreApplication::setApplicationName("testjsodbqueries");
    QCoreApplication::setApplicationVersion("1.0");

    removeDbFiles();
    mOwner = new JsonDbOwner(this);
    mOwner->setOwnerId("com.example.JsonDbTestQueries");
    JsonDbPartitionSpec spec;
    spec.name = QStringLiteral("com.example.JsonDbTestQueries");
    spec.path = QDir::currentPath();
    mJsonDbPartition = new JsonDbPartition(this);
    mJsonDbPartition->setPartitionSpec(spec);
    mJsonDbPartition->setDefaultOwner(mOwner);
    mJsonDbPartition->open();

    QJsonArray contactList = readJsonFile(":/queries/dataset.json").toArray();
    foreach (QJsonValue v, contactList) {
        JsonDbObject object(v.toObject());
        QString type = object.value("_type").toString();

        // see dataset.json for data types. there's tight coupling between the code
        // in these tests and the data set.
        if (type == QLatin1String("dragon") || type == QLatin1String("bunny")) {
            QString name = object.value("name").toString();
            QStringList names = name.split(" ");
            QJsonObject nameObject;
            nameObject.insert("first", names[0]);
            nameObject.insert("last", names[names.size()-1]);
            object.insert("name", nameObject);
        }
        verifyGoodWriteResult(mJsonDbPartition->updateObject(mOwner, object));
    }
}

void TestJsonDbQueries::cleanupTestCase()
{
    if (mJsonDbPartition) {
        mJsonDbPartition->close();
        delete mJsonDbPartition;
        mJsonDbPartition = 0;
    }
    removeDbFiles();
}

void TestJsonDbQueries::init()
{
    // Set total number of objects and total default objects not used by the
    // data set to use for verification
    JsonDbQueryResult queryResult = find(mOwner, QString("[*]"));
    mTotalObjects = queryResult.data.size();

    // extract stats from data set and calculate a few others
    queryResult = find(mOwner,  QString("[?_type=\"data-stats\"]"));
    mDataStats = queryResult.data.at(0).toVariantMap();
    mDataStats["num-default-objects"] = mTotalObjects - mDataStats["num-objects"].toInt();

    JsonDbObject statObject = JsonDbObject::fromVariantMap(mDataStats);
    verifyGoodWriteResult(mJsonDbPartition->updateObject(mOwner, statObject));
}

void TestJsonDbQueries::cleanup()
{
}

void TestJsonDbQueries::queryAll()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[*]"));
    QCOMPARE(queryResult.data.size(), mTotalObjects);
}

void TestJsonDbQueries::queryOneType()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type=\"dragon\"]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "dragon")));
}

void TestJsonDbQueries::queryOneOrOtherType()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type=\"dragon\"|_type=\"bunny\"]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt() + mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", QStringList() << "dragon" << "bunny")));
}

void TestJsonDbQueries::queryTypesIn()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type in [\"dragon\",\"bunny\"]]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt() + mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", QStringList() << "dragon" << "bunny")));
}

void TestJsonDbQueries::queryUnion()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type=\"bunny\"][?age=8|type=\"demon\"]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-bunnies-age-8"].toInt() + mDataStats["num-demon-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", QStringList() << "bunny")));

    // verify it still works if one of the fields has an index
    // verify can use notExists on an indexed field
    JsonDbObject index;
    index.insert("_uuid", QString("{df462800-ba09-470f-b3f6-9af39a64f9bb}"));
    index.insert("_type", QString("Index"));
    index.insert("name", QString("age"));
    index.insert("propertyName", QString("age"));
    index.insert("propertyType", QString("string"));
    JsonDbWriteResult result = mJsonDbPartition->updateObject(mOwner, index);
    verifyGoodWriteResult(result);

    queryResult = find(mOwner, QLatin1String("[?_type=\"bunny\"][?age=8|type=\"demon\"]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-bunnies-age-8"].toInt() + mDataStats["num-demon-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", QStringList() << "bunny")));

    index.markDeleted();
    result = mJsonDbPartition->updateObject(mOwner, index, JsonDbPartition::Replace);
    verifyGoodWriteResult(result);
}

void TestJsonDbQueries::queryFieldExists()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?color exists]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "dragon")));
}

void TestJsonDbQueries::queryFieldNotExists()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?color notExists][? _type = \"bunny\" | _type=\"dragon\"][/_type]"));
    QCOMPARE(queryResult.data.size(),
             mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "bunny")));

    // verify can use notExists on an indexed field
    JsonDbObject index;
    index.insert("_type", QString("Index"));
    index.insert("name", QString("color"));
    index.insert("propertyName", QString("color"));
    index.insert("propertyType", QString("string"));
    verifyGoodWriteResult(mJsonDbPartition->updateObject(mOwner, index));

    // notExists on an indexed field will choose a different sortKey
    queryResult = find(mOwner, QLatin1String("[?color notExists][? _type = \"bunny\" | _type=\"dragon\"]"));
    QVERIFY(!queryResult.sortKeys.contains(QLatin1String("color")));
    QCOMPARE(queryResult.data.size(),
             mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "bunny")));

    // notExists on an indexed field will choose a different sortKey, even if we try to force it
    queryResult = find(mOwner, QLatin1String("[?color notExists][? _type = \"bunny\" | _type=\"dragon\"][/color]"));
    QVERIFY(!queryResult.sortKeys.contains(QLatin1String("color")));
    QCOMPARE(queryResult.data.size(),
             mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "bunny")));

    // exists on an indexed field will choose the field as sortKey
    queryResult = find(mOwner, QLatin1String("[?color exists][? _type = \"bunny\" | _type=\"dragon\"]"));
    QVERIFY(queryResult.sortKeys.contains(QLatin1String("color")));
    QCOMPARE(queryResult.data.size(),
             mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "dragon")));
}

void TestJsonDbQueries::queryLessThan()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?age < 5]"));
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<double>("age", QList<double>() << 0 << 1 << 2 << 3 << 4)));
}

void TestJsonDbQueries::queryLessThanOrEqual()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?age <= 5]"));
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<double>("age", QList<double>() << 0 << 1 << 2 << 3 << 4 << 5)));
}

void TestJsonDbQueries::queryGreaterThan()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?age > 5]"));
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<double>("age", QList<double>() << 6 << 7 << 8)));
}

void TestJsonDbQueries::queryGreaterThanOrEqual()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?age >= 5]"));
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<double>("age", QList<double>() << 5 << 6 << 7 << 8)));
}

void TestJsonDbQueries::queryNotEqual()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type != \"dragon\"]"));
    QCOMPARE(queryResult.data.size(), mTotalObjects - mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldNotEqualTo<QString>("_type", "dragon")));
}

void TestJsonDbQueries::queryQuotedProperties()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type = \"dragon\"][?\"eye-color\" = \"red\"][/_type]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-red-eyes"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "dragon")));

    queryResult = find(mOwner, QLatin1String("[?_type = \"dragon\"][?\"eye-color\" = \"red\"][= \"eye-color\"]"));
    // no longer supported
    QCOMPARE(queryResult.data.size(), 0);
    QCOMPARE(queryResult.code, JsonDbError::MissingQuery);

    queryResult = find(mOwner, QLatin1String("[?\"eye-color\" = \"red\"][= [\"eye-color\", age ]]"));
    // no longer supported
    QCOMPARE(queryResult.data.size(), 0);
    QCOMPARE(queryResult.code, JsonDbError::MissingQuery);

    queryResult = find(mOwner, QLatin1String("[?_type=\"dragon\"][/_type][?\"eye-color\" = \"red\"][= {\"color-of-eyes\": \"eye-color\" }]"));
    // object values are returned in queryResult.data
    QCOMPARE(queryResult.data.size(), mDataStats["num-red-eyes"].toInt());
    QCOMPARE(queryResult.data.at(0).value("color-of-eyes").toString(), QString("red"));
    QCOMPARE(queryResult.data.at(0).count(), 1);
}

void TestJsonDbQueries::querySortedByIndexName()
{
    JsonDbObject index;
    index.insert("_uuid", QString("{fee2baf1-a2f8-4a43-b717-ab32408bdbdb}"));
    index.insert("_type", QString("Index"));
    index.insert("name", QString("dragonSort"));
    index.insert("propertyName", QString("age"));
    index.insert("propertyType", QString("number"));
    verifyGoodWriteResult(mJsonDbPartition->updateObject(mOwner, index));

    JsonDbObject index2;
    index2.insert("_uuid", QString("{23765dcd-0d43-47f3-820d-2407bfb4e5f0}"));
    index2.insert("_type", QString("Index"));
    index2.insert("name", QString("age"));
    index2.insert("propertyName", QString("age"));
    index2.insert("propertyType", QString("number"));
    verifyGoodWriteResult(mJsonDbPartition->updateObject(mOwner, index2));

    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type = \"dragon\"][/age]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckSortOrder<double>("age", QList<double>() << 0 << 0 << 2 << 2 << 4 << 4 << 6 << 6 << 8 << 8)));

    queryResult = find(mOwner, QLatin1String("[?_type = \"dragon\"][/dragonSort]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckSortOrder<double>("age", QList<double>() << 0 << 0 << 2 << 2 << 4 << 4 << 6 << 6 << 8 << 8)));

    queryResult = find(mOwner, QLatin1String("[?_type = \"dragon\"][\\age]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckSortOrder<double>("age", QList<double>() << 8 << 8 << 6 << 6 << 4 << 4 << 2 << 2 << 0 << 0)));

    queryResult = find(mOwner, QLatin1String("[?_type = \"dragon\"][\\dragonSort]"));
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckSortOrder<double>("age", QList<double>() << 8 << 8 << 6 << 6 << 4 << 4 << 2 << 2 << 0 << 0)));

    index.markDeleted();
    index2.markDeleted();
    QList<JsonDbObject> objects;
    objects << index << index2;
    JsonDbWriteResult result = mJsonDbPartition->updateObjects(mOwner, objects, JsonDbPartition::Replace);
    verifyGoodWriteResult(result);
}

void TestJsonDbQueries::queryContains()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?friends contains \"spike\" ]"));
    QCOMPARE(queryResult.code, JsonDbError::NoError);
    QCOMPARE(queryResult.data.count(), 1);
    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?friends contains  { \"name\" : \"puffy\", \"dog\" : false } ]"));
    QCOMPARE(queryResult.code, JsonDbError::NoError);
    QCOMPARE(queryResult.data.count(), 1);
    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?friends contains { \"name\" : \"puffy\", \"dog\" : true } | friends contains { \"name\" : \"rover\", \"dog\" : true } ]"));
    QCOMPARE(queryResult.code, JsonDbError::NoError);
    QCOMPARE(queryResult.data.count(), 1);
    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?friends contains  [\"spike\", \"rover\"] ]"));
    QCOMPARE(queryResult.code, JsonDbError::NoError);
    QCOMPARE(queryResult.data.count(), 1);

    // invalid queries
    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?friends contains  { \"name\" : \"puffy\" \"dog\" : false } ]"));
    QVERIFY(queryResult.code != JsonDbError::NoError);
    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?friends contains  [\"spike\", \"rover\" ]"));
    QVERIFY(queryResult.code != JsonDbError::NoError);
    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?friends contains  \"spike\", \"rover\" ] ]"));
    QVERIFY(queryResult.code != JsonDbError::NoError);

    // verify it works on an indexed property
    JsonDbObject index;
    index.insert("_type", QString("Index"));
    index.insert("name", QString("friends"));
    index.insert("propertyName", QString("friends"));
    verifyGoodWriteResult(mJsonDbPartition->updateObject(mOwner, index));

    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?friends contains \"spike\" ][/friends]"));
    QCOMPARE(queryResult.code, JsonDbError::NoError);
    // contains is an unindexable query constraint, so it uses _type instead of friends
    QCOMPARE(queryResult.sortKeys.at(0), QLatin1String("_type"));
    QCOMPARE(queryResult.data.count(), 1);
}

void TestJsonDbQueries::queryInvalid()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("foo"));
    QVERIFY(queryResult.code != JsonDbError::NoError);
}

void TestJsonDbQueries::queryRegExp()
{
    JsonDbQueryResult queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?name =~ \"/*ov*/w\" ]"));
    QCOMPARE(queryResult.code, JsonDbError::NoError);
    QCOMPARE(queryResult.data.count(), 1);

    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?name !=~ \"/*ov*/w\" ]"));
    QCOMPARE((int)queryResult.code, (int)JsonDbError::NoError);
    QCOMPARE(queryResult.data.count(), mDataStats["num-dogs"].toInt()-1);

    QJsonObject bindings;
    bindings.insert(QLatin1String("regexp"), QLatin1String("/*ov*/w"));
    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?name =~ %regexp ]"), bindings);
    QCOMPARE(queryResult.code, JsonDbError::NoError);
    QCOMPARE(queryResult.data.count(), 1);

    bindings.insert(QLatin1String("regexp"), QLatin1String("/*ov*/w"));
    queryResult = find(mOwner, QLatin1String("[?_type = \"dog\"][?name !=~ %regexp ]"), bindings);
    QCOMPARE(queryResult.code, JsonDbError::NoError);
    QCOMPARE(queryResult.data.count(), mDataStats["num-dogs"].toInt()-1);
}

void TestJsonDbQueries::queryExtract()
{
    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][={name:name, uuid:_uuid}]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE(queryResult.code, JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("name")).toObject().value(QLatin1String("first")).toString(), QLatin1String("thomas"));
        QCOMPARE(bunny.value(QLatin1String("name")).toObject().value(QLatin1String("last")).toString(), QLatin1String("reim"));
        QCOMPARE(bunny.value(QLatin1String("uuid")).toString(), QLatin1String("{a58f8d1a-bc0d-484f-8fed-803144f5df83}"));
        QCOMPARE(bunny.keys().size(), 2);
    }

    // check nested objects
    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][={firstName:name.first, lastName:name.last, uuid:_uuid}]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE(queryResult.code, JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("firstName")).toString(), QLatin1String("thomas"));
        QCOMPARE(bunny.value(QLatin1String("lastName")).toString(), QLatin1String("reim"));
        QCOMPARE(bunny.value(QLatin1String("uuid")).toString(), QLatin1String("{a58f8d1a-bc0d-484f-8fed-803144f5df83}"));
        QCOMPARE(bunny.keys().size(), 3);
    }

    // check very nested objects
    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][={v:one.two.three.four.five.foobar}]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE(queryResult.code, JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("v")).toString(), QLatin1String("bazinga"));
        QCOMPARE(bunny.keys().size(), 1);
    }

    // check extracting array items by index
    {
        QString query = QLatin1String("[?_type = \"dog\"][?name = \"spike\"][={name:name, f:friends, firstFriend:friends.0, secondFriendName:friends.1.name}]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE(queryResult.code, JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject dog = queryResult.data.at(0);
        QCOMPARE(dog.value(QLatin1String("name")).toString(), QLatin1String("spike"));
        QCOMPARE(dog.value(QLatin1String("firstFriend")).toObject().value(QLatin1String("name")).toString(), QLatin1String("rover"));
        QCOMPARE(dog.value(QLatin1String("secondFriendName")).toString(), QLatin1String("puffy"));
        QCOMPARE(dog.value(QLatin1String("f")).toArray().size(), 2);
        QCOMPARE(dog.keys().size(), 4);
    }

    // check _indexValue objects
    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][={firstName:name.first, iv: _indexValue, lastName:name.last}][/_type]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE(queryResult.code, JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("firstName")).toString(), QLatin1String("thomas"));
        QCOMPARE(bunny.value(QLatin1String("lastName")).toString(), QLatin1String("reim"));
        QCOMPARE(bunny.value(QLatin1String("iv")).toString(), QLatin1String("bunny"));
        QCOMPARE(bunny.keys().size(), 3);
    }
}

void TestJsonDbQueries::queryExtractLink()
{
    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][={firstName:name.first, linkedType:link->_type, linkedFirstName:link->name, uuid:_uuid, linkedUuid:link->_uuid}]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE(queryResult.code, JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("firstName")).toString(), QLatin1String("thomas"));
        QCOMPARE(bunny.value(QLatin1String("linkedType")).toString(), QLatin1String("dog"));
        QCOMPARE(bunny.value(QLatin1String("linkedFirstName")).toString(), QLatin1String("spike"));
        QCOMPARE(bunny.value(QLatin1String("uuid")).toString(), QLatin1String("{a58f8d1a-bc0d-484f-8fed-803144f5df83}"));
        QCOMPARE(bunny.value(QLatin1String("linkedUuid")).toString(), QLatin1String("{40e4823c-d45e-4ba6-8c3c-faaaf3a8495f}"));
        QCOMPARE(bunny.keys().size(), 5);
    }

    // check multi-level link
    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists]"
            "[={"
            "type:link->link2->link2->type, "
            "firstName:link->link2->link2->name.first, "
            "lastName:link->link2->link2->name.last"
            "}]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE(queryResult.code, JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("type")).toString(), QLatin1String("demon"));
        QCOMPARE(bunny.value(QLatin1String("firstName")).toString(), QLatin1String("stan"));
        QCOMPARE(bunny.value(QLatin1String("lastName")).toString(), QLatin1String("young"));
        QCOMPARE(bunny.keys().size(), 3);
    }
}

void TestJsonDbQueries::queryJoinedObject()
{
    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][?link->name = \"spike\"]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE((int)queryResult.code, (int)JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("name")).toObject().value(QLatin1String("first")).toString(), QLatin1String("thomas"));
        QVERIFY(!bunny.contains(QLatin1String("link2")));
    }

    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][?link->name = \"spike\"][?age = 8]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE((int)queryResult.code, (int)JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("name")).toObject().value(QLatin1String("first")).toString(), QLatin1String("thomas"));
        QVERIFY(!bunny.contains(QLatin1String("link2")));
    }

    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][?link->name = \"spike\"][?friends exists]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE((int)queryResult.code, (int)JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 0);
    }

    {
        QString query = QLatin1String("[?_type = \"bunny\"][?link exists][?link->name = \"spike\"][?link->friends exists]");
        JsonDbQueryResult queryResult = find(mOwner, query);
        QCOMPARE((int)queryResult.code, (int)JsonDbError::NoError);
        QCOMPARE(queryResult.data.count(), 1);
        QJsonObject bunny = queryResult.data.at(0);
        QCOMPARE(bunny.value(QLatin1String("name")).toObject().value(QLatin1String("first")).toString(), QLatin1String("thomas"));
        QVERIFY(!bunny.contains(QLatin1String("link2")));
    }
}

QTEST_MAIN(TestJsonDbQueries)
#include "testjsondbqueries.moc"
