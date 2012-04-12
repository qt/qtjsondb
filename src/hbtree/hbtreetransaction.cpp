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

#include <QDebug>
#include "hbtree.h"
#include "hbtreetransaction.h"

HBtreeTransaction::HBtreeTransaction(HBtree *btree, HBtreeTransaction::Type type)
    : btree_(btree), type_(type)
{
    Q_ASSERT(btree_);
}

HBtreeTransaction::~HBtreeTransaction()
{
    Q_ASSERT(btree_);
}

bool HBtreeTransaction::put(const QByteArray &key, const QByteArray &value)
{
    Q_ASSERT(btree_);
    return btree_->put(this, key, value);
}

bool HBtreeTransaction::get(const QByteArray &key, QByteArray *pValue)
{
    Q_ASSERT(pValue);
    QByteArray val = get(key);
    if (val.isEmpty())
        return false;
    *pValue = val;
    return true;
}

QByteArray HBtreeTransaction::get(const QByteArray &key)
{
    Q_ASSERT(btree_);
    return btree_->get(this, key);
}

bool HBtreeTransaction::del(const QByteArray &key)
{
    Q_ASSERT(btree_);
    return btree_->del(this, key);
}

bool HBtreeTransaction::commit(quint64 tag)
{
    Q_ASSERT(btree_);
    if (isReadOnly()) {
        btree_->abort(this);
        return true;
    }
    return btree_->commit(this, tag);
}

void HBtreeTransaction::abort()
{
    Q_ASSERT(btree_);
    btree_->abort(this);
}
