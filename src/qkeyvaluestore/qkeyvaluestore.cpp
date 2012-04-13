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

#include "qkeyvaluestore.h"
#include "qkeyvaluestore_p.h"
#include "qkeyvaluestoreentry.h"
#include "qkeyvaluestoretxn.h"

#include <QDataStream>
#include <QDateTime>
#include <QCryptographicHash>

#include <QDebug>

#include <unistd.h>

QKeyValueStore::QKeyValueStore() :
    p(0)
{
}

QKeyValueStore::QKeyValueStore(const QString &name)
{
    p = new QKeyValueStorePrivate(this, name);
}

bool QKeyValueStore::open()
{
    if (!p)
        return false;
    return p->open() ? false : true;
}

bool QKeyValueStore::close()
{
    if (!p)
        return false;
    return p->close() ? false : true;
}

void QKeyValueStore::setFileName(QString name)
{
    if (p)
        return;
    p = new QKeyValueStorePrivate(this, name);
}

QKeyValueStoreTxn *QKeyValueStore::beginRead()
{
    Q_ASSERT(p);
    return p->beginRead();
}

QKeyValueStoreTxn *QKeyValueStore::beginWrite()
{
    Q_ASSERT(p);
    return p->beginWrite();
}

bool QKeyValueStore::sync()
{
    Q_ASSERT(p);
    return p->sync();
}

void QKeyValueStore::setSyncThreshold(quint32 threshold)
{
    p->m_syncThreshold = threshold;
    if (0 == threshold)
        p->m_autoSync = false;
    else
        p->m_autoSync = true;
}

QKeyValueStoreTxn *QKeyValueStore::writeTransaction() const
{
    Q_ASSERT(p);
    return p->writeTransaction();
}

/*
 * PRIVATE  API BELOW
 */

QKeyValueStorePrivate::QKeyValueStorePrivate(QKeyValueStore *parent, const QString &name) :
    m_parent(parent),
    m_name(name),
    m_writeTransaction(0),
    m_currentTag(0),
    // By default we resort to automatic syncing. Set threshold to zero to disable this.
    m_autoSync(true),
    m_runCompaction(false),
    m_needCompaction(false),
    // To prevent compaction from happening
    // Same threshold as AOB
    m_compactThreshold(1000),
    m_compactableOperations(0),
    m_state(Closed),
    m_pendingBytes(0),
    m_syncThreshold(1024),
    m_marker(0x55AAAA55),
    m_ongoingReadTransactions(0),
    m_ongoingWriteTransactions(0)
{
    m_file = new QKeyValueStoreFile(m_name + ".dat");
}

QKeyValueStorePrivate::~QKeyValueStorePrivate()
{
    syncToDisk();
    delete m_file;
}

int QKeyValueStorePrivate::open()
{
    bool result = false;
    Q_ASSERT(m_file);
    // Let's open the data file.
    result = m_file->open();
    if (!result) {
        m_state = Error;
        return -1;
    }
    // Small optimization, if the file size is 0 then just return
    if ((qint64)0 == m_file->size()) {
        m_state = Ready;
        return 0;
    }
    // Let's check the journal file contents.
    quint64 dataTimestamp = checkFileContents();
    // If the journal check returns 0, then we need to rebuild everything.
    if (0 == dataTimestamp) {
        rebuildBTree();
    } else {
        // We need to examine the btree file and see if we have a valid btree.
        quint64 btreeTimestamp = checkTreeContents();
        /*
         * 0 is not a valid timestamp... well it is if the journal was started
         * exactly on the epoch but that is very unlikely.
         */
        if (dataTimestamp != btreeTimestamp) {
            // Well, we need to rebuild the btree.
            rebuildBTree();
        }
    }
    m_state = Ready;
    return 0;
}

int QKeyValueStorePrivate::close()
{
    if (m_state == Error)
        return -1;
    syncToDisk();
    m_file->close();
    m_state = Closed;
    return 0;
}

bool QKeyValueStorePrivate::sync()
{
    syncToDisk();
    return true;
}

