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


#include "hbtree.h"
#include "hbtreetransaction.h"
#include "hbtreecursor.h"
#include "hbtree_p.h"
#include "orderedlist_p.h"

class TestHBtree: public QObject
{
    Q_OBJECT
public:
    TestHBtree();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void orderedList();

    void openClose();
    void reopen();
    void reopenMultiple();
    void create();
    void addSomeSingleNodes();
    void splitLeafOnce_1K();
    void splitManyLeafs_1K();
    void testOverflow_3K();
    void testMultiOverflow_20K();
    void addDeleteNodes_100Bytes();
    void last();
    void first();
    void insertRandom_200BytesTo1kValues();
    void insertHugeData_10Mb();
    void splitBranch_1kData();
    void splitBranchWithOverflows();
    void cursorExactMatch();
    void cursorFuzzyMatch();
    void cursorNext();
    void cursorPrev();
    void lastMultiPage();
    void firstMultiPage();
    void prev();
    void prev2();
    void multiBranchSplits();
    void rollback();
    void multipleRollbacks();
    void createWithCmp();
    void variableSizeKeysAndData();
    void compareSequenceOfVarLengthKeys();
    void asciiAsSortedNumbers();
    void deleteReinsertVerify_data();
    void deleteReinsertVerify();
    void rebalanceEmptyTree();
    void reinsertion();
    void nodeComparisons();
    void tag();
    void cursors();
    void markerOnReopen_data();
    void markerOnReopen();
    void corruptSyncMarker1_data();
    void corruptSyncMarker1();
    void corruptBothSyncMarkers_data();
    void corruptBothSyncMarkers();
    void cursorWhileDelete_data();
    void cursorWhileDelete();
    void getDataFromLastSync();
    void deleteAlotNoSyncReopen_data();
    void deleteAlotNoSyncReopen();

private:
    void corruptSinglePage(int psize, int pgno = -1, qint32 type = -1);
    void set_data();
    HBtree *db;
    HBtreePrivate *d;

    bool printOutCollectibles_;
};

const char * sizeStr(size_t sz)
{
    static char buffer[256];
    const size_t kb = 1024;
    const size_t mb = kb * kb;
    if (sz > mb) {
        sprintf(buffer, "%.2f mb", (float)sz / mb);
    } else if (sz > kb) {
        sprintf(buffer, "%.2f kb", (float)sz / kb);
    } else {
        sprintf(buffer, "%zu bytes", sz);
    }
    return buffer;
}

int myRand(int min, int max)
{
    float multiplier = (float)qrand() / (float)RAND_MAX;
    return (int)(multiplier * (float)(max - min)) + min;
}

int myRand(int r)
{
    return (int)(((float)qrand() / (float)RAND_MAX) * (float)r);
}

static const char dbname[] = "tst_HBtree.db";

void TestHBtree::corruptSinglePage(int psize, int pgno, qint32 type)
{
    const int asize = psize / 4;
    quint32 *page = new quint32[asize];
    QFile::OpenMode om = QFile::ReadWrite;

    if (pgno == -1)  // we'll be appending
        om |= QFile::Append;
    QFile file(dbname);
    QVERIFY(file.open(om));
    QVERIFY(file.seek((pgno == -1 ? 0 : pgno * psize)));
    QVERIFY(file.read((char*)page, asize));

    if (pgno == -1)
        pgno = file.size() / psize; // next pgno
    page[2] = pgno;
    if (type > 0)
        page[1] = type; // set page flag if specified

    for (int j = 3; j < asize; ++j) // randomly corrupt page (skip type and pgno)
        page[j] = rand();

    QVERIFY(file.seek(pgno * psize));
    QCOMPARE(file.write((char*)page, psize), (qint64)psize);
    file.close();

    delete [] page;
}

void TestHBtree::set_data()
{
    const int lessItems = 100;
    const int smallKeys = 100;
    const int smallData = 100;
    const int moreItems = 1000;
    const int bigKeys = 1000;
    const int bigData = 1000;
    const int overflowData = 2000;
    const int multiOverflowData = 5000;
    const int syncRate = 100;

    QTest::addColumn<int>("numItems");
    QTest::addColumn<int>("keySize");
    QTest::addColumn<int>("valueSize");
    QTest::addColumn<int>("syncRate");

    QTest::newRow("Less items, small keys, small data") << lessItems << smallKeys << smallData << syncRate;
    QTest::newRow("more items, small keys, small data") << moreItems << smallKeys << smallData << syncRate;
    QTest::newRow("Less items, big keys, small data") << lessItems << bigKeys << smallData << syncRate;
    QTest::newRow("more items, big keys, small data") << moreItems << bigKeys << smallData << syncRate;
    QTest::newRow("Less items, small keys, big data") << lessItems << smallKeys << bigData << syncRate;
    QTest::newRow("more items, small keys, big data") << moreItems << smallKeys << bigData << syncRate;
    QTest::newRow("Less items, big keys, big data") << lessItems << bigKeys << bigData << syncRate;
    QTest::newRow("more items, big keys, big data") << moreItems << bigKeys << bigData << syncRate;

    QTest::newRow("Less items, small keys, overflow data") << lessItems << smallKeys << overflowData << syncRate;
    QTest::newRow("more items, small keys, overflow data") << moreItems << smallKeys << overflowData << syncRate;
    QTest::newRow("Less items, big keys, overflow data") << lessItems << bigKeys << overflowData << syncRate;
    QTest::newRow("more items, big keys, overflow data") << moreItems << bigKeys << overflowData << syncRate;

    QTest::newRow("Less items, small keys, multi-overflow data") << lessItems << smallKeys << multiOverflowData << syncRate;
    QTest::newRow("more items, small keys, multi-overflow data") << moreItems << smallKeys << multiOverflowData << syncRate;
    QTest::newRow("Less items, big keys, multi-overflow data") << lessItems << bigKeys << multiOverflowData << syncRate;
    QTest::newRow("more items, big keys, multi-overflow data") << moreItems << bigKeys << multiOverflowData << syncRate;
}

