/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef HBTREE_H
#define HBTREE_H

#include <QDebug>
#include <QByteArray>
#include <QScopedPointer>
#include <QFile>

#include "hbtreetransaction.h"
#include "hbtreecursor.h"
#include "hbtreeglobal.h"

QT_BEGIN_NAMESPACE_HBTREE

extern bool gDebugHBtree;

class TestHBtree;
class TestBtrees;

class HBtreePrivate;
class HBtree
{
public:
    struct Stat
    {
        Stat()
            : numCommits(0), numSyncs(0), numBranchPages(0), numLeafPages(0), numOverflowPages(0), numEntries(0),
              numBranchSplits(0), numLeafSplits(0), depth(0),
              reads(0), hits(0), writes(0), psize(0), ksize(0)
        {}

        int numCommits;
        int numSyncs;
        int numBranchPages;
        int numLeafPages;
        int numOverflowPages;
        int numEntries;
        int numBranchSplits;
        int numLeafSplits;
        int depth;

        qint32 reads;
        qint32 hits;
        qint32 writes;
        quint32 psize;
        quint32 ksize;

        Stat &operator += (const Stat &o)
        {
            reads += o.reads;
            hits += o.hits;
            writes += o.writes;
            psize = o.psize;
            ksize = o.ksize;

            numCommits += o.numCommits;
            numSyncs += o.numSyncs;
            numEntries += o.numEntries;
            numBranchPages += o.numBranchPages;
            numBranchSplits += o.numBranchSplits;
            numLeafPages += o.numLeafPages;
            numLeafSplits += numLeafSplits;
            numOverflowPages += o.numOverflowPages;

            depth = qMax(depth, o.depth);
            return *this;
        }
    };


    typedef HBtreeCursor CursorType;
    typedef HBtreeTransaction TransactionType;
    typedef Stat StatType;

    enum OpenMode {
        ReadWrite,
        ReadOnly
    };

    HBtree();
    HBtree(const QString &fileName);
    ~HBtree();

    typedef int (*CompareFunction)(const QByteArray &, const QByteArray &);

    void setFileName(const QString &fileName);
    void setOpenMode(OpenMode mode);
    void setAutoSyncRate(int rate) { autoSyncRate_ = rate; }
    void setCompareFunction(CompareFunction compareFunction);
    void setCacheSize(int size);

    QString fileName() const;
    OpenMode openMode() const;
    int autoSyncRate() const { return autoSyncRate_; }

    bool open();
    void close();
    bool isOpen() const;
    int lastWriteError() const;
    int lastReadError() const;

    bool open(OpenMode mode) { setOpenMode(mode); return open(); }

    size_t size() const;
    bool sync();
    bool rollback();
    bool clearData();

    HBtreeTransaction *beginTransaction(HBtreeTransaction::Type type);

    HBtreeTransaction *beginRead()
    { return beginTransaction(HBtreeTransaction::ReadOnly); }
    HBtreeTransaction *beginWrite()
    { return beginTransaction(HBtreeTransaction::ReadWrite); }

    const Stat& stats() const { return stats_; }

    int count() const { return stats_.numEntries; }
    quint32 tag() const;
    bool isWriting() const;
    HBtreeTransaction *writeTransaction() const;

    QString errorMessage() const;

private:
    friend class HBtreeTransaction;
    bool commit(HBtreeTransaction *transaction, quint64 tag);
    void abort(HBtreeTransaction *transaction);
    bool put(HBtreeTransaction *transaction, const QByteArray &key, const QByteArray &value);
    QByteArray get(HBtreeTransaction *transaction, const QByteArray &key);
    bool del(HBtreeTransaction *transaction, const QByteArray &key);

    friend class HBtreeCursor;
    bool doCursorOp(HBtreeCursor *cursor, HBtreeCursor::Op op, const QByteArray &key = QByteArray(), HBtreeCursor::RangePolicy policy = HBtreeCursor::EqualOrGreater);

    Q_DECLARE_PRIVATE(HBtree)
    Q_DISABLE_COPY(HBtree)

    QScopedPointer<HBtreePrivate> d_ptr;

    int autoSyncRate_;
    Stat stats_;

    friend class TestHBtree;
    friend class TestBtrees;
};


inline QDebug operator << (QDebug dbg, const HBtree::Stat &stats)
{
    dbg.nospace() << "[writes:" << stats.writes
                  << ",reads:" << stats.reads
                  << ",commits:" << stats.numCommits
                  << ",syncs:" << stats.numSyncs
                  << ",branches:" << stats.numBranchPages
                  << ",leaves:" << stats.numLeafPages
                  << ",entries:" << stats.numEntries
                  << ",depth:" << stats.depth
                  << ",hits:" << stats.hits
                  << "]";
    return dbg.space();
}

QT_END_NAMESPACE_HBTREE

#endif // HBTREE_H
