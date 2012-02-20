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

#ifndef QMANAGEDBTREE_H
#define QMANAGEDBTREE_H

#include <QMap>
#include <QSet>
#include "qbtree.h"
#include "qmanagedbtreetxn.h"

class QBtree;

class QManagedBtree
{
public:
    QManagedBtree();
    ~QManagedBtree();

    bool open(const QString &filename, QBtree::DbFlags flags = QBtree::Default);
    void close();

    QManagedBtreeTxn beginRead(quint32 tag);
    QManagedBtreeTxn beginRead();
    QManagedBtreeTxn beginWrite();

    bool isWriteTxnActive() const
    { return mWriter.txn != NULL; }
    bool isReadTxnActive(quint32 tag) const;
    bool isReadTxnActive() const;
    bool numActiveReadTxns() const
    { return mReaders.size() > 0; }

    QManagedBtreeTxn existingWriteTxn();

    bool putOne(const QByteArray &key, const QByteArray &value);
    bool getOne(const QByteArray &key, QByteArray *value);
    bool removeOne(const QByteArray &key);

    bool clearData();

    QString errorMessage() const;

    QString fileName() const
    { Q_ASSERT(mBtree); return mBtree->fileName(); }
    quint64 count() const
    { Q_ASSERT(mBtree); return mBtree->count(); }
    quint32 tag() const
    { Q_ASSERT(mBtree); return mBtree->tag(); }
    bool compact()
    { Q_ASSERT(mBtree); return mBtree->compact(); }
    bool rollback()
    { Q_ASSERT(mBtree && !numActiveReadTxns() && !isWriteTxnActive()); return mBtree->rollback(); }
    struct btree *handle() const
    { Q_ASSERT(mBtree); return mBtree->handle(); }
    void setAutoCompactRate(int rate) const
    { Q_ASSERT(mBtree); mBtree->setAutoCompactRate(rate); }
    void setCmpFunc(QBtree::CmpFunc cmp)
    { Q_ASSERT(mBtree); mBtree->setCmpFunc(cmp); }
    void setCacheSize(int size)
    { Q_ASSERT(mBtree); mBtree->setCacheSize(size); }
    QBtree *btree() const
    { return mBtree; }
    QBtree::Stat stat() const;

private:
    friend class QManagedBtreeTxn;
    void remove(QManagedBtreeTxn *txn);
    void add(QManagedBtreeTxn *txn);

    bool commit(QManagedBtreeTxn *txn, quint32 tag);
    void abort(QManagedBtreeTxn *txn);

    struct RefedTxn {
        QBtreeTxn *txn;
        QSet<QManagedBtreeTxn* > clients;
    };
    typedef QMap<quint32, RefedTxn> RefedTxnMap;

    QBtree *mBtree;
    RefedTxn mWriter;
    RefedTxnMap mReaders;

    QManagedBtree(const QManagedBtree&);
};

#endif // QMANAGEDBTREE_H
