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

#ifndef HBTREEASSERT_P_H
#define HBTREEASSERT_P_H

#include "hbtreeglobal.h"

#include <QDebug>

#if !defined(QT_NO_DEBUG) || defined(QT_FORCE_ASSERTS)
#   define USE_HBTREE_ASSERT
#endif

QT_BEGIN_NAMESPACE_HBTREE

class HBtreeAssert
{
public:
    HBtreeAssert &HBTREE_ASSERT_A;
    HBtreeAssert &HBTREE_ASSERT_B;

    HBtreeAssert()
        : HBTREE_ASSERT_A(*this), HBTREE_ASSERT_B(*this), ignore_(false), copied_(false)
    {}

    HBtreeAssert(const HBtreeAssert &other)
        : HBTREE_ASSERT_A(*this), HBTREE_ASSERT_B(*this)
    { Q_UNUSED(other); copied_ = true; }

    void operator =(const HBtreeAssert &other)
    { Q_UNUSED(other); copied_ = true; }

    ~HBtreeAssert();

    template <class T>
    HBtreeAssert &print(const char *str, T val)
    {
        qDebug().nospace() << "\t" << str << ": " << val;
        return *this;
    }

    HBtreeAssert &operator () (const char *expr, const char *file, const char *func, int line);
    HBtreeAssert &ignore()
    { ignore_ = true; return *this; }
    HBtreeAssert &message(const QString &msg)
    { message_ = msg; return *this; }

private:
    bool ignore_;
    QString message_;
    QString assertStr_;
    bool copied_;
};

#define HBTREE_ASSERT_A(x) HBTREE_ASSERT_OP(x, B)
#define HBTREE_ASSERT_B(x) HBTREE_ASSERT_OP(x, A)

#ifndef USE_HBTREE_ASSERT
#define HBTREE_ASSERT_OP(x, next) HBTREE_ASSERT_A.HBTREE_ASSERT_ ## next
#   define HBTREE_ASSERT(expr) \
    if (false) \
        HBtreeAssert __hbtree_assert = HBtreeAssert().HBTREE_ASSERT_A
#else
#define HBTREE_ASSERT_OP(x, next) HBTREE_ASSERT_A.print(#x, (x)).HBTREE_ASSERT_ ## next
#   define HBTREE_ASSERT(expr) \
    if (!(expr)) \
        HBtreeAssert __hbtree_assert = HBtreeAssert()(#expr, __FILE__, __FUNCTION__, __LINE__).HBTREE_ASSERT_A
#endif

QT_END_NAMESPACE_HBTREE

#endif
