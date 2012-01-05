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

#include "aodb.h"
#include "btree.h"

class TestJsonDbBdb: public QObject
{
    Q_OBJECT
public:
    TestJsonDbBdb();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void create();
    void last();
    void lastMultiPage();
    void firstMultiPage();
    void prev();
    void prev2();
    void rollback();
    void multipleRollbacks();
    void createWithCmp();
    void readAndWrite();
    void variableSizeKeysAndData();
    void transactionTag();
    void compareSequenceOfVarLengthKeys();
    void syncMarker();
    void corruptedPage();
    void tag();
    void btreeRollback();
    void pageChecksum();
    void keySizes();
    void prefixSizes();

private:
    void corruptSinglePage(int psize, int pgno = -1, qint32 flags = -1);
    AoDb *bdb;
};

TestJsonDbBdb::TestJsonDbBdb()
    : bdb(NULL)
{
}

static const char dbname[] = "tst_jsondb_bdb.db";

void TestJsonDbBdb::initTestCase()
{
    QFile::remove(dbname);
    bdb = new AoDb;
    if (!bdb->open(dbname, AoDb::NoSync))
        Q_ASSERT(false);
}

void TestJsonDbBdb::cleanupTestCase()
{
    delete bdb;
    bdb = 0;
    QFile::remove(dbname);
}

void TestJsonDbBdb::create()
{
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    bool ok;
    QByteArray result;

    // write first entry
    ok = bdb->put(key1, value1);
    QVERIFY(ok);

    // read it
    ok = bdb->get(key1, result);
    QVERIFY(ok);
    QCOMPARE(value1, result);

    // read non-existing entry
    ok = bdb->get(key2, result);
    QVERIFY(!ok);

    // write second entry
    ok = bdb->put(key2, value2);
    QVERIFY(ok);

    // read both entries
    ok = bdb->get(key1, result);
    QVERIFY(ok);
    QCOMPARE(value1, result);

    ok = bdb->get(key2, result);
    QVERIFY(ok);
    QCOMPARE(value2, result);
}

void TestJsonDbBdb::last()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    bool ok;
    QByteArray result;

    ok = bdb->clearData();
    QVERIFY(ok);

    // write first entry
    ok = bdb->put(key1, value1);
    QVERIFY(ok);

    // test cursor->last()
    AoDbCursor *cursor = bdb->cursor();
    ok = cursor->last();
    QVERIFY(ok);
    QByteArray outkey1;
    ok = cursor->currentKey(outkey1);
    QVERIFY(ok);
    QCOMPARE(key1, outkey1);
    delete cursor;

    // write second entry
    ok = bdb->put(key2, value2);
    QVERIFY(ok);

    // test cursor->last()
    cursor = bdb->cursor();
    ok = cursor->last();
    QVERIFY(ok);
    QByteArray outkey2;
    ok = cursor->currentKey(outkey2);
    QVERIFY(ok);
    QCOMPARE(key2, outkey2);
    delete cursor;

    // write zeroth entry
    ok = bdb->put(key0, value0);
    QVERIFY(ok);

    // test cursor->last()
    cursor = bdb->cursor();
    ok = cursor->last();
    QVERIFY(ok);
    QByteArray outkey3;
    ok = cursor->currentKey(outkey3);
    QVERIFY(ok);
    QCOMPARE(key2, outkey3);

}

void TestJsonDbBdb::lastMultiPage()
{
    QByteArray value0("baz");

    bool ok;
    QByteArray result;

    ok = bdb->clearData();
    QVERIFY(ok);

    for (int i = 0; i < 1024; i++) {
        // write first entry
        QByteArray baKey(4, 0);
        qToBigEndian(i, (uchar *)baKey.data());
        ok = bdb->begin();
        QVERIFY(ok);
        ok = bdb->put(baKey, value0);
        QVERIFY(ok);
        ok = bdb->commit(0);
        QVERIFY(ok);

        AoDbCursor *cursor = bdb->cursor();
        ok = cursor->last();
        QVERIFY(ok);
        QByteArray outkey1;
        ok = cursor->currentKey(outkey1);
        QVERIFY(ok);
        QCOMPARE(baKey, outkey1);
        while (cursor->prev()) {
            QByteArray outkey2;
            cursor->currentKey(outkey2);
            //qDebug() << outkey1.toHex() << outkey2.toHex();
            QVERIFY(memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) > 0);
            outkey1 = outkey2;
        }
        delete cursor;

    }
}

