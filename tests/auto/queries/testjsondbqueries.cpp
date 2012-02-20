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
#include "jsondb-strings.h"
#include "jsondb-error.h"

#include "../../shared/util.h"


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

QT_USE_NAMESPACE_JSONDB

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
    void queryFieldExists();
    void queryFieldNotExists();
    void queryLessThan();
    void queryLessThanOrEqual();
    void queryGreaterThan();
    void queryGreaterThanOrEqual();
    void queryNotEqual();
    void queryQuotedProperties();
    void querySortedByIndexName();

private:
    void removeDbFiles();

    template <class CheckType>
    bool confirmEachObject(const JsonDbObjectList &data, CheckType checker)
    {
        for (int i = 0; i < data.size(); ++i)
            if (!checker(data.at(i)))
                return false;
        return true;
    }

private:
    JsonDb *mJsonDb;
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

TestJsonDbQueries::TestJsonDbQueries()
    : mJsonDb(NULL), mOwner(0)
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

void TestJsonDbQueries::initTestCase()
{
    qsrand(QDateTime::currentDateTime().toTime_t());
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setOrganizationDomain("nrcc.noklab.com");
    QCoreApplication::setApplicationName("testjsodbqueries");
    QCoreApplication::setApplicationVersion("1.0");

    removeDbFiles();
    gVerbose = false;
    mJsonDb = new JsonDb(kFilename, this);
    mOwner = new JsonDbOwner(this);
    mOwner->setOwnerId("com.noklab.nrcc.JsonDbTestQueries");
    mJsonDb->open();

    QFile contactsFile(":/queries/dataset.json");
    QVERIFY2(contactsFile.exists(), "Err: dataset.json doesn't exist!");

    contactsFile.open(QIODevice::ReadOnly);
    QByteArray json = contactsFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    QVERIFY2(ok, parser.errorString().toAscii());
    QVariantList contactList = parser.result().toList();
    foreach (QVariant v, contactList) {
        JsonDbObject object(QJsonObject::fromVariantMap(v.toMap()));
        QString type = object.value("_type").toString();

        // see dataset.json for data types. there's tight coupling between the code
        // in these tests and the data set.
        if (type == QString("dragon") || type == QString("bunny")) {
            QString name = object.value("name").toString();
            QStringList names = name.split(" ");
            QJsonObject nameObject;
            nameObject.insert("first", names[0]);
            nameObject.insert("last", names[names.size()-1]);
            object.insert("name", nameObject);
        }
        verifyGoodResult(mJsonDb->create(mOwner, object));
    }
}

void TestJsonDbQueries::cleanupTestCase()
{
    if (mJsonDb) {
        mJsonDb->close();
        delete mJsonDb;
        mJsonDb = 0;
    }
    removeDbFiles();
}

void TestJsonDbQueries::init()
{
    // Set total number of objects and total default objects not used by the
    // data set to use for verification
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[*]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    mTotalObjects = queryResult.data.size();

    // extract stats from data set and calculate a few others
    query = QJsonObject();
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"data-stats\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    mDataStats = queryResult.data.at(0).toVariantMap();
    mDataStats["num-default-objects"] = mTotalObjects - mDataStats["num-objects"].toInt();

    JsonDbObject statObject = JsonDbObject::fromVariantMap(mDataStats);
    verifyGoodResult(mJsonDb->update(mOwner, statObject));
}

void TestJsonDbQueries::cleanup()
{
}

void TestJsonDbQueries::queryAll()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[*]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mTotalObjects);
}

void TestJsonDbQueries::queryOneType()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"dragon\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "dragon")));
}

void TestJsonDbQueries::queryOneOrOtherType()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"dragon\"|_type=\"bunny\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt() + mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", QStringList() << "dragon" << "bunny")));
}

void TestJsonDbQueries::queryTypesIn()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type in [\"dragon\",\"bunny\"]]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt() + mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", QStringList() << "dragon" << "bunny")));
}

void TestJsonDbQueries::queryFieldExists()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?color exists]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "dragon")));
}

void TestJsonDbQueries::queryFieldNotExists()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?color notExists][? _type = \"bunny\" | _type=\"dragon\"][/_type]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(),
             mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "bunny")));
}

