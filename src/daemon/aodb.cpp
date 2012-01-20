/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QFileInfo>
#include <QStringList>
#include <QDebug>

#include "aodb.h"
#include "btree.h"

#include <errno.h>
#include <string.h>

// Every gCompactRate commits, compact the btree. Do not compact if zero.
int gCompactRate = qgetenv("JSONDB_COMPACT_RATE").size() ? ::atoi(qgetenv("JSONDB_COMPACT_RATE")) : 1000;
// Every gSyncRate commits, fsync the btree. Do not fsync if zero.
int gSyncRate = qgetenv("JSONDB_SYNC_RATE").size() ? ::atoi(qgetenv("JSONDB_SYNC_RATE")) : 100;

// number of pages to keep in cache
int gCacheSize = qgetenv("BENCHMARK_MANY").size() ? ::atoi(qgetenv("BENCHMARK_MANY")) : 32;

AoDb::AoDb()
    : mBtreeFlags(0), mBtree(0), mTxn(0), mCmp(0), mCommitCount(0), mCacheSize(gCacheSize)
{
}

AoDb::~AoDb()
{
    close();
}

bool AoDb::clearData()
{
    CmpFunc cmp = mCmp;
    int cache = mCacheSize;
    if (!drop())
        return false;
    setCmpFunc(cmp);
    setCacheSize(cache);
    if (!reopen())
        return false;
    return true;
}

bool AoDb::drop()
{
    close();
    int rc = unlink(mFilename.toLocal8Bit().constData());
    if (rc)
        qWarning() << "AoDb::drop" << "unlink failed" << errno << mFilename;
    mCmp = 0;
    mCacheSize = gCacheSize;
    return (rc == 0);
}

bool AoDb::compact()
{
    Q_ASSERT(mBtree);
    if (!mBtree)
        return false;

    unsigned int flags = btree_get_flags(mBtree);

    if (flags & BT_RDONLY)
        return false;

    if (btree_compact(mBtree) != BT_SUCCESS)
        return false;

    if (!reopen())
        return false;

    return true;
}

QString AoDb::errorMessage() const
{
    return QString("AoDb: %1: %2").arg(mFilename, strerror(errno));
}

bool AoDb::setCmpFunc(AoDb::CmpFunc cmp)
{
    if (mBtree)
        btree_set_cmp(mBtree, (bt_cmp_func)cmp);
    mCmp = cmp;
    return true;
}

bool AoDb::open(const QString &filename, AoDb::DbFlags flags)
{
    QFileInfo fi(filename);
    if (fi.suffix().isEmpty())
        fi.setFile(filename + ".db");
    fi.makeAbsolute();
    mFilename = fi.filePath();

    int btflags = 0;
    if (flags & AoDb::ReverseKeys)
        btflags |= BT_REVERSEKEY;
    if (flags & AoDb::NoSync)
        btflags |= BT_NOSYNC;
    if (flags & AoDb::ReadOnly)
        btflags |= BT_RDONLY;
    if (flags & AoDb::UseSyncMarker)
        btflags |= BT_USEMARKER;
    if (flags & AoDb::NoPageChecksums)
        btflags |= BT_NOPGCHECKSUM;

    mBtreeFlags = btflags;
    mBtree = btree_open(mFilename.toLocal8Bit().constData(), btflags, 0644);
    if (!mBtree) {
        qDebug() << "Aodb::open" << "failed" << errno;
        return false;
    }
    btree_set_cache_size(mBtree, mCacheSize);
    btree_set_cmp(mBtree, mCmp);

    Q_ASSERT(mBtree);
    return true;
}

quint64 AoDb::count() const
{
    Q_ASSERT(mBtree);
    const struct btree_stat *stat = btree_stat(mBtree);
    return stat->entries;
}

