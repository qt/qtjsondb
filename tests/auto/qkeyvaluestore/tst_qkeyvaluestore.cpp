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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QFile>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <unistd.h>
#include <string.h>

#include "qkeyvaluestore.h"
#include "qkeyvaluestoretxn.h"
#include "qkeyvaluestorecursor.h"
#include "qkeyvaluestorefile.h"
#include "qkeyvaluestoreentry.h"

char testString[] = "this is a test";
quint32 testStringSize = 14;

class QKeyValueStoreTest : public QObject
{
    Q_OBJECT
    QString m_dbName;
    QString m_journal;
    QString m_tree;
    QString m_treeCopy;

public:
    QKeyValueStoreTest();
    void removeTemporaryFiles();
private Q_SLOTS:
    void sanityCheck();
    void storeAndLoad();
    void transactionLifetime();
    void autoSync();
    void manualSync();
    void compactionAfterRemove();
    void compactionAfterPut();
    void compactionAutoTrigger();
    void compactionContinuation();
    void fastOpen();
    void testNonAsciiChars();
    void buildBTreeFromScratch();
    void buildBTreeFromKnownState();
    void cursorSanityCheck();
    void cursorAscendingOrder();
    void cursorSeek();
    void cursorSeekRange();
    void fileSanityCheck();
    void fileWrite();
    void fileRead();
    void fileOffset();
    void fileSync();
    void fileTruncate();
#if 0
#endif
};

QKeyValueStoreTest::QKeyValueStoreTest()
{
    m_dbName = "db";
    m_journal = m_dbName + ".dat";
    m_tree = m_dbName + ".btr";
    m_treeCopy = m_dbName + ".old";
}

void QKeyValueStoreTest::removeTemporaryFiles()
{
    QFile::remove(m_journal);
    QFile::remove(m_tree);
    QFile::remove(m_treeCopy);
}

/*
 * In a way this is a useless test, however it fulfills a purpose.
 * All this operations should succeed unless something is really wrong.
 * So despite not testing the functionality, we test that the API returns
 * the right values.
 */
void QKeyValueStoreTest::sanityCheck()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    bool test = false;
    test = storage.open();
    QCOMPARE(test, true);

    QKeyValueStoreTxn *readTxn = storage.beginRead();
    QVERIFY2(readTxn, "Read txn is NULL");
    QKeyValueStoreTxn *writeTxn = storage.beginWrite();
    QVERIFY2(writeTxn, "Write txn is NULL");

    delete readTxn;
    delete writeTxn;

    test = storage.close();
    QCOMPARE(test, true);
    removeTemporaryFiles();
}

/*
 * Simple test. We store some values, then read the values back.
 */
void QKeyValueStoreTest::storeAndLoad()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    bool test = false;
    test = storage.open();
    QCOMPARE(test, true);

    // Set the sync threshold very low
    storage.setSyncThreshold(5);

    QByteArray key("key");
    QByteArray value("value");

    // Create a write transaction
    QKeyValueStoreTxn *writeTxn = storage.beginWrite();
    QVERIFY2(writeTxn, "Write txn is NULL");
    test = writeTxn->put(key, value);
    QCOMPARE(test, true);
    test = writeTxn->commit(100);
    QCOMPARE(test, true);

    // Create a read transaction
    QKeyValueStoreTxn *readTxn = storage.beginRead();
    QVERIFY2(readTxn, "Read txn is NULL");
    QByteArray retrievedValue;
    test = readTxn->get(key, &retrievedValue);
    QCOMPARE(test, true);
    QCOMPARE(value, retrievedValue);
    delete readTxn;

    test = storage.close();
    QCOMPARE(test, true);
    removeTemporaryFiles();
}

/*
 * We create a write transaction and store some values into the db.
 * Afterwards we create two transactions, one read and one write and
 * start writing on the db. The read transaction shouldn't see the changes.
 * After commiting the changes we create a new read transaction and see the
 * new values.
 */
void QKeyValueStoreTest::transactionLifetime()
{
    QByteArray key0("key0"), value0("value0");
    QByteArray key1("key1"), value1("value1");
    QByteArray key2("key2"), value2("value2");
    QByteArray key3("key3"), value3("value3");

    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    QVERIFY(storage.open());

    // Set the sync threshold very low
    storage.setSyncThreshold(5);

    // Create a write transaction
    QKeyValueStoreTxn *writeTxn = storage.beginWrite();
    QVERIFY2(writeTxn, "Write txn is NULL");
    QVERIFY(writeTxn->put(key0, value0));
    QVERIFY(writeTxn->commit(100));
#if defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
    // Now, the transaction is commited. Let's create the other two.
    QKeyValueStoreTxn *readTxn = storage.beginRead();
    QVERIFY2(readTxn, "Read txn is NULL");
    QKeyValueStoreTxn *newWriteTxn = storage.beginWrite();
    QVERIFY2(newWriteTxn, "newWrite txn is NULL");

    // Write some more entries
    QVERIFY(newWriteTxn->put(key1, value1));
    QVERIFY(newWriteTxn->put(key2, value2));
    QVERIFY(newWriteTxn->put(key3, value3));

    // Try to read those entries
    QByteArray recoveredValue;
    QVERIFY(readTxn->get(key0, &recoveredValue));
    QCOMPARE(recoveredValue, value0);
    QVERIFY(!readTxn->get(key1, &recoveredValue));
    QVERIFY(!readTxn->get(key2, &recoveredValue));
    QVERIFY(!readTxn->get(key3, &recoveredValue));

    // However the write transaction can see it
    QVERIFY(newWriteTxn->get(key1, &recoveredValue));
    QCOMPARE(value1, recoveredValue);

    // Commit the writes
    QVERIFY(newWriteTxn->commit(101));

    // Try to get them, once more
    QVERIFY(!readTxn->get(key1, &recoveredValue));
    QVERIFY(!readTxn->get(key2, &recoveredValue));
    QVERIFY(!readTxn->get(key3, &recoveredValue));

    // Create a new read transaction and get them
    QKeyValueStoreTxn *newReadTxn = storage.beginRead();
    QByteArray recoveredValue1, recoveredValue2, recoveredValue3;
    // Try to get them, once more
    QVERIFY(newReadTxn->get(key1, &recoveredValue1));
    QCOMPARE(value1, recoveredValue1);
    QVERIFY(newReadTxn->get(key2, &recoveredValue2));
    QCOMPARE(value2, recoveredValue2);
    QVERIFY(newReadTxn->get(key3, &recoveredValue3));
    QCOMPARE(value3, recoveredValue3);
#endif
    QVERIFY(storage.close());
    removeTemporaryFiles();
}

