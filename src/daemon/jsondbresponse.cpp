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

#include <QMap>
#include <QDebug>

#include "jsondbresponse.h"
#include "jsondbsettings.h"
#include "jsondb-strings.h"
#include <qjsonobject.h>
#include <qjsonvalue.h>

QT_BEGIN_NAMESPACE_JSONDB

void JsonDbResponse::setError(QJsonObject &map, int code, const QString &message)
{
    map.insert(JsonDbString::kCodeStr, QJsonValue(code));
    map.insert(JsonDbString::kMessageStr, message);
}

QJsonObject JsonDbResponse::makeError(int code, const QString &message)
{
    QJsonObject map;
    setError(map, code, message);
    return map;
}

QJsonObject JsonDbResponse::makeResponse(QJsonObject &resultmap, QJsonObject &errormap, bool silent)
{
    QJsonObject map;
    if (jsondbSettings->debug() && !silent && !errormap.isEmpty()) {
        qCritical() << errormap;
    }
    if (!resultmap.isEmpty())
        map.insert( JsonDbString::kResultStr, resultmap);
    else
        map.insert( JsonDbString::kResultStr, QJsonValue());

    if (!errormap.isEmpty())
        map.insert( JsonDbString::kErrorStr, errormap );
    else
        map.insert( JsonDbString::kErrorStr, QJsonValue());
    return map;
}

QJsonObject JsonDbResponse::makeResponse(QJsonObject &resultmap)
{
    QJsonObject map;
    if (!resultmap.isEmpty())
        map.insert( JsonDbString::kResultStr, resultmap);
    else
        map.insert( JsonDbString::kResultStr, QJsonValue());

    map.insert( JsonDbString::kErrorStr, QJsonValue());
    return map;
}

QJsonObject JsonDbResponse::makeErrorResponse(QJsonObject &resultmap,
                                          int code, const QString &message, bool silent)
{
    QJsonObject errormap;
    setError(errormap, code, message);
    return makeResponse(resultmap, errormap, silent);
}

QJsonObject JsonDbResponse::makeErrorResponse(int code, const QString &message, bool silent)
{
    QJsonObject resultmap, errormap;
    setError(errormap, code, message);
    return makeResponse(resultmap, errormap, silent);
}

bool JsonDbResponse::responseIsError(QJsonObject responseMap)
{
    return responseMap.contains(JsonDbString::kErrorStr)
            && (responseMap.value(JsonDbString::kErrorStr).type() == QJsonValue::Object);
}

QT_END_NAMESPACE_JSONDB
