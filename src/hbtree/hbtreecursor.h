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

#ifndef HBTREECURSOR_H
#define HBTREECURSOR_H

#include "hbtreeglobal.h"

#include <QByteArray>
#include <QStack>

QT_BEGIN_NAMESPACE_HBTREE

class HBtree;
class HBtreeTransaction;
class HBtreeCursor
{
public:
    enum RangePolicy
    {
        EqualOrGreater,
        EqualOrLess
    };

    HBtreeCursor();
    explicit HBtreeCursor(HBtreeTransaction * transaction);
    explicit HBtreeCursor(HBtree *btree, bool commited = false);
    ~HBtreeCursor();


    QByteArray key() const { return key_; }
    QByteArray value() const { return value_; }
    bool current(QByteArray *pKey, QByteArray *pValue);

    bool last();
    bool first();
    bool next();
    bool previous();
    bool seek(const QByteArray &key);
    bool seekRange(const QByteArray &key, RangePolicy policy = EqualOrGreater);

    HBtreeCursor(const HBtreeCursor &other);
    HBtreeCursor &operator = (const HBtreeCursor &other);
private:
    enum Op {
        First,
        Last,
        Previous,
        Next,
        ExactMatch,
        FuzzyMatch
    };

    friend class HBtree;
    friend class HBtreePrivate;

    QByteArray key_;
    QByteArray value_;
    HBtreeTransaction *transaction_;
    HBtree *btree_;
    quint32 lastLeaf_;
    bool valid_;

    bool doOp(Op op, const QByteArray &key = QByteArray());
};

QT_END_NAMESPACE_HBTREE

#endif // HBTREECURSOR_H
