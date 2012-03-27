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
#include <QMap>
#include <QString>

#include "hbtree.h"
#include "hbtree_p.h"
#include "qbtree.h"
#include "hbtreetransaction.h"
#include "qbtreetxn.h"
#include "qbtreecursor.h"
#include "qkeyvaluestore.h"
#include "qkeyvaluestoretxn.h"
#include "qkeyvaluestorecursor.h"

class TestBtrees: public QObject
{
    Q_OBJECT
public:
    TestBtrees();

    enum BtreeType {
        Hybrid,
        AppendOnly,
        KeyValue
    };

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void openClose_data();
    void openClose();

    void insertItem_data();
    void insertItem();

    void insert1000Items_data();
    void insert1000Items();

    void delete1000Items_data();
    void delete1000Items();

    void find1000Items_data();
    void find1000Items();

    void searchRange_data();
    void searchRange();

    void compact_data();
    void compact();

private:

    HBtree *hybridDb;
    QBtree *appendOnlyDb;
    QKeyValueStore *keyValueDb;
    const HBtreePrivate *hybridPrivate;

    struct SizeStat {
        SizeStat()
            : hybridSize(0), appendOnlySize(0), keyValueSize(0), numCollectible(0)
        {}
        qint64 hybridSize;
        qint64 appendOnlySize;
        qint64 keyValueSize;
        int numCollectible;
    };

    QMap<QString, SizeStat> sizeStats_;
};

TestBtrees::TestBtrees()
    : hybridDb(0), appendOnlyDb(0), keyValueDb(0)
{
}

static const char hybridDbFileName[] = "tst_hbtree.db";
static const char appendOnlyDbFileName[] = "tst_aobtree.db";
static const char keyValueDbFileName[] = "tst_kvs";
static const char hybridDataTag[] = "Hybrid";
static const char appendOnlyDataTag[] = "Append-Only";
static const char keyValueDataTag[] = "KeyValue";


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

void TestBtrees::initTestCase()
{
}

void TestBtrees::cleanupTestCase()
{
    qDebug() << "Printing stats:";
    QMap<QString, SizeStat>::const_iterator it = sizeStats_.constBegin();
    while (it != sizeStats_.constEnd()) {
        qDebug() << it.key();
        qDebug() << "\tAppend-Only:" << sizeStr(it.value().appendOnlySize);
        qDebug() << "\tHybrid:" << sizeStr(it.value().hybridSize) << "with" << it.value().numCollectible << "reusable pages";
        qDebug() << "\tKeyValue:" << sizeStr(it.value().keyValueSize);
        ++it;
    }
}

void TestBtrees::init()
{
    QFile::remove(hybridDbFileName);
    QFile::remove(appendOnlyDbFileName);
    QString kvsTree = keyValueDbFileName;
    kvsTree.append(".btr");
    QString kvsJournal = keyValueDbFileName;
    kvsJournal.append(".dat");
    QString kvsOldJournal = keyValueDbFileName;
    kvsOldJournal.append(".old");
    QFile::remove(kvsTree);
    QFile::remove(kvsJournal);
    QFile::remove(kvsOldJournal);

    hybridDb = new HBtree(hybridDbFileName);
    appendOnlyDb = new QBtree(appendOnlyDbFileName);
    keyValueDb = new QKeyValueStore(QString(keyValueDbFileName));

    if (!hybridDb->open(HBtree::ReadWrite))
        Q_ASSERT(false);

    if (!appendOnlyDb->open(QBtree::NoSync | QBtree::UseSyncMarker))
        Q_ASSERT(false);

    if (!keyValueDb->open())
        Q_ASSERT(false);

    appendOnlyDb->setAutoCompactRate(1000);
    // Manual sync only
    keyValueDb->setSyncThreshold(0);

    hybridPrivate = hybridDb->d_func();
}

