/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
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
#include "jsondb-strings.h"
#include "jsondb-error.h"

#include "../../shared/util.h"


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

QT_USE_NAMESPACE_JSONDB

class TestJsonDbQueries: public QObject
{
    Q_OBJECT
public:
    TestJsonDbQueries();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void queryAll();
    void queryOneType();
    void queryOneOrOtherType();
    void queryTypesIn();
    void queryFieldExists();
    void queryLessThan();
    void queryNotEqual();


private:
    void removeDbFiles();

    template <class CheckType>
    bool confirmEachObject(const QsonMap &result, CheckType checker)
    {
        QsonMap map = result.subObject(JsonDbString::kResultStr);
        QsonList data = map.value<QsonList>("data");
        for (int i = 0; i < data.size(); ++i)
            if (!checker(qsonToVariant(data.objectAt(i)).toMap()))
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


struct CheckObjectFieldEqualTo
{
    CheckObjectFieldEqualTo(QString fld, QString value)
        : field(fld), singlevalue(value)
    {}
    CheckObjectFieldEqualTo(QString fld, QStringList values)
        : field(fld), listOfValues(values)
    {}
    bool operator() (const QVariantMap &obj) const {
        if (!singlevalue.isEmpty())
            return obj[field].toString() == singlevalue;
        bool ok = false;
        foreach (QString str, listOfValues) {
            ok |= obj[field].toString() == str;
        }
        return ok;
    }
    QString field;
    QString singlevalue;
    QStringList listOfValues;
};

struct CheckObjectFieldNotEqualTo : public CheckObjectFieldEqualTo
{
    CheckObjectFieldNotEqualTo(QString fld, QString value)
        : CheckObjectFieldEqualTo(fld, value)
    {}
    CheckObjectFieldNotEqualTo(QString fld, QStringList values)
        : CheckObjectFieldEqualTo(fld, values)
    {}
    bool operator() (const QVariantMap &obj) const {
        return !CheckObjectFieldEqualTo::operator ()(obj);
    }
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

    // Set total number of default objects to use for verification
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[*]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    int numDefaultObjects = qsonToVariant(result.subObject(JsonDbString::kResultStr).value<QsonElement>("length")).toInt();

    QFile contactsFile(findFile(SRCDIR, "dataset.json"));
    QVERIFY2(contactsFile.exists(), "Err: dataset.json doesn't exist!");

    contactsFile.open(QIODevice::ReadOnly);
    QByteArray json = contactsFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok)
        qDebug() << parser.errorString();
    QVariantList contactList = parser.result().toList();
    foreach (QVariant v, contactList) {
        QsonMap object(variantToQson(v.toMap()));
        QString type = object.valueString("_type");

        // see dataset.json for data types. there's tight coupling between the code
        // in these tests and the data set.

        if (type == QString("dragon") || type == QString("bunny")) {
            QString name = object.valueString("name");
            QStringList names = name.split(" ");
            QsonMap nameObject;
            nameObject.insert("first", names[0]);
            nameObject.insert("last", names[names.size()-1]);
            object.insert("name", nameObject);
        } else if (type == QString("data-stats")) {
            object.insert("num-default-objects", numDefaultObjects);
        }
        verifyGoodResult(mJsonDb->create(mOwner, object));
    }

    // extract stats from data set and calculate a few others
    query = QsonMap();
    result = QsonMap();
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"data-stats\"]"));
    result = mJsonDb->find(mOwner, query);
    mDataStats = qsonToVariant(result.subObject(JsonDbString::kResultStr).value<QsonList>("data").objectAt(0)).toMap();


    mTotalObjects = mDataStats["num-objects"].toInt() +
                    numDefaultObjects +
                    1; // +1 for the data-stats object
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

void TestJsonDbQueries::cleanup()
{
}

void TestJsonDbQueries::queryAll()
{
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[*]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyResultField(result, "length", mTotalObjects);
}

void TestJsonDbQueries::queryOneType()
{
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"dragon\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyResultField(result, "length", mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(result, CheckObjectFieldEqualTo("_type", "dragon")));
}

void TestJsonDbQueries::queryOneOrOtherType()
{
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type=\"dragon\"|_type=\"bunny\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyResultField(result, "length", mDataStats["num-dragons"].toInt() + mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(result, CheckObjectFieldEqualTo("_type", QStringList() << "dragon" << "bunny")));
}

void TestJsonDbQueries::queryTypesIn()
{
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type in [\"dragon\",\"bunny\"]]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyResultField(result, "length", mDataStats["num-dragons"].toInt() + mDataStats["num-bunnies"].toInt());
    QVERIFY(confirmEachObject(result, CheckObjectFieldEqualTo("_type", QStringList() << "dragon" << "bunny")));
}

void TestJsonDbQueries::queryFieldExists()
{
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?color exists]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyResultField(result, "length", mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(result, CheckObjectFieldEqualTo("_type", "dragon")));
}

void TestJsonDbQueries::queryLessThan()
{
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?age < 5]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    QVERIFY(confirmEachObject(result, CheckObjectFieldEqualTo("age", QStringList() << "0" << "1" << "2" << "3" << "4")));
}

void TestJsonDbQueries::queryNotEqual()
{
    QsonMap query;
    query.insert(JsonDbString::kQueryStr, QString("[?_type != \"dragon\"]"));
    QsonMap result = mJsonDb->find(mOwner, query);
    verifyResultField(result, "length", mTotalObjects - mDataStats["num-dragons"].toInt());
    QVERIFY(confirmEachObject(result, CheckObjectFieldNotEqualTo("_type", "dragon")));
}

QTEST_MAIN(TestJsonDbQueries)
#include "testjsondbqueries.moc"