/*
 * It seems that QMap and QByteArray do not share its love for non-ascii
 * characters. This test case checks that.
 */
void QKeyValueStoreTest::testNonAsciiChars()
{
    removeTemporaryFiles();

    char rawKey0[] = { 0, 0, 0, 2, 0 };
    char rawKey1[] = { 0, 0, 0, 2, 1 };
    char rawKey2[] = { 0, 0, 0, 2, 2 };
    char rawKey3[] = { 0, 0, 0, 2, 3 };
    int rawKeySize = 5;
    QByteArray key0(rawKey0, rawKeySize);
    QByteArray key1(rawKey1, rawKeySize);
    QByteArray key2(rawKey2, rawKeySize);
    QByteArray key3(rawKey3, rawKeySize);
    char rawValue0[] = { 1, 0, 0, 2, 0 };
    char rawValue1[] = { 1, 0, 0, 2, 1 };
    char rawValue2[] = { 1, 0, 0, 2, 2 };
    char rawValue3[] = { 1, 0, 0, 2, 3 };
    int rawValueSize = 5;
    QByteArray value0(rawValue0, rawValueSize);
    QByteArray value1(rawValue1, rawValueSize);
    QByteArray value2(rawValue2, rawValueSize);
    QByteArray value3(rawValue3, rawValueSize);

    QKeyValueStore storage(m_dbName);
    QVERIFY(storage.open());
    // Set the sync threshold very low
    storage.setSyncThreshold(5);

    // Create a write transaction
    QKeyValueStoreTxn *writeTxn = storage.beginWrite();
    QVERIFY2(writeTxn, "Write txn is NULL");
    QVERIFY(writeTxn->put(key0, value0));
    QVERIFY(writeTxn->put(key1, value1));
    QVERIFY(writeTxn->put(key2, value2));
    QVERIFY(writeTxn->put(key3, value3));
    QVERIFY(writeTxn->commit(100));

    QKeyValueStoreTxn *readTxn = storage.beginRead();
    QVERIFY2(readTxn, "Read txn is NULL");
    QByteArray recoveredValue;
    QVERIFY(readTxn->get(key0, &recoveredValue));
    QCOMPARE(recoveredValue, value0);
    QVERIFY(readTxn->get(key1, &recoveredValue));
    QCOMPARE(recoveredValue, value1);
    QVERIFY(readTxn->get(key2, &recoveredValue));
    QCOMPARE(recoveredValue, value2);
    QVERIFY(readTxn->get(key3, &recoveredValue));
    QCOMPARE(recoveredValue, value3);
    QVERIFY(readTxn->abort());

    removeTemporaryFiles();
}

/*
 * We create a database and start writing elements to it.
 * We count the number of marks on the DB and check the status
 * of the btree.
 */
void QKeyValueStoreTest::autoSync()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    QVERIFY(storage.open());
    /*
     * We write 100 entries, and check if the automatic sync kicked in.
     * This can be verified by checking the number of marks on the db file.
     * Entries 0..9 weight 20 bytes, and the rest 21. That's about 2090
     * bytes, therefore we should have 2 marks (2nd mark comes at 2048 bytes).
     */
    QByteArray value("012345678901234");
    for (int i = 0; i < 100; i++) {
        QKeyValueStoreTxn *writeTxn = storage.beginWrite();
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        QVERIFY(writeTxn->put(key, value));
        QVERIFY(writeTxn->commit(i + 1024));
    }
    QVERIFY(storage.close());
    // Now we open the file and inspect it.
    QFile db(m_journal);
    db.open(QIODevice::ReadOnly);
    QVERIFY(db.size() != 0);
    QDataStream stream(&db);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 count = 0, found = 0;
    int numberOfMarkers = 0;
    quint32 m_marker = 0x55AAAA55;
    quint32 tag = 0;
    quint64 dataTimestamp = 0;
    stream >> count;
    while (!stream.atEnd()) {
        QByteArray key;
        stream >> key;
        quint8 operation = 0;
        stream >> operation;
        quint32 offsetToStart = 0;
        stream >> offsetToStart;
        QByteArray value;
        stream >> value;
        quint32 hash = 0xFFFFFFFF;
        stream >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream >> tag;
            quint32 marker = 0;
            stream >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream >> dataTimestamp;
                stream >> hash;
                numberOfMarkers++;
                stream >> count;
            } else {
                count = marker;
            }
            found = 0;
        }
    }
    // 2 automatic markers and one after calling close
    QCOMPARE(numberOfMarkers, 3);
    // And now to inspect the btree
    QFile tree(m_tree);
    tree.open(QIODevice::ReadOnly);
    QDataStream treeStream(&tree);
    treeStream.setByteOrder(QDataStream::LittleEndian);
    quint64 treeTimestamp = 0;
    found = 0;
    treeStream >> count;
    QCOMPARE(count, (quint32)100);
    while (!treeStream.atEnd()) {
        QByteArray key;
        qint64 value;
        treeStream >> key;
        treeStream >> value;
        found++;
        if (count == found)
            break;
    }
    treeStream >> treeTimestamp;
    quint32 hash = 0xFFFFFFFF;
    treeStream >> hash;
    quint32 computedHash = qHash(treeTimestamp);
    QCOMPARE(hash, computedHash);
    QCOMPARE(treeTimestamp, dataTimestamp);

    removeTemporaryFiles();
}

