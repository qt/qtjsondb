/****************************************************************************
**
** Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef QBTREE_H
#define QBTREE_H

#include <QByteArray>
#include <QString>
#include <QList>
#include <QtEndian>

#include "qbtreedata.h"

class QBtreeTxn;
struct btree;
struct btree_stat;

class QBtree
{
public:
    enum DbFlag {
        Default=0x0000,
        ReverseKeys=0x001,
        NoSync=0x004,
        ReadOnly=0x008,
        UseSyncMarker=0x010
    };
    Q_DECLARE_FLAGS(DbFlags, DbFlag)

    QBtree();
    ~QBtree();

    bool open(const QString &filename, DbFlags flags = Default);
    void close();

    enum TxnFlag { TxnReadWrite, TxnReadOnly };
    QBtreeTxn *begin(TxnFlag flag = TxnReadWrite);

    QBtreeTxn *beginReadWrite()
    { return begin(QBtree::TxnReadWrite); }
    QBtreeTxn *beginRead()
    { return begin(QBtree::TxnReadOnly); }
    QBtreeTxn *beginRead(quint32 tag);

    bool rollback();

    QString fileName() const { return mFilename; }
    const struct btree_stat *stat() const;
    quint64 count() const;
    btree *handle() const { return mBtree; }
    quint32 tag() const;

    bool compact();
    void sync();

    int autoSyncRate() const;
    void setAutoSyncRate(int rate);

    int autoCompactRate() const;
    void setAutoCompactRate(int rate);

    typedef int (*CmpFunc)(const char *a, size_t asize, const char *b, size_t bsize, void *context);
    bool setCmpFunc(CmpFunc cmp);

    void setCacheSize(unsigned int cacheSize);
    void dump() const;

private:
    bool reopen();

    friend class QBtreeTxn;
    inline void addTxn(QBtreeTxn *txn) { mTxns.append(txn); }
    inline void removeTxn(QBtreeTxn *txn) { mTxns.removeOne(txn); }

    QString mFilename;
    btree *mBtree;
    CmpFunc mCmp;
    int mCacheSize;

    QList<QBtreeTxn *> mTxns;

    int mCommitCount;
    int mAutoCompactRate;
    int mAutoSyncRate;

    Q_DISABLE_COPY(QBtree)
};


Q_DECLARE_OPERATORS_FOR_FLAGS(QBtree::DbFlags)

#endif // QBTREE_H