void TestJsonDbBdb::firstMultiPage()
{
    QByteArray value0("baz");

    bool ok;
    QByteArray result;

    ok = bdb->clearData();
    QVERIFY(ok);

    for (int i = 1024; i > 0; i--) {
        // write first entry
        QByteArray baKey(4, 0);
        qToBigEndian(i, (uchar *)baKey.data());
        ok = bdb->begin();
        QVERIFY(ok);
        ok = bdb->put(baKey, value0);
        QVERIFY(ok);
        ok = bdb->commit(0);
        QVERIFY(ok);

        AoDbCursor *cursor = bdb->cursor();
        ok = cursor->first();
        QVERIFY(ok);
        QByteArray outkey1;
        ok = cursor->currentKey(outkey1);
        QVERIFY(ok);
        QCOMPARE(baKey, outkey1);
        while (cursor->next()) {
            QByteArray outkey2;
            cursor->currentKey(outkey2);
            //qDebug() << outkey1.toHex() << outkey2.toHex();
            QVERIFY(memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) < 0);
            outkey1 = outkey2;
        }
        delete cursor;

    }
}

void TestJsonDbBdb::prev()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    bool ok;
    QByteArray result;

    ok = bdb->clearData();
    QVERIFY(ok);

    // write entries
    ok = bdb->put(key0, value0);
    QVERIFY(ok);
    ok = bdb->put(key1, value1);
    QVERIFY(ok);
    ok = bdb->put(key2, value2);
    QVERIFY(ok);

    // go to end
    AoDbCursor *cursor = bdb->cursor();
    ok = cursor->last();
    QVERIFY(ok);
    // test prev
    ok = cursor->prev();
    QVERIFY(ok);
    QByteArray outkey;
    ok = cursor->currentKey(outkey);
    QVERIFY(ok);
    QCOMPARE(key1, outkey);
    delete cursor;

    cursor = bdb->cursor();
    // test prev without initialization is same as last()
    ok = cursor->prev();
    QVERIFY(ok);
    ok = cursor->currentKey(outkey);
    QVERIFY(ok);
    QCOMPARE(key2, outkey);

    // prev to key1
    ok = cursor->prev();
    QVERIFY(ok);
    ok = cursor->currentKey(outkey);
    QVERIFY(ok);
    QCOMPARE(key1, outkey);

    // prev to key0
    ok = cursor->prev();
    QVERIFY(ok);
    ok = cursor->currentKey(outkey);
    QVERIFY(ok);
    QCOMPARE(key0, outkey);

    // prev to eof
    ok = cursor->prev();
    QVERIFY(!ok);

    delete cursor;
}


void TestJsonDbBdb::prev2()
{
    bool ok = bdb->clearData();
    QVERIFY(ok);

    QFile file(dbname);
    int maxSize = file.size();

    int amount = ::getenv("BENCHMARK_AMOUNT") ? ::atoi(::getenv("BENCHMARK_AMOUNT")) : 40000;
    for (int i = 0; i < amount; ++i) {
        QByteArray data = QUuid::createUuid().toRfc4122();
        bool ok = bdb->put(data, QByteArray("value_")+QByteArray::number(i));
        QVERIFY(ok);
        int size = file.size();
        if (size > maxSize)
            maxSize = size;
        if ((i % 1000) == 999) {
            bdb->compact();
            qDebug() << "   compacted." << file.size();;
        }
    }

    AoDbCursor *c = bdb->cursor();
    QVERIFY(c->first());
    int cnt = 1;
    while (c->next()) ++cnt;
    QCOMPARE(cnt, amount);
    delete c;

    AoDbCursor *r = bdb->cursor();
    QVERIFY(r->last());
    int rcnt = 1;
    while (r->prev()) ++rcnt;

    QCOMPARE(rcnt, amount);
    delete r;
    qDebug() << "maxSize" << maxSize << "amount" << amount;
}

int keyCmp(const char *aptr, size_t asiz, const char *bptr, size_t bsiz, void *)
{
    QString a((QChar *)aptr, asiz/2);
    QString b((QChar *)bptr, bsiz/2);
    qDebug() << "keyCmp" << a << b;
    if (a < b)
        return -1;
    else if (a > b)
        return 1;
    else
        return 0;
}