/*
 * We create a database and start writing elements to it.
 * We count the number of marks on the DB and check the status
 * of the btree.
 */
void QKeyValueStoreTest::manualSync()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    // Set it on manual sync mode
    storage.setSyncThreshold(0);
    // Now run the test
    QVERIFY(storage.open());
    /*
     * We write 100 entries, and check if the automatic sync kicked in.
     * This can be verified by checking the number of marks on the db file.
     * Entries 0..9 weight 20 bytes, and the rest 21. That's about 2090
     * bytes. Since this is in manual sync mode, we should have no markers until
     * we call close or sync.
     */
    QByteArray value("012345678901234");
    for (int i = 0; i < 100; i++) {
        QKeyValueStoreTxn *writeTxn = storage.beginWrite();
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        QVERIFY(writeTxn->put(key, value));
        QVERIFY(writeTxn->commit(100));
    }
    QVERIFY(storage.close());
    // Now we open the file and inspect it.
    QFile db(m_journal);
    db.open(QIODevice::ReadOnly);
    QDataStream stream(&db);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 count = 0, found = 0;
    int numberOfMarkers = 0;
    quint32 m_marker = 0x55AAAA55;
    quint32 tag = 0;
    quint64 dataTimestamp = 0;
    stream >> count;
    while (!stream.atEnd()) {
        QByteArray key;
        stream >> key;
        quint8 operation = 0;
        stream >> operation;
        quint32 offsetToStart = 0;
        stream >> offsetToStart;
        QByteArray value;
        stream >> value;
        quint32 hash = 0xFFFFFFFF;
        stream >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream >> tag;
            quint32 marker = 0;
            stream >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream >> dataTimestamp;
                stream >> hash;
                numberOfMarkers++;
                stream >> count;
            } else
                count = marker;
            found = 0;
        }
    }
    // One marker after calling close
    QCOMPARE(numberOfMarkers, 1);
    // And now to inspect the btree
    QFile tree(m_tree);
    tree.open(QIODevice::ReadOnly);
    QDataStream treeStream(&tree);
    treeStream.setByteOrder(QDataStream::LittleEndian);
    quint64 treeTimestamp = 0;
    found = 0;
    treeStream >> count;
    QCOMPARE(count, (quint32)100);
    while (!treeStream.atEnd()) {
        QByteArray key;
        qint64 value;
        treeStream >> key;
        treeStream >> value;
        found++;
        if (count == found)
            break;
    }
    treeStream >> treeTimestamp;
    quint32 hash = 0xFFFFFFFF;
    treeStream >> hash;
    quint32 computedHash = qHash(treeTimestamp);
    QCOMPARE(hash, computedHash);
    QCOMPARE(treeTimestamp, dataTimestamp);

    removeTemporaryFiles();
}

/*
 * To test compaction we need to add elements and then
 * remove some of them. The compacted file should not have those elements.
 */
