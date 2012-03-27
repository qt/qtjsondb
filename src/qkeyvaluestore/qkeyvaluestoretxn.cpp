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

#include "qkeyvaluestoretxn.h"
#include "qkeyvaluestoreentry.h"

#include <QDataStream>
#include <QHash>
#include <QCryptographicHash>
#include <QDebug>

QKeyValueStoreTxn::QKeyValueStoreTxn(QKeyValueStoreTxnType type)
{
    bool readOnly = true;
    switch (type) {
    case ReadOnly:
        break;
    case ReadWrite:
        readOnly = false;
    default:
        break;
    }
    p = new QKeyValueStoreTxnPrivate(readOnly);
}

QKeyValueStoreTxn::~QKeyValueStoreTxn()
{
    // Given that we might have a temporary write transaction we need to make
    // sure that we don't leave any dangling pointers.
    if (p->m_store->m_writeTransaction == this) {
        if (p && p->m_store && p->m_store->m_writeTransaction)
            p->m_store->reportReadyWrite();
    }
    delete p;
}

void QKeyValueStoreTxn::setStore(QKeyValueStorePrivate *store)
{
    p->m_store = store;
#if defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
    p->m_offsets = p->m_store->m_offsets;
#endif
}

bool QKeyValueStoreTxn::get(const QByteArray &key, QByteArray *value)
{
    return p->get(key, value);
}

/*
 * The transaction object is no longer valid after this.
 */
bool QKeyValueStoreTxn::commit(quint32 tag)
{
    bool result = p->commit(tag);
    delete this;
    return result;
}

/*
 * The transaction object is no longer valid after this.
 */
bool QKeyValueStoreTxn::abort()
{
    bool result = p->abort();
    delete this;
    return result;
}

bool QKeyValueStoreTxn::put(const QByteArray &key, const QByteArray &value)
{
    return p->put(key, value);
}

bool QKeyValueStoreTxn::remove(const QByteArray &key)
{
    return p->remove(key);
}

/*
 * PRIVATE  API BELOW
 */

#if defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
#define TXN_OFFSETS m_offsets
#else
#define TXN_OFFSETS m_store->m_offsets
#endif
QKeyValueStoreTxnPrivate::QKeyValueStoreTxnPrivate(bool readonly)
{
    m_readonly = readonly;
    m_incommingBytes = 0;
    m_tag = 0;
#if !defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
    m_localModifications = false;
#endif
}

QKeyValueStoreTxnPrivate::~QKeyValueStoreTxnPrivate()
{
}

bool QKeyValueStoreTxnPrivate::put(const QByteArray &key, const QByteArray &value)
{
    Q_ASSERT(!m_readonly);
    m_incommingBytes += (key.size() + value.size());
    QKeyValueStoreEntry * entry = new QKeyValueStoreEntry();
    entry->setKey(key);
    entry->setOperation(QKeyValueStoreEntry::Add);
    entry->setValue(value);
    m_incomming[key] = entry;
#if !defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
    m_localModifications = true;
#endif
    TXN_OFFSETS[key] = 0;

    return true;
}

bool QKeyValueStoreTxnPrivate::remove(const QByteArray &key)
{
    // First of all, is the key stored on disk?
    if (TXN_OFFSETS.contains(key)) {
        // Record the operation and remove it.
        m_incommingBytes += key.size();
        QKeyValueStoreEntry *entry = new QKeyValueStoreEntry();
        entry->setKey(key);
        entry->setOperation(QKeyValueStoreEntry::Remove);
        m_incomming[key] = entry;
#if !defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
        m_localModifications = true;
#endif
        TXN_OFFSETS.remove(key);
        return true;
    }
    // Is the key on the incomming queue?
    if (m_incomming.contains(key)) {
        m_incomming.remove(key);
        // Since we add "phantom" elements, we need to delete those too!
#if !defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
        m_localModifications = true;
#endif
        TXN_OFFSETS.remove(key);
        return true;
    }
    return false;
}