void TestBtrees::cleanup()
{
    QString keyValueJournal(keyValueDbFileName);
    QString keyValueTree(keyValueDbFileName);
    keyValueJournal.append(".dat");
    keyValueTree.append(".btr");

    QString tag = QTest::currentDataTag();

    if (tag == hybridDataTag) {
        QFile file(hybridDbFileName);
        file.open(QFile::ReadOnly);
        SizeStat &ss = sizeStats_[QTest::currentTestFunction()];
        ss.hybridSize = qMax(file.size(), ss.hybridSize);
        ss.numCollectible = qMax(hybridPrivate->collectiblePages_.size(), ss.numCollectible);
    } else if (tag == appendOnlyDataTag) {
        QFile file(appendOnlyDbFileName);
        file.open(QFile::ReadOnly);
        SizeStat &ss = sizeStats_[QTest::currentTestFunction()];
        ss.appendOnlySize = qMax(file.size(), ss.appendOnlySize);
    } else if (tag == keyValueDataTag) {
        QFile journal(keyValueJournal);
        QFile tree(keyValueTree);
        journal.open(QFile::ReadOnly);
        tree.open(QFile::ReadOnly);
        qint64 journalSize = 0, treeSize = 0, totalSize = 0;
        journalSize = journal.size();
        treeSize = tree.size();
        totalSize = journalSize + treeSize;
        SizeStat &ss = sizeStats_[QTest::currentTestFunction()];
        ss.keyValueSize = qMax(totalSize, ss.keyValueSize);
    }

    delete hybridDb;
    delete appendOnlyDb;
    delete keyValueDb;
    appendOnlyDb = 0;
    hybridDb = 0;
    keyValueDb = 0;
    QFile::remove(hybridDbFileName);
    QFile::remove(appendOnlyDbFileName);
    QFile::remove(keyValueJournal);
    QFile::remove(keyValueTree);
}

void TestBtrees::openClose_data()
{
    QTest::addColumn<int>("btreeType");
    QTest::newRow(hybridDataTag) << (int)Hybrid;
    QTest::newRow(appendOnlyDataTag) << (int)AppendOnly;
    QTest::newRow(keyValueDataTag) << (int)KeyValue;
}

void TestBtrees::openClose()
{
    QFETCH(int, btreeType);

    if (btreeType == Hybrid) {
        QBENCHMARK {
            hybridDb->close();
            QVERIFY(hybridDb->open());
        }
    } else if (btreeType == AppendOnly){
        QBENCHMARK {
            appendOnlyDb->close();
            QVERIFY(appendOnlyDb->open());
        }
    } else if (btreeType == KeyValue){
        QBENCHMARK {
            keyValueDb->close();
            QVERIFY(keyValueDb->open());
        }
    }
}

void TestBtrees::insertItem_data()
{
    QTest::addColumn<int>("btreeType");
    QTest::newRow(hybridDataTag) << (int)Hybrid;
    QTest::newRow(appendOnlyDataTag) << (int)AppendOnly;
    QTest::newRow(keyValueDataTag) << (int)KeyValue;
}

void TestBtrees::insertItem()
{
    QFETCH(int, btreeType);
    int i = 0;
    if (btreeType == Hybrid) {
        QBENCHMARK {
            ++i;
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            HBtreeTransaction *txn = hybridDb->beginWrite();
            QVERIFY(txn);
            QVERIFY(txn->put(key, value));
            QVERIFY(txn->commit(i));
        }
    } else if (btreeType == AppendOnly){
        QBENCHMARK {
            ++i;
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QBtreeTxn *txn = appendOnlyDb->beginWrite();
            QVERIFY(txn);
            QVERIFY(txn->put(key, value));
            QVERIFY(txn->commit(i));
        }
    } else if (btreeType == KeyValue){
        QBENCHMARK {
            ++i;
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
            QVERIFY(txn);
            QVERIFY(txn->put(key, value));
            QVERIFY(txn->commit(i));
        }
    }
}

void TestBtrees::insert1000Items_data()
{
    QTest::addColumn<int>("btreeType");
    QTest::newRow(hybridDataTag) << (int)Hybrid;
    QTest::newRow(appendOnlyDataTag) << (int)AppendOnly;
    QTest::newRow(keyValueDataTag) << (int)KeyValue;
}

void TestBtrees::insert1000Items()
{
    QFETCH(int, btreeType);
    int numItems = 1000;

    if (btreeType == Hybrid) {
        QBENCHMARK {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                QByteArray value = QByteArray::number(i);
                HBtreeTransaction *txn = hybridDb->beginWrite();
                QVERIFY(txn);
                QVERIFY(txn->put(key, value));
                QVERIFY(txn->commit(i));
            }
        }
    } else if (btreeType == AppendOnly){
        QBENCHMARK {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                QByteArray value = QByteArray::number(i);
                QBtreeTxn *txn = appendOnlyDb->beginWrite();
                QVERIFY(txn);
                QVERIFY(txn->put(key, value));
                QVERIFY(txn->commit(i));
            }
        }
    } else if (btreeType == KeyValue){
        QBENCHMARK {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                QByteArray value = QByteArray::number(i);
                QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
                QVERIFY(txn);
                QVERIFY(txn->put(key, value));
                QVERIFY(txn->commit(i));
            }
        }
    }
}

