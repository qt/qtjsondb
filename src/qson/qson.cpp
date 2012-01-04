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

#include "qson_p.h"

#include "qvariant.h"

#include "json.h"

QT_BEGIN_NAMESPACE_JSONDB

static int qsonobjectid = qRegisterMetaType<QsonObject>("QsonObject");
static int qsonmapid = qRegisterMetaType<QsonMap>("QsonMap");

QDebug operator<<(QDebug dbg, const QsonObject &obj)
{
    if (obj.type() == QsonObject::MapType) {
        QsonMap map(obj);
        if (map.isDocument()) {
            dbg.nospace() << "QsonDocument(";
        } else if (map.isMeta()) {
            dbg.nospace() << "QsonMeta(";
        } else {
            dbg.nospace() << "QsonMap(";
        }
    } else if (obj.type() == QsonObject::ListType) {
        dbg.nospace() << "QsonList(";
    }

    // TODO:
    dbg << JsonWriter().toString(qsonToVariant(obj));

    dbg.nospace() << ")";
    return dbg;
}

static QVariant qsonListItemToVariant(const QsonList &list, int pos);
static QVariant qsonMapItemToVariant(const QsonMap &map, const QString &key);

static QVariantList qsonListToVariant(const QsonList &list)
{
    int count = list.count();
    QVariantList result;
    result.reserve(count);
    for (int i = 0; i < count; ++i)
        result.append(qsonListItemToVariant(list, i));
    return result;
}

static QVariantMap qsonMapToVariant(const QsonMap &map)
{
    QVariantMap result;
    QStringList keys = map.keys();
    foreach (const QString &key, keys)
        result.insert(key, qsonMapItemToVariant(map, key));
    return result;
}

static QVariant qsonListItemToVariant(const QsonList &list, int pos)
{
    switch (list.typeAt(pos)) {
    case QsonObject::NullType: return QVariant();
    case QsonObject::BoolType: return list.at<bool>(pos);
    case QsonObject::IntType: return list.at<qint64>(pos);
    case QsonObject::UIntType: return list.at<quint64>(pos);
    case QsonObject::DoubleType: return list.at<double>(pos);
    case QsonObject::StringType: return list.at<QString>(pos);
    case QsonObject::ListType: return qsonListToVariant(list.at<QsonList>(pos));
    case QsonObject::MapType: return qsonMapToVariant(list.at<QsonMap>(pos));
    default:
        break;
    }
    return QVariant();
}
static QVariant qsonMapItemToVariant(const QsonMap &map, const QString &key)
{
    switch (map.valueType(key)) {
    case QsonObject::NullType: return QVariant();
    case QsonObject::BoolType: return map.value<bool>(key);
    case QsonObject::IntType: return map.value<qint64>(key);
    case QsonObject::UIntType: return map.value<quint64>(key);
    case QsonObject::DoubleType: return map.value<double>(key);
    case QsonObject::StringType: return map.value<QString>(key);
    case QsonObject::ListType: return qsonListToVariant(map.value<QsonList>(key));
    case QsonObject::MapType: return qsonMapToVariant(map.value<QsonMap>(key));
    default:
        break;
    }
    return QVariant();

}

QVariant qsonToVariant(const QsonObject &object)
{
    switch (object.type()) {
    case QsonObject::ListType:
        return qsonListToVariant(object.toList());
    case QsonObject::MapType:
        return qsonMapToVariant(object.toMap());
    case QsonObject::StringType:
        return QsonElement(object).value<QString>();
    case QsonObject::DoubleType:
        return QsonElement(object).value<double>();
    case QsonObject::IntType:
        return QsonElement(object).value<qint64>();
    case QsonObject::UIntType:
        return QsonElement(object).value<quint64>();
    case QsonObject::BoolType:
        return QsonElement(object).value<bool>();
    case QsonObject::NullType:
        return QVariant();
    default:
        break;
    }
    return QVariant();
}

QsonObject variantToQson(const QVariant &object)
{
    switch (object.type()) {
    case QVariant::Map: {
        QsonMap result;
        QVariantMap map = object.toMap();
        foreach (const QString &key, map.keys()) {
            QsonObject obj = variantToQson(map.value(key));
            result.insert(key, obj);
        }
        return result;
    }
    case QVariant::List:
    case QVariant::StringList: {
        QsonList result;
        foreach (const QVariant &v, object.toList()) {
            QsonObject obj = variantToQson(v);
            result.append(obj);
        }
        return result;
    }
    case QVariant::String: {
        QsonElement result;
        result.setValue(object.toString());
        return result;
    }
    case QVariant::Double: {
        QsonElement result;
        result.setValue(object.toDouble());
        return result;
    }
    case QVariant::LongLong:
    case QVariant::Int: {
        QsonElement result;
        result.setValue(object.toLongLong());
        return result;
    }
    case QVariant::ULongLong:
    case QVariant::UInt: {
        QsonElement result;
        result.setValue(object.toULongLong());
        return result;
    }
    case QVariant::Bool: {
        QsonElement result;
        result.setValue(object.toBool());
        return result;
    }
    default:
        break;
    }
    return QsonObject();
}

QT_END_NAMESPACE_JSONDB
