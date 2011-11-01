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

#include "qbtree.h"
#include "qbtreelocker.h"

class TestQBtree: public QObject
{
    Q_OBJECT
public:
    TestQBtree();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

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
    void readFromTag();
    void btreeRollback();
    void lockers();

private:
    QBtree *db;
};

TestQBtree::TestQBtree()
    : db(NULL)
{
}

static const char dbname[] = "tst_qbtree.db";

void TestQBtree::initTestCase()
{
}

void TestQBtree::cleanupTestCase()
{
}

void TestQBtree::init()
{
    QFile::remove(dbname);
    db = new QBtree;
    if (!db->open(dbname, QBtree::NoSync))
        Q_ASSERT(false);
    db->setAutoCompactRate(1000);
}

void TestQBtree::cleanup()
{
    delete db;
    db = 0;
    QFile::remove(dbname);
}

void TestQBtree::create()
{
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    QByteArray result;

    QBtreeTxn *txn = db->begin();
    QVERIFY(txn);
    // write first entry
    QVERIFY(txn->put(key1, value1));

    // read it
    QVERIFY(txn->get(key1, &result));
    QCOMPARE(value1, result);

    // read non-existing entry
    QVERIFY(!txn->get(key2, &result));

    // write second entry
    QVERIFY(txn->put(key2, value2));

    // read both entries
    QVERIFY(txn->get(key1, &result));
    QCOMPARE(value1, result);

    QVERIFY(txn->get(key2, &result));
    QCOMPARE(value2, result);

    txn->commit(42);
}

void TestQBtree::last()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    QBtreeTxn *txn = db->begin();
    // write first entry
    QVERIFY(txn->put(key1, value1));

    // test cursor->last()
    {
        QBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        QByteArray outkey1;
        QVERIFY(cursor.current(&outkey1, 0));
        QCOMPARE(key1, outkey1);
    }

    // write second entry
    QVERIFY(txn->put(key2, value2));

    // test cursor->last()
    {
        QBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        QByteArray outkey2;
        QVERIFY(cursor.current(&outkey2, 0));
        QCOMPARE(key2, outkey2);
    }

    // write zeroth entry
    QVERIFY(txn->put(key0, value0));

    // test cursor->last()
    {
        QBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        QByteArray outkey3;
        QVERIFY(cursor.current(&outkey3, 0));
        QCOMPARE(key2, outkey3);
    }

    txn->commit(42);
}

void TestQBtree::lastMultiPage()
{
    QByteArray value0("baz");

    for (int i = 0; i < 1024; i++) {
        // write first entry
        QByteArray baKey(4, 0);
        qToBigEndian(i, (uchar *)baKey.data());
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);
        QVERIFY(txn->put(baKey, value0));
        QVERIFY(txn->commit(0));

        txn = db->begin(QBtree::TxnReadOnly);
        QVERIFY(txn);
        QBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        QByteArray outkey1;
        QVERIFY(cursor.current(&outkey1, 0));
        QCOMPARE(baKey, outkey1);
        while (cursor.prev()) {
            QByteArray outkey2;
            cursor.current(&outkey2, 0);
            //qDebug() << outkey1.toHex() << outkey2.toHex();
            QVERIFY(memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) > 0);
            outkey1 = outkey2;
        }
        txn->abort();
    }
}

void TestQBtree::firstMultiPage()
{
    QByteArray value0("baz");

    for (int i = 1024; i > 0; i--) {
        // write first entry
        QByteArray baKey(4, 0);
        qToBigEndian(i, (uchar *)baKey.data());
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);
        QVERIFY(txn->put(baKey, value0));
        QVERIFY(txn->commit(0));

        txn = db->begin(QBtree::TxnReadOnly);
        QVERIFY(txn);
        QBtreeCursor cursor(txn);
        QVERIFY(cursor.first());
        QByteArray outkey1;
        QVERIFY(cursor.current(&outkey1, 0));
        QCOMPARE(baKey, outkey1);
        while (cursor.next()) {
            QByteArray outkey2;
            cursor.current(&outkey2, 0);
            //qDebug() << outkey1.toHex() << outkey2.toHex();
            QVERIFY(memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) < 0);
            outkey1 = outkey2;
        }
        txn->abort();
    }
}

