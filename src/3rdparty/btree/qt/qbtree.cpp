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

#include <QDebug>

#include "btree.h"
#include "qbtreetxn.h"

#include "qbtree.h"

#include <errno.h>
#include <string.h>

QBtree::QBtree()
    : mBtree(0), mCmp(0), mCacheSize(0), mFlags(0),
      mWrites(0), mReads(0), mHits(0),
      mCommitCount(0), mAutoCompactRate(0)
{
}

QBtree::QBtree(const QString &filename)
    : mFilename(filename), mBtree(0), mCmp(0), mCacheSize(0), mFlags(0),
      mWrites(0), mReads(0), mHits(0),
      mCommitCount(0), mAutoCompactRate(0)
{
}

QBtree::~QBtree()
{
    close();
}

bool QBtree::open()
{
    Q_ASSERT(!mFilename.isEmpty());
    close();
    mBtree = btree_open(mFilename.toLocal8Bit().constData(), mFlags, 0644);
    if (!mBtree) {
        qDebug() << "QBtree::reopen" << "failed" << errno;
        return false;
    }
    btree_set_cache_size(mBtree, mCacheSize);
    btree_set_cmp(mBtree, (bt_cmp_func)mCmp);
    return true;
}

void QBtree::close()
{
    if (mBtree) {
        btree_close(mBtree);
        mBtree = 0;
    }
}

QBtreeTxn *QBtree::begin(QBtree::TxnFlag flag)
{
    Q_ASSERT(mBtree);
    if (!mBtree) {
        qCritical() << "QBtree::begin" << "no tree" << mFilename;
        return 0;
    }

    btree_txn *txn = btree_txn_begin(mBtree, flag == QBtree::TxnReadOnly ? 1 : 0);
    if (!txn)
        return 0;
    return new QBtreeTxn(this, txn);
}

QBtreeTxn *QBtree::beginRead(quint32 tag)
{
    Q_ASSERT(mBtree);
    if (!mBtree) {
        qCritical() << "QBtree::begin" << "no tree" << mFilename;
        return 0;
    }

    btree_txn *txn = btree_txn_begin_with_tag(mBtree, tag);
    if (!txn)
        return 0;

    return new QBtreeTxn(this, txn);
}

bool QBtree::rollback()
{
    Q_ASSERT(mBtree);
    Q_ASSERT(!btree_get_txn(mBtree));
    return btree_rollback(mBtree) == BT_SUCCESS;
}

bool QBtree::compact()
{
    Q_ASSERT(mBtree);

    if (btree_get_flags(mBtree) & BT_RDONLY)
        return false;

    const struct btree_stat *stat = btree_stat(mBtree);
    mWrites += stat->writes;
    mReads += stat->reads;
    mHits += stat->hits;

    if (btree_compact(mBtree) != BT_SUCCESS)
        return false;

    if (!open())
        return false;

    mCommitCount = 0;
    return true;
}

bool QBtree::sync()
{
    if (mBtree)
        return btree_sync(mBtree) == BT_SUCCESS;
    return false;
}

void QBtree::setCmpFunc(QBtree::CmpFunc cmp)
{
    mCmp = cmp;
    if (mBtree)
        btree_set_cmp(mBtree, (bt_cmp_func)cmp);
}

void QBtree::setFileName(const QString &filename)
{
    mFilename = filename;
}

void QBtree::setFlags(DbFlags flags)
{
    int btflags = 0;
    if (flags & QBtree::ReverseKeys)
        btflags |= BT_REVERSEKEY;
    if (flags & QBtree::NoSync)
        btflags |= BT_NOSYNC;
    if (flags & QBtree::ReadOnly)
        btflags |= BT_RDONLY;
    if (flags & QBtree::UseSyncMarker)
        btflags |= BT_USEMARKER;
    if (flags & QBtree::NoPageChecksums)
        btflags |= BT_NOPGCHECKSUM;

    mFlags = btflags;
}

quint64 QBtree::count() const
{
    Q_ASSERT(mBtree);
    const struct btree_stat *stat = btree_stat(mBtree);
    return stat->entries;
}

void QBtree::setAutoCompactRate(int rate)
{
    mAutoCompactRate = rate;
}

quint32 QBtree::tag() const
{
    Q_ASSERT(mBtree);
    const struct btree_stat *stat = btree_stat(mBtree);
    return stat->tag;
}

QBtree::Stat QBtree::stat() const
{
    if (!mBtree)
        return QBtree::Stat();

    const struct btree_stat *stat = btree_stat(mBtree);
    QBtree::Stat result;
    result.reads = stat->reads + mReads;
    result.hits = stat->hits + mHits;
    result.writes = stat->writes + mWrites;
    result.ksize = stat->ksize;
    result.psize = stat->psize;
    return result;
}

void QBtree::setCacheSize(unsigned int cacheSize)
{
    mCacheSize = cacheSize;
    if (mBtree)
        btree_set_cache_size(mBtree, cacheSize);
}

void QBtree::dump() const
{
    btree_dump(mBtree);
}

bool QBtree::commit(QBtreeTxn *txn, quint32 tag)
{
    Q_ASSERT(txn);
    Q_ASSERT(txn->isReadWrite());
    Q_ASSERT(txn->handle());

    if (!btree_txn_commit(txn->handle(), tag, 0) == BT_SUCCESS)
        return false;

    delete txn;

    mCommitCount++;
    if (mAutoCompactRate && mCommitCount > mAutoCompactRate)
        compact();

    return true;
}

void QBtree::abort(QBtreeTxn *txn)
{
    Q_ASSERT(txn);
    Q_ASSERT(txn->handle());
    btree_txn_abort(txn->handle());
    delete txn;
}