void QKeyValueStoreTest::compactionAfterRemove()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    // Set it on manual sync mode
    storage.setSyncThreshold(0);
    // Now run the test
    QVERIFY(storage.open());
    QByteArray value("012345678901234");
    for (int i = 0; i < 100; i++) {
        QKeyValueStoreTxn *writeTxn = storage.beginWrite();
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        QVERIFY(writeTxn->put(key, value));
        QVERIFY(writeTxn->commit(i));
    }
    // Let's remove all of them but one
    for (int i = 0; i < 99; i++) {
        QKeyValueStoreTxn *writeTxn = storage.beginWrite();
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        QVERIFY(writeTxn->remove(key));
        QVERIFY(writeTxn->commit(i));
    }
    QVERIFY(storage.sync());
    // Check that all elements are there.
    qint32 addOperations = 0, removeOperations = 0;
    QFile check1(m_dbName + ".dat");
    QVERIFY(check1.open(QIODevice::ReadOnly));
    QDataStream stream1(&check1);
    stream1.setByteOrder(QDataStream::LittleEndian);
    quint32 count = 0, found = 0;
    int numberOfMarkers = 0;
    quint32 m_marker = 0x55AAAA55;
    quint32 tag = 0;
    quint64 dataTimestamp = 0;
    stream1 >> count;
    while (!stream1.atEnd()) {
        QByteArray key;
        stream1 >> key;
        quint8 operation = 0;
        stream1 >> operation;
        // Add
        if (operation == QKeyValueStoreEntry::Add)
            addOperations++;
        else
            removeOperations++;
        quint32 offsetToStart = 0;
        stream1 >> offsetToStart;
        QByteArray value;
        stream1 >> value;
        quint32 hash = 0xFFFFFFFF;
        stream1 >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream1 >> tag;
            quint32 marker = 0;
            stream1 >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream1 >> dataTimestamp;
                stream1 >> hash;
                numberOfMarkers++;
                stream1 >> count;
            } else
                count = marker;
            found = 0;
        }
    }
    check1.close();
    QCOMPARE(addOperations, 100);
    QCOMPARE(removeOperations, 99);
    // Let's compact the file and check that only one element is there.
    QVERIFY(storage.compact());
    QFile check2(m_dbName + ".dat");
    QVERIFY(check2.open(QIODevice::ReadOnly));
    QDataStream stream2(&check2);
    stream2.setByteOrder(QDataStream::LittleEndian);
    addOperations = 0;
    removeOperations = 0;
    while (!stream2.atEnd()) {
        QByteArray key;
        stream2 >> key;
        quint8 operation = 0;
        stream2 >> operation;
        if (operation == QKeyValueStoreEntry::Add)
            addOperations++;
        else
            removeOperations++;
        quint32 offsetToStart = 0;
        stream2 >> offsetToStart;
        QByteArray value;
        stream2 >> value;
        quint32 hash = 0xFFFFFFFF;
        stream2 >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream2 >> tag;
            quint32 marker = 0;
            stream2 >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream2 >> dataTimestamp;
                stream2 >> hash;
                numberOfMarkers++;
                stream2 >> count;
            } else
                count = marker;
            found = 0;
        }
    }
    check2.close();
    QCOMPARE(addOperations, 1);
    QCOMPARE(removeOperations, 0);
    // Close the file
    QVERIFY(storage.close());
    removeTemporaryFiles();
}

/*
 * To test compaction we need to add elements and update them.
 * The compacted file should have only one version of each element.
 */
void QKeyValueStoreTest::compactionAfterPut()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    // Set it on manual sync mode
    storage.setSyncThreshold(0);
    // Now run the test
    QVERIFY(storage.open());
    QByteArray value("012345678901234");
    for (int i = 0; i < 100; i++) {
        QKeyValueStoreTxn *writeTxn = storage.beginWrite();
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QByteArray key("key-0");
        QVERIFY(writeTxn->put(key, value));
        QVERIFY(writeTxn->commit(i));
    }
    QVERIFY(storage.sync());
    // Check that all elements are there.
    qint32 addOperations = 0, removeOperations = 0;
    QFile check1(m_dbName + ".dat");
    QVERIFY(check1.open(QIODevice::ReadOnly));
    QDataStream stream1(&check1);
    stream1.setByteOrder(QDataStream::LittleEndian);
    quint32 count = 0, found = 0;
    int numberOfMarkers = 0;
    quint32 m_marker = 0x55AAAA55;
    quint32 tag = 0;
    quint64 dataTimestamp = 0;
    stream1 >> count;
    while (!stream1.atEnd()) {
        QByteArray key;
        stream1 >> key;
        quint8 operation = 0;
        stream1 >> operation;
        // Add
        if (operation == QKeyValueStoreEntry::Add)
            addOperations++;
        else
            removeOperations++;
        quint32 offsetToStart = 0;
        stream1 >> offsetToStart;
        QByteArray value;
        stream1 >> value;
        quint32 hash = 0xFFFFFFFF;
        stream1 >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream1 >> tag;
            quint32 marker = 0;
            stream1 >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream1 >> dataTimestamp;
                stream1 >> hash;
                numberOfMarkers++;
                stream1 >> count;
            } else
                count = marker;
            found = 0;
        }
    }
    check1.close();
    QCOMPARE(addOperations, 100);
    QCOMPARE(removeOperations, 0);
    // Let's compact the file and check that only one element is there.
    QVERIFY(storage.compact());
    QFile check2(m_dbName + ".dat");
    QVERIFY(check2.open(QIODevice::ReadOnly));
    QDataStream stream2(&check2);
    stream2.setByteOrder(QDataStream::LittleEndian);
    addOperations = 0;
    removeOperations = 0;
    while (!stream2.atEnd()) {
        QByteArray key;
        stream2 >> key;
        quint8 operation = 0;
        stream2 >> operation;
        if (operation == QKeyValueStoreEntry::Add)
            addOperations++;
        else
            removeOperations++;
        quint32 offsetToStart = 0;
        stream2 >> offsetToStart;
        QByteArray value;
        stream2 >> value;
        quint32 hash = 0xFFFFFFFF;
        stream2 >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream2 >> tag;
            quint32 marker = 0;
            stream2 >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream2 >> dataTimestamp;
                stream2 >> hash;
                numberOfMarkers++;
                stream2 >> count;
            } else
                count = marker;
            found = 0;
        }
    }
    check2.close();
    QCOMPARE(addOperations, 1);
    QCOMPARE(removeOperations, 0);
    // Close the file
    QVERIFY(storage.close());
    removeTemporaryFiles();
}

/*
 * To test this type of compaction we need to add elements until we trigger
 * a compaction round. There are two cases, after remove or after too many
 * updates.
 */
