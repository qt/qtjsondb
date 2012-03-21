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

#include "hbtree.h"
#include "hbtreetransaction.h"
#include "hbtreecursor.h"


HBtreeCursor::HBtreeCursor()
    : transaction_(0), btree_(0)
{
}

HBtreeCursor::HBtreeCursor(HBtreeTransaction *transaction)
    : transaction_(transaction), lastLeaf_(0), valid_(false)
{
    btree_ = transaction->btree_;
}

HBtreeCursor::HBtreeCursor(HBtree *btree, bool commited)
    : transaction_(0), btree_(btree), lastLeaf_(0), valid_(false)
{
    if (!commited)
        transaction_ = btree_->writeTransaction();
}

HBtreeCursor::~HBtreeCursor()
{
}

HBtreeCursor::HBtreeCursor(const HBtreeCursor &other)
{
    // TODO
    Q_ASSERT(0);
    Q_UNUSED(other);
}

HBtreeCursor &HBtreeCursor::operator =(const HBtreeCursor &other)
{
    // TODO
    Q_ASSERT(0);
    Q_UNUSED(other);
    return *this;
}

bool HBtreeCursor::current(QByteArray *pKey, QByteArray *pValue)
{
    if (!valid_)
        return false;
    if (pKey)
        *pKey = key_;
    if (pValue)
        *pValue = value_;
    return true;
}

bool HBtreeCursor::last()
{
    Q_ASSERT(btree_);
    return btree_->doCursorOp(this, Last);
}

bool HBtreeCursor::first()
{
    Q_ASSERT(btree_);
    return btree_->doCursorOp(this, First);
}

bool HBtreeCursor::next()
{
    Q_ASSERT(btree_);
    return btree_->doCursorOp(this, Next);
}

bool HBtreeCursor::previous()
{
    Q_ASSERT(btree_);
    return btree_->doCursorOp(this, Previous);
}

bool HBtreeCursor::seek(const QByteArray &key)
{
    Q_ASSERT(btree_);
    return btree_->doCursorOp(this, ExactMatch, key);
}

bool HBtreeCursor::seekRange(const QByteArray &key)
{
    Q_ASSERT(btree_);
    return btree_->doCursorOp(this, FuzzyMatch, key);
}
