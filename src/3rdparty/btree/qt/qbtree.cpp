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

#include "qbtree.h"
#include "btree.h"

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

bool QBtree::setCmpFunc(QBtree::CmpFunc cmp)
{
    if (mBtree)
        btree_set_cmp(mBtree, (bt_cmp_func)cmp);
    mCmp = cmp;
    return true;
}

bool QBtree::open(const QString &filename, QBtree::DbFlags flags)
{
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

    mBtree = btree_open(mFilename.toLocal8Bit().constData(), btflags, 0644);
    if (!mBtree) {
        qDebug() << "QBtree::open" << "failed" << errno;
        return false;
    }
    if (mCacheSize)
        btree_set_cache_size(mBtree, mCacheSize);
    btree_set_cmp(mBtree, mCmp);

    return true;
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

quint64 QBtree::count() const
{
    Q_ASSERT(mBtree);
    const struct btree_stat *stat = btree_stat(mBtree);
    return stat->entries;
}

bool QBtreeTxn::get(const QByteArray &baKey, QByteArray *baValue) const
{
    Q_ASSERT(baValue);
    struct btval btkey;
    btkey.data = (void *)baKey.constData();
    btkey.size = baKey.size();
    btkey.free_data = 0;
    btkey.mp = 0;

    struct btval btvalue;
    if (btree_txn_get(mDb->mBtree, mTxn, &btkey, &btvalue) != BT_SUCCESS)
        return false;

    baValue->resize(btvalue.size);
    memcpy(baValue->data(), btvalue.data, btvalue.size);
    btval_reset(&btvalue);
    return true;
}

bool QBtreeTxn::get(const QBtreeData &key, QBtreeData *value) const
{
    return get(key.constData(), key.size(), value);
}

bool QBtreeTxn::get(const char *key, int keySize, QBtreeData *value) const
{
    Q_ASSERT(value);
    struct btval btkey;
    btkey.data = (void *)key;
    btkey.size = keySize;
    btkey.free_data = 0;
    btkey.mp = 0;

    struct btval btvalue;
    int ok = btree_txn_get(mDb->mBtree, mTxn, &btkey, &btvalue);
    if (ok != BT_SUCCESS)
        return false;
    *value = QBtreeData(&btvalue);
    return true;
}

bool QBtreeTxn::put(const QByteArray &baKey, const QByteArray &baValue)
{
    return put(baKey.constData(), baKey.size(), baValue.constData(), baValue.size());
}

bool QBtreeTxn::put(const QBtreeData &key, const QBtreeData &value)
{
    return put(key.constData(), key.size(), value.constData(), value.size());
}

bool QBtreeTxn::put(const char *key, int keySize, const char *value, int valueSize)
{
    struct btval btkey;
    btkey.data = (void *)key;
    btkey.size = keySize;
    btkey.free_data = 0;
    btkey.mp = 0;
    struct btval btvalue;
    btvalue.data = (void *)value;
    btvalue.size = valueSize;
    btvalue.free_data = 0;
    btvalue.mp = 0;

    int ok = btree_txn_put(mDb->mBtree, mTxn, &btkey, &btvalue, 0);
    if (ok != BT_SUCCESS)
        qDebug() << "btree_txn_put" << ok << errno << endl << mDb->fileName();
    return ok == BT_SUCCESS;
}

bool QBtreeTxn::remove(const QByteArray &baKey)
{
    return remove(baKey.constData(), baKey.size());
}

bool QBtreeTxn::remove(const QBtreeData &key)
{
    return remove(key.constData(), key.size());
}

bool QBtreeTxn::remove(const char *key, int keySize)
{
    struct btval btkey;
    btkey.data = (void *)key;
    btkey.size = keySize;
    btkey.free_data = 0;
    btkey.mp = 0;
    if (btree_txn_del(mDb->mBtree, mTxn, &btkey, 0) != BT_SUCCESS)
        qDebug() << "db->del" << errno << QByteArray(key, keySize).toHex() << endl << mDb->fileName();
    return true;
}

void QBtree::sync()
{
    if (mBtree)
        btree_sync(mBtree);
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

QBtreeTxn::QBtreeTxn(QBtree *db, btree_txn *txn)
    : mDb(db), mTxn(txn)
{
    db->mTxns.append(this);
}

QBtreeTxn::~QBtreeTxn()
{
    mDb->mTxns.removeOne(this);
    if (mTxn) {
        qCritical() << "QBtreeTxn: transaction in progress aborted.";
        btree_txn_abort(mTxn);
        mTxn = 0;
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


bool QBtreeTxn::commit(quint32 tag)
{
    bool ret = false;
    if (mTxn) {
        unsigned int flags = (mDb->mAutoSyncRate && mDb->mCommitCount % mDb->mAutoSyncRate == 0) ? BT_FORCE_MARKER : 0;
        ret = btree_txn_commit(mTxn, tag, flags) == BT_SUCCESS;
        mTxn = 0;
    } else {
        qCritical() << "QBtreeTxn::commit()" << "no txn" << mDb->fileName();
    }
    bool needCompact = ret && mDb->mAutoCompactRate && mDb->mCommitCount++ > mDb->mAutoCompactRate;
    QBtree *db = mDb;

    mDb->mTxns.removeOne(this);
    delete this;

    if (needCompact) {
        db->compact();
        db->mCommitCount = 0;
    }
    return ret;
}

bool QBtreeTxn::abort()
{
    bool ret = false;
    if (mTxn) {
        btree_txn_abort(mTxn);
        mTxn = 0;
        ret = true;
    } else {
        qCritical() << "QBtree::abort()" << "no txn";
    }
    mDb->mTxns.removeOne(this);
    delete this;
    return ret;
}

quint32 QBtreeTxn::tag() const
{
    return btree_txn_get_tag(mTxn);
}

quint32 QBtree::tag() const
{
    Q_ASSERT(mBtree);
    const struct btree_stat *stat = btree_stat(mBtree);
    return stat->tag;
}

bool QBtree::rollback()
{
    Q_ASSERT(mTxns.isEmpty());
    Q_ASSERT(mBtree);
    return btree_rollback(mBtree) == BT_SUCCESS;
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

QBtreeCursor::QBtreeCursor()
    : mTxn(0), mBtreeCursor(0)
{
}

QBtreeCursor::QBtreeCursor(QBtreeTxn *txn)
    : mTxn(txn), mBtreeCursor(0)
{
    if (mTxn)
        mBtreeCursor = btree_txn_cursor_open(mTxn->mDb->mBtree, mTxn->mTxn);
}

QBtreeCursor::~QBtreeCursor()
{
    if (mBtreeCursor)
        btree_cursor_close(mBtreeCursor);
}

QBtreeCursor::QBtreeCursor(const QBtreeCursor &other)
    : mTxn(other.mTxn), mBtreeCursor(0), mKey(other.mKey), mValue(other.mValue)
{
    if (mTxn) {
        mBtreeCursor = btree_txn_cursor_open(mTxn->mDb->mBtree, mTxn->mTxn);
        // go to the same position as the other cursor
        if (!mKey.isNull())
            seek(mKey);
    }
}

QBtreeCursor &QBtreeCursor::operator=(const QBtreeCursor &other)
{
    if (this != &other) {
        if (mBtreeCursor)
            btree_cursor_close(mBtreeCursor);
        mTxn = other.mTxn;
        mKey = other.mKey;
        mValue = other.mValue;
        if (mTxn) {
            mBtreeCursor = btree_txn_cursor_open(other.mTxn->mDb->mBtree, other.mTxn->mTxn);
            // go to the same position as the other cursor
            if (!mKey.isNull())
                seek(mKey);
        }
    }
    return *this;
}

bool QBtreeCursor::first()
{
    struct btval btkey, btvalue;
    btkey.data = btvalue.data = 0;
    btkey.size = btvalue.size = 0;
    btkey.free_data = btvalue.free_data = 0;
    btkey.mp = btvalue.mp = 0;
    if (btree_cursor_get(mBtreeCursor, &btkey, &btvalue, BT_FIRST) != BT_SUCCESS) {
        btval_reset(&btkey);
        btval_reset(&btvalue);
        mKey = mValue = QBtreeData();
        return false;
    }
    mKey = QBtreeData(&btkey);
    mValue = QBtreeData(&btvalue);
    return true;
}

bool QBtreeCursor::current(QBtreeData *key, QBtreeData *value) const
{
    if (key)
        *key = mKey;
    if (value)
        *value = mValue;
    return true;
}

bool QBtreeCursor::current(QByteArray *key, QByteArray *value) const
{
    if (key)
        *key = mKey.toByteArray();
    if (value)
        *value = mValue.toByteArray();
    return true;
}

bool QBtreeCursor::last()
{
    struct btval btkey, btvalue;
    btkey.data = btvalue.data = 0;
    btkey.size = btvalue.size = 0;
    btkey.free_data = btvalue.free_data = 0;
    btkey.mp = btvalue.mp = 0;
    if (btree_cursor_get(mBtreeCursor, &btkey, &btvalue, BT_LAST) != BT_SUCCESS) {
        btval_reset(&btkey);
        btval_reset(&btvalue);
        mKey = mValue = QBtreeData();
        return false;
    }
    mKey = QBtreeData(&btkey);
    mValue = QBtreeData(&btvalue);
    return true;
}

bool QBtreeCursor::next()
{
    struct btval btkey, btvalue;
    btkey.data = btvalue.data = 0;
    btkey.size = btvalue.size = 0;
    btkey.free_data = btvalue.free_data = 0;
    btkey.mp = btvalue.mp = 0;
    if (btree_cursor_get(mBtreeCursor, &btkey, &btvalue, BT_NEXT) != BT_SUCCESS) {
        btval_reset(&btkey);
        btval_reset(&btvalue);
        mKey = mValue = QBtreeData();
        return false;
    }
    mKey = QBtreeData(&btkey);
    mValue = QBtreeData(&btvalue);
    return true;
}

bool QBtreeCursor::prev()
{
    struct btval btkey, btvalue;
    btkey.data = btvalue.data = 0;
    btkey.size = btvalue.size = 0;
    btkey.free_data = btvalue.free_data = 0;
    btkey.mp = btvalue.mp = 0;
    if (btree_cursor_get(mBtreeCursor, &btkey, &btvalue, BT_PREV) != BT_SUCCESS) {
        btval_reset(&btkey);
        btval_reset(&btvalue);
        mKey = mValue = QBtreeData();
        return false;
    }
    mKey = QBtreeData(&btkey);
    mValue = QBtreeData(&btvalue);
    return true;
}

bool QBtreeCursor::seek(const char *key, int size, bool exact)
{
    struct btval btkey;
    btkey.data = (void *)key;
    btkey.size = size;
    btkey.free_data = 0;
    btkey.mp = 0;

    struct btval btvalue;
    btvalue.data = 0;
    btvalue.size = 0;
    btvalue.free_data = 0;
    btvalue.mp = 0;

    if (btree_cursor_get(mBtreeCursor, &btkey, &btvalue, exact ? BT_CURSOR_EXACT : BT_CURSOR) != BT_SUCCESS) {
        btval_reset(&btkey);
        btval_reset(&btvalue);
        mKey = mValue = QBtreeData();
        return false;
    }
    mKey = QBtreeData(&btkey);
    mValue = QBtreeData(&btvalue);
    return true;
}

bool QBtreeCursor::seek(const QByteArray &baKey)
{
    return seek(baKey.constData(), baKey.size(), true);
}

bool QBtreeCursor::seek(const QBtreeData &key)
{
    return seek(key.constData(), key.size(), true);
}

bool QBtreeCursor::seekRange(const QByteArray &baKey)
{
    return seek(baKey.constData(), baKey.size(), false);
}

bool QBtreeCursor::seekRange(const QBtreeData &key)
{
    return seek(key.constData(), key.size(), false);
}
