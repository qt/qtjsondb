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
#include <QList>
#include <QMap>
#include <QVariantList>
#include <QString>
#include <QStringList>

#include "jsondbnotification.h"
#include "jsondbquery.h"
#include "jsondb-strings.h"

QT_BEGIN_NAMESPACE_JSONDB

JsonDbNotification::JsonDbNotification(const JsonDbOwner *owner, const QString &uuid, const QString& query,
                           QStringList actions, const QString &partition)
    : mOwner(owner)
    , mUuid(uuid)
    , mQuery(query)
    , mActions(None)
    , mPartition(partition)
{
    foreach (QString s, actions) {
        if (s == JsonDbString::kCreateStr)
            mActions |= Create;
        else if (s == JsonDbString::kUpdateStr)
            mActions |= Update;
        else if ((s == "delete") || (s == JsonDbString::kRemoveStr))
            mActions |= Delete;
    }
}
JsonDbNotification::~JsonDbNotification()
{
    if (mCompiledQuery) {
        delete mCompiledQuery;
        mCompiledQuery = 0;
    }
}

QT_END_NAMESPACE_JSONDB