TestHBtree::TestHBtree()
    : db(NULL), printOutCollectibles_(true)
{
}

void TestHBtree::initTestCase()
{
}

void TestHBtree::cleanupTestCase()
{
}

void TestHBtree::init()
{
    QFile::remove(dbname);
    db = new HBtree(dbname);
    db->setAutoSyncRate(100);
    if (!db->open(HBtree::ReadWrite))
        Q_ASSERT(false);
    d = db->d_func();
    printOutCollectibles_ = true;
}

void TestHBtree::cleanup()
{
    qDebug() << "Size:" << sizeStr(db->size()) << " Pages:" << db->size() / d->spec_.pageSize;
    qDebug() << "Stats:" << db->stats();
    if (printOutCollectibles_)
        qDebug() << "Collectible pages: " << d->collectiblePages_;
    qDebug() << "Cache size:" << d->cache_.size();

    delete db;
    db = 0;
    QFile::remove(dbname);
}

void TestHBtree::orderedList()
{
    OrderedList<HBtreePrivate::NodeKey, HBtreePrivate::NodeValue> list;

    typedef HBtreePrivate::NodeKey Key;
    typedef HBtreePrivate::NodeValue Value;

    Key key;

    key = Key(0, QByteArray("B"));
    list.insert(key, Value("_B_"));

    key = Key(0, QByteArray("A"));
    list.insert(key, Value("_A_"));

    key = Key(0, QByteArray("C"));
    list.insert(key, Value("_C_"));

    QCOMPARE(list.size(), 3);
    QVERIFY((list.constBegin() + 0).key().data == QByteArray("A"));
    QVERIFY((list.constBegin() + 1).key().data == QByteArray("B"));
    QVERIFY((list.constBegin() + 2).key().data == QByteArray("C"));

    QVERIFY(list.contains(Key(0, "A")));
    QVERIFY(!list.contains(Key(0, "AA")));
    QVERIFY(!list.contains(Key(0, "D")));

    QVERIFY(list.lowerBound(Key(0, "A")) == list.constBegin());
    QVERIFY(list.lowerBound(Key(0, "AA")) == list.constBegin()+1);
    QVERIFY(list.lowerBound(Key(0, "B")) == list.constBegin()+1);
    QVERIFY(list.lowerBound(Key(0, "D")) == list.constEnd());

    QVERIFY(list.upperBound(Key(0, "A")) == list.constBegin()+1);
    QVERIFY(list.upperBound(Key(0, "AA")) == list.constBegin()+1);
    QVERIFY(list.upperBound(Key(0, "B")) == list.constBegin()+2);
    QVERIFY(list.upperBound(Key(0, "D")) == list.constEnd());

    QCOMPARE(list.size(), 3);
    QVERIFY(list[Key(0, "C")].data == QByteArray("_C_"));
    QCOMPARE(list.size(), 3);
    list[Key(0, "C")].data = QByteArray("_C2_");
    QVERIFY(list[Key(0, "C")].data == QByteArray("_C2_"));
    QCOMPARE(list.size(), 3);
    QVERIFY(list[Key(0, "AA")].data.isEmpty());
    QVERIFY(list.contains(Key(0, "AA")));
    QCOMPARE(list.size(), 4);

    QVERIFY(list.find(Key(0, "D")) == list.constEnd());

    QCOMPARE(list.size(), 4);
    list.insert(Key(0, "B"), Value("_B2_"));
    QCOMPARE(list.size(), 4);

    QCOMPARE(list.value(Key(0, "A")).data, QByteArray("_A_"));
    QCOMPARE(list.value(Key(0, "B")).data, QByteArray("_B2_"));
    QCOMPARE(list.value(Key(0, "C")).data, QByteArray("_C2_"));
    QCOMPARE(list.value(Key(0, "AA")).data, QByteArray(""));
}

void TestHBtree::openClose()
{
    // init/cleanup does open and close;
    return;
}

void TestHBtree::reopen()
{
    db->close();
    QVERIFY(db->open());
    QCOMPARE(db->size(), (size_t)d->spec_.pageSize * 3);
}

