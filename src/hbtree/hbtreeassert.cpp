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

#include "hbtreeassert_p.h"

#undef HBTREE_ASSERT_A
#undef HBTREE_ASSERT_B

HBtreeAssert::~HBtreeAssert()
{
    if (copied_)
        return;
    if (!ignore_) {
        if (message_.size())
            assertStr_ += QLatin1String(" [Message]: ") + message_;
        qFatal("%s", assertStr_.toLatin1().constData());
    } else {
        QString str = QLatin1String("\tSILENT ") + assertStr_;
        qDebug("%s", str.toLatin1().constData());
        if (message_.size())
            qDebug().nospace() << "\n\t[Message]: " << message_;
    }
}

HBtreeAssert &HBtreeAssert::operator ()(const char *expr, const char *file, const char *func, int line)
{
    assertStr_ = QString(QLatin1String("ASSERT: %1 in file %2, line %3, from function '%4'"))
            .arg(QLatin1String(expr)).arg(QLatin1String(file)).arg(line).arg(QLatin1String(func));
    qDebug().nospace() << "CONDITION FAILURE: ";
    return *this;
}