bool AoDb::get(const QByteArray &baKey, QByteArray &baValue)
{
    Q_ASSERT(mBtree);
    struct btval btkey;
    btkey.data = (void*)baKey.constData();
    btkey.size = baKey.size();
    btkey.free_data = 0;
    btkey.mp = 0;

    struct btval btvalue;
    int ok = btree_txn_get(mBtree, mTxn, &btkey, &btvalue);
    if (ok == BT_SUCCESS) {
        baValue.resize(btvalue.size); 
        memcpy(baValue.data(), btvalue.data, btvalue.size);
        btval_reset(&btvalue);
    } else {
        //qDebug() << "btree_txn_get" << ok << errno;
    }
    return ok == BT_SUCCESS;
}

bool AoDb::put(const QByteArray &baKey, const QByteArray &baValue)
{
    Q_ASSERT(mBtree);
    struct btval btkey;
    btkey.data = (void*)baKey.constData();
    btkey.size = baKey.size();
    btkey.free_data = 0;
    btkey.mp = 0;
    struct btval btvalue;
    btvalue.data = (void*)baValue.constData();
    btvalue.size = baValue.size();
    btvalue.free_data = 0;
    btvalue.mp = 0;

    int ok = btree_txn_put(mBtree, mTxn, &btkey, &btvalue, 0);
    if (ok != BT_SUCCESS) {
        qDebug() << "btree_txn_put" << ok << errno << endl << mFilename;
    }
    return (ok == BT_SUCCESS);
}

bool AoDb::remove(const QByteArray &baKey)
{
    Q_ASSERT(mBtree);
    struct btval btkey;
    btkey.data = (void*)baKey.constData();
    btkey.size = baKey.size();
    btkey.free_data = 0;
    btkey.mp = 0;
    int ok = btree_txn_del(mBtree, mTxn, &btkey, 0);
    if (ok != BT_SUCCESS) qDebug() << "db->del" << ok << errno << baKey.toHex() << endl
                                   << mFilename;
    return (ok == BT_SUCCESS);
}

void AoDb::sync()
{
    if (mBtree)
        btree_sync(mBtree);
}

void AoDb::close()
{
    if (mBtree) {
        if (mTxn) {
            qCritical() << "AoDb::close()" << "transaction in progress aborted.";
            btree_txn_abort(mTxn);
            mTxn = 0;
        }
        if (mBtree) {
            btree_close(mBtree);
            mBtree = 0;
        }
    }
}

bool AoDb::begin()
{
    Q_ASSERT(mBtree);
    if (mBtree && !mTxn) {
        mTxn = btree_txn_begin(mBtree, 0);
        if (!mTxn) {
          qCritical() << "AoDb::begin" << "txn failed" << errno << mFilename;
          return false;
        }
        return true;
    } else if (mTxn) {
        qCritical() << "AoDb::begin" << "txn already started" << mFilename;
        return false;
    } else {
        qCritical() << "AoDb::begin" << "no tree";
        return false;
    }
}

bool AoDb::beginRead()
{
    Q_ASSERT(mBtree);
    if (mBtree && !mTxn) {
        mTxn = btree_txn_begin(mBtree, 1);
        if (!mTxn) {
          qCritical() << "AoDb::beginRead" << "txn failed" << errno << mFilename;
          return false;
        }
        return true;
    } else if (mTxn) {
        qCritical() << "AoDb::begin" << "txn already started" << mFilename;
        return false;
    } else {
        qCritical() << "AoDb::begin" << "no tree";
        return false;
    }
}

bool AoDb::commit(quint32 tag)
{
    Q_ASSERT(mTxn);
    if (mTxn) {
        unsigned int flags = (gSyncRate && mCommitCount % gSyncRate == 0) ? BT_FORCE_MARKER : 0;
        int rc = btree_txn_commit(mTxn, tag, flags);
        mTxn = 0;
        if (gCompactRate && (mCommitCount++ > gCompactRate)) {
            compact();
            mCommitCount = 0;
        }
        return rc == 0;
    } else {
        qCritical() << "AoDb::commit()" << "no txn" << mFilename;
        return false;
    }
}