void QKeyValueStoreTest::compactionAutoTrigger()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    // Now run the test
    QVERIFY(storage.open());
    // Manual sync
    storage.setSyncThreshold(0);
    // Compact after 100 operations
    storage.setCompactThreshold(100);
    QByteArray value("012345678901234");
    QByteArray key("key-0");
    for (int i = 0; i < 100; i++) {
        QKeyValueStoreTxn *writeTxn = storage.beginWrite();
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QVERIFY(writeTxn->put(key, value));
        QVERIFY(writeTxn->commit(i));
    }
    // After sync there should be only one element
    QVERIFY(storage.sync());
    qint32 addOperations = 0, removeOperations = 0;
    QFile check1(m_dbName + ".dat");
    QVERIFY(check1.open(QIODevice::ReadOnly));
    QDataStream stream1(&check1);
    stream1.setByteOrder(QDataStream::LittleEndian);
    quint32 count = 0, found = 0;
    int numberOfMarkers = 0;
    quint32 m_marker = 0x55AAAA55;
    quint32 tag = 0;
    quint64 dataTimestamp = 0;
    stream1 >> count;
    while (!stream1.atEnd()) {
        QByteArray key;
        stream1 >> key;
        quint8 operation = 0;
        stream1 >> operation;
        // Add
        if (operation == QKeyValueStoreEntry::Add)
            addOperations++;
        else
            removeOperations++;
        quint32 offsetToStart = 0;
        stream1 >> offsetToStart;
        QByteArray value;
        stream1 >> value;
        quint32 hash = 0xFFFFFFFF;
        stream1 >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream1 >> tag;
            quint32 marker = 0;
            stream1 >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream1 >> dataTimestamp;
                stream1 >> hash;
                numberOfMarkers++;
                stream1 >> count;
            } else
                count = marker;
            found = 0;
        }
    }
    check1.close();
    // There should be only one element
    QCOMPARE(addOperations, 1);
    QCOMPARE(removeOperations, 0);

    removeTemporaryFiles();
}

/*
 * What we test here is if the file is usable after compaction.
 * First test is to write some more elements and then to close it
 * and open it.
 */
void QKeyValueStoreTest::compactionContinuation()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    // Manual sync
    storage.setSyncThreshold(0);
    // Compact after 100 operations
    storage.setCompactThreshold(100);
    // Now run the test
    QVERIFY(storage.open());
    {
        QByteArray value("012345678901234");
        QByteArray key("key-0");
        for (int i = 0; i < 100; i++) {
            QKeyValueStoreTxn *writeTxn = storage.beginWrite();
            QVERIFY2(writeTxn, "writeTxn is NULL");
            QVERIFY(writeTxn->put(key, value));
            QVERIFY(writeTxn->commit(i));
        }
    }
    // After sync there should be only one element
    QVERIFY(storage.sync());
    qint32 addOperations = 0, removeOperations = 0;
    QFile check1(m_dbName + ".dat");
    QVERIFY(check1.open(QIODevice::ReadOnly));
    QDataStream stream1(&check1);
    stream1.setByteOrder(QDataStream::LittleEndian);
    quint32 count = 0, found = 0;
    int numberOfMarkers = 0;
    quint32 m_marker = 0x55AAAA55;
    quint32 tag = 0;
    quint64 dataTimestamp = 0;
    stream1 >> count;
    while (!stream1.atEnd()) {
        QByteArray key;
        stream1 >> key;
        quint8 operation = 0;
        stream1 >> operation;
        // Add
        if (operation == QKeyValueStoreEntry::Add)
            addOperations++;
        else
            removeOperations++;
        quint32 offsetToStart = 0;
        stream1 >> offsetToStart;
        QByteArray value;
        stream1 >> value;
        quint32 hash = 0xFFFFFFFF;
        stream1 >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream1 >> tag;
            quint32 marker = 0;
            stream1 >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream1 >> dataTimestamp;
                stream1 >> hash;
                numberOfMarkers++;
                stream1 >> count;
            } else
                count = marker;
            found = 0;
        }
    }
    check1.close();
    // There should be only one element
    QCOMPARE(addOperations, 1);
    QCOMPARE(removeOperations, 0);
    QCOMPARE(storage.tag(), (quint32)99);
    // Now we add some more elements and see what happens
    {
        QByteArray value("012345678901234");
        QByteArray key("key-1");
        for (int i = 100; i < 200; i++) {
            QKeyValueStoreTxn *writeTxn = storage.beginWrite();
            QVERIFY2(writeTxn, "writeTxn is NULL");
            QVERIFY(writeTxn->put(key, value));
            QVERIFY(writeTxn->commit(i));
        }
    }
    QVERIFY(storage.sync());
    addOperations = 0;
    removeOperations = 0;
    QFile check2(m_dbName + ".dat");
    QVERIFY(check2.open(QIODevice::ReadOnly));
    QDataStream stream2(&check2);
    stream2.setByteOrder(QDataStream::LittleEndian);
    count = 0;
    found = 0;
    numberOfMarkers = 0;
    stream2 >> count;
    while (!stream2.atEnd()) {
        QByteArray key;
        stream2 >> key;
        quint8 operation = 0;
        stream2 >> operation;
        // Add
        if (operation == QKeyValueStoreEntry::Add)
            addOperations++;
        else
            removeOperations++;
        quint32 offsetToStart = 0;
        stream2 >> offsetToStart;
        QByteArray value;
        stream2 >> value;
        quint32 hash = 0xFFFFFFFF;
        stream2 >> hash;
        found++;
        if (count == found) {
            // Do we have a marker?
            stream2 >> tag;
            quint32 marker = 0;
            stream2 >> marker;
            if (marker == m_marker) {
                // Yes we do!
                stream2 >> dataTimestamp;
                stream2 >> hash;
                numberOfMarkers++;
                stream2 >> count;
            } else
                count = marker;
            found = 0;
        }
    }
    check2.close();
    // There should be only two elements
    QCOMPARE(addOperations, 2);
    QCOMPARE(removeOperations, 0);

    // Now close the file
    QVERIFY(storage.close());
    // And open it in a new element
    QKeyValueStore storage2(m_dbName);
    QVERIFY(storage2.open());
    QCOMPARE(storage2.tag(), (quint32)199);
    removeTemporaryFiles();
}