void TestJsonDbBdb::createWithCmp()
{
    bdb->clearData();
    bdb->setCmpFunc(keyCmp);
    QString str1("1");
    QByteArray key1 = QByteArray::fromRawData((const char *)str1.data(), str1.size()*2);
    QByteArray value1("foo");
    QString str2("2");
    QByteArray key2 = QByteArray::fromRawData((const char *)str2.data(), str2.size()*2);
    QByteArray value2("bar");

    bool ok;
    QByteArray result;

    // write first entry
    ok = bdb->put(key1, value1);
    QVERIFY(ok);

    // read it
    ok = bdb->get(key1, result);
    QVERIFY(ok);
    QCOMPARE(value1, result);

    // read non-existing entry
    ok = bdb->get(key2, result);
    QVERIFY(!ok);

    // write second entry
    ok = bdb->put(key2, value2);
    QVERIFY(ok);

    // read both entries
    ok = bdb->get(key1, result);
    QVERIFY(ok);
    QCOMPARE(value1, result);

    ok = bdb->get(key2, result);
    QVERIFY(ok);
    QCOMPARE(value2, result);
}

void TestJsonDbBdb::rollback()
{
    QByteArray key1("22");
    QByteArray value1("foo");
    QByteArray key2("42");
    QByteArray value2("bar");

    bool ok;
    QByteArray result;

    // write first entry
    ok = bdb->put(key1, value1);
    QVERIFY(ok);

    {
        // start transaction
        ok = bdb->begin();
        QVERIFY(ok);

        // re-write the first entry
        ok = bdb->remove(key1);
        QVERIFY(ok);

        ok = bdb->put(key1, value2);
        QVERIFY(ok);

        // write second entry
        ok = bdb->put(key2, value2);
        QVERIFY(ok);

        // abort the transaction
        ok = bdb->abort();
        QVERIFY(ok);
    }

    // read both entries
    ok = bdb->get(key1, result);
    QVERIFY(ok);
    QCOMPARE(value1, result);

    ok = bdb->get(key2, result);
    QVERIFY(!ok);
}

void TestJsonDbBdb::multipleRollbacks()
{
    QByteArray key1("101");
    QByteArray value1("foo");
    QByteArray key2("102");
    QByteArray value2("bar");

    bool ok;
    QByteArray result;

    {
        ok = bdb->begin();
        QVERIFY(ok);
        // write first entry
        ok = bdb->put(key1, value1);
        QVERIFY(ok);
        ok = bdb->commit(0);
        QVERIFY(ok);
    }

    {
        // start transaction
        ok = bdb->begin();
        QVERIFY(ok);

        // re-write the first entry
        ok = bdb->remove(key1);
        QVERIFY(ok);
        ok = bdb->put(key1, value2);
        QVERIFY(ok);

        // abort the transaction
        ok = bdb->abort();
        QVERIFY(ok);
    }

    {
        // start transaction
        ok = bdb->begin();
        QVERIFY(ok);

        // write second entry
        ok = bdb->put(key2, value2);
        QVERIFY(ok);

        // abort the transaction
        ok = bdb->abort();
        QVERIFY(ok);
    }

    // read both entries
    ok = bdb->get(key1, result);
    QVERIFY(ok);
    QCOMPARE(value1, result);

    ok = bdb->get(key2, result);
    QVERIFY(!ok);
}

void TestJsonDbBdb::readAndWrite()
{
    static const char dbname[] = "readandwrite.db";
    QFile::remove(dbname);

    AoDb wdb;
    bool ok = wdb.open(dbname, AoDb::Default);
    QVERIFY(ok);

    wdb.begin();
    wdb.put(QByteArray("foo"), QByteArray("bar"));
    wdb.put(QByteArray("bla"), QByteArray("bla"));
    wdb.commit(1);

    AoDb rdb1;
    ok = rdb1.open(dbname, AoDb::ReadOnly);
    QVERIFY(ok);

    QByteArray value;
    ok = rdb1.get("foo", value);
    QVERIFY(ok);
    QCOMPARE(value, QByteArray("bar"));
    ok = rdb1.get("bla", value);
    QVERIFY(ok);
    QCOMPARE(value, QByteArray("bla"));

    wdb.begin();
    wdb.put(QByteArray("foo2"), QByteArray("bar2"));
    wdb.put(QByteArray("bar"), QByteArray("baz"));
    // do not commit yet

    rdb1.beginRead();
    ok = rdb1.get("foo2", value);
    QVERIFY(!ok);

    AoDb rdb2;
    ok = rdb2.open(dbname, AoDb::ReadOnly);
    QVERIFY(ok);

    ok = rdb2.get("foo", value);
    QVERIFY(ok);
    ok = rdb2.get("foo2", value);
    QVERIFY(!ok);

    wdb.commit(2);

    ok = rdb2.get("foo", value);
    QVERIFY(ok);
    ok = rdb2.get("foo2", value);
    QVERIFY(ok);
    QCOMPARE(value, QByteArray("bar2"));

    ok = rdb1.get("foo", value);
    QVERIFY(ok);
    ok = rdb1.get("foo2", value);
    QVERIFY(!ok);
    rdb1.abort();
    QFile::remove(dbname);
}


