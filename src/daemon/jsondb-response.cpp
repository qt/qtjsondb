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

#include <QMap>
#include <QDebug>

#include <QtJsonDbQson/private/qson_p.h>

#include "jsondb-response.h"
#include "jsondb-strings.h"

QT_ADDON_JSONDB_BEGIN_NAMESPACE

extern bool gVerbose;

void JsonDbResponse::setError(QsonMap &map, int code, const QString &message)
{
    map.insert(JsonDbString::kCodeStr, code);
    map.insert(JsonDbString::kMessageStr, message);
}

QsonMap JsonDbResponse::makeError(int code, const QString &message)
{
    QsonMap map;
    setError(map, code, message);
    return map;
}

QsonMap JsonDbResponse::makeResponse(QsonMap &resultmap, QsonMap &errormap, bool silent)
{
    QsonMap map;
    if (gVerbose && !silent && !errormap.isEmpty()) {
        qCritical() << errormap;
    }
    if (!resultmap.isEmpty())
        map.insert( JsonDbString::kResultStr, resultmap);
    else
        map.insert( JsonDbString::kResultStr, QsonObject::NullValue);

    if (!errormap.isEmpty())
        map.insert( JsonDbString::kErrorStr, errormap );
    else
        map.insert( JsonDbString::kErrorStr, QsonObject::NullValue);
    return map;
}

QsonMap JsonDbResponse::makeResponse(QsonMap &resultmap)
{
    QsonMap map;
    if (!resultmap.isEmpty())
        map.insert( JsonDbString::kResultStr, resultmap);
    else
        map.insert( JsonDbString::kResultStr, QsonObject::NullValue);

    map.insert( JsonDbString::kErrorStr, QsonObject::NullValue);
    return map;
}

QsonMap JsonDbResponse::makeErrorResponse(QsonMap &resultmap,
                                          int code, const QString &message, bool silent)
{
    QsonMap errormap;
    setError(errormap, code, message);
    return makeResponse(resultmap, errormap, silent);
}

QsonMap JsonDbResponse::makeErrorResponse(int code, const QString &message, bool silent)
{
    QsonMap resultmap, errormap;
    setError(errormap, code, message);
    return makeResponse(resultmap, errormap, silent);
}

bool JsonDbResponse::responseIsError(QsonMap responseMap)
{
    return responseMap.contains(JsonDbString::kErrorStr)
        && !responseMap.isNull(JsonDbString::kErrorStr);
}

QT_ADDON_JSONDB_END_NAMESPACE
