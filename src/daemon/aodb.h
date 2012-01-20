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

#ifndef _AODB_H_
#define _AODB_H_

#include <QFile>
#include <QtEndian>

QT_BEGIN_HEADER

class btree;
struct btree_txn;
struct btval;
struct btree_stat;
class AoDbCursor;

class AoDb
{
public:
    enum DbFlag {
        Default=0x0000,
        ReverseKeys=0x001,
        NoSync=0x004,
        ReadOnly=0x008,
        UseSyncMarker=0x010,
        NoPageChecksums=0x020
    };
    Q_DECLARE_FLAGS(DbFlags, DbFlag)

    AoDb();
    ~AoDb();

    bool open(const QString &filename, DbFlags flags = Default);
    void close();

    bool get(const QByteArray &baKey, QByteArray &baValue);
    bool put(const QByteArray &baKey, const QByteArray &baValue);
    bool remove(const QByteArray &baKey);

    void sync();

    bool begin();
    bool beginRead();
    bool commit(quint32 tag);
    bool abort();
    bool isTransaction() const { return mTxn != 0; }

    int revert();

    QString fileName() const { return mFilename; }
    const struct btree_stat *stat() const;
    quint64 count() const;
    btree *handle() const { return mBtree; }
    quint32 tag() const;

    bool drop();
    bool clearData();
    bool compact();

    QString errorMessage() const;

    typedef int (*CmpFunc)(const char *a, size_t asize, const char *b, size_t bsize, void *context);
    bool setCmpFunc(CmpFunc cmp);

    void setCacheSize(unsigned int cacheSize);
    void dump();

    AoDbCursor *cursor();

private:
    bool reopen();
    QString mFilename;
    int mBtreeFlags;
    btree *mBtree;
    btree_txn *mTxn;
    CmpFunc mCmp;
    int mCommitCount;
    int mCacheSize;

    AoDb(const AoDb&);
    friend class AoDbCursor;
};

class AoDbCursor
{
public:
    AoDbCursor(AoDb *bdb, bool committedStateOnly=false);
    virtual ~AoDbCursor();
    bool first();
    bool current(QByteArray &baKey, QByteArray &baValue);
    bool currentKey(QByteArray &baKey);
    bool currentValue(QByteArray &baValue);
    bool last();
    bool next();
    bool prev();
    bool seek(const QByteArray &baKey);
    bool seekRange(const QByteArray &baKey);
    QString errorMessage() const;

private:
    AoDb *mAoDb;
    struct cursor *mBtreeCursor;
    struct btree_txn *mTxn;
    QScopedPointer<struct btval, QScopedPointerPodDeleter> mBtkey, mBtvalue;
    AoDbCursor(const AoDbCursor&);
};

class AoDbLocker
{
public:
    AoDbLocker();
    ~AoDbLocker();

    void add(AoDb *db);
    bool begin();
    bool abort();
    bool commit(quint32 tag);

    void setCommitTag(quint32 tag) { mCommitTag = tag; }
    quint32 commitTag() const { return mCommitTag; }

    quint32 tag() const;

    QString errorMessage() const { return mLastError; }

private:
    QList<AoDb *> mDbs;
    quint32 mTag;
    quint32 mCommitTag;
    QString mLastError;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AoDb::DbFlags)

QT_END_HEADER

#endif