void TestQBtree::prev()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    QBtreeTxn *txn = db->begin();
    // write entries
    QVERIFY(txn->put(key0, value0));
    QVERIFY(txn->put(key1, value1));
    QVERIFY(txn->put(key2, value2));

    // go to end
    {
        QBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        // test prev
        QVERIFY(cursor.prev());
        QByteArray outkey;
        QVERIFY(cursor.current(&outkey, 0));
        QCOMPARE(key1, outkey);
    }

    {
        QBtreeCursor cursor(txn);
        // test prev without initialization is same as last()
        QVERIFY(cursor.prev());
        QByteArray outkey;
        QVERIFY(cursor.current(&outkey, 0));
        QCOMPARE(key2, outkey);

        // prev to key1
        QVERIFY(cursor.prev());
        QVERIFY(cursor.current(&outkey, 0));
        QCOMPARE(key1, outkey);

        // prev to key0
        QVERIFY(cursor.prev());
        QVERIFY(cursor.current(&outkey, 0));
        QCOMPARE(key0, outkey);

        // prev to eof
        QVERIFY(!cursor.prev());
    }
    txn->abort();
}

void TestQBtree::prev2()
{
    QFile file(dbname);
    int maxSize = file.size();

    int amount = ::getenv("BENCHMARK_AMOUNT") ? ::atoi(::getenv("BENCHMARK_AMOUNT")) : 40000;
    for (int i = 0; i < amount; ++i) {
        QByteArray data = QUuid::createUuid().toRfc4122();
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);
        QVERIFY(txn->put(data, QByteArray("value_")+QByteArray::number(i)));
        txn->commit(0);
        int size = file.size();
        if (size > maxSize)
            maxSize = size;
    }

    QBtreeTxn *txn = db->begin(QBtree::TxnReadOnly);
    QVERIFY(txn);
    QBtreeCursor c(txn);
    QVERIFY(c.first());
    int cnt = 1;
    while (c.next()) ++cnt;
    QCOMPARE(cnt, amount);

    QBtreeCursor r(txn);
    QVERIFY(r.last());
    int rcnt = 1;
    while (r.prev()) ++rcnt;

    QCOMPARE(rcnt, amount);
    txn->abort();
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

void TestQBtree::createWithCmp()
{
    db->setCmpFunc(keyCmp);
    QString str1("1");
    QByteArray key1 = QByteArray::fromRawData((const char *)str1.data(), str1.size()*2);
    QByteArray value1("foo");
    QString str2("2");
    QByteArray key2 = QByteArray::fromRawData((const char *)str2.data(), str2.size()*2);
    QByteArray value2("bar");

    QByteArray result;

    QBtreeTxn *txn = db->begin();
    QVERIFY(txn);

    // write first entry
    QVERIFY(txn->put(key1, value1));

    // read it
    QVERIFY(txn->get(key1, &result));
    QCOMPARE(value1, result);

    // read non-existing entry
    QVERIFY(!txn->get(key2, &result));

    // write second entry
    QVERIFY(txn->put(key2, value2));

    // read both entries
    QVERIFY(txn->get(key1, &result));
    QCOMPARE(value1, result);

    QVERIFY(txn->get(key2, &result));
    QCOMPARE(value2, result);

    txn->abort();
}

void TestQBtree::rollback()
{
    QByteArray key1("22");
    QByteArray value1("foo");
    QByteArray key2("42");
    QByteArray value2("bar");

    QByteArray result;

    QBtreeTxn *txn = db->begin();
    QVERIFY(txn);
    // write first entry
    QVERIFY(txn->put(key1, value1));
    txn->commit(42);

    {
        // start transaction
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);

        // re-write the first entry
        QVERIFY(txn->remove(key1));

        QVERIFY(txn->put(key1, value2));

        // write second entry
        QVERIFY(txn->put(key2, value2));

        // abort the transaction
        QVERIFY(txn->abort());
    }

    txn = db->begin(QBtree::TxnReadOnly);
    QVERIFY(txn);

    // read both entries
    QVERIFY(txn->get(key1, &result));
    QCOMPARE(value1, result);

    QVERIFY(!txn->get(key2, &result));

    txn->abort();
}

