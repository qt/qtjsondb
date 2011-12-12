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

#include <QDebug>

#include "btree.h"
#include "qbtreetxn.h"

#include "qbtree.h"

#include <errno.h>
#include <string.h>

QBtree::QBtree()
    : mBtree(0), mCmp(0), mCacheSize(0), mCommitCount(0),
      mAutoCompactRate(0), mAutoSyncRate(0)
{
}

QBtree::~QBtree()
{
    close();
}

bool QBtree::open(const QString &filename, QBtree::DbFlags flags)
{
    close();

    mFilename = filename;

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

    mBtree = btree_open(mFilename.toLocal8Bit().constData(), btflags, 0644);
    if (!mBtree) {
        qDebug() << "QBtree::open" << "failed" << mFilename << errno;
        return false;
    }
    if (mCacheSize)
        btree_set_cache_size(mBtree, mCacheSize);
    btree_set_cmp(mBtree, mCmp);

    return true;
}

void QBtree::close()
{
    if (!mTxns.isEmpty()) {
        qCritical() << "QBtree closed with active transactions";
        mTxns.clear();
    }

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
    Q_ASSERT(mTxns.isEmpty());
    Q_ASSERT(mBtree);
    return btree_rollback(mBtree) == BT_SUCCESS;
}

bool QBtree::compact()
{
    Q_ASSERT(mBtree);

    if (btree_get_flags(mBtree) & BT_RDONLY)
        return false;

    if (btree_compact(mBtree) != BT_SUCCESS)
        return false;

    if (!reopen())
        return false;

    return true;
}

void QBtree::sync()
{
    if (mBtree)
        btree_sync(mBtree);
}

bool QBtree::setCmpFunc(QBtree::CmpFunc cmp)
{
    if (mBtree)
        btree_set_cmp(mBtree, (bt_cmp_func)cmp);
    mCmp = cmp;
    return true;
}

quint64 QBtree::count() const
{
    Q_ASSERT(mBtree);
    const struct btree_stat *stat = btree_stat(mBtree);
    return stat->entries;
}

int QBtree::autoSyncRate() const
{
    return mAutoSyncRate;
}

void QBtree::setAutoSyncRate(int rate)
{
    mAutoSyncRate = rate;
}

int QBtree::autoCompactRate() const
{
    return mAutoCompactRate;
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

const struct btree_stat *QBtree::stat() const
{
    Q_ASSERT(mBtree);
    return btree_stat(mBtree);
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

bool QBtree::reopen()
{
    Q_ASSERT(mBtree);
    unsigned int flags = btree_get_flags(mBtree);
    btree_close(mBtree);
    mBtree = btree_open(mFilename.toLocal8Bit().constData(), flags, 0644);
    if (!mBtree) {
        qDebug() << "QBtree::reopen" << "failed" << errno;
        return false;
    }
    if (mCacheSize)
        btree_set_cache_size(mBtree, mCacheSize);
    btree_set_cmp(mBtree, mCmp);
    return true;
}