void TestHBtree::reopenMultiple()
{
    const int numItems = 32;
    const int keySize = 64;
    const int valueSize = 64;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    const int retries = 5;

    for (int i = 0; i < retries; ++i) {
        db->close();
        db->open();
        QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
        while (it != keyValues.end()) {
            HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
            QVERIFY(transaction);
            QByteArray result = transaction->get(it.key());
            QCOMPARE(result, it.value());
            transaction->abort();
            ++it;
        }
    }
}

void TestHBtree::create()
{
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");
    QByteArray key3("3");
    QByteArray value3("baz");

    QByteArray result;

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->isReadWrite());

    // write first entry
    QVERIFY(txn->put(key1, value1));

    // read it
    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    // read non-existing entry
    result = txn->get(key2);
    QVERIFY(result.isEmpty());

    // write second entry
    QVERIFY(txn->put(key2, value2));

    // read both entries
    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value2, result);

    QVERIFY(txn->commit(42));
    QVERIFY(db->sync());

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->isReadOnly());

    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value2, result);

    txn->abort();

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->isReadWrite());

    QVERIFY(txn->put(key3, value3));

    // read all
    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value2, result);

    result = txn->get(key3);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value3, result);

    QVERIFY(txn->commit(32));
    QVERIFY(db->sync());

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->isReadOnly());

    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value2, result);

    result = txn->get(key3);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value3, result);

    txn->abort();

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->isReadWrite());

    QVERIFY(txn->remove(key2));

    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(result.isEmpty());

    result = txn->get(key3);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value3, result);

    txn->commit(22);
    QVERIFY(db->sync());

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->isReadOnly());

    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(result.isEmpty());

    result = txn->get(key3);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value3, result);

    txn->abort();
}

void TestHBtree::addSomeSingleNodes()
{
    const int numItems = 50;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QString::number(i).toAscii()));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QString::number(i).toAscii());
        transaction->abort();
    }
}

void TestHBtree::splitLeafOnce_1K()
{
    const int numBytes = 1000;
    const int numItems = 4;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::splitManyLeafs_1K()
{
    const int numBytes = 1000;
    const int numItems = 255;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::testOverflow_3K()
{
    const int numBytes = 3000;
    const int numItems = 10;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::testMultiOverflow_20K()
{
    const int numBytes = 20000; // must cause multiple overflow pages to be created
    const int numItems = 100;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::addDeleteNodes_100Bytes()
{
    const int numBytes = 200;
    const int numItems = 100;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
        QCOMPARE(i + 1, db->stats().numEntries);
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->remove(QString::number(i).toAscii()));
        transaction->commit(i);
        QCOMPARE(numItems - i - 1, db->stats().numEntries);
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray());
        transaction->abort();
    }
}
void TestHBtree::last()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
    // write first entry
    QVERIFY(transaction->put(key1, value1));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.last());
        QByteArray outkey1;
        cursor.current(&outkey1, 0);
        QCOMPARE(key1, outkey1);
    }

    // write second entry
    QVERIFY(transaction->put(key2, value2));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.last());
        QByteArray outkey2;
        cursor.current(&outkey2, 0);
        QCOMPARE(key2, outkey2);
    }

    // write zeroth entry
    QVERIFY(transaction->put(key0, value0));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.last());
        QByteArray outkey3;
        cursor.current(&outkey3, 0);
        QCOMPARE(key2, outkey3);
    }

    transaction->commit(42);
}

void TestHBtree::first()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
    // write first entry
    QVERIFY(transaction->put(key1, value1));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.first());
        QByteArray outkey1;
        cursor.current(&outkey1, 0);
        QCOMPARE(key1, outkey1);
    }

    // write second entry
    QVERIFY(transaction->put(key2, value2));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.first());
        QByteArray outkey2;
        cursor.current(&outkey2, 0);
        QCOMPARE(key1, outkey2);
    }

    // write zeroth entry
    QVERIFY(transaction->put(key0, value0));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.first());
        QByteArray outkey3;
        cursor.current(&outkey3, 0);
        QCOMPARE(key0, outkey3);
    }

    transaction->commit(42);
}

void TestHBtree::insertRandom_200BytesTo1kValues()
{
    const int numItems = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(qrand()).toAscii();
        QByteArray value(myRand(200, 1000) , 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }
}

void TestHBtree::insertHugeData_10Mb()
{
    QSKIP("This test takes too long");

    const int numBytes = 10000000;
    const int numItems = 100;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::splitBranch_1kData()
{
    const int numItems = 5000; // must cause multiple branch splits
    const int valueSize = 1000;
    const int keySize = 16;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }
}

void TestHBtree::splitBranchWithOverflows()
{
    // Bug, overflow pages get lost at a split.
    // This splits the page at the 308th insertion. The first
    // get transaction on a read fails after a split

    const int numItems = 1000; // must cause a split
    const int keySize = 255; // Only 3 1k keys can fit then we get a split
    const int valueSize = 4000; // must be over overflow threashold
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }

}

void TestHBtree::cursorExactMatch()
{
    const int numItems = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray ba = QString::number(myRand(10,1000)).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(ba, ba));
        keyValues.insert(ba, ba);
        QVERIFY(transaction->commit(i));
    }

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();

    while (it != keyValues.end()) {
        QVERIFY(cursor.seek(it.key()));
        QCOMPARE(cursor.key(), it.key());
        ++it;
    }

    transaction->abort();
}