void TestQBtree::multipleRollbacks()
{
    QByteArray key1("101");
    QByteArray value1("foo");
    QByteArray key2("102");
    QByteArray value2("bar");

    QByteArray result;

    {
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);
        // write first entry
        QVERIFY(txn->put(key1, value1));
        QVERIFY(txn->commit(0));
    }

    {
        // start transaction
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);

        // re-write the first entry
        QVERIFY(txn->remove(key1));
        QVERIFY(txn->put(key1, value2));

        // abort the transaction
        QVERIFY(txn->abort());
    }

    {
        // start transaction
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);

        // write second entry
        QVERIFY(txn->put(key2, value2));

        // abort the transaction
        QVERIFY(txn->abort());
    }

    QBtreeTxn *txn = db->begin();

    // read both entries
    QVERIFY(txn->get(key1, &result));
    QCOMPARE(value1, result);

    QVERIFY(!txn->get(key2, &result));
    txn->abort();
}

void TestQBtree::readAndWrite()
{
    QBtree &wdb = *db;

    QBtreeTxn *wdbtxn = wdb.begin();
    QVERIFY(wdbtxn);
    QVERIFY(wdbtxn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(wdbtxn->put(QByteArray("bla"), QByteArray("bla")));
    QVERIFY(wdbtxn->commit(1));

    QBtree rdb1;
    QVERIFY(rdb1.open(dbname, QBtree::ReadOnly));

    QBtreeTxn *rdb1txn = rdb1.begin(QBtree::TxnReadOnly);
    QByteArray value;
    QVERIFY(rdb1txn->get("foo", &value));
    QCOMPARE(value, QByteArray("bar"));
    QVERIFY(rdb1txn->get("bla", &value));
    QCOMPARE(value, QByteArray("bla"));
    rdb1txn->abort();

    wdbtxn = wdb.begin();
    wdbtxn->put(QByteArray("foo2"), QByteArray("bar2"));
    wdbtxn->put(QByteArray("bar"), QByteArray("baz"));
    // do not commit yet

    rdb1txn = rdb1.begin(QBtree::TxnReadOnly);
    QVERIFY(rdb1txn);
    QVERIFY(!rdb1txn->get("foo2", &value));

    QBtree rdb2;
    QVERIFY(rdb2.open(dbname, QBtree::ReadOnly));

    QBtreeTxn *rdb2txn = rdb2.begin(QBtree::TxnReadOnly);
    QVERIFY(rdb2txn);
    QVERIFY(rdb2txn->get("foo", &value));
    QVERIFY(!rdb2txn->get("foo2", &value));

    QVERIFY(wdbtxn->commit(2));

    QVERIFY(rdb2txn->get("foo", &value));
    QVERIFY(!rdb2txn->get("foo2", &value));

    rdb1txn->abort();
    rdb1txn = rdb1.begin(QBtree::TxnReadOnly);
    QVERIFY(rdb1txn);
    QVERIFY(rdb1txn->get("foo", &value));
    QVERIFY(rdb1txn->get("foo2", &value));
    QCOMPARE(value, QByteArray("bar2"));
    rdb1txn->abort();
    rdb2txn->abort();
}


void TestQBtree::variableSizeKeysAndData()
{
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
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);
        QVERIFY(txn->put(key, value));
        QVERIFY(txn->commit(0));
    }

    QBtreeTxn *txn = db->begin(QBtree::TxnReadOnly);
    // Delete every second object
    QBtreeCursor cursor(txn);
    QVERIFY(cursor.first());
    QByteArray key;
    QVERIFY(cursor.current(&key, 0));
    bool remove = true;
    int counter = 0;
    while (cursor.next()) {
        counter++;
        cursor.current(&key, 0);
        if (remove) {
            remove = false;
            QBtreeTxn *wtxn = db->begin();
            QVERIFY(wtxn);
            QVERIFY(wtxn->remove(key));
            QVERIFY(wtxn->commit(0));
        }
        else remove = true;
    }
    txn->abort();
}