void TestBtrees::delete1000Items_data()
{
    QTest::addColumn<int>("btreeType");
    QTest::newRow(hybridDataTag) << (int)Hybrid;
    QTest::newRow(appendOnlyDataTag) << (int)AppendOnly;
    QTest::newRow(keyValueDataTag) << (int)KeyValue;
}

void TestBtrees::delete1000Items()
{
    QFETCH(int, btreeType);
    int numItems = 1000;

    if (btreeType == Hybrid) {
        HBtreeTransaction *txn = hybridDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems; ++i) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    } else if (btreeType == AppendOnly) {
        QBtreeTxn *txn = appendOnlyDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems; ++i) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    } else if (btreeType == KeyValue) {
        QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems; ++i) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    }

    if (btreeType == Hybrid) {
        QBENCHMARK_ONCE {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                HBtreeTransaction *txn = hybridDb->beginWrite();
                QVERIFY(txn);
                QVERIFY(txn->remove(key));
                QVERIFY(txn->commit(i));
            }
        }
    } else if (btreeType == AppendOnly) {
        QBENCHMARK_ONCE {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                QBtreeTxn *txn = appendOnlyDb->beginWrite();
                QVERIFY(txn);
                QVERIFY(txn->remove(key));
                QVERIFY(txn->commit(i));
            }
        }
    } else if (btreeType == KeyValue) {
        QBENCHMARK_ONCE {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
                QVERIFY(txn);
                QVERIFY(txn->remove(key));
                QVERIFY(txn->commit(i));
            }
        }
    }
}

void TestBtrees::find1000Items_data()
{
    QTest::addColumn<int>("btreeType");
    QTest::newRow(hybridDataTag) << (int)Hybrid;
    QTest::newRow(appendOnlyDataTag) << (int)AppendOnly;
    QTest::newRow(keyValueDataTag) << (int)KeyValue;
}

void TestBtrees::find1000Items()
{
    QFETCH(int, btreeType);
    int numItems = 1000;

    if (btreeType == Hybrid) {
        HBtreeTransaction *txn = hybridDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems; ++i) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    } else if (btreeType == AppendOnly) {
        QBtreeTxn *txn = appendOnlyDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems; ++i) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    } else if (btreeType == KeyValue) {
        QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems; ++i) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    }

    if (btreeType == Hybrid) {
        QBENCHMARK {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                QByteArray value = QByteArray::number(i);
                HBtreeTransaction *txn = hybridDb->beginRead();
                QVERIFY(txn);
                QCOMPARE(txn->get(key), value);
                txn->abort();
            }
        }
    } else if (btreeType == AppendOnly) {
        QBENCHMARK {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                QByteArray value = QByteArray::number(i);
                QByteArray baOut;
                QBtreeTxn *txn = appendOnlyDb->beginRead();
                QVERIFY(txn);
                QVERIFY(txn->get(key, &baOut));
                QCOMPARE(baOut, value);
                txn->abort();
            }
        }
    } else if (btreeType == KeyValue) {
        QBENCHMARK {
            for (int i = 0; i < numItems; ++i) {
                QByteArray key = QByteArray::number(i);
                QByteArray value = QByteArray::number(i);
                QByteArray baOut;
                QKeyValueStoreTxn *txn = keyValueDb->beginRead();
                QVERIFY(txn);
                QVERIFY(txn->get(key, &baOut));
                QCOMPARE(baOut, value);
                txn->abort();
            }
        }
    }
}

void TestBtrees::searchRange_data()
{
    QTest::addColumn<int>("btreeType");
    QTest::newRow(hybridDataTag) << (int)Hybrid;
    QTest::newRow(appendOnlyDataTag) << (int)AppendOnly;
    QTest::newRow(keyValueDataTag) << (int)KeyValue;
}