void TestJsonDbQueries::queryLessThan()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?age < 5]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<double>("age", QList<double>() << 0 << 1 << 2 << 3 << 4)));
}

void TestJsonDbQueries::queryLessThanOrEqual()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?age <= 5]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<double>("age", QList<double>() << 0 << 1 << 2 << 3 << 4 << 5)));
}

void TestJsonDbQueries::queryGreaterThan()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?age > 5]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<double>("age", QList<double>() << 6 << 7 << 8)));
}

void TestJsonDbQueries::queryGreaterThanOrEqual()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?age >= 5]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<double>("age", QList<double>() << 5 << 6 << 7 << 8)));
}

void TestJsonDbQueries::queryNotEqual()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type != \"dragon\"]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mTotalObjects - mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldNotEqualTo<QString>("_type", "dragon")));
}

void TestJsonDbQueries::queryQuotedProperties()
{
    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type = \"dragon\"][?\"eye-color\" = \"red\"][/_type]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-red-eyes"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckObjectFieldEqualTo<QString>("_type", "dragon")));

    query.insert(JsonDbString::kQueryStr, QString("[?_type = \"dragon\"][?\"eye-color\" = \"red\"][= \"eye-color\"]"));
    queryResult = mJsonDb->find(mOwner, query);
    // single values are returned in queryResult.values
    QCOMPARE(queryResult.values.size(), mDataStats["num-red-eyes"].toInt());
    QCOMPARE(queryResult.values.at(0).toString(), QString("red"));

    query.insert(JsonDbString::kQueryStr, QString("[?\"eye-color\" = \"red\"][= [\"eye-color\", age ]]"));
    queryResult = mJsonDb->find(mOwner, query);
    // array values are returned in queryResult.values
    QCOMPARE(queryResult.values.size(), mDataStats["num-red-eyes"].toInt());
    QCOMPARE(queryResult.values.at(0).toArray().at(0).toString(), QString("red"));

    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"dragon\"][/_type][?\"eye-color\" = \"red\"][= {\"color-of-eyes\": \"eye-color\" }]"));
    queryResult = mJsonDb->find(mOwner, query);
    // object values are returned in queryResult.data
    QCOMPARE(queryResult.data.size(), mDataStats["num-red-eyes"].toInt());
    QCOMPARE(queryResult.data.at(0).value("color-of-eyes").toString(), QString("red"));
}

void TestJsonDbQueries::querySortedByIndexName()
{
    JsonDbObject index;
    index.insert("_type", QString("Index"));
    index.insert("name", QString("dragonSort"));
    index.insert("propertyName", QString("age"));
    index.insert("propertyType", QString("number"));
    verifyGoodResult(mJsonDb->create(mOwner, index));

    index = JsonDbObject();
    index.insert("_type", QString("Index"));
    index.insert("propertyName", QString("age"));
    index.insert("propertyType", QString("number"));
    verifyGoodResult(mJsonDb->create(mOwner, index));

    QJsonObject query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type = \"dragon\"][/age]"));
    JsonDbQueryResult queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckSortOrder<double>("age", QList<double>() << 0 << 0 << 2 << 2 << 4 << 4 << 6 << 6 << 8 << 8)));

    query = QJsonObject();
    query.insert(JsonDbString::kQueryStr, QString("[?_type = \"dragon\"][/dragonSort]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckSortOrder<double>("age", QList<double>() << 0 << 0 << 2 << 2 << 4 << 4 << 6 << 6 << 8 << 8)));

    query = QJsonObject();
    query.insert(JsonDbString::kQueryStr, QString("[?_type = \"dragon\"][\\age]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckSortOrder<double>("age", QList<double>() << 8 << 8 << 6 << 6 << 4 << 4 << 2 << 2 << 0 << 0)));

    query = QJsonObject();
    query.insert(JsonDbString::kQueryStr, QString("[?_type = \"dragon\"][\\dragonSort]"));
    queryResult = mJsonDb->find(mOwner, query);
    QCOMPARE(queryResult.data.size(), mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(queryResult.data, CheckSortOrder<double>("age", QList<double>() << 8 << 8 << 6 << 6 << 4 << 4 << 2 << 2 << 0 << 0)));
}

QTEST_MAIN(TestJsonDbQueries)
#include "testjsondbqueries.moc"