void TestHBtree::cursorFuzzyMatch()
{
    const int numItems = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray ba = QString::number(i * 2 + (i % 2)).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(ba, ba));
        keyValues.insert(ba, ba);
        QVERIFY(transaction->commit(i));
    }

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    for (int i = 0; i < numItems * 2; ++i) {
        QByteArray ba = QString::number(i).toAscii();
        bool ok = cursor.seekRange(ba);
        QMap<QByteArray, QByteArray>::iterator it = keyValues.lowerBound(ba);

        if (it == keyValues.end())
            QVERIFY(!ok);
        else
            QCOMPARE(cursor.key(), it.key());
    }

    transaction->abort();
}

void TestHBtree::cursorNext()
{
    const int numItems = 500;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray ba = QString::number(myRand(0,100000)).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(ba, ba));
        keyValues.insert(ba, ba);
        QVERIFY(transaction->commit(i));
    }

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();

    while (it != keyValues.end()) {
        QVERIFY(cursor.next());
        QCOMPARE(cursor.key(), it.key());
        ++it;
    }

    transaction->abort();
}

void TestHBtree::cursorPrev()
{
    const int numItems = 500;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray ba = QString::number(myRand(0,100000)).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(ba, ba));
        keyValues.insert(ba, ba);
        QVERIFY(transaction->commit(i));
    }

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    QMap<QByteArray, QByteArray>::iterator it = keyValues.end();

    if (keyValues.size()) {
        do {
                --it;
                QVERIFY(cursor.previous());
                QCOMPARE(cursor.key(), it.key());
        } while (it != keyValues.begin());
    }

    transaction->abort();
}

void TestHBtree::lastMultiPage()
{
    QByteArray value0("baz");

    for (int i = 0; i < 1024; i++) {
        // write first entry
        QByteArray baKey(4, 0);
        qToBigEndian(i, (uchar *)baKey.data());
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(baKey, value0));
        QVERIFY(txn->commit(0));

        txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(txn);
        HBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        QByteArray outkey1;
        cursor.current(&outkey1, 0);
        QCOMPARE(baKey, outkey1);
        while (cursor.previous()) {
            QByteArray outkey2;
            cursor.current(&outkey2, 0);
            QVERIFY(memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) > 0);
            outkey1 = outkey2;
        }
        txn->abort();
    }
}

void TestHBtree::firstMultiPage()
{
    QByteArray value0("baz");

    for (int i = 1024; i > 0; i--) {
        // write first entry
        QByteArray baKey(4, 0);
        qToBigEndian(i, (uchar *)baKey.data());
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(baKey, value0));
        QVERIFY(txn->commit(0));

        txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(txn);
        HBtreeCursor cursor(txn);
        QVERIFY(cursor.first());
        QByteArray outkey1;
        cursor.current(&outkey1, 0);
        QCOMPARE(baKey, outkey1);
        while (cursor.next()) {
            QByteArray outkey2;
            cursor.current(&outkey2, 0);
            QVERIFY(memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) < 0);
            outkey1 = outkey2;
        }
        txn->abort();
    }
}

void TestHBtree::prev()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    // write entries
    QVERIFY(txn->put(key0, value0));
    QVERIFY(txn->put(key1, value1));
    QVERIFY(txn->put(key2, value2));

    // go to end
    {
        HBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        // test prev
        QVERIFY(cursor.previous());
        QByteArray outkey;
        cursor.current(&outkey, 0);
        QCOMPARE(key1, outkey);
    }

    {
        HBtreeCursor cursor(txn);
        // test prev without initialization is same as last()
        QVERIFY(cursor.previous());
        QByteArray outkey;
        cursor.current(&outkey, 0);
        QCOMPARE(key2, outkey);

        // prev to key1
        QVERIFY(cursor.previous());
        cursor.current(&outkey, 0);
        QCOMPARE(key1, outkey);

        // prev to key0
        QVERIFY(cursor.previous());
        cursor.current(&outkey, 0);
        QCOMPARE(key0, outkey);

        // prev to eof
        QVERIFY(!cursor.previous());
    }
    txn->abort();
}

void TestHBtree::prev2()
{
    QFile file(dbname);
    int maxSize = file.size();

    int amount = ::getenv("BENCHMARK_AMOUNT") ? ::atoi(::getenv("BENCHMARK_AMOUNT")) : 40000;
    for (int i = 0; i < amount; ++i) {
        QByteArray data = QUuid::createUuid().toRfc4122();
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(data, QByteArray("value_")+QByteArray::number(i)));
        txn->commit(0);
        int size = file.size();
        if (size > maxSize)
            maxSize = size;
    }

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    HBtreeCursor c(txn);
    QVERIFY(c.first());
    int cnt = 1;
    while (c.next()) ++cnt;
    QCOMPARE(cnt, amount);

    HBtreeCursor r(txn);
    QVERIFY(r.last());
    int rcnt = 1;
    while (r.previous()) ++rcnt;

    QCOMPARE(rcnt, amount);
    txn->abort();
}

