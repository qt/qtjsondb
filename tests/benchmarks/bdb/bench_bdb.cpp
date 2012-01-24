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

#include <QtTest/QtTest>
#include <QtCore/QVector>

#include "aodb.h"
#include "btree.h"


class TestJsonDb: public QObject
{
    Q_OBJECT
public:
    TestJsonDb();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void checksumCalculations_data();
    void checksumCalculations();

    void benchmarkNoSyncs_data();
    void benchmarkNoSyncs();

private:
};

TestJsonDb::TestJsonDb()
{
}

static const char dbname[] = "tst_bench_bdb.db";

void TestJsonDb::initTestCase()
{
    QFile::remove(dbname);

}

void TestJsonDb::cleanupTestCase()
{
    QFile::remove(dbname);
}

void TestJsonDb::checksumCalculations_data()
{
    QTest::addColumn<bool>("checksum");
    QTest::addColumn<bool>("reading");
    QTest::newRow("reading without checksum") << false << true;
    QTest::newRow("reading with checksum") << true << true;
    QTest::newRow("writing without checksum") << false << false;
    QTest::newRow("writing with checksum") << true << false;
}

void TestJsonDb::checksumCalculations()
{
    QFETCH(bool, checksum);
    QFETCH(bool, reading);

    const int count = 100;
    QVector<QByteArray> keys, values;
    QByteArray value;
    AoDb db;
    AoDb::DbFlags flags = AoDb::NoSync | AoDb::UseSyncMarker;

    if (!checksum)
        flags |= AoDb::NoPageChecksums;

    QFile::remove(dbname);

    bool ok = db.open(dbname, flags);
    QVERIFY(ok);

    for (int i = 0; i < count; ++i) {
        int k = i;
        int v = k + count;
        keys.append(QByteArray((char*)&k, sizeof(int)));
        values.append(QByteArray((char*)&v, sizeof(int)));
        if (reading)
            QVERIFY(db.put(keys[i], values[i]));
    }

    QBENCHMARK {
        for (int i = 0; i < count; ++i) {
            if (reading)
                QVERIFY(db.get(keys[i], value));
            else
                QVERIFY(db.put(keys[i], values[i]));
        }
    }

    db.close();
    QVERIFY(QFile::remove(dbname));
}

void TestJsonDb::benchmarkNoSyncs_data()
{
    QTest::addColumn<bool>("sync");
    QTest::newRow("without syncing") << false;
    QTest::newRow("with syncing") << true;
}

void TestJsonDb::benchmarkNoSyncs()
{
    QFETCH(bool, sync);
    AoDb db;
    AoDb::DbFlags flags = AoDb::UseSyncMarker;
    if (!sync)
        flags |= AoDb::NoSync;

    const int count = 10;
    QByteArray value;

    QFile::remove(dbname);

    bool ok = db.open(dbname, flags);
    QVERIFY(ok);

    QBENCHMARK {
        for (int i = 0; i < count; ++i) {
            QByteArray key((char*)&i, sizeof(int));
            QVERIFY(db.put(key, key));
            QVERIFY(db.get(key, value));
        }
    }

    db.close();
    QFile::remove(dbname);
}


QTEST_MAIN(TestJsonDb)
#include "bench_bdb.moc"
