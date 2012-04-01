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

#ifndef JSONDB_MANAGED_BTREE_H
#define JSONDB_MANAGED_BTREE_H

#include <QMap>
#include <QSet>
#include "jsondbmanagedbtreetxn.h"
#include "jsondbpartitionglobal.h"

QT_BEGIN_HEADER

class QBtree;
class QBtreeTxn;

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class Q_JSONDB_PARTITION_EXPORT JsonDbManagedBtree
{
public:
    JsonDbManagedBtree();
    ~JsonDbManagedBtree();

    bool open(const QString &filename);
    void close();

    JsonDbManagedBtreeTxn beginRead(quint32 tag);
    JsonDbManagedBtreeTxn beginRead();
    JsonDbManagedBtreeTxn beginWrite();

    bool isWriteTxnActive() const
    { return mWriter.txn != NULL; }
    bool isReadTxnActive(quint32 tag) const;
    bool isReadTxnActive() const;
    bool numActiveReadTxns() const
    { return mReaders.size() > 0; }

    JsonDbManagedBtreeTxn existingWriteTxn();

    bool putOne(const QByteArray &key, const QByteArray &value);
    bool getOne(const QByteArray &key, QByteArray *value);
    bool removeOne(const QByteArray &key);

    bool clearData();

    QString errorMessage() const;

    QString fileName() const;
    quint64 count() const;
    quint32 tag() const;
    bool compact();
    bool rollback();
    struct btree *handle() const;
    void setAutoCompactRate(int rate) const;
    void setCacheSize(int size);
    QBtree *btree() const;

private:
    friend class JsonDbManagedBtreeTxn;
    void remove(JsonDbManagedBtreeTxn *txn);
    void add(JsonDbManagedBtreeTxn *txn);

    bool commit(JsonDbManagedBtreeTxn *txn, quint32 tag);
    void abort(JsonDbManagedBtreeTxn *txn);

    struct RefedTxn {
        QBtreeTxn *txn;
        QSet<JsonDbManagedBtreeTxn* > clients;
    };
    typedef QMap<quint32, RefedTxn> RefedTxnMap;

    QBtree *mBtree;
    RefedTxn mWriter;
    RefedTxnMap mReaders;

    JsonDbManagedBtree(const JsonDbManagedBtree&);
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_MANAGED_BTREE_H