/*
 * We store values in the file.
 * We close it and then try to open it again.
 */
void QKeyValueStoreTest::fastOpen()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    QVERIFY(storage.open());

    // Disable the autosync
    storage.setSyncThreshold(0);

    QByteArray key1("key1");
    QByteArray value1("value1");

    // Create a write transaction
    QKeyValueStoreTxn *writeTxn1 = storage.beginWrite();
    QVERIFY2(writeTxn1, "Write txn1 is NULL");
    QVERIFY(writeTxn1->put(key1, value1));
    QVERIFY(writeTxn1->commit(100));
    QVERIFY(storage.sync());
    // At this point we have one marker.
    QByteArray key2("key2");
    QByteArray value2("value2");

    // Create a write transaction
    QKeyValueStoreTxn *writeTxn2 = storage.beginWrite();
    QVERIFY2(writeTxn2, "Write txn2 is NULL");
    QVERIFY(writeTxn2->put(key2, value2));
    QVERIFY(writeTxn2->commit(200));
    // Now we have two markers, so we close the storage.
    QVERIFY(storage.close());

    /*
     * We open the storage and count the markers. There should be two of them,
     * however since this will be a fast open, we will only find the last one.
     * That means there will be only one marker.
     */
    QKeyValueStore storage2(m_dbName);
    QVERIFY(storage2.open());
    // Now we count the markers
    QCOMPARE(storage2.markers(), 1);

    // Done, let's go.
    QVERIFY(storage2.close());
    removeTemporaryFiles();
}

/*
 * We write entries to the database and then close it.
 * We delete the btree file and open the database again.
 * The algorithm should rebuild the btree from the journal.
 */
void QKeyValueStoreTest::buildBTreeFromScratch()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    QVERIFY(storage.open());
    QByteArray value("012345678901234");
    for (int i = 0; i < 100; i++) {
        QKeyValueStoreTxn *writeTxn = storage.beginWrite();
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        QVERIFY(writeTxn->put(key, value));
        QVERIFY(writeTxn->commit(i));
    }
    // Now we close the file
    QVERIFY(storage.close());
    // Remove the file
    QFile::remove(m_tree);
    // Now create a new storage and see if it builds the tree.
    QKeyValueStore storage2(m_dbName);
    QVERIFY(storage2.open());
    QKeyValueStoreTxn *readTxn = storage2.beginRead();
    QVERIFY2(readTxn, "readTxn is NULL");
    QByteArray getKey("key-89");
    QByteArray getValue;
    QVERIFY(readTxn->get(getKey, &getValue));
    QCOMPARE(getValue, value);
    storage2.close();

    removeTemporaryFiles();
}

/*
 * We write entries to the database and then close it.
 * We copy the btree file and then add some more entries to the db.
 * We replace the new btree file with the old one and open the database again.
 * The algorithm should rebuild the btree from the journal.
 */
void QKeyValueStoreTest::buildBTreeFromKnownState()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    bool test = false;
    test = storage.open();
    QCOMPARE(test, true);
    QByteArray value("012345678901234");
    for (int i = 0; i < 100; i++) {
        QKeyValueStoreTxn *writeTxn = storage.beginWrite();
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        test = writeTxn->put(key, value);
        QCOMPARE(test, true);
        test = writeTxn->commit(100);
        QCOMPARE(test, true);
    }
    // At this time we only have two markers.
    QFile::copy(m_tree, m_treeCopy);
    // Now we close the file
    test = storage.close();
    // Remove the file
    QFile::remove(m_tree);
    QFile::rename(m_treeCopy, m_tree);

    // Now create a new storage and see if it builds the tree.
    QKeyValueStore storage2(m_dbName);
    test = storage2.open();
    QCOMPARE(test, true);
    QKeyValueStoreTxn *readTxn = storage2.beginRead();
    QVERIFY2(readTxn, "readTxn is NULL");
    QByteArray getKey("key-89");
    QByteArray getValue;
    test = readTxn->get(getKey, &getValue);
    QCOMPARE(test, true);
    QCOMPARE(getValue, value);
    storage2.close();
    removeTemporaryFiles();
}

/*
 * This is a very useless test, but everything here should work.
 */
