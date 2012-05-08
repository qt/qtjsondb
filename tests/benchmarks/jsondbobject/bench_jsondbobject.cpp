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

#include "jsondbobject.h"

QT_USE_NAMESPACE_JSONDB_PARTITION

class TestJsonDbObject: public QObject
{
    Q_OBJECT
public:
    TestJsonDbObject();

private slots:
    void init();
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void valueByPath_data();
    void valueByPath();
};

TestJsonDbObject::TestJsonDbObject()
{
}

void TestJsonDbObject::initTestCase()
{
}

void TestJsonDbObject::init()
{
}

void TestJsonDbObject::cleanupTestCase()
{
}

void TestJsonDbObject::cleanup()
{
}

void TestJsonDbObject::valueByPath_data()
{
    QTest::addColumn<bool>("stringlist");

    QTest::newRow("string") << false;
    QTest::newRow("stringlist") << true;
}

void TestJsonDbObject::valueByPath()
{
    QFETCH(bool, stringlist);

    static const char objectJson[] =
            "{"
            "    \"_type\": \"Person\","
            "    \"name\": { \"first\":\"Tony\", \"second\":\"Stark\" },"
            "    \"nickname\": \"IronMan\","
            "    \"age\": 42,"
            "    \"friends\": [ \"Hulk\", \"Thor\", { \"name\": \"Black Widow\" }, \"Hawkeye\" ],"
            "    \"score\": 123.456"
            "}";
    JsonDbObject object = QJsonDocument::fromJson(QByteArray::fromRawData(objectJson, sizeof(objectJson))).object();

    const QString Name = QStringLiteral("name");
    const QString NameDotFirst = QStringLiteral("name.first");
    const QString FriendsDotTwo = QStringLiteral("friends.2");
    const QString FriendsDotTwoDotName = QStringLiteral("friends.2.name");

    if (stringlist) {
        const QStringList NameList = Name.split(QLatin1Char('.'));
        const QStringList NameDotFirstList = NameDotFirst.split(QLatin1Char('.'));
        const QStringList FriendsDotTwoList = FriendsDotTwo.split(QLatin1Char('.'));
        const QStringList FriendsDotTwoDotNameList = FriendsDotTwoDotName.split(QLatin1Char('.'));
        QBENCHMARK {
            object.valueByPath(NameList);
            object.valueByPath(NameDotFirstList);
            object.valueByPath(FriendsDotTwoList);
            object.valueByPath(FriendsDotTwoDotNameList);
        }
    } else {
        QBENCHMARK {
            object.valueByPath(Name);
            object.valueByPath(NameDotFirst);
            object.valueByPath(FriendsDotTwo);
            object.valueByPath(FriendsDotTwoDotName);
        }
    }
}

QTEST_MAIN(TestJsonDbObject)

#include "bench_jsondbobject.moc"