/*
 * This is an expensive operation, however it can reduce the file size
 * considerably.
 * We traverse the tree and copy only those elements that are needed,
 * discarding the history.
 */
bool QKeyValueStorePrivate::compact()
{
    // Are the any ongoing write transactions going on?
    // If so, abort and do it later.
    if (m_writeTransaction) {
        m_runCompaction = true;
        return true;
    }
    quint32 numberOfElements = m_offsets.count();
    // We walk the tree and copy elements to the new file.
    QKeyValueStoreFile *file = new QKeyValueStoreFile(m_name + ".dat.cpt", true);
    if (!file->open())
        return false;
    if (file->write(&numberOfElements, sizeof(quint32)) != sizeof(quint32))
        return false;
    QMap<QByteArray, qint64>::iterator i;
    for (i = m_offsets.begin(); i != m_offsets.end(); ++i) {
        // Retrieve the current data
        int readBytes = 0, writeBytes = 0;
        qint64 offset = i.value();
        // We need to read the offset to the start of the record too
        quint32 offsetToStart = 0;
        m_file->setOffset(offset - sizeof(quint32));
        readBytes = m_file->read((void *)&offsetToStart, sizeof(quint32));
        if (readBytes < 0)
            return false;
        if ((quint32)readBytes < sizeof(quint32))
            return false;
        m_file->rewind(offsetToStart);
        quint32 rawRecoveredKeySize = 0, rawValueSize = 0;
        char *rawRecoveredKey = 0, *rawValue = 0;
        // Read the key
        readBytes = m_file->read((void *)&rawRecoveredKeySize, sizeof(quint32));
        if (readBytes < 0)
            return false;
        if ((quint32)readBytes < sizeof(quint32))
            return false;
        QByteArray recoveredKey;
        recoveredKey.resize(rawRecoveredKeySize);
        rawRecoveredKey = recoveredKey.data();
        readBytes = m_file->read((void *)rawRecoveredKey, rawRecoveredKeySize);
        if (readBytes < 0)
            return false;
        if ((quint32)readBytes < rawRecoveredKeySize)
            return false;
        // Read the operation
        quint8 operation = 0;
        readBytes = m_file->read((void *)&operation, sizeof(quint8));
        if (readBytes < 0)
            return false;
        if ((quint32)readBytes < sizeof(quint8))
            return false;
        // Hope over the offset
        m_file->fastForward(sizeof(quint32));
        // Read the value
        readBytes = m_file->read((void *)&rawValueSize, sizeof(quint32));
        if (readBytes < 0)
            return false;
        if ((quint32)readBytes < sizeof(quint32))
            return false;
        QByteArray value;
        value.resize(rawValueSize);
        rawValue = value.data();
        readBytes = m_file->read((void *)rawValue, rawValueSize);
        if (readBytes < 0)
            return false;
        if ((quint32)readBytes < rawValueSize)
            return false;
        // Read  the hash
        quint32 hash = 0;
        readBytes = m_file->read((void *)&hash, sizeof(quint32));
        if (readBytes < 0)
            return false;
        if ((quint32)readBytes < sizeof(quint32))
            return false;
        // First fix the offset
        i.value() = file->size() + offsetToStart;
        // Now write the key
        writeBytes = file->write(&rawRecoveredKeySize, sizeof(quint32));
        if (writeBytes < 0)
            return false;
        writeBytes = file->write(rawRecoveredKey, rawRecoveredKeySize);
        if (writeBytes < 0)
            return false;
        // Write the operation
        writeBytes = file->write(&operation, sizeof(quint8));
        if (writeBytes < 0)
            return false;
        // Write the offset to start
        writeBytes = file->write(&offsetToStart, sizeof(quint32));
        if (writeBytes < 0)
            return false;
        // Write the value
        writeBytes = file->write(&rawValueSize, sizeof(quint32));
        if (writeBytes < 0)
            return false;
        writeBytes = file->write(rawValue, rawValueSize);
        if (writeBytes < 0)
            return false;
        // Write the hash
        writeBytes = file->write(&hash, sizeof(quint32));
        if (writeBytes < 0)
            return false;
    }
    // Write the last tag to close the file and mark preserve the state
    int bytes = file->write(&m_currentTag, sizeof(quint32));
    if (bytes < 0)
        return false;
    file->close();
    m_file->close();
    QFile::copy(m_name + ".dat", m_name + ".old");
    QFile::remove(m_name + ".dat");
    QFile::copy(m_name + ".dat.cpt", m_name + ".dat");
    QFile::remove(m_name + ".dat.cpt");
    QFile::remove(m_name + ".old");
    delete m_file;
    delete file;
    m_file = new QKeyValueStoreFile(m_name + ".dat", false);
    m_runCompaction = false;
    m_compactableOperations = 0;
    m_state = DataToSync;
    return m_file->open();
}