void TestBtrees::searchRange()
{
    QFETCH(int, btreeType);
    int numItems = 1000;
    int gapLength = 100;

    if (btreeType == Hybrid) {
        HBtreeTransaction *txn = hybridDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems * gapLength; i += gapLength) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    } else if (btreeType == AppendOnly) {
        QBtreeTxn *txn = appendOnlyDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems * gapLength; i += gapLength) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    } else if (btreeType == KeyValue) {
        QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
        QVERIFY(txn);
        for (int i = 0; i < numItems * gapLength; i += gapLength) {
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
        }
        QVERIFY(txn->commit(0));
    }


    if (btreeType == Hybrid) {
        QBENCHMARK {
            for (int i = 0; i < (numItems * gapLength) - (gapLength); i += (gapLength / 10)) {
                QByteArray key = QByteArray::number(i);
                HBtreeTransaction *txn = hybridDb->beginRead();
                QVERIFY(txn);
                HBtreeCursor cursor(txn);
                QVERIFY(cursor.seekRange(key));
                txn->abort();
            }
        }
    } else if (btreeType == AppendOnly) {
        QBENCHMARK {
            for (int i = 0; i < (numItems * gapLength) - (gapLength); i += (gapLength / 10)) {
                QByteArray key = QByteArray::number(i);
                QByteArray baOut;
                QBtreeTxn *txn = appendOnlyDb->beginRead();
                QBtreeCursor cursor(txn);
                QVERIFY(cursor.seekRange(key));
                txn->abort();
            }
        }
    } else if (btreeType == KeyValue) {
        QBENCHMARK {
            for (int i = 0; i < (numItems * gapLength) - (gapLength); i += (gapLength / 10)) {
                QByteArray key = QByteArray::number(i);
                QKeyValueStoreTxn *txn = keyValueDb->beginRead();
                QKeyValueStoreCursor cursor(txn);
                QVERIFY(cursor.seekRange(key));
                txn->abort();
            }
        }
    }
}

void TestBtrees::compact_data()
{
    QTest::addColumn<int>("btreeType");
    QTest::newRow(appendOnlyDataTag) << (int)AppendOnly;
    QTest::newRow(keyValueDataTag) << (int)KeyValue;
}

void TestBtrees::compact()
{
    QFETCH(int, btreeType);
    int numItems = 1000;

    if (btreeType == AppendOnly) {
        for (int i = 0; i < numItems; ++i) {
            QBtreeTxn *txn = appendOnlyDb->beginWrite();
            QVERIFY(txn);
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
            QVERIFY(txn->commit(i));
        }
        appendOnlyDb->sync();
        for (int i = 0; i < numItems; ++i) {
            QBtreeTxn *txn = appendOnlyDb->beginWrite();
            QVERIFY(txn);
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
            QVERIFY(txn->commit(i));
        }
        appendOnlyDb->sync();
        for (int i = 0; i < numItems/2; ++i) {
            QBtreeTxn *txn = appendOnlyDb->beginWrite();
            QVERIFY(txn);
            QByteArray key = QByteArray::number(i);
            QVERIFY(txn->remove(key));
            QVERIFY(txn->commit(i));
        }
        appendOnlyDb->sync();
    } else if (btreeType == KeyValue) {
        keyValueDb->setSyncThreshold(0);
        for (int i = 0; i < numItems; ++i) {
            QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
            QVERIFY(txn);
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
            QVERIFY(txn->commit(i));
        }
        keyValueDb->sync();
        for (int i = 0; i < numItems; ++i) {
            QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
            QVERIFY(txn);
            QByteArray key = QByteArray::number(i);
            QByteArray value = QByteArray::number(i);
            QVERIFY(txn->put(key, value));
            QVERIFY(txn->commit(i));
        }
        keyValueDb->sync();
        for (int i = 0; i < numItems/2; ++i) {
            QKeyValueStoreTxn *txn = keyValueDb->beginWrite();
            QVERIFY(txn);
            QByteArray key = QByteArray::number(i);
            QVERIFY(txn->remove(key));
            QVERIFY(txn->commit(i));
        }
        keyValueDb->sync();
    }
    if (btreeType == AppendOnly) {
        QBENCHMARK_ONCE {
            QVERIFY(appendOnlyDb->compact());
        }
    } else if (btreeType == KeyValue) {
        QBENCHMARK_ONCE {
            QVERIFY(keyValueDb->compact());
        }
    }
}

QTEST_MAIN(TestBtrees)
#include "main.moc"
