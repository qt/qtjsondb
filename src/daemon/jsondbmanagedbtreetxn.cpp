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

#include <QDebug>
#include "qbtreetxn.h"
#include "jsondbmanagedbtree.h"
#include "jsondbmanagedbtreetxn.h"

JsonDbManagedBtreeTxn::JsonDbManagedBtreeTxn()
    : mTxn(0), mBtree(0), mTag(0), mIsRead(false)
{
}

JsonDbManagedBtreeTxn::~JsonDbManagedBtreeTxn()
{
    reset(0, 0);
}

JsonDbManagedBtreeTxn::JsonDbManagedBtreeTxn(JsonDbManagedBtree *mbtree, QBtreeTxn *txn)
    : mTxn(0), mBtree(0), mTag(0), mIsRead(false)
{
    reset(mbtree, txn);
}

JsonDbManagedBtreeTxn::JsonDbManagedBtreeTxn(const JsonDbManagedBtreeTxn &other)
    : mTxn(0), mBtree(0), mTag(0), mIsRead(false)
{
    reset(other.mBtree, other.mTxn);
}

JsonDbManagedBtreeTxn &JsonDbManagedBtreeTxn::operator = (const JsonDbManagedBtreeTxn &other)
{
    if (this == &other)
        return *this;
    reset(other.mBtree, other.mTxn);
    return *this;
}

bool JsonDbManagedBtreeTxn::get(const QByteArray &baKey, QByteArray *baValue) const
{
    Q_ASSERT(mTxn);
    return mTxn->get(baKey, baValue);
}

bool JsonDbManagedBtreeTxn::put(const QByteArray &baKey, const QByteArray &baValue)
{
    Q_ASSERT(mTxn);
    return mTxn->put(baKey, baValue);
}

bool JsonDbManagedBtreeTxn::remove(const QByteArray &baKey)
{
    Q_ASSERT(mTxn);
    return mTxn->remove(baKey);
}

bool JsonDbManagedBtreeTxn::commit(quint32 tag)
{
    Q_ASSERT(mTxn);
    if (mTxn->isReadOnly()) {
        qWarning() << "JsonDbManagedBtreeTxn::commit: commit on read only txn doesn't make sense. Aborting.";
        mBtree->abort(this);
        return true;
    }
    return mBtree->commit(this, tag);
}

void JsonDbManagedBtreeTxn::abort()
{
    Q_ASSERT(mTxn);
    mBtree->abort(this);
}

const QString JsonDbManagedBtreeTxn::errorMessage() const
{
    return mBtree->errorMessage();
}

void JsonDbManagedBtreeTxn::reset(JsonDbManagedBtree *mbtree, QBtreeTxn *txn)
{
    if (mTxn && mBtree) {
        mBtree->remove(this);
    }

    mTxn = txn;
    mBtree = mbtree;

    if (txn && mbtree) {
        Q_ASSERT(mbtree->handle() == txn->btree()->handle());
        Q_ASSERT(txn->handle());
        mTag = txn->tag();
        mIsRead = txn->isReadOnly();
        mbtree->add(this);
    }
}