QKeyValueStoreTxn *QKeyValueStorePrivate::beginRead()
{
    QKeyValueStoreTxn *txn = new QKeyValueStoreTxn(QKeyValueStoreTxn::ReadOnly);
    Q_ASSERT(txn);
    txn->setStore(this);
    txn->setBTree(m_parent);
    m_ongoingReadTransactions++;
    return txn;
}

QKeyValueStoreTxn *QKeyValueStorePrivate::beginWrite()
{
    if (m_ongoingWriteTransactions)
        return (QKeyValueStoreTxn *)0;
    QKeyValueStoreTxn *txn = new QKeyValueStoreTxn(QKeyValueStoreTxn::ReadWrite);
    Q_ASSERT(txn);
    txn->setStore(this);
    txn->setBTree(m_parent);
    m_writeTransaction = txn;
    m_ongoingWriteTransactions++;
    return txn;
}

bool QKeyValueStorePrivate::isWriting() const
{
    return m_ongoingWriteTransactions ? true : false;
}

QKeyValueStoreTxn *QKeyValueStorePrivate::writeTransaction()
{
    return m_writeTransaction;
}

void QKeyValueStorePrivate::addEntry(QKeyValueStoreEntry *entry)
{
    if (!entry)
        return;
    QByteArray key = entry->key();
    if (entry->operation() == QKeyValueStoreEntry::Remove) {
        m_offsets.remove(key);
        m_pendingBytes += key.size();
        m_compactableOperations++;
    } else {
        if (m_offsets.contains(key))
            m_compactableOperations++;
        m_offsets[key] = entry->offset();
        m_pendingBytes += (entry->value().size() + key.size());
    }
    m_state = DataToSync;
}

void QKeyValueStorePrivate::reportReadyWrite()
{
    // Do we need to sync to disk?
    if (m_autoSync)
        if (m_syncThreshold <= m_pendingBytes)
            syncToDisk();
    m_ongoingWriteTransactions--;
    m_writeTransaction = 0;
    if (m_runCompaction)
        compact();
}

void QKeyValueStorePrivate::reportReadyRead()
{
    // Not much to do, just fix the counters...
    m_ongoingReadTransactions--;
}

void QKeyValueStorePrivate::syncToDisk()
{
    if (m_compactableOperations >= m_compactThreshold)
        compact();
    if (m_state != DataToSync)
        return;
    // Write the marker
    m_file->write((void *)&m_marker, sizeof(quint32));
    // We write the timestamp + hash
    quint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    m_file->write((void *)&timestamp, sizeof(quint64));
    quint32 hash = qHash(timestamp);
    m_file->write((void *)&hash, sizeof(quint32));
    // Now we sync
    m_file->sync();
    // Sync the btree
    syncTree(timestamp);
    // Reset the counter
    m_pendingBytes = 0;
    m_state = Ready;
}

void QKeyValueStorePrivate::syncTree(quint64 timestamp)
{
    QKeyValueStoreFile btree(m_name + ".btr", true);
    if (!btree.open())
        return;
    quint32 count = m_offsets.count();
    btree.write((void *)&count, sizeof(quint32));
    QMapIterator<QByteArray, qint64> i(m_offsets);
    while (i.hasNext()) {
        i.next();
        QByteArray key = i.key();
        int keySize = key.size();
        qint64 value = i.value();
        char *keyData = key.data();
        btree.write((void *)&keySize, sizeof(int));
        btree.write(keyData, (quint32)keySize);
        btree.write((void *)&value, sizeof(qint64));
    }
    // Now the timestamp
    btree.write((void *)&timestamp, sizeof(quint64));
    quint32 hash = qHash(timestamp);
    btree.write((void *)&hash, sizeof(quint32));
}

