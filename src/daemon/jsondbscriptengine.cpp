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

#include "jsondbobject.h"

#include <QJSValue>
#include <QJSValueIterator>
#include <QStringBuilder>
#include <QStringList>
#include <QCryptographicHash>

#include <qjsondocument.h>

#include "jsondb-strings.h"
#include "jsondbproxy.h"
#include "jsondbscriptengine.h"

QT_ADDON_JSONDB_BEGIN_NAMESPACE

QJsonValue JsonDbScriptEngine::fromJSValue(const QJSValue &v)
{
    if (v.isNull())
        return QJsonValue(QJsonValue::Null);
    if (v.isNumber())
        return QJsonValue(v.toNumber());
    if (v.isString())
        return QJsonValue(v.toString());
    if (v.isBool())
        return QJsonValue(v.toBool());
    if (v.isArray()) {
        QJsonArray a;
        int size = v.property("length").toInt();
        for (int i = 0; i < size; i++) {
            a.append(fromJSValue(v.property(i)));
        }
        return a;
    }
    if (v.isObject()) {
        QJSValueIterator it(v);
        QJsonObject o;
        while (it.hasNext()) {
            it.next();
            QString name = it.name();
            QJSValue value = it.value();
            o.insert(name, fromJSValue(value));
        }
        return o;
    }
    return QJsonValue(QJsonValue::Undefined);
}

QJSValue JsonDbScriptEngine::toJSValue(const QJsonValue &v, QJSEngine *scriptEngine)
{
    switch (v.type()) {
    case QJsonValue::Null:
        return QJSValue(QJSValue::NullValue);
    case QJsonValue::Undefined:
        return QJSValue(QJSValue::UndefinedValue);
    case QJsonValue::Double:
        return QJSValue(v.toDouble());
    case QJsonValue::String:
        return QJSValue(v.toString());
    case QJsonValue::Bool:
        return QJSValue(v.toBool());
    case QJsonValue::Array: {
        QJSValue jsArray = scriptEngine->newArray();
        QJsonArray array = v.toArray();
        for (int i = 0; i < array.size(); i++)
            jsArray.setProperty(i, toJSValue(array.at(i), scriptEngine));
        return jsArray;
    }
    case QJsonValue::Object:
        return toJSValue(v.toObject(), scriptEngine);
    }
    return QJSValue(QJSValue::UndefinedValue);
}

QJSValue JsonDbScriptEngine::toJSValue(const QJsonObject &object, QJSEngine *scriptEngine)
{
    QJSValue jsObject = scriptEngine->newObject();
    for (QJsonObject::const_iterator it = object.begin(); it != object.end(); ++it)
        jsObject.setProperty(it.key(), toJSValue(it.value(), scriptEngine));
    return jsObject;
}

static QJSEngine *sScriptEngine;

QJSEngine *JsonDbScriptEngine::scriptEngine()
{
    if (!sScriptEngine) {
        sScriptEngine = new QJSEngine();
        QJSValue globalObject = sScriptEngine->globalObject();
        globalObject.setProperty("console", sScriptEngine->newQObject(new Console()));
    }
    return sScriptEngine;
}

QT_ADDON_JSONDB_END_NAMESPACE