bool AoDb::abort()
{
    Q_ASSERT(mTxn);
    if (mTxn) {
        btree_txn_abort(mTxn);
        mTxn = 0;
        return true;
    } else {
        qCritical() << "AoDb::abort()" << "no txn";
        return false;
    }
}

quint32 AoDb::tag() const
{
    Q_ASSERT(mBtree);
    const struct btree_stat *stat = btree_stat(mBtree);
    return stat->tag;
}

int AoDb::revert()
{
    Q_ASSERT(mBtree);
    mTxn = 0;
    return btree_rollback(mBtree);
}

const struct btree_stat *AoDb::stat() const
{
    Q_ASSERT(mBtree);
    return btree_stat(mBtree);
}

AoDbCursor *AoDb::cursor()
{
    return new AoDbCursor(this);
}

void AoDb::setCacheSize(unsigned int cacheSize)
{
    if (mBtree)
        btree_set_cache_size(mBtree, cacheSize);
    mCacheSize = cacheSize;
}
void AoDb::dump()
{
    btree_dump(mBtree);
}

bool AoDb::reopen()
{
    close();
    mBtree = btree_open(mFilename.toLocal8Bit().constData(), mBtreeFlags, 0644);
    if (!mBtree) {
        qDebug() << "Aodb::reopen" << "failed" << errno;
        return false;
    }
    btree_set_cache_size(mBtree, mCacheSize);
    btree_set_cmp(mBtree, mCmp);
    return true;
}

AoDbCursor::AoDbCursor(AoDb *bdb, bool committedStateOnly)
    : mAoDb(bdb)
{
    Q_ASSERT(bdb->mBtree);
    if (committedStateOnly)
        mTxn = 0;
    else
        mTxn = bdb->mTxn;
    mBtreeCursor = btree_txn_cursor_open(bdb->mBtree, mTxn);
    if (!mBtreeCursor)
        qDebug() << "AoDbCursor" << errno;
    mBtkey.reset((struct btval *)malloc(sizeof(struct btval)));
    memset(mBtkey.data(), 0, sizeof(struct btval));
    mBtvalue.reset((struct btval *)malloc(sizeof(struct btval)));
    memset(mBtvalue.data(), 0, sizeof(struct btval));
}


AoDbCursor::~AoDbCursor()
{
    if (mBtreeCursor)
        btree_cursor_close(mBtreeCursor);
    btval_reset(mBtkey.data());
    btval_reset(mBtvalue.data());
    mTxn = 0;
    mBtreeCursor = 0;
}

bool AoDbCursor::first()
{
    btval_reset(mBtkey.data());
    btval_reset(mBtvalue.data());
    int ok = btree_cursor_get(mBtreeCursor, mBtkey.data(), mBtvalue.data(), BT_FIRST);
    if (ok == BT_SUCCESS) {
        return true;
    } else {
        btval_reset(mBtkey.data());
        return false;
    }
}

bool AoDbCursor::current(QByteArray &baKey, QByteArray &baValue)
{
    if (!mBtkey->size)
        return false;

    baKey = QByteArray((const char *)mBtkey->data, mBtkey->size);

    baValue = QByteArray((const char *)mBtvalue->data, mBtvalue->size);
    return true;
}

bool AoDbCursor::currentKey(QByteArray &baKey)
{
    if (!mBtkey->size)
        return false;

    baKey = QByteArray((const char *)mBtkey->data, mBtkey->size);

    return true;
}

bool AoDbCursor::currentValue(QByteArray &baValue)
{
    if (!mBtkey->size)
        return false;

    baValue = QByteArray((const char *)mBtvalue->data, mBtvalue->size);

    return true;
}

bool AoDbCursor::last()
{
    btval_reset(mBtkey.data());
    btval_reset(mBtvalue.data());
    int ok = btree_cursor_get(mBtreeCursor, mBtkey.data(), mBtvalue.data(), BT_LAST);
    if (ok == BT_SUCCESS) {
        return true;
    } else {
        btval_reset(mBtkey.data());
        return false;
    }
}