/*
 * The situation goes like this: we don't know how many timestamps we have,
 * we might not have timestamps at all. We skipped the reading process when
 * opening the journal to speed up things. That means that we need to read
 * things here and find out the timestamps. At the same time we need to rebuild
 * the tree using the journal entries.
 * For that reason we resort to read the journal from scratch and rebuild the
 * tree from scratch.
 */
int QKeyValueStorePrivate::rebuildBTree()
{
    if (!m_offsets.isEmpty())
        m_offsets.clear();
    if (!m_dataMarks.isEmpty())
        m_dataMarks.clear();
    m_file->setOffset((qint64)0);
    quint32 count = 0, found = 0;
    m_file->read((void *)&count, sizeof(quint32));
    while (found < count) {
        char *rawKey = 0, *rawValue = 0;
        quint32 rawKeySize = 0, rawValueSize = 0;
        quint32 offsetToStart = 0;
        quint8 operation = 0;
        quint32 hash = 0xFFFFFFFF;
        qint64 offset = 0;
        offset = m_file->offset();
        m_file->read((void *)&rawKeySize, sizeof(quint32));
        QByteArray key;
        key.resize(rawKeySize);
        rawKey = key.data();
        m_file->read((void *)rawKey, rawKeySize);
        m_file->read((void *)&operation, sizeof(quint8));
        m_file->read((void *)&offsetToStart, sizeof(quint32));
        m_file->read((void *)&rawValueSize, sizeof(quint32));
        QByteArray value;
        value.resize(rawValueSize);
        rawValue = value.data();
        m_file->read((void *)rawValue, rawValueSize);
        m_file->read((void *)&hash, sizeof(quint32));
        // Calculate the hash
        quint32 computedHash = 0xFFFFFFFF;
        switch (operation) {
        case QKeyValueStoreEntry::Add:
            computedHash = qHash(key + value);
            m_offsets[key] = offset + offsetToStart;
            break;
        case QKeyValueStoreEntry::Remove:
            computedHash = qHash(key);
            m_offsets.remove(key);
            break;
        default:
            break;
        }
        /*
         * The question here is what to do if the entry hash does not match?
         * Marking the journal as invalid is an option, but then we lose
         * all the entries. A simple option is to ignore the entry with
         * a bad hash, but in that case we might run into troubles afterwards
         * because there might be dangling entries.
         * On second thoughts, the whole purpose of having a per entry hash
         * is to be able to detect which entries are problematic, so we will
         * just ignore the entries with a bad hash.
         */
        if (computedHash != hash)
            m_offsets.remove(key);
        found++;
        if (count == found) {
            quint32 tag = 0;
            m_file->read((void *)&tag, sizeof(quint32));
            // Do we have a marker?
            quint32 marker = 0;
            quint64 fileTimestamp = 0;
            m_file->read((void *)&marker, sizeof(quint32));
            if (marker == m_marker) {
                // Yes we do!
                m_file->read((void *)&fileTimestamp, sizeof(quint64));
                m_file->read((void *)&hash, sizeof(quint32));
                // Is it a valid hash?
                if (hash == qHash(fileTimestamp)) {
                    /*
                     * There are two options here, to accept the previous
                     * entries as valid or to remove them.
                     * We already know that each entry is a valid one, so
                     * the only real problem is the fact that the marker is
                     * invalid. Ignoring the entries will lose all the
                     * information we have recovered. Accepting them means
                     * we accept that the sync failed but the data is ok.
                     * For now we will accept the entries but do not record
                     * the marker. This way we just keep going and if we find
                     * a new valid marker we just use that one instead.
                     */
                    m_dataMarks[fileTimestamp] = m_file->offset();
                }
                /*
                 * The tag is written at commit time, while the marker is
                 * written at sync time. That is why we include the tag
                 * regardless of the hash check.
                 */
                m_currentTag = tag;
                m_file->read((void *)&count, sizeof(quint32));
            } else
                count = marker;
            found = 0;
        }
    }
    return 0;
}