void TestHBtree::multiBranchSplits()
{
    const int numItems = 1000; // must cause multiple branch splits
    const int valueSize = 1000;
    const int keySize = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    while (it != keyValues.end()) {
        QVERIFY(cursor.next());
        QCOMPARE(cursor.key(), it.key());
        ++it;
    }
    transaction->abort();
}

int keyCmp(const QByteArray &a, const QByteArray &b)
{
    QString as((const QChar*)a.constData(), a.size() / 2);
    QString bs((const QChar*)b.constData(), b.size() / 2);
    if (as < bs)
        return -1;
    else if (as > bs)
        return 1;
    else
        return 0;
}

void TestHBtree::createWithCmp()
{
    db->setCompareFunction(keyCmp);
    QString str1("1");
    QByteArray key1 = QByteArray::fromRawData((const char *)str1.data(), str1.size()*2);
    QByteArray value1("foo");
    QString str2("2");
    QByteArray key2 = QByteArray::fromRawData((const char *)str2.data(), str2.size()*2);
    QByteArray value2("bar");

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);

    // write first entry
    QVERIFY(txn->put(key1, value1));

    // read it
    QCOMPARE(value1, txn->get(key1));

    // read non-existing entry
    QVERIFY(txn->get(key2).isEmpty());

    // write second entry
    QVERIFY(txn->put(key2, value2));

    // read both entries
    QCOMPARE(value1, txn->get(key1));
    QCOMPARE(value2, txn->get(key2));

    txn->abort();
}

void TestHBtree::rollback()
{
    QByteArray key1("22");
    QByteArray value1("foo");
    QByteArray key2("42");
    QByteArray value2("bar");

    QByteArray result;

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    // write first entry
    QVERIFY(txn->put(key1, value1));
    txn->commit(42);

    {
        // start transaction
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);

        // re-write the first entry
        QVERIFY(txn->remove(key1));

        QVERIFY(txn->put(key1, value2));

        // write second entry
        QVERIFY(txn->put(key2, value2));

        // abort the transaction
        txn->abort();
    }

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);

    // read both entries
    QCOMPARE(value1, txn->get(key1));

    QVERIFY(txn->get(key2).isEmpty());

    txn->abort();
}

void TestHBtree::multipleRollbacks()
{
    QByteArray key1("101");
    QByteArray value1("foo");
    QByteArray key2("102");
    QByteArray value2("bar");

    {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        // write first entry
        QVERIFY(txn->put(key1, value1));
        QVERIFY(txn->commit(0));
    }

    {
        // start transaction
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);

        // re-write the first entry
        QVERIFY(txn->remove(key1));
        QVERIFY(txn->put(key1, value2));

        // abort the transaction
        txn->abort();
    }

    {
        // start transaction
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);

        // write second entry
        QVERIFY(txn->put(key2, value2));

        // abort the transaction
        txn->abort();
    }

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);

    // read both entries
    QCOMPARE(value1, txn->get(key1));
    QVERIFY(txn->get(key2).isEmpty());
    txn->abort();
}

void TestHBtree::variableSizeKeysAndData()
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
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(key, value));
        QVERIFY(txn->commit(0));
    }
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    // Delete every second object
    HBtreeCursor cursor(txn);
    QVERIFY(cursor.first());
    QByteArray key;
    cursor.current(&key, 0);
    bool remove = true;
    int counter = 0;
    while (cursor.next()) {
        counter++;
        if (remove) {
            remove = false;
            QVERIFY(txn->remove(cursor.key()));
        }
        else remove = true;
    }
    txn->commit(0);

    // Set this to false because after all those deleted we get a shit load of collectible pages
    // I don't know what to do with all of them. Commit them to disk on a new GC Page type?
    printOutCollectibles_ = false;
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

int cmpVarLengthKeys(const QByteArray &a, const QByteArray &b)
{
    int acount = findLongestSequenceOf(a.constData(), a.size(), 'a');
    int bcount = findLongestSequenceOf(b.constData(), b.size(), 'a');

    if (acount == bcount) {
        return QString::compare(a, b);
    } else {
        return (acount > bcount) ? 1 : ((acount < bcount) ? -1 : 0);
    }
}

bool cmpVarLengthKeysForQVec(const QByteArray &a, const QByteArray &b)
{
    return cmpVarLengthKeys(a, b) < 0;
}


void TestHBtree::compareSequenceOfVarLengthKeys()
{
    const char sequenceChar = 'a';
    const int numElements = 1024;
    const int minKeyLength = 20;
    const int maxKeyLength = 25;

    db->setCompareFunction(cmpVarLengthKeys);

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

    for (int i = 0; i < vec.size(); ++i) {
        int count = findLongestSequenceOf(vec[i].constData(), vec[i].size(), sequenceChar);
        QByteArray value((const char*)&count, sizeof(count));
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(vec[i], vec[i]));
        QVERIFY(txn->commit(i));
    }

    // Sort QVector to use as verification of bdb sort order
    qSort(vec.begin(), vec.end(), cmpVarLengthKeysForQVec);

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    HBtreeCursor cursor(txn);

    QByteArray key;
    QByteArray value;
    int i = 0;
    while (cursor.next()) {
        cursor.current(&key, 0);
        cursor.current(0, &value);
        QCOMPARE(key, vec[i++]);
    }
    txn->abort();
}