bool AoDbCursor::next()
{
    btval_reset(mBtkey.data());
    btval_reset(mBtvalue.data());
    int ok = btree_cursor_get(mBtreeCursor, mBtkey.data(), mBtvalue.data(), BT_NEXT);
    if (ok == BT_SUCCESS) {
        return true;
    } else {
        btval_reset(mBtkey.data());
        return false;
    }
}

bool AoDbCursor::prev()
{
    btval_reset(mBtkey.data());
    btval_reset(mBtvalue.data());
    int ok = btree_cursor_get(mBtreeCursor, mBtkey.data(), mBtvalue.data(), BT_PREV);
    return ok == BT_SUCCESS;
}

bool AoDbCursor::seek(const QByteArray &baKey)
{
    btval_reset(mBtkey.data());
    btval_reset(mBtvalue.data());
    mBtkey->data = (void*)baKey.constData();
    mBtkey->size = baKey.size();
    int ok = btree_cursor_get(mBtreeCursor, mBtkey.data(), mBtvalue.data(), BT_CURSOR_EXACT);
    if (ok == BT_SUCCESS) {
        return true;
    } else {
        btval_reset(mBtkey.data());
        return false;
    }
}

bool AoDbCursor::seekRange(const QByteArray &baKey)
{
    btval_reset(mBtkey.data());
    btval_reset(mBtvalue.data());
    mBtkey->data = (void*)baKey.constData();
    mBtkey->size = baKey.size();
    int ok = btree_cursor_get(mBtreeCursor, mBtkey.data(), mBtvalue.data(), BT_CURSOR);
    return ok == BT_SUCCESS;
}

QString AoDbCursor::errorMessage() const
{
    return QString("AoDbCursor: %1: %2").arg(mAoDb->fileName(), strerror(errno));
}

AoDbLocker::AoDbLocker()
    : mTag(0), mCommitTag(0)
{
}

AoDbLocker::~AoDbLocker()
{
    commit(mCommitTag);
}

void AoDbLocker::add(AoDb *db)
{
    if (mDbs.contains(db)) {
        qCritical("Attempting to add the same db to the transaction twice!");
        Q_ASSERT(false);
        return;
    }
    mDbs.append(db);
}

bool AoDbLocker::begin()
{
    mLastError = QString();
    mTag = 0;
    if (mDbs.isEmpty())
        return false;
    for (int i = 0; i < mDbs.size(); ++i) {
        AoDb *db = mDbs.at(i);
        if (!db->begin()) {
            mLastError = db->errorMessage();
            for (int k = 0; k < i; ++k) {
                AoDb *db = mDbs.at(k);
                db->abort();
            }
            mDbs.clear();
            return false;
        }
        if (i == 0) {
            mTag = db->tag();
        } else {
            if (mTag != db->tag()) {
                mLastError = QString::fromLatin1("Tag mismatch %1 != %2 (in %3)")
                        .arg(mTag).arg(db->tag()).arg(db->fileName());
                for (int k = 0; k < i; ++k) {
                    AoDb *db = mDbs.at(k);
                    db->abort();
                }
                mDbs.clear();
                return false;
            }
        }
    }
    return true;
}

quint32 AoDbLocker::tag() const
{
    return mTag;
}

bool AoDbLocker::abort()
{
    QStringList errors;
    foreach (AoDb *db, mDbs) {
        if (!db->abort()) {
            errors.append(db->errorMessage());
        }
    }
    mDbs.clear();
    mLastError = errors.join("\n");
    return errors.isEmpty();
}

bool AoDbLocker::commit(quint32 tag)
{
    QStringList errors;
    foreach (AoDb *db, mDbs) {
        if (!db->commit(tag)) {
            errors.append(db->errorMessage());
        }
    }
    mDbs.clear();
    mLastError = errors.join("\n");
    return errors.isEmpty();
}