/*
 * In this method we check if the last thing written to it was a marker.
 * If so, we just read the last timestamp and go on from there.
 * If not we return 0 to indicate that we need to replay the journal and
 * rebuild the tree.
 */
quint64 QKeyValueStorePrivate::checkFileContents()
{
    qint64 size = m_file->size();
    int result = 0;
    if (size == 0)
        return 0;
    // tag - marker - timestamp - hash
    qint64 offset = size - (sizeof(quint32) + sizeof(quint32) + sizeof(quint64) + sizeof(quint32));
    m_file->setOffset(offset);
    quint32 tempTag = 0, tempMarker = 0;
    result = m_file->read((void *)&tempTag, sizeof(quint32));
    if (result <= 0)
        return 0;
    result = m_file->read((void *)&tempMarker, sizeof(quint32));
    if (result <= 0)
        return 0;
    if (tempMarker == m_marker) {
        quint64 tempTimestamp = 0;
        quint32 tempHash = 0;
        result = m_file->read((void *)&tempTimestamp, sizeof(quint64));
        if (result <= 0)
            return 0;
        result = m_file->read((void *)&tempHash, sizeof(quint32));
        if (result <= 0)
            return 0;
        if (tempHash == qHash(tempTimestamp)) {
            m_dataMarks[tempTimestamp] = offset;
            m_currentTag = tempTag;
            return tempTimestamp;
        }
    }
    /*
     * If the file was not closed correctly, we should abort the read operation.
     * We return 0 to signal that we need to rebuild the tree, since there will
     * be some operations missing.
     * While rebuilding the tree we can start from a known point and check the
     * new entries.
     */
    return 0;
}

/*
 * This method goes through the btree file and checks its contents.
 * There is only one btree version at any given time, so we try to build
 * it and if there are problems with the file we just bail out.
 */
quint64 QKeyValueStorePrivate::checkTreeContents()
{
    QString name = m_name + ".btr";
    QKeyValueStoreFile btree(name);
    if (!btree.open()) {
        return 0;
    }
    if (btree.size() == 0) {
        return 0;
    }
    quint32 count = 0, found = 0;
    quint64 timestamp = 0;
    int result = 0;
    result = btree.read((void *)&count, sizeof(quint32));
    if (result <=0)
        return 0;
    while (found < count) {
        qint64 offset = 0;
        // The key is already in SHA1 format
        char *rawKey = 0;
        quint32 rawKeySize;
        result = btree.read((void *)&rawKeySize, sizeof(quint32));
        // <= 0 means either EOF or problems, therefore we just abort
        if (result <= 0) {
            if (!m_offsets.isEmpty())
                m_offsets.clear();
            return 0;
        }
        QByteArray key;
        key.resize(rawKeySize);
        rawKey = key.data();
        result = btree.read((void *)rawKey, rawKeySize);
        if (result <= 0) {
            if (!m_offsets.isEmpty())
                m_offsets.clear();
            return 0;
        }
        result = btree.read((void *)&offset, sizeof(qint64));
        if (result <= 0) {
            if (!m_offsets.isEmpty())
                m_offsets.clear();
            return 0;
        }
        m_offsets[key] = offset;
        found++;
        if (count == found) {
            // Check the timestamp
            result = btree.read((void *)&timestamp, sizeof(quint64));
            if (result <= 0) {
                if (!m_offsets.isEmpty())
                    m_offsets.clear();
                return 0;
            }
            quint32 hash = 0;
            result = btree.read((void *)&hash, sizeof(quint32));
            if (result <= 0) {
                if (!m_offsets.isEmpty())
                    m_offsets.clear();
                return 0;
            }
            if (hash != qHash(timestamp))
                timestamp = 0;
            break;
        }
    }
    return timestamp;
}