void TestQBtree::transactionTag()
{
    QBtreeTxn *txn = db->begin();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(txn->put(QByteArray("bla"), QByteArray("bla")));
    QVERIFY(txn->commit(1));
    QCOMPARE(db->tag(), quint32(1));

    QBtree rdb;
    QVERIFY(rdb.open(dbname, QBtree::ReadOnly));
    QCOMPARE(rdb.tag(), quint32(1));
    QBtreeTxn *rdbtxn = rdb.begin(QBtree::TxnReadOnly);
    QCOMPARE(rdb.tag(), quint32(1));
    QCOMPARE(rdbtxn->tag(), quint32(1));

    txn = db->begin();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(txn->commit(2));
    QCOMPARE(db->tag(), quint32(2));

    QCOMPARE(rdb.tag(), quint32(1));
    rdbtxn->abort();

    rdbtxn = rdb.begin(QBtree::TxnReadOnly);
    QCOMPARE(rdbtxn->tag(), quint32(2));
    rdbtxn->abort();
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

void TestQBtree::compareSequenceOfVarLengthKeys()
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

    QVERIFY(db->setCmpFunc(cmpVarLengthKeys));

    for (int i = 0; i < vec.size(); ++i) {
        int count = findLongestSequenceOf(vec[i].constData(), vec[i].size(), sequenceChar);
        QByteArray value((const char*)&count, sizeof(count));
        QBtreeTxn *txn = db->begin();
        QVERIFY(txn);
        QVERIFY(txn->put(vec[i], value));
        QVERIFY(txn->commit(i));
    }

    // Sort QVector to use as verification of bdb sort order
    qSort(vec.begin(), vec.end(), cmpVarLengthKeysForQVec);

    QBtreeTxn *txn = db->begin(QBtree::TxnReadOnly);
    QVERIFY(txn);
    QBtreeCursor cursor(txn);

    QByteArray key;
    QByteArray value;
    int i = 0;
    while (cursor.next()) {
        QVERIFY(cursor.current(&key, 0));
        QVERIFY(cursor.current(0, &value));
        QCOMPARE(key, vec[i++]);
    }
    txn->abort();
}

void TestQBtree::syncMarker()
{
    db->close();
    QVERIFY(db->open(dbname, QBtree::NoSync | QBtree::UseSyncMarker));

    QBtreeTxn *txn = db->begin();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("123")));
    QVERIFY(txn->commit(5));
    db->sync();

    // now commit without explicit sync, i.e.without marker
    txn = db->begin();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("bar"), QByteArray("456")));
    QVERIFY(txn->commit(6));

    QBtree db2;
    QVERIFY(db2.open(dbname, QBtree::NoSync | QBtree::UseSyncMarker));
    QByteArray value;
    txn = db2.begin(QBtree::TxnReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->get(QByteArray("foo"), &value));
    QCOMPARE(value, QByteArray("123"));
    QVERIFY(!txn->get(QByteArray("bar"), &value));
    txn->abort();
}

void TestQBtree::corruptedPage()
{
    QBtreeTxn *txn = db->begin();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("123")));
    QVERIFY(txn->commit(42));

    db->close();

    QFile file(dbname);
    QVERIFY(file.open(QFile::Append));
    file.write(QByteArray(4096, 8)); // write one page of garbage
    file.close();

    QVERIFY(db->open(dbname, QBtree::NoSync));
    QCOMPARE(db->tag(), 42u);
    txn = db->begin();
    QVERIFY(txn);
    QCOMPARE(txn->tag(), 42u);
    QByteArray value;
    QVERIFY(txn->get(QByteArray("foo"), &value));
    QCOMPARE(value, QByteArray("123"));
    txn->abort();
}

