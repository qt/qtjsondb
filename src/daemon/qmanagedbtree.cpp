/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the FOO module of the Qt Toolkit.
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

#include <QDebug>
#include <QFile>
#include <errno.h>
#include "btree.h"
#include "qbtree.h"
#include "qbtreetxn.h"
#include "qmanagedbtree.h"
#include "qmanagedbtreetxn.h"

// Every gCompactRate commits, compact the btree. Do not compact if zero.
int gCompactRate = qgetenv("JSONDB_COMPACT_RATE").size() ? ::atoi(qgetenv("JSONDB_COMPACT_RATE")) : 1000;
// Every gSyncRate commits, fsync the btree. Do not fsync if zero.
int gSyncRate = qgetenv("JSONDB_SYNC_RATE").size() ? ::atoi(qgetenv("JSONDB_SYNC_RATE")) : 100;

QManagedBtree::QManagedBtree()
    : mBtree(new QBtree())
{
    mWriter.txn = 0;
    mBtree->setAutoCompactRate(gCompactRate);
    mBtree->setAutoCompactRate(gSyncRate);
}

QManagedBtree::~QManagedBtree()
{
    close();
    delete mBtree;
}

bool QManagedBtree::open(const QString &filename, QBtree::DbFlags flags)
{
    mBtree->setFileName(filename);
    mBtree->setFlags(flags);
    return mBtree->open();
}

void QManagedBtree::close()
{
    Q_ASSERT(mBtree);
    if (mWriter.clients.size()) {
        qWarning() << "~QManagedBtree::close" << "write txn still in progress. Aborting.";
        mWriter.txn->abort();
    }

    if (mReaders.size()) {
        qWarning() << "~QManagedBtree::close" << "read txns still in progress. Aborting.";
        foreach(RefedTxn rtxn, mReaders)
            rtxn.txn->abort();
    }

    mReaders.clear();
    mWriter.txn = 0;
    mWriter.clients.clear();
    mBtree->close();;
}

QManagedBtreeTxn QManagedBtree::beginRead(quint32 tag)
{
    Q_ASSERT(mBtree);

    RefedTxnMap::iterator it = mReaders.find(tag);
    if (it != mReaders.end())
        return QManagedBtreeTxn(this, it.value().txn);

    QBtreeTxn *btxn = mBtree->beginRead(tag);

    if (!btxn)
        return QManagedBtreeTxn();

    RefedTxn rtxn;
    rtxn.txn = btxn;
    rtxn.clients = QSet<QManagedBtreeTxn *>();
    mReaders.insert(tag, rtxn);

    return QManagedBtreeTxn(this, btxn);
}

QManagedBtreeTxn QManagedBtree::beginRead()
{
    Q_ASSERT(mBtree);
    quint32 tag = mBtree->tag();
    return beginRead(tag);
}

QManagedBtreeTxn QManagedBtree::beginWrite()
{
    Q_ASSERT(mBtree);

    if (mWriter.txn) {
        qWarning() << "QManagedBtree::beginWrite:" << "write still in progress. Retrieving exiting writer";
        return existingWriteTxn();
    }

    QBtreeTxn *btxn = mBtree->beginReadWrite();

    if (!btxn)
        return QManagedBtreeTxn();

    Q_ASSERT(mWriter.txn == 0);
    Q_ASSERT(mWriter.clients.empty());
    mWriter.txn = btxn;
    mWriter.clients.clear();

    return QManagedBtreeTxn(this, btxn);
}

bool QManagedBtree::isReadTxnActive(quint32 tag) const
{
    Q_ASSERT(mBtree);
    return mReaders.find(tag) != mReaders.end();
}

bool QManagedBtree::isReadTxnActive() const
{
    Q_ASSERT(mBtree);
    return isReadTxnActive(mBtree->tag());
}

QManagedBtreeTxn QManagedBtree::existingWriteTxn()
{
    if (!mWriter.txn)
        return QManagedBtreeTxn();
    return QManagedBtreeTxn(this, mWriter.txn);
}

bool QManagedBtree::commit(QManagedBtreeTxn *txn, quint32 tag)
{
    // Only commit on writes
    Q_ASSERT(txn);
    Q_ASSERT(txn->txn()->isReadWrite());
    QBtreeTxn *btxn = mWriter.txn;
    mWriter.txn = 0;
    bool ok = btxn->commit(tag);
    remove(txn);
    return ok;
}