void TestJsonDbBdb::variableSizeKeysAndData()
{
    static const char dbname[] = "variablesize.db";
    QFile::remove(dbname);

    AoDb wdb;
    bool ok = wdb.open(dbname, AoDb::NoSync);
    QVERIFY(ok);

    QByteArray keyPrefix[10] = {
        QByteArray("0001234567890123456789"),
        QByteArray("000123456789"),
        QByteArray("00012345678"),
        QByteArray("0001234567"),
        QByteArray("000123456"),
        QByteArray("00012345"),
        QByteArray("0001234"),
        QByteArray("000123"),
        QByteArray("00012"),
        QByteArray("1")};

    ok = wdb.clearData();
    QVERIFY(ok);

    /* initialize random seed: */
    srand ( 0 ); //QDateTime::currentMSecsSinceEpoch() );

    for (int i = 0; i < 1024; i++) {
        // Create a key with one of the prefixes from above
        // Start by selecting one of the key prefixes
        QByteArray key = keyPrefix[rand()%10];
        int length = rand() % 128 + 1;
        QByteArray keyPostfix(length, ' ');
        for (int j=0; j<length; j++) {
            keyPostfix[j] = quint8(rand()%255);
        }
        key += keyPostfix;

        length = rand() % 1024 + 1;
        // Create a random length value with random bytes
        QByteArray value(length, ' ');
        for (int j=0; j<length; j++) {
            value[j] = quint8(rand()%255);
        }
        ok = wdb.begin();
        QVERIFY(ok);
        ok = wdb.put(key, value);
        QVERIFY(ok);
        ok = wdb.commit(0);
        QVERIFY(ok);
    }

    // Delete every second object
    AoDbCursor *cursor = wdb.cursor();
    ok = cursor->first();
    QVERIFY(ok);
    QByteArray key;
    ok = cursor->currentKey(key);
    QVERIFY(ok);
    bool remove = true;
    int counter = 0;
    while (cursor->next()) {
        counter++;
        cursor->currentKey(key);
        if (remove) {
            remove = false;
            ok = wdb.begin();
            QVERIFY(ok);
            ok = wdb.remove(key);
            QVERIFY(ok);
            ok = wdb.commit(0);
            QVERIFY(ok);
        }
        else remove = true;
    }
    delete cursor;

    wdb.close();
    QFile::remove(dbname);
}

void TestJsonDbBdb::transactionTag()
{
    static const char dbname[] = "readandwrite.db";
    QFile::remove(dbname);

    AoDb wdb;
    bool ok = wdb.open(dbname, AoDb::Default);
    QVERIFY(ok);

    ok = wdb.begin();
    QVERIFY(ok);
    ok = wdb.put(QByteArray("foo"), QByteArray("bar"));
    QVERIFY(ok);
    ok = wdb.put(QByteArray("bla"), QByteArray("bla"));
    QVERIFY(ok);
    ok = wdb.commit(1);
    QVERIFY(ok);
    QCOMPARE(wdb.tag(), quint32(1));

    AoDb rdb;
    ok = rdb.open(dbname, AoDb::ReadOnly);
    QVERIFY(ok);
    QCOMPARE(rdb.tag(), quint32(1));
    rdb.beginRead();
    QCOMPARE(rdb.tag(), quint32(1));

    ok = wdb.begin();
    QVERIFY(ok);
    ok = wdb.put(QByteArray("foo"), QByteArray("bar"));
    QVERIFY(ok);
    ok = wdb.commit(2);
    QVERIFY(ok);
    QCOMPARE(wdb.tag(), quint32(2));

    QCOMPARE(rdb.tag(), quint32(1));
    rdb.abort();
    rdb.beginRead();
    QCOMPARE(rdb.tag(), quint32(2));
    rdb.abort();
    QFile::remove(dbname);
}