int asciiCmpFunc(const QByteArray &a, const QByteArray &b) {
//    qDebug() << a << b;
    int na = a.toInt();
    int nb = b.toInt();
    return na < nb ? -1 : (na > nb ? 1 : 0);
}

bool asciiCmpFuncForVec(const QByteArray &a, const QByteArray &b) {
    return asciiCmpFunc(a, b) < 0;
}

void TestHBtree::asciiAsSortedNumbers()
{
    const int numItems = 1000;
    QVector<QByteArray> keys;

    db->setCompareFunction(asciiCmpFunc);

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(qrand()).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, key));
        keys.append(key);
        QVERIFY(transaction->commit(i));
    }

    qSort(keys.begin(), keys.end(), asciiCmpFuncForVec);

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(transaction);
    HBtreeCursor cursor(transaction);
    QVector<QByteArray>::iterator it = keys.begin();
    while (it != keys.end()) {
        QVERIFY(cursor.next());
        QCOMPARE(cursor.key(), *it);
        QCOMPARE(cursor.value(),*it);
        ++it;
    }
    transaction->abort();
}

void TestHBtree::deleteReinsertVerify_data()
{
    QTest::addColumn<bool>("useCmp");
    QTest::newRow("With custom compare") << true;
    QTest::newRow("Without custom compare") << false;
}

void TestHBtree::deleteReinsertVerify()
{
    QFETCH(bool, useCmp);

    if (useCmp)
        db->setCompareFunction(asciiCmpFunc);

    const int numItems = 1000;

    QVector<QByteArray> keys;
    QMap<QByteArray, QByteArray> keyValueMap;
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        QByteArray value = QString::number(rand()).toAscii();
        keys.append(key);
        keyValueMap.insert(key, value);
        QVERIFY(txn->put(key, value));
    }
    QVERIFY(txn->commit(100));

    if (useCmp)
        qSort(keys.begin(), keys.end(), asciiCmpFuncForVec);
    else
        qSort(keys); // sort by qbytearray oeprator < since that's default in HBtree

    QCOMPARE(keyValueMap.size(), db->count());

    // Remove every other key
    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QMap<QByteArray, QByteArray> removedKeyValues;
    for (int i = 0; i < numItems; i += 2) {
        int idx = i;
        QByteArray removedValue = keyValueMap[keys[idx]];
        QCOMPARE(keyValueMap.remove(keys[idx]), 1);
        removedKeyValues.insert(keys[idx], removedValue);
        QVERIFY(txn->remove(keys[idx]));
    }
    QVERIFY(txn->commit(200));

    // Verify
    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QMap<QByteArray, QByteArray>::iterator it = keyValueMap.begin();
    while (it != keyValueMap.end()) {
        QCOMPARE(txn->get(it.key()), it.value());
        ++it;
    }
    it = removedKeyValues.begin();
    while (it != removedKeyValues.end()) {
        QCOMPARE(txn->get(it.key()), QByteArray());
        ++it;
    }
    txn->abort();

    // Reinsert
    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    it = removedKeyValues.begin();
    while (it != removedKeyValues.end()) {
        QVERIFY(txn->put(it.key(), it.value()));
        keyValueMap.insert(it.key(), it.value());
        ++it;
    }

    // Verify in order.
    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    HBtreeCursor cursor(txn);
    for (int i = 0; i < numItems; ++i) {
        QVERIFY(cursor.next());
        it = keyValueMap.find(keys[i]);
        QVERIFY(it != keyValueMap.end());
        QCOMPARE(cursor.key(), keys[i]);
        QCOMPARE(cursor.value(), it.value());
    }
    txn->abort();

}

void TestHBtree::rebalanceEmptyTree()
{
    QByteArray k1("foo");
    QByteArray v1("bar");
    QByteArray k2("ding");
    QByteArray v2("dong");
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(k1, v1));
    QVERIFY(txn->commit(0));

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QCOMPARE(txn->get(k1), v1);
    QVERIFY(txn->remove(k1));
    QVERIFY(txn->get(k1).isEmpty());
    QVERIFY(txn->commit(0));

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->get(k1).isEmpty());
    QVERIFY(txn->put(k2, v2));
    QCOMPARE(txn->get(k2), v2);
    QVERIFY(txn->commit(0));

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->remove(k2));
    QVERIFY(txn->commit(0));

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->get(k1).isEmpty());
    QVERIFY(txn->get(k2).isEmpty());
    QVERIFY(txn->commit(0));
}

void TestHBtree::reinsertion()
{
    const int numItems = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(qrand()).toAscii();
        QByteArray value(myRand(200, 1000) , 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }

    for (int i = 0; i < numItems * 2; ++i) {
        it = keyValues.begin() + myRand(0, numItems);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(it.key(), it.value()));
        QVERIFY(transaction->commit(i));
    }

    it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }
}

