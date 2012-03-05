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
#include "jsondbowner.h"
#include "jsondbpartition.h"
#include "jsondbquery.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"

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
    return mJsonDbPartition->queryObjects(owner, JsonDbQuery::parse(query, bindings));
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
    mJsonDbPartition = new JsonDbPartition(kFilename, QStringLiteral("com.example.JsonDbTestQueries"), mOwner, this);
    mJsonDbPartition->open();

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
    // single values are returned in queryResult.values
    QCOMPARE(queryResult.values.size(), mDataStats["num-red-eyes"].toInt());
    QCOMPARE(queryResult.values.at(0).toString(), QString("red"));

    queryResult = find(mOwner,  QLatin1String("[?\"eye-color\" = \"red\"][= [\"eye-color\", age ]]"));
    // array values are returned in queryResult.values
    QCOMPARE(queryResult.values.size(), mDataStats["num-red-eyes"].toInt());
    QCOMPARE(queryResult.values.at(0).toArray().at(0).toString(), QString("red"));

    queryResult = find(mOwner, QLatin1String("[?_type=\"dragon\"][/_type][?\"eye-color\" = \"red\"][= {\"color-of-eyes\": \"eye-color\" }]"));
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
    verifyGoodWriteResult(mJsonDbPartition->updateObject(mOwner, index));

    index = JsonDbObject();
    index.insert("_type", QString("Index"));
    index.insert("propertyName", QString("age"));
    index.insert("propertyType", QString("number"));
    verifyGoodWriteResult(mJsonDbPartition->updateObject(mOwner, index));

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
}

QTEST_MAIN(TestJsonDbQueries)
#include "testjsondbqueries.moc"
