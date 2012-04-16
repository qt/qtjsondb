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

#ifndef JSONDB_BTREE_H
#define JSONDB_BTREE_H

#define JSONDB_USE_HBTREE

#include "jsondbpartitionglobal.h"

#ifndef JSONDB_USE_HBTREE
#include "qbtree.h"
#include "qbtreecursor.h"
#include "qbtreetxn.h"
#else
#include "hbtree.h"
#include "hbtreecursor.h"
#include "hbtreetransaction.h"
#endif

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbBtree
{
public:

    enum OpenFlag {
        Default,
        ReadOnly
    };
    Q_DECLARE_FLAGS(OpenFlags, OpenFlag)

#ifdef JSONDB_USE_HBTREE
    typedef HBtree Btree;
#else
    typedef QBtree Btree;
#endif

    typedef Btree::CursorType Cursor;
    typedef Btree::TransactionType Transaction;
    typedef Btree::StatType Stat;

    typedef int (*CompareFunction)(const QByteArray &, const QByteArray &);

    JsonDbBtree();
    ~JsonDbBtree();

    void setFileName(const QString &filename);
    QString fileName() const;

    bool open(OpenFlags flags = Default);
    void close();
    bool isOpen() const;

    Transaction *beginRead()
    { Q_ASSERT(mBtree); return mBtree->beginRead(); }
    Transaction *beginWrite()
    { Q_ASSERT(mBtree); return mBtree->beginWrite(); }

    bool isWriting() const
    { Q_ASSERT(mBtree); return mBtree->isWriting(); }

    Transaction *writeTransaction()
    { Q_ASSERT(mBtree); return mBtree->writeTransaction(); }

    QString errorMessage() const
    { Q_ASSERT(mBtree); return mBtree->errorMessage(); }

    quint64 count() const
    { Q_ASSERT(mBtree); return mBtree->count(); }
    quint32 tag() const
    { Q_ASSERT(mBtree); return mBtree->tag(); }
    void setCompareFunction(CompareFunction cmp)
    { Q_ASSERT(mBtree); mBtree->setCompareFunction(cmp); }
    void setCacheSize(int size)
    { Q_ASSERT(mBtree); mBtree->setCacheSize(size); }
    Btree *btree() const
    { return mBtree; }
    Stat stats() const;
    bool sync()
    { Q_ASSERT(mBtree); return mBtree->sync(); }

    bool putOne(const QByteArray &key, const QByteArray &value);
    bool getOne(const QByteArray &key, QByteArray *value);
    bool removeOne(const QByteArray &key);

    bool clearData();

    bool compact();
    bool rollback();
    void setAutoCompactRate(int rate) const;

private:
    Btree *mBtree;
    JsonDbBtree(const JsonDbBtree&);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(JsonDbBtree::OpenFlags)

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_BTREE_H
