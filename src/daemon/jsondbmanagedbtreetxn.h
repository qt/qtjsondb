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

#ifndef JSONDB_MANAGED_BTREE_TXN_H
#define JSONDB_MANAGED_BTREE_TXN_H


class QBtree;
class QBtreeTxn;
class JsonDbManagedBtree;

class JsonDbManagedBtreeTxn
{
public:
    JsonDbManagedBtreeTxn();
    ~JsonDbManagedBtreeTxn();
    JsonDbManagedBtreeTxn(const JsonDbManagedBtreeTxn &other);
    JsonDbManagedBtreeTxn &operator = (const JsonDbManagedBtreeTxn &other);

    bool isValid() const { return mTxn != NULL; }
    bool operator == (const JsonDbManagedBtreeTxn &rhs) const
    { return mTxn == rhs.mTxn; }
    bool operator != (const JsonDbManagedBtreeTxn &rhs) const
    { return mTxn != rhs.mTxn; }

    bool get(const QByteArray &baKey, QByteArray *baValue) const;
    bool put(const QByteArray &baKey, const QByteArray &baValue);
    bool remove(const QByteArray &baKey);

    bool commit(quint32 tag);
    void abort();

    const QBtreeTxn *txn() const { return mTxn; }
    const JsonDbManagedBtree *btree() const { return mBtree; }

    const QString errorMessage() const;

    quint32 tag() const { return mTag; }
    bool isReadOnly() const { return mIsRead; }

private:
    friend class JsonDbManagedBtree;
    void reset(JsonDbManagedBtree *mbtree, QBtreeTxn *txn);
    JsonDbManagedBtreeTxn(JsonDbManagedBtree *mbtree, QBtreeTxn *txn);

    QBtreeTxn *mTxn;
    JsonDbManagedBtree *mBtree;
    quint32 mTag;
    bool mIsRead;

    typedef void (JsonDbManagedBtreeTxn::*SafeBool)() const;
    void noBoolComparisons () const {}
public:
    operator SafeBool() const {
        return isValid() ? &JsonDbManagedBtreeTxn::noBoolComparisons : 0;
    }
};


#endif // JSONDB_MANAGED_BTREE_TXN_H