void TestQBtree::tag()
{
    QBtreeTxn *txn = db->begin();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("123")));
    QVERIFY(txn->commit(42));

    txn = db->begin();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("123")));
    QCOMPARE(db->tag(), 42u);
    // do not commit just yet

    QBtreeTxn *rtxn = db->begin(QBtree::TxnReadOnly);
    QVERIFY(rtxn);
    QCOMPARE(rtxn->tag(), 42u);

    QVERIFY(txn->commit(64));
    QCOMPARE(db->tag(), 64u);
    QCOMPARE(rtxn->tag(), 42u);
    rtxn->abort();
    rtxn = db->begin(QBtree::TxnReadOnly);
    QVERIFY(rtxn);
    QCOMPARE(rtxn->tag(), 64u);
    rtxn->abort();
}

void TestQBtree::readFromTag()
{
    QBtreeTxn *txn = db->beginReadWrite();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(txn->commit(1));

    txn = db->beginReadWrite();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("bla"), QByteArray("bla")));
    QVERIFY(txn->put(QByteArray("zzz"), QByteArray("zzz")));
    QVERIFY(txn->commit(2));

    txn = db->beginReadWrite();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("zzz")));
    QVERIFY(txn->remove(QByteArray("zzz")));
    QVERIFY(txn->commit(3));

    QByteArray value;

    txn = db->beginRead();
    QVERIFY(txn);
    QCOMPARE(txn->tag(), quint32(3));
    QVERIFY(!txn->get(QByteArray("zzz"), &value));
    QVERIFY(txn->get(QByteArray("foo"), &value));
    QCOMPARE(value, QByteArray("zzz"));
    QVERIFY(txn->get(QByteArray("bla"), &value));
    QCOMPARE(value, QByteArray("bla"));
    txn->abort();

    txn = db->beginRead(2);
    QVERIFY(txn);
    QCOMPARE(txn->tag(), quint32(2));
    QVERIFY(txn->get(QByteArray("zzz"), &value));
    QVERIFY(txn->get(QByteArray("foo"), &value));
    QCOMPARE(value, QByteArray("bar"));
    txn->abort();

    txn = db->beginRead(1);
    QVERIFY(txn);
    QCOMPARE(txn->tag(), quint32(1));
    QVERIFY(!txn->get(QByteArray("zzz"), &value));
    QVERIFY(!txn->get(QByteArray("bla"), &value));
    QVERIFY(txn->get(QByteArray("foo"), &value));
    QCOMPARE(value, QByteArray("bar"));
    txn->abort();

    QVERIFY(!db->beginRead(4));
    QVERIFY(!db->beginRead(-1u));
}

void TestQBtree::btreeRollback()
{
    QBtreeTxn *txn = db->beginReadWrite();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(txn->commit(1));

    txn = db->beginReadWrite();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("bar"), QByteArray("baz")));
    QVERIFY(txn->commit(2));

    QCOMPARE(db->tag(), 2u);
    QVERIFY(db->rollback());
    QCOMPARE(db->tag(), 1u);

    txn = db->beginRead();
    QVERIFY(txn);
    QCOMPARE(txn->tag(), 1u);
    QByteArray value;
    QVERIFY(txn->get(QByteArray("foo"), &value));
    QVERIFY(!txn->get(QByteArray("bar"), &value));
    txn->abort();
}

void TestQBtree::lockers()
{
    QBtreeTxn *txn = db->beginReadWrite();
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(txn->commit(1));

    {
        QBtreeReadLocker r1(db);
        QVERIFY(r1.isValid());
        QCOMPARE(r1.tag(), 1u);

        {
            QBtreeWriteLocker w(db);
            QVERIFY(w.isValid());
            w.setAutoCommitTag(42u);
            QVERIFY(w.put(QByteArray("bar"), QByteArray("baz")));
        }

        QBtreeReadLocker r2(db);
        QVERIFY(r2.isValid());
        QCOMPARE(r2.tag(), 42u);

        QByteArray result;
        QVERIFY(!r1.get(QByteArray("bar"), &result));
        QVERIFY(r2.get(QByteArray("bar"), &result));
    }
}

QTEST_MAIN(TestQBtree)
#include "main.moc"