void TestHBtree::nodeComparisons()
{
    db->setCompareFunction(asciiCmpFunc);
    HBtreePrivate::NodeKey nkey10(d->compareFunction_, "10");
    HBtreePrivate::NodeKey nkey7(d->compareFunction_, "7");
    HBtreePrivate::NodeKey nkey33(d->compareFunction_, "33");

    QVERIFY(nkey10 > nkey7);
    QVERIFY(nkey33 > nkey7);
    QVERIFY(nkey33 > nkey10);

    QVERIFY(nkey10 != nkey7);
    QVERIFY(nkey33 != nkey7);
    QVERIFY(nkey33 != nkey10);

    QVERIFY(nkey7 < nkey10);
    QVERIFY(nkey7 < nkey33);
    QVERIFY(nkey10 < nkey33);

    QVERIFY(nkey10 == nkey10);

    QVERIFY(nkey7 <= nkey33);
    QVERIFY(nkey33 <= nkey33);

    QVERIFY(nkey33 >= nkey7);
    QVERIFY(nkey33 >= nkey33);
}

void TestHBtree::tag()
{
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("123")));
    QVERIFY(txn->commit(42u));

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("123")));
    QCOMPARE(db->tag(), 42u);
    // do not commit just yet

    HBtreeTransaction *rtxn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(rtxn);
    QCOMPARE(rtxn->tag(), 42u);

    QVERIFY(txn->commit(64u));
    QCOMPARE(db->tag(), 64u);
    QCOMPARE(rtxn->tag(), 42u);
    rtxn->abort();
    rtxn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(rtxn);
    QCOMPARE(rtxn->tag(), 64u);
    rtxn->abort();
}

void TestHBtree::cursors()
{
    QSKIP("cursor copy ctors not implemented yet");

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    txn->put(QByteArray("1"), QByteArray("a"));
    txn->put(QByteArray("2"), QByteArray("b"));
    txn->put(QByteArray("3"), QByteArray("c"));
    txn->put(QByteArray("4"), QByteArray("d"));
    txn->commit(0);

    txn = db->beginRead();

    QByteArray k1, k2;
    HBtreeCursor c1;
    HBtreeCursor c2(txn);

    c2.first();
    c2.current(&k1, 0);
    QCOMPARE(k1, QByteArray("1"));

    c2.next();
    c2.current(&k1, 0);
    QCOMPARE(k1, QByteArray("2"));

    c1 = c2;
    c1.current(&k1, 0);
    c2.current(&k2, 0);
    QCOMPARE(k1, k2);

    c1.next();
    c1.current(&k1, 0);
    c2.current(&k2, 0);
    QCOMPARE(k1, QByteArray("3"));
    QCOMPARE(k2, QByteArray("2"));

    HBtreeCursor c3(c1);
    c3.next();
    c1.current(&k1, 0);
    c3.current(&k2, 0);
    QCOMPARE(k1, QByteArray("3"));
    QCOMPARE(k2, QByteArray("4"));

    txn->abort();
}

void TestHBtree::markerOnReopen_data()
{
    // This test won't work if numCommits results in an auto sync or a split
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::markerOnReopen()
{
    QFETCH(quint32, numCommits);
    const quint32 pageSize = d->spec_.pageSize;

    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    QCOMPARE(d->marker_.info.number, 1u);
    QCOMPARE(d->collectiblePages_.size(), 0);
    QCOMPARE(d->size_, quint32(pageSize * 4));
    QCOMPARE(d->marker_.meta.revision, numCommits);
    QCOMPARE(d->marker_.meta.syncId, 1u);
    QCOMPARE(d->marker_.meta.root, 3u);
    QCOMPARE(d->marker_.meta.tag, (quint64)numCommits - 1);

    db->close();
    QVERIFY(db->open());

    QCOMPARE(d->marker_.info.number, 1u);
    QCOMPARE(d->collectiblePages_.size(), 0);
    QCOMPARE(d->size_, quint32(pageSize * 4));
    QCOMPARE(d->marker_.meta.revision, numCommits);
    QCOMPARE(d->marker_.meta.syncId, 1u);
    QCOMPARE(d->marker_.meta.root, 3u);
    QCOMPARE(d->marker_.meta.tag, (quint64)numCommits - 1);

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray::number(1000), QByteArray("1000")));
    QVERIFY(txn->commit(1000));

    // Synced page should not be used
    QCOMPARE(d->marker_.info.number, 1u);
    QCOMPARE(d->collectiblePages_.size(), 0);
    QCOMPARE(d->size_, quint32(pageSize * 5));
    QCOMPARE(d->marker_.meta.revision, numCommits + 1);
    QCOMPARE(d->marker_.meta.syncId, 2u);
    QCOMPARE(d->marker_.meta.root, 4u);
    QCOMPARE(d->marker_.meta.tag, (quint64)1000);

    QVERIFY(db->sync());

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray::number(2000), QByteArray("2000")));
    QVERIFY(txn->commit(2000));

    QCOMPARE(d->marker_.info.number, 1u);
    QCOMPARE(d->collectiblePages_.size(), 0);
    QCOMPARE(d->size_, quint32(pageSize * 5));
    QCOMPARE(d->marker_.meta.revision, numCommits + 2);
    QCOMPARE(d->marker_.meta.syncId, 3u);
    QCOMPARE(d->marker_.meta.root, 3u);
    QCOMPARE(d->marker_.meta.tag, (quint64)2000);
}

