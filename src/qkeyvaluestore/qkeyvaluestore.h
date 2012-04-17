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

#ifndef QKEYVALUESTORE_H
#define QKEYVALUESTORE_H

#include <QString>
#include <QByteArray>
#include <QMap>

#include "qkeyvaluestore_p.h"

class QKeyValueStoreTxn;
class QKeyValueStoreCursor;
class QKeyValueStore {
    QKeyValueStorePrivate *p;
public:
    QKeyValueStore();
    QKeyValueStore(const QString &name);
    bool open();
    bool close();
    bool compact() { return p->compact(); }
    bool dump() { return false; }
    bool rollback() { return false; }
    QString fileName() const { return p->m_name; }
    void setFileName(QString name);
    // Transaction support
    QKeyValueStoreTxn *beginRead();
    QKeyValueStoreTxn *beginWrite();
    // Operations required by the BTree implementation
    bool isWriting() const { return p->isWriting(); }
    bool sync();
    quint64 count() const { return p->m_offsets.count(); }
    quint32 tag() const { return p->m_currentTag; }
    void setCacheSize(int size) { Q_UNUSED(size); }
    int cacheSize() const { return 0; }
    typedef int (*CompareFunction)(const QByteArray &, const QByteArray &);
    void setCompareFunction(CompareFunction cmp) { Q_UNUSED(cmp); }

    quint32 syncThreshold() const { return p->m_syncThreshold; }
    void setSyncThreshold(quint32 threshold);
    quint32 compactThreshold() const { return p->m_compactThreshold; }
    void setCompactThreshold(quint32 threshold) { p->m_compactThreshold = threshold; }
    // This is mostly needed for testing purposes.
    int markers() const { return p->m_dataMarks.count(); }

    // To support jsondbtree.h
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
    typedef QKeyValueStoreCursor CursorType;
    typedef QKeyValueStoreTxn TransactionType;
    typedef Stat StatType;
    QKeyValueStoreTxn *writeTransaction() const;
    QString errorMessage() const { return QString("Some error"); }
    Stat stats_;
    const Stat& stats() const { return stats_; }

private:
    // We put this here for now, since we are not using it.
    CompareFunction m_cmp;
};

#endif // QKEYVALUESTORE_H