int findLongestSequenceOf(const char *a, size_t size, char x)
{
    int result = 0;
    int count = 0;
    for (size_t i = 0; i < size; ++i) {
        if (count > result)
            result = count;

        if (count) {
            if (a[i] == x)
                count++;
            else
                count = 0;
            continue;
        }

        count = a[i] == x ? 1 : 0;
    }

    if (count > result)
        result = count;

    return result;
}

int cmpVarLengthKeys(const char *aptr, size_t asize, const char *bptr, size_t bsize, void *)
{
    int acount = findLongestSequenceOf(aptr, asize, 'a');
    int bcount = findLongestSequenceOf(bptr, bsize, 'a');

    if (acount == bcount) {
        return QString::compare(QString::fromAscii(aptr, asize), QString::fromAscii(bptr, bsize));
    } else {
        return (acount > bcount) ? 1 : ((acount < bcount) ? -1 : 0);
    }
}

bool cmpVarLengthKeysForQVec(const QByteArray &a, const QByteArray &b)
{
    return cmpVarLengthKeys(a.constData(), a.size(), b.constData(), b.size(), 0) < 0;
}

int myRand(int r)
{
    return (int)(((float)qrand() / (float)RAND_MAX) * (float)r);
}

void TestJsonDbBdb::compareSequenceOfVarLengthKeys()
{
    const char sequenceChar = 'a';
    const int numElements = 1000;
    const int minKeyLength = 20;
    const int maxKeyLength = 25;

    // Create vector of variable length keys of sequenceChar
    QVector<QByteArray> vec;
    for (int i = 0; i < numElements; ++i) {
        QByteArray k(minKeyLength + myRand(maxKeyLength - minKeyLength), sequenceChar);

        // Change character at random indexed
        for (int j = 0; j < k.size(); ++j) {
            if (myRand(2) > 0)
                k[j] = 'a' + myRand(26);
        }
        vec.append(k);
    }

    bool ok = bdb->clearData();
    QVERIFY(ok);

    ok = bdb->setCmpFunc(cmpVarLengthKeys);
    QVERIFY(ok);

    for (int i = 0; i < vec.size(); ++i) {
        int count = findLongestSequenceOf(vec[i].constData(), vec[i].size(), sequenceChar);
        QByteArray value((const char*)&count, sizeof(count));
        ok = bdb->put(vec[i], value);
        QVERIFY(ok);
    }

    // Sort QVector to use as verification of bdb sort order
    qSort(vec.begin(), vec.end(), cmpVarLengthKeysForQVec);

    AoDbCursor *cursor = bdb->cursor();
    QVERIFY(cursor != NULL);

    QByteArray key;
    QByteArray value;
    int i = 0;
    while (cursor->next()) {
        ok = cursor->currentKey(key);
        QVERIFY(ok);

        ok = cursor->currentValue(value);
        QVERIFY(ok);

        QCOMPARE(key, vec[i++]);
    }

    delete cursor;
}

void TestJsonDbBdb::syncMarker()
{
    bdb->clearData();

    AoDb db;
    bool ok = db.open(dbname, AoDb::NoSync | AoDb::UseSyncMarker);
    QVERIFY(ok);

    ok = db.begin();
    QVERIFY(ok);
    ok = db.put(QByteArray("foo"), QByteArray("123"));
    QVERIFY(ok);
    ok = db.commit(5);
    QVERIFY(ok);
    db.sync();

    // now commit without explicit sync, i.e.without marker
    ok = db.begin();
    QVERIFY(ok);
    ok = db.put(QByteArray("bar"), QByteArray("456"));
    QVERIFY(ok);
    ok = db.commit(6);
    QVERIFY(ok);

    AoDb db2;
    ok = db2.open(dbname, AoDb::NoSync | AoDb::UseSyncMarker);
    QVERIFY(ok);
    QByteArray value;
    ok = db2.get(QByteArray("foo"), value);
    QVERIFY(ok);
    QCOMPARE(value, QByteArray("123"));
    ok = db2.get(QByteArray("bar"), value);
    QVERIFY(!ok);
}