void QKeyValueStoreTest::cursorSanityCheck()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    bool test = false;
    test = storage.open();
    QCOMPARE(test, true);
    QByteArray value("012345678901234");
    QKeyValueStoreTxn *writeTxn = storage.beginWrite();
    for (int i = 0; i < 100; i++) {
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        test = writeTxn->put(key, value);
        QCOMPARE(test, true);
        QCOMPARE(test, true);
    }
    // Now we have 100 items, let's use the cursor and iterate over the first and the last.
    QKeyValueStoreCursor *cursor = new QKeyValueStoreCursor(writeTxn);
    // At this point the cursor should be pointing to the first item
    QByteArray firstElementKey("key-0");
    QByteArray lastElementKey("key-99");
    QByteArray recoveredKey;
    QByteArray recoveredValue;
    QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
    QCOMPARE(recoveredKey, firstElementKey);
    QCOMPARE(recoveredValue, value);
    // Move to the last element
    QVERIFY(cursor->last());
    QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
    QCOMPARE(recoveredKey, lastElementKey);
    QCOMPARE(recoveredValue, value);
    removeTemporaryFiles();
}

/*
 * This is a funny test, ascending order means ascending in a string kind of way.
 * Therefore the keys are ordered as follows:
 * key-0
 * key-1
 * key-10
 * key-11
 * ...
 * key-2
 * key-20
 * ...
 * key-98
 * key-99
 */
void QKeyValueStoreTest::cursorAscendingOrder()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    bool test = false;
    test = storage.open();
    QCOMPARE(test, true);
    QByteArray value("012345678901234");
    QKeyValueStoreTxn *writeTxn = storage.beginWrite();
    for (int i = 0; i < 100; i++) {
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        test = writeTxn->put(key, value);
        QCOMPARE(test, true);
    }
    // Now we have 100 items, let's use the cursor and iterate over them
    QKeyValueStoreCursor *cursor = new QKeyValueStoreCursor(writeTxn);
    // At this point the cursor should be pointing to the first item
    for (int i = 0; i < 10; i++) {
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        QByteArray recoveredKey;
        QByteArray recoveredValue;
        QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
        QCOMPARE(recoveredKey, key);
        QCOMPARE(recoveredValue, value);
        QVERIFY(cursor->next());
        if (0 == i)
            continue;
        for (int j = 0; j < 10; j++, cursor->next()) {
            QString baseKey2(baseKey);
            QString number2;
            number2.setNum(j);
            baseKey2.append(number2);
            key = baseKey2.toAscii();
            QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
            QCOMPARE(recoveredKey, key);
            QCOMPARE(recoveredValue, value);
        }
    }
    removeTemporaryFiles();
}

void QKeyValueStoreTest::cursorSeek()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    QVERIFY(storage.open());
    QByteArray value("012345678901234");
    QKeyValueStoreTxn *writeTxn = storage.beginWrite();
    for (int i = 0; i < 100; i++) {
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        QVERIFY(writeTxn->put(key, value));
    }
    // Now we have 100 items, let's use the cursor and iterate over them.
    QKeyValueStoreCursor *cursor = new QKeyValueStoreCursor(writeTxn);
    // At this point the cursor should be pointing to the first item
    // First let's try seeking a known element
    QByteArray key55("key-55");
    QByteArray recoveredKey;
    QByteArray recoveredValue;
    QVERIFY(cursor->seek(key55));
    QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
    QCOMPARE(recoveredKey, key55);
    QCOMPARE(recoveredValue, value);
    // Now let's try an unknown element
    QByteArray key101("key-101");
    QVERIFY2(!cursor->seek(key101), "We have an unknown element on the array (101)");
    // And let's finish with a new known element
    QByteArray key10("key-10");
    QVERIFY(cursor->seek(key10));
    QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
    QCOMPARE(recoveredKey, key10);
    QCOMPARE(recoveredValue, value);
    // Done
    removeTemporaryFiles();
}

/*
 * seek and seekRange are similar. The difference is on the "not found"
 * reply. While seeks returns false, seekRange can return an element.
 */
void QKeyValueStoreTest::cursorSeekRange()
{
    removeTemporaryFiles();
    QKeyValueStore storage(m_dbName);
    QVERIFY(storage.open());
    QByteArray value("012345678901234");
    QKeyValueStoreTxn *writeTxn = storage.beginWrite();
    for (int i = 0; i < 100; i++) {
        QVERIFY2(writeTxn, "writeTxn is NULL");
        QString number;
        number.setNum(i);
        QString baseKey("key-");
        baseKey.append(number);
        QByteArray key = baseKey.toAscii();
        QVERIFY(writeTxn->put(key, value));
    }
    // Now we have 100 items, let's use the cursor and iterate over them.
    QKeyValueStoreCursor *cursor = new QKeyValueStoreCursor(writeTxn);
    // At this point the cursor should be pointing to the first item
    // First let's try seeking a known element
    QByteArray key55("key-55");
    QByteArray recoveredKey;
    QByteArray recoveredValue;
    QVERIFY(cursor->seekRange(key55));
    QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
    QCOMPARE(recoveredKey, key55);
    QCOMPARE(recoveredValue, value);
    // Now let's try an unknown element
    QByteArray key999("key-999");
    QVERIFY2(!cursor->seekRange(key999), "We have an unknown element on the array (999)");
    // And let's finish with a new known element
    QByteArray key10("key-10");
    QVERIFY(cursor->seekRange(key10));
    QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
    QCOMPARE(recoveredKey, key10);
    QCOMPARE(recoveredValue, value);
    // Now we try elements that are not found but have a greater than
    QByteArray key101("key-101");
    QByteArray key101greater("key-11");
    QVERIFY(cursor->seekRange(key101));
    QVERIFY(cursor->current(&recoveredKey, &recoveredValue));
    QCOMPARE(recoveredKey, key101greater);
    QCOMPARE(recoveredValue, value);
    // Done
    removeTemporaryFiles();
}