void TestHBtree::corruptSyncMarker1_data()
{
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::corruptSyncMarker1()
{
    QFETCH(quint32, numCommits);

    quint32 psize = d->spec_.pageSize;
    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    QCOMPARE(d->collectiblePages_.size(), 0);

    for (int i = 0; i < 5; ++i) {
        db->close();

        corruptSinglePage(psize, 1, HBtreePrivate::PageInfo::Marker);

        QVERIFY(db->open());

        QCOMPARE(d->collectiblePages_.size(), 0);

        for (quint32 i = 0; i < numCommits; ++i) {
            HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
            QVERIFY(txn);
            QCOMPARE(txn->get(QByteArray::number(i)), QByteArray::number(i));
            txn->abort();
        }
    }
}

void TestHBtree::corruptBothSyncMarkers_data()
{
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::corruptBothSyncMarkers()
{
    QFETCH(quint32, numCommits);

    quint32 psize = d->spec_.pageSize;
    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    for (int i = 0; i < 1; ++i) {
        db->close();

        corruptSinglePage(psize, 1, HBtreePrivate::PageInfo::Marker);
        corruptSinglePage(psize, 2, HBtreePrivate::PageInfo::Marker);

        QVERIFY(!db->open());
    }
}

void TestHBtree::cursorWhileDelete_data()
{
    set_data();
}

void TestHBtree::cursorWhileDelete()
{
    QFETCH(int, numItems);
    QFETCH(int, keySize);
    QFETCH(int, valueSize);
    QFETCH(int, syncRate);

    db->setAutoSyncRate(syncRate);

    // Insert data
    QMap<QByteArray, QByteArray> keyValues;
    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + (i % ('z' - 'a')));
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    // Delete all
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    HBtreeCursor cursor1(txn);
    while (cursor1.next()) {
        QVERIFY(txn->del(cursor1.key()));
    }
    txn->commit(0);

    // Verify
    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        QCOMPARE(txn->get(it.key()), QByteArray());
        ++it;
    }
    txn->abort();

    // Reinsert
    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    it = keyValues.begin();
    while (it != keyValues.end()) {
        QVERIFY(txn->put(it.key(), it.value()));
        ++it;
    }
    txn->commit(0);

    // Verify
    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    HBtreeCursor cursor2(txn);
    it = keyValues.begin();
    while (cursor2.next()) {
        QCOMPARE(it.key(), cursor2.key());
        QCOMPARE(it.value(), cursor2.value());
        ++it;
    }
    txn->abort();
}

void TestHBtree::getDataFromLastSync()
{
    db->setAutoSyncRate(0);

    int numCommits = 1000;
    for (int i = 0; i < numCommits / 2; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }
    db->sync();

    for (int i = numCommits / 2; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    d->close(false);

    QVERIFY(db->open());
    QCOMPARE((int)db->tag(), numCommits / 2 - 1);

    for (int i = 0; i < numCommits / 2; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(txn);
        QCOMPARE(txn->get(QByteArray::number(i)), QByteArray::number(i));
        txn->abort();
    }

    for (int i = numCommits / 2; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->get(QByteArray::number(i)).isEmpty());
        txn->abort();
    }
}

void TestHBtree::deleteAlotNoSyncReopen_data()
{
    set_data();
}

void TestHBtree::deleteAlotNoSyncReopen()
{
    db->setAutoSyncRate(0);
    QFETCH(int, numItems);
    QFETCH(int, valueSize);

    // Put all in
    QMap<QByteArray, QByteArray> keyValues;
    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QByteArray::number(i);
        QByteArray value(valueSize, 'a' + (i % ('z' - 'a')));
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        QVERIFY(transaction->commit(i));
        keyValues.insert(key, value);
    }

    // Delete first half
    for (int i = 0; i < numItems / 2; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->remove(QByteArray::number(i)));
        QVERIFY(transaction->commit(i));
    }

    QVERIFY(d->sync());

    // Delete second half
    for (int i = numItems / 2; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->remove(QByteArray::number(i)));
        QVERIFY(transaction->commit(i));
    }

    // Close without sync and reopen
    d->close(false);
    QVERIFY(db->open());

    // Verify not first half and yes second half
    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(transaction);
    for (int i = 0; i < numItems / 2; ++i) {
        QCOMPARE(transaction->get(QByteArray::number(i)), QByteArray());
    }
    for (int i = numItems / 2; i < numItems; ++i) {
        QCOMPARE(transaction->get(QByteArray::number(i)), keyValues[QByteArray::number(i)]);
    }
    transaction->abort();

    // Put in first half
    for (int i = 0; i < numItems / 2; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QByteArray::number(i), keyValues[QByteArray::number(i)]));
        QVERIFY(transaction->commit(i));

        // Verify all
        transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        for (int j = 0; j < i; ++j) {
            QCOMPARE(transaction->get(QByteArray::number(j)), keyValues[QByteArray::number(j)]);
        }
        for (int j = numItems / 2; j < numItems; ++j) {
            QCOMPARE(transaction->get(QByteArray::number(j)), keyValues[QByteArray::number(j)]);
        }
        transaction->abort();
    }
}

QTEST_MAIN(TestHBtree)
#include "main.moc"