void TestJsonDbBdb::corruptedPage()
{
    bdb->clearData();

    QVERIFY(bdb->begin());
    QVERIFY(bdb->put(QByteArray("foo"), QByteArray("123")));
    QVERIFY(bdb->commit(42));

    bdb->close();

    QFile file(dbname);
    QVERIFY(file.open(QFile::Append));
    file.write(QByteArray(4096, 8)); // write one page of garbage
    file.close();

    QVERIFY(bdb->open(dbname, AoDb::NoSync));
    QCOMPARE(bdb->tag(), 42u);
    QByteArray value;
    QVERIFY(bdb->get(QByteArray("foo"), value));
    QCOMPARE(value, QByteArray("123"));
}

void TestJsonDbBdb::tag()
{
    bdb->clearData();

    QVERIFY(bdb->begin());
    QVERIFY(bdb->put(QByteArray("foo"), QByteArray("123")));
    QVERIFY(bdb->commit(42));

    struct btree *bt = bdb->handle();
    struct btree_txn *rwtxn = btree_txn_begin(bt, 0);
    struct btval btkey, btvalue;
    memset(&btkey, 0, sizeof(btkey));
    memset(&btvalue, 0, sizeof(btvalue));
    btkey.data = (void *)"foo";
    btkey.size = 4;
    btvalue.data = (void *)"123";
    btvalue.size = 4;
    QCOMPARE(btree_txn_put(bt, rwtxn, &btkey, &btvalue, 0), BT_SUCCESS);
    btval_reset(&btkey);
    btval_reset(&btvalue);
    QCOMPARE(btree_txn_get_tag(rwtxn), 42u);

    struct btree_txn *rotxn = btree_txn_begin(bt, 1);
    QCOMPARE(btree_txn_get_tag(rotxn), 42u);

    QCOMPARE(btree_txn_commit(rwtxn, 64, 0), BT_SUCCESS);
    struct btree_txn *rotxn2 = btree_txn_begin(bt, 1);
    QCOMPARE(btree_txn_get_tag(rotxn), 42u);
    QCOMPARE(btree_txn_get_tag(rotxn2), 64u);
    btree_txn_abort(rotxn);
    btree_txn_abort(rotxn2);
}

void TestJsonDbBdb::btreeRollback()
{
    bdb->clearData();

    QVERIFY(bdb->begin());
    QVERIFY(bdb->put(QByteArray("foo"), QByteArray("123")));
    QVERIFY(bdb->commit(1));

    QVERIFY(bdb->begin());
    QVERIFY(bdb->put(QByteArray("bar"), QByteArray("baz")));
    QVERIFY(bdb->commit(2));

    struct btree *bt = bdb->handle();

    QVERIFY(btree_rollback(bt) == BT_SUCCESS);

    QByteArray value;
    QVERIFY(bdb->get(QByteArray("foo"), value));
    QVERIFY(!bdb->get(QByteArray("bar"), value));
}

void TestJsonDbBdb::corruptSinglePage(int psize, int pgno, qint32 flag)
{
    const int asize = psize / 4;
    quint32 *page = new quint32[asize];
    QFile::OpenMode om = QFile::ReadWrite;

    if (pgno == -1)  // we'll be appending
        om |= QFile::Append;

    if (bdb->handle())
        bdb->close();

    QFile file(dbname);
    QVERIFY(file.open(om));
    QVERIFY(file.seek((pgno == -1 ? 0 : pgno * psize)));
    QVERIFY(file.read((char*)page, psize));

    if (pgno == -1)
        pgno = file.size() / psize; // next pgno
    page[1] = pgno;
    if (flag > 0)
        page[2] = flag; // set page flag if specified

    for (int j = 3; j < asize; ++j) // randomly corrupt page (skip flag and pgno)
        page[j] = rand();

    QVERIFY(file.seek(pgno * psize));
    QCOMPARE(file.write((char*)page, psize), (qint64)psize);
    file.close();

    delete [] page;
}