/*
 * As all the other sanity checks this is pretty useless, although it
 * checks that the API returns the correct values.
 */
void QKeyValueStoreTest::fileSanityCheck()
{
    removeTemporaryFiles();
    QKeyValueStoreFile file("db.dat");
    QVERIFY(file.open());
    QCOMPARE(file.size(), (qint64)0);
    QVERIFY(file.close());
    removeTemporaryFiles();
}

/*
 * Create a file and write something to it, then check the file size.
 */
void QKeyValueStoreTest::fileWrite()
{
    removeTemporaryFiles();
    QKeyValueStoreFile file("db.dat");
    QVERIFY(file.open());
    QCOMPARE(file.size(), (qint64)0);
    QCOMPARE(file.write(testString, testStringSize), (qint32)testStringSize);
    QCOMPARE(file.size(), (qint64)testStringSize);
    QVERIFY(file.close());
    removeTemporaryFiles();
}

/*
 * Create a file and write something to it, then check the file size.
 * Once that is done, read the content back and check it is the same.
 */
void QKeyValueStoreTest::fileRead()
{
    removeTemporaryFiles();
    QKeyValueStoreFile file("db.dat");
    QVERIFY(file.open());
    QCOMPARE(file.size(), (qint64)0);
    QCOMPARE(file.write(testString, testStringSize), (qint32)testStringSize);
    QCOMPARE(file.size(), (qint64)testStringSize);
    // Now read the content back
    char buffer[15];
    file.setOffset(0);
    QCOMPARE(file.read(buffer, testStringSize), (qint32)testStringSize);
    QCOMPARE(strncmp(buffer, testString, testStringSize), 0);
    QVERIFY(file.close());
    removeTemporaryFiles();
}

/*
 * Create a file and write some data into it. Store the offset and then
 * write some more data into it. Read the new data using the stored offset.
 * Check the total file size.
 */
void QKeyValueStoreTest::fileOffset()
{
    removeTemporaryFiles();
    QKeyValueStoreFile file("db.dat");
    QVERIFY(file.open());
    QCOMPARE(file.size(), (qint64)0);
    QCOMPARE(file.write(testString, testStringSize), (qint32)testStringSize);
    QCOMPARE(file.size(), (qint64)testStringSize);
    qint64 offset = file.size();
    // Write some more data into the file
    QCOMPARE(file.write(testString, testStringSize), (qint32)testStringSize);
    QCOMPARE(file.size(), (qint64)(2*testStringSize));
    // Now read the data from the stored offset.
    char buffer[15];
    file.setOffset(offset);
    QCOMPARE(file.read(buffer, testStringSize), (qint32)testStringSize);
    QCOMPARE(strncmp(buffer, testString, testStringSize), 0);
    offset = file.size();
    // Write some more data into the file
    QCOMPARE(file.write(testString, testStringSize), (qint32)testStringSize);
    QCOMPARE(file.size(), (qint64)(3*testStringSize));
    // Now read the data from the stored offset.
    file.setOffset(offset);
    QCOMPARE(file.read(buffer, testStringSize), (qint32)testStringSize);
    QCOMPARE(strncmp(buffer, testString, testStringSize), 0);
    QVERIFY(file.close());
    removeTemporaryFiles();
}

/*
 * Create a new file and write some data to it. Without closing the file,
 * open a new instance of it and start reading it. It might or not return the
 * contents. On the first file call sync. Now read on the second file, the content
 * should be visible.
 */
void QKeyValueStoreTest::fileSync()
{
    removeTemporaryFiles();
    QKeyValueStoreFile file("db.dat");
    QKeyValueStoreFile file2("db.dat");
    QVERIFY(file.open());
    QCOMPARE(file.size(), (qint64)0);
    QCOMPARE(file.write(testString, testStringSize), (qint32)testStringSize);
    QCOMPARE(file.size(), (qint64)testStringSize);
    // Open the file a second time and try to read from it.
    QVERIFY(file2.open());
    char buffer[15];
    file.setOffset(0);
    QCOMPARE(file2.read(buffer, testStringSize), (qint32)testStringSize);
    QCOMPARE(file2.size(), (qint64)testStringSize);
    QVERIFY(file2.close());
    // Now sync the file
    file.sync();
    // Open the other file and check the content
    QVERIFY(file2.open());
    QCOMPARE(file2.size(), (qint64)testStringSize);
    QVERIFY(file2.close());
    QVERIFY(file.close());
    removeTemporaryFiles();
}

/*
 * Create a new file and write some data to it checking the file size.
 * Open the file again with the truncate flag on and check the file size.
 */
void QKeyValueStoreTest::fileTruncate()
{
    removeTemporaryFiles();
    QKeyValueStoreFile file("db.dat");
    QVERIFY(file.open());
    QCOMPARE(file.size(), (qint64)0);
    QCOMPARE(file.write(testString, testStringSize), (qint32)testStringSize);
    QCOMPARE(file.size(), (qint64)testStringSize);
    QVERIFY(file.close());
    QKeyValueStoreFile file2("db.dat", true);
    QVERIFY(file2.open());
    QCOMPARE(file2.size(), (qint64)0);
    QVERIFY(file2.close());
    removeTemporaryFiles();
}

#if 0
#endif
QTEST_APPLESS_MAIN(QKeyValueStoreTest)

#include "tst_qkeyvaluestore.moc"