bool QKeyValueStoreTxnPrivate::get(const QByteArray &key, QByteArray *value)
{
    int readBytes = 0;
    qint64 offset = -1;
    if (!value) {
        return false;
    }
    // Is it in the buffer?
    if (m_incomming.contains(key)) {
        QKeyValueStoreEntry *entry = m_incomming.value(key);
        if (entry->operation() == QKeyValueStoreEntry::Add) {
            *value = entry->value();
            return true;
        }
        // If the last operation was a remove operation we need to signal that by saying not found
        return false;
    }
    // Is it on the file?
    if (!TXN_OFFSETS.contains(key)) {
        return false;
    }
    offset = TXN_OFFSETS.value(key);
    if (offset == 0) {
        // This is an impossible situation, if offset == 0 then it should be in the incomming queue.
        return false;
    }
    m_store->m_file->setOffset(offset);
    quint32 rawRecoveredKeySize = 0, rawValueSize = 0;
    char *rawRecoveredKey = 0, *rawValue = 0;
    readBytes = m_store->m_file->read((void *)&rawRecoveredKeySize, sizeof(quint32));
    if (readBytes < 0)
        return false;
    if ((quint32)readBytes < sizeof(quint32))
        return false;
    QByteArray recoveredKey;
    if ((quint32)recoveredKey.capacity() < rawRecoveredKeySize) {
        recoveredKey.resize(rawRecoveredKeySize);
    }
    rawRecoveredKey = recoveredKey.data();
    readBytes = m_store->m_file->read((void *)rawRecoveredKey, rawRecoveredKeySize);
    if (readBytes < 0)
        return false;
    if ((quint32)readBytes < rawRecoveredKeySize)
        return false;
    quint8 operation = 0;
    readBytes = m_store->m_file->read((void *)&operation, sizeof(quint8));
    if (readBytes < 0)
        return false;
    if ((quint32)readBytes < sizeof(quint8))
        return false;
    readBytes = m_store->m_file->read((void *)&rawValueSize, sizeof(quint32));
    if (readBytes < 0)
        return false;
    if ((quint32)readBytes < sizeof(quint32))
        return false;
    if ((quint32)value->capacity() < rawValueSize) {
        value->resize(rawValueSize);
    }
    rawValue = value->data();
    readBytes = m_store->m_file->read((void *)rawValue, rawValueSize);
    if (readBytes < 0)
        return false;
    if ((quint32)readBytes < rawValueSize)
        return false;
    quint32 hash = 0;
    readBytes = m_store->m_file->read((void *)&hash, sizeof(quint32));
    if (readBytes < 0)
        return false;
    if ((quint32)readBytes < sizeof(quint32))
        return false;
    // Do we have the right data?
    if (recoveredKey != key) {
        return false;
    }
    // Check that the data is valid
    if (hash != qHash(recoveredKey + *value)) {
        return false;
    }
    return true;
}

bool QKeyValueStoreTxnPrivate::commit(quint32 tag)
{
    if (m_readonly)
        return false;
    if (m_incomming.isEmpty())
        return true;
    // First the number of objects
    quint32 count = m_incomming.count();
    m_store->m_file->write((void *)&count, sizeof(quint32));
    foreach (QKeyValueStoreEntry *entry, m_incomming) {
        QByteArray key = entry->key();
        quint32 keySize = key.size();
        QByteArray value = entry->value();
        quint32 valueSize = value.size();
        quint8 operation = entry->operation();
        quint32 hash = entry->hash();
        char *valueData = value.data();
        entry->setOffset(m_store->m_file->size());
        m_store->m_file->write((void *)&keySize, sizeof(quint32));
        m_store->m_file->write((void *)key.constData(), keySize);
        m_store->m_file->write((void *)&operation, sizeof(quint8));
        m_store->m_file->write((void *)&valueSize, sizeof(quint32));
        m_store->m_file->write((void *)valueData, valueSize);
        m_store->m_file->write((void *)&hash, sizeof(quint32));
        m_store->addEntry(entry);
        // Adjust the counters
        m_incommingBytes -= (keySize + valueSize);
    }
    m_tag = tag;
    m_store->m_file->write((void *)&tag, sizeof(quint32));
    qDeleteAll(m_incomming);
    m_incomming.clear();
    m_store->m_currentTag = m_tag;
    m_store->reportReadyWrite();
    return true;
}

bool QKeyValueStoreTxnPrivate::abort()
{
    if (m_incomming.isEmpty())
        return true;
    qDeleteAll(m_incomming);
    m_incomming.clear();
    m_incommingBytes = 0;
#if !defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
    // Remove phantom elements from the offsets
    if (m_localModifications) {
        QMap<QByteArray, qint64>::iterator i = TXN_OFFSETS.begin();
        while (i != TXN_OFFSETS.end()) {
            if (i.value() == (qint64)0)
                i = TXN_OFFSETS.erase(i);
            else
                ++i;
        }
    } // No modifications, just go on!
#endif
    if (m_readonly)
        m_store->reportReadyRead();
    else
        m_store->reportReadyWrite();
    return true;
}