void QManagedBtree::abort(QManagedBtreeTxn *txn)
{
    Q_ASSERT(txn);
    QBtreeTxn *btxn = 0;
    if (txn->isReadOnly()) {
        RefedTxnMap::iterator it = mReaders.find(txn->tag());
        Q_ASSERT(it != mReaders.end());
        if (it.value().clients.size() > 1) // more readers present, just remove this one
            return remove(txn);
        btxn = it.value().txn;
        it.value().txn = 0;
    } else {
        Q_ASSERT(txn->mTxn == mWriter.txn);
        btxn = mWriter.txn;
        mWriter.txn = 0;
    }

    btxn->abort();
    remove(txn);
}

void QManagedBtree::remove(QManagedBtreeTxn *txn)
{
    // Internal btree txn should be either commited or aborted at this point
    // If it's not then it's just QManagedBtreeTxn's dtor being a tease.
    if (!txn->isReadOnly()) {
        if (mWriter.txn) { // commit/abort not called
            if (mWriter.clients.size() > 1) {
                mWriter.clients.remove(txn);
                txn->mTxn = 0;
                txn->mBtree = 0;
                return;
            }
            if (mWriter.clients.size() == 1) {
                qWarning() << "QManagedBtree::remove:" << "single write txn uncommited. Aborting.";
                mWriter.txn->abort();
            }
        }
        Q_ASSERT(!btree_get_txn(mBtree->handle()));
        foreach(QManagedBtreeTxn *client, mWriter.clients) { // tell clients
            client->mTxn = 0;
            client->mBtree = 0;
        }
        mWriter.txn = 0;
        mWriter.clients.clear();
    } else {
        RefedTxnMap::iterator it = mReaders.find(txn->tag());
        Q_ASSERT(it != mReaders.end());
        txn->mTxn = 0;
        txn->mBtree = 0;
        it.value().clients.remove(txn);
        if (it.value().clients.size() == 0) {
            if (it.value().txn) { // abort hasn't been called if this is not null
                it.value().txn->abort();
            }
            mReaders.erase(it);
        }
    }
}

void QManagedBtree::add(QManagedBtreeTxn *txn)
{
    Q_ASSERT(txn && txn->txn());
    if (txn->txn()->isReadWrite()) {
        Q_ASSERT(mWriter.txn);
        Q_ASSERT(mWriter.txn == txn->txn());
        mWriter.clients.insert(txn);
    } else {
        RefedTxnMap::iterator it = mReaders.find(txn->txn()->tag());
        Q_ASSERT(it != mReaders.end());
        Q_ASSERT(it.value().txn == txn->txn());
        it.value().clients.insert(txn);
    }
}

bool QManagedBtree::putOne(const QByteArray &key, const QByteArray &value)
{
    bool inTransaction = isWriteTxnActive();
    QManagedBtreeTxn txn = inTransaction ? existingWriteTxn() : beginWrite();
    bool ok = txn.put(key, value);
    if (!inTransaction) {
        qWarning() << "QManagedBtree::putOne" << "auto commiting tag 0";
        ok &= txn.commit(0);
    }
    return ok;
}

bool QManagedBtree::getOne(const QByteArray &key, QByteArray *value)
{
    bool inTransaction = isWriteTxnActive();
    QManagedBtreeTxn txn = inTransaction ? existingWriteTxn() : beginRead();
    bool ok = txn.get(key, value);
    if (!inTransaction)
        txn.abort();
    return ok;
}

bool QManagedBtree::removeOne(const QByteArray &key)
{
    bool inTransaction = isWriteTxnActive();
    QManagedBtreeTxn txn = inTransaction ? existingWriteTxn() : beginWrite();
    bool ok = txn.remove(key);
    if (!inTransaction){
        qWarning() << "QManagedBtree::removeOne" << "auto commiting tag 0";
        ok &= txn.commit(0);
    }
    return ok;
}

QString QManagedBtree::errorMessage() const
{
    return QString("QManagedBtree: %1, %2").arg(mBtree->fileName(), strerror(errno));
}

bool QManagedBtree::clearData()
{
    Q_ASSERT(numActiveReadTxns() == 0 && isWriteTxnActive() == false);
    close();
    QFile::remove(mBtree->fileName());
    return mBtree->open();
}