void TestJsonDbBdb::pageChecksum()
{
    const qint64 psize = bdb->stat()->psize;
    QByteArray value;

    bdb->clearData();

    QVERIFY(bdb->begin());
    QVERIFY(bdb->put(QByteArray("foo1"), QByteArray("bar1")));
    QVERIFY(bdb->commit(1));

    QVERIFY(bdb->begin());
    QVERIFY(bdb->put(QByteArray("foo2"), QByteArray("bar2")));
    QVERIFY(bdb->commit(2));

    QVERIFY(bdb->begin());
    QVERIFY(bdb->put(QByteArray("foo3"), QByteArray("bar3")));
    QVERIFY(bdb->commit(3));

    bdb->close();

    QFile f0(dbname);
    QCOMPARE(f0.size(), psize * 7); // Should have 7 pages in db
    f0.close();

    corruptSinglePage(psize, 6); // corrupt page 6 (the meta with tag 3)

    QFile f1(dbname);
    QCOMPARE(f1.size(), psize * 7); // Should have 7 pages in db
    f1.close();

    corruptSinglePage(psize); // add corrupted page

    QFile f2(dbname);
    QCOMPARE(f2.size(), psize * 8);  // Should have 8 pages in db
    f2.close();

    QVERIFY(bdb->open(dbname, AoDb::NoSync));
    QCOMPARE(bdb->tag(), 2u); // page with tag 3 corrupted, should get tag 2

    QVERIFY(bdb->get(QByteArray("foo1"), value));
    QCOMPARE(value, QByteArray("bar1"));
    QVERIFY(bdb->get(QByteArray("foo2"), value));
    QCOMPARE(value, QByteArray("bar2"));

    QVERIFY(!bdb->get(QByteArray("foo3"), value)); // should not exist

    bdb->close();

    corruptSinglePage(psize, 3); // corrupt page 3 (leaf with key foo2)

    QFile f3(dbname);
    QCOMPARE(f3.size(), psize * 8);  // Should have 9 pages in db
    f3.close();

    QVERIFY(bdb->open(dbname, AoDb::NoSync));
    QVERIFY(!bdb->get(QByteArray("foo1"), value)); // page 3 should be corrupted
    QVERIFY(!bdb->get(QByteArray("foo2"), value)); // page 3 should be corrupted

    // Can revert to tag 1 here, bdb functions not implemented yet though

    bdb->close();
}

void TestJsonDbBdb::keySizes()
{
    const int numlegal = 10;
    const int numillegal = 3;

    QByteArray value;
    QVector<QByteArray> legalkeys;
    QVector<QByteArray> illegalkeys;
    QVector<QByteArray> values;

    bdb->clearData();
    bdb->setCmpFunc(0);

    qDebug() << "Testing with max key size:" << bdb->stat()->ksize;

    for (int i = 0; i < numlegal; ++i) {
        legalkeys.append(QByteArray(bdb->stat()->ksize - i, 'a' + i));
        if (i < numillegal)
            illegalkeys.append(QByteArray(bdb->stat()->ksize + i + 1, 'a' + i));
        values.append(QByteArray(500 + myRand(2000), 'a' + i));
    }

    for (int i = 0; i < numlegal; ++i) {
        QVERIFY(bdb->put(legalkeys[i], values[i]));
    }

    for (int i = 0; i < numillegal; ++i) {
        QVERIFY(!bdb->put(illegalkeys[i], values[i]));
    }

    for (int i = 0; i < legalkeys.size(); ++i) {
        QVERIFY(bdb->get(legalkeys[i], value));
        QCOMPARE(value, values[i]);
    }

    for (int i = 0; i < illegalkeys.size(); ++i) {
        QVERIFY(!bdb->get(illegalkeys[i], value));
    }
}

void TestJsonDbBdb::prefixSizes()
{
    // This test is for when key size of bigger than prefix size.
    // If keysize == 255 (the default btree key size) then we change
    // the key we insert.
    const int count = 100;
    const int pfxsize = 300;
    const int keysize = 10;
    QVector<QByteArray> keys;

    bdb->clearData();

    for (int i = 0; i < count; ++i) {
        QByteArray key(pfxsize + keysize, 'a');
        for (int j = 0; j < keysize; ++j)
            key[pfxsize + j] = '0' + myRand(10);
        if (bdb->stat()->ksize == 255) // chop off if max key size is 255
            key = key.mid(key.size() - 255);
        keys.append(key);
    }

    for (int i = 0; i < keys.size(); ++i)
        QVERIFY(bdb->put(keys[i], QString::number(i).toAscii()));
}

QTEST_MAIN(TestJsonDbBdb)
#include "tst_jsondb_bdb.moc"
