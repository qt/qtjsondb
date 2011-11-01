/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "qbtreelocker.h"
#include "qbtree.h"

#include <QDebug>

QBtreeReadLocker::QBtreeReadLocker(QBtree *db)
    : txn(db ? db->begin(QBtree::TxnReadOnly) : 0)
{
    if (!db)
        qWarning("QBtreeReadLocker: constructed without QBtree object!");
    if (db && !txn) {
        qWarning() << "QBtreeReadLocker: hm, failed to start read transaction on" << db->fileName();
        Q_ASSERT(false);
    }
}

QBtreeReadLocker::~QBtreeReadLocker()
{
    abort();
}

void QBtreeReadLocker::abort()
{
    if (txn)
        txn->abort();
    txn = 0;
}

quint32 QBtreeReadLocker::tag() const
{
    return txn ? txn->tag() : quint32(0);
}

bool QBtreeReadLocker::get(const QByteArray &baKey, QByteArray *baValue) const
{
    return txn ? txn->get(baKey, baValue) : false;
}

bool QBtreeReadLocker::get(const char *key, int keySize, QBtreeData *value) const
{
    return txn ? txn->get(key, keySize, value) : false;
}

bool QBtreeReadLocker::get(const QBtreeData &key, QBtreeData *value) const
{
    return txn ? txn->get(key, value) : false;
}

class QBtreeWriteLockerPrivate
{
public:
    QBtreeTxn *txn;
    quint32 autoCommitTag;
    int isAutoCommitSet : 1;

    QBtreeWriteLockerPrivate()
        : txn(0), autoCommitTag(0), isAutoCommitSet(false)
    { }
};

QBtreeWriteLocker::QBtreeWriteLocker(QBtree *db)
    : d_ptr(new QBtreeWriteLockerPrivate)
{
    if (!db)
        qWarning("QBtreeWriteLocker: constructed without QBtree object!");
    d_ptr->txn = db ? db->begin(QBtree::TxnReadWrite) : 0;
}

QBtreeWriteLocker::~QBtreeWriteLocker()
{
    if (d_ptr->isAutoCommitSet)
        commit(d_ptr->autoCommitTag);
    else
        abort();
    delete d_ptr;
}

bool QBtreeWriteLocker::isValid() const
{
    return d_ptr->txn != 0;
}

void QBtreeWriteLocker::abort()
{
    if (d_ptr->txn)
        d_ptr->txn->abort();
    d_ptr->txn = 0;
}

bool QBtreeWriteLocker::commit(quint32 tag)
{
    if (!d_ptr->txn)
        return false;
    QBtreeTxn *txn = d_ptr->txn;
    d_ptr->txn = 0;
    return txn->commit(tag);
}

quint32 QBtreeWriteLocker::tag() const
{
    return d_ptr->txn ? d_ptr->txn->tag() : quint32(0);
}

void QBtreeWriteLocker::setAutoCommitTag(quint32 tag)
{
    d_ptr->autoCommitTag = tag;
    d_ptr->isAutoCommitSet = true;
}

quint32 QBtreeWriteLocker::autoCommitTag() const
{
    return d_ptr->autoCommitTag;
}

void QBtreeWriteLocker::unsetAutoCommitTag()
{
    d_ptr->isAutoCommitSet = false;
}

bool QBtreeWriteLocker::get(const QByteArray &baKey, QByteArray *baValue) const
{
    return d_ptr->txn ? d_ptr->txn->get(baKey, baValue) : false;
}

bool QBtreeWriteLocker::get(const char *key, int keySize, QBtreeData *value) const
{
    return d_ptr->txn ? d_ptr->txn->get(key, keySize, value) : false;
}

bool QBtreeWriteLocker::get(const QBtreeData &key, QBtreeData *value) const
{
    return d_ptr->txn ? d_ptr->txn->get(key, value) : false;
}

bool QBtreeWriteLocker::put(const QByteArray &baKey, const QByteArray &baValue)
{
    return d_ptr->txn ? d_ptr->txn->put(baKey, baValue) : false;
}

bool QBtreeWriteLocker::put(const char *key, int keySize, const char *value, int valueSize)
{
    return d_ptr->txn ? d_ptr->txn->put(key, keySize, value, valueSize) : false;
}

bool QBtreeWriteLocker::put(const QBtreeData &baKey, const QBtreeData &baValue)
{
    return d_ptr->txn ? d_ptr->txn->put(baKey, baValue) : false;
}

bool QBtreeWriteLocker::remove(const QByteArray &baKey)
{
    return d_ptr->txn ? d_ptr->txn->remove(baKey) : false;
}

bool QBtreeWriteLocker::remove(const QBtreeData &key)
{
    return d_ptr->txn ? d_ptr->txn->remove(key) : false;
}

bool QBtreeWriteLocker::remove(const char *key, int keySize)
{
    return d_ptr->txn ? d_ptr->txn->remove(key, keySize) : false;
}
