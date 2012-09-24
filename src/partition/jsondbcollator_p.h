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

#ifndef JSONDBCOLLATOR_P_H
#define JSONDBCOLLATOR_P_H

#ifndef NO_COLLATION_SUPPORT

#include <QtCore/qglobal.h>

#include "jsondbcollator.h"

#include <unicode/utypes.h>
#include <unicode/ucol.h>
#include <unicode/ustring.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbCollatorPrivate
{
public:
    QAtomicInt ref;
    bool modified;
    QLocale locale;
    JsonDbCollator::Collation collation;
    JsonDbCollator::Strength strength;
    JsonDbCollator::Options options;

    UCollator *collator;

    JsonDbCollatorPrivate()
        : modified(true),
          collation(JsonDbCollator::Default),
          strength(JsonDbCollator::TertiaryStrength),
          options(0),
          collator(0)
    { ref.store(1); }
    ~JsonDbCollatorPrivate();

    int compare(ushort *s1, int len1, ushort *s2, int len2);

private:
    Q_DISABLE_COPY(JsonDbCollatorPrivate)
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // NO_COLLATION_SUPPORT

#endif // JSONDBCOLLATOR_P_H
