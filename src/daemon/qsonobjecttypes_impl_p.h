/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
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
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSONOBJECTTYPES_IMPL_P_H
#define QSONOBJECTTYPES_IMPL_P_H

#include "jsondb-global.h"
#include "qsonobjecttypes_p.h"
#include "schemamanager_p.h"

QT_BEGIN_NAMESPACE_JSONDB

inline QJsonObjectTypes::ValueList::ValueList(const QJsonArray &list) : QJsonArray(list)
{}

inline uint QJsonObjectTypes::ValueList::size() const
{
    return QJsonArray::size();
}

inline QJsonObjectTypes::ValueList::const_iterator QJsonObjectTypes::ValueList::constBegin() const
{
    return const_iterator(0, this);
}

inline QJsonObjectTypes::ValueList::const_iterator QJsonObjectTypes::ValueList::constEnd() const
{
    return const_iterator(size(), this);
}

inline QJsonObjectTypes::ValueList::const_iterator::const_iterator()
    : m_index(-1)
    , m_list(0)
{}

inline QJsonObjectTypes::Value QJsonObjectTypes::ValueList::const_iterator::operator *() const
{
    Q_ASSERT(isValid());
    return Value(m_index, *m_list);
}

inline bool QJsonObjectTypes::ValueList::const_iterator::operator !=(const const_iterator &other) const
{
    return m_index != other.m_index || m_list != other.m_list;
}

inline QJsonObjectTypes::ValueList::const_iterator& QJsonObjectTypes::ValueList::const_iterator::operator ++()
{
    m_index++;
    return *this;
}

inline QJsonObjectTypes::ValueList::const_iterator::const_iterator(int begin, const QJsonArray *list)
    : m_index(begin)
    , m_list(list)
{}

inline bool QJsonObjectTypes::ValueList::const_iterator::isValid() const
{
    return m_index != -1 && m_index < m_list->size();
}

inline QJsonObjectTypes::Value::Value(Key propertyName, const QJsonObject &map)
    : m_index(-1)
    , m_property(propertyName)
    , m_value(map)
    , m_type(propertyName.isEmpty() ? RootMap : Map)
{}

inline QJsonObjectTypes::Value::Value(const int index, const QJsonArray &list)
    : m_index(index)
    , m_value(list)
    , m_type(List)
{}

inline int QJsonObjectTypes::Value::toInt(bool *ok) const
{
    if (m_intCache.isValid()) {
        *ok = true;
        return m_intCache.value();
    }
    int result = 0;
    QJsonValue::Type type;
    switch (m_type) {
    case Map:
        type = typeMap();
        if (type == QJsonValue::Double) {
            QJsonValue v = map().value(m_property);
            double doubleResult = v.toDouble();
            int intResult = (int)doubleResult;
            if ((double)intResult == doubleResult) {
                *ok = true;
                result = intResult;
            } else {
                *ok = false;
            }
        } else {
            *ok = false;
        }
        m_intCache.set(*ok, result);
        return result;
    case List:
        type = typeList();
        if (type == QJsonValue::Double) {
            QJsonValue v = list().at(m_index);
            double doubleResult = v.toDouble();
            int intResult = (int)doubleResult;
            if ((double)intResult == doubleResult) {
                *ok = true;
                result = intResult;
            } else {
                *ok = false;
            }
        } else {
            *ok = false;
        }
        m_intCache.set(*ok, result);
        return result;
    case RootMap:
        break;
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return -1;
}

inline double QJsonObjectTypes::Value::toDouble(bool *ok) const
{
    if (m_doubleCache.isValid()) {
        *ok = true;
        return m_doubleCache.value();
    }
    double result;
    QJsonValue::Type type;
    switch (m_type) {
    case Map:
        type = typeMap();
        *ok = type == QJsonValue::Double;
        result = map().value(m_property).toDouble();
        m_doubleCache.set(*ok, result);
        return result;
    case List:
        type = typeList();
        *ok = type == QJsonValue::Double;
        result = list().at(m_index).toDouble();
        m_doubleCache.set(*ok, result);
        return result;
    case RootMap:
        break;
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return -1;
}

inline QJsonObjectTypes::ValueList QJsonObjectTypes::Value::toList(bool *ok) const
{
    *ok = true;
    switch (m_type) {
    case Map:
        *ok = typeMap() == QJsonValue::Array;
        return map().value(m_property).toArray();
    case List:
        *ok = typeList() == QJsonValue::Array;
        return list().at(m_index).toArray();
    case RootMap:
        return m_value.toArray();
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return ValueList(QJsonArray());
}

inline QString QJsonObjectTypes::Value::toString(bool *ok) const
{
    switch (m_type) {
    case Map:
        *ok = typeMap() == QJsonValue::String;
        return map().value(m_property).toString();
    case List:
        *ok = typeList() == QJsonValue::String;
        return list().at(m_index).toString();
    case RootMap:
        *ok = true;
        return QString(); // useful for debugging
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return QString();
}

inline bool QJsonObjectTypes::Value::toBool(bool *ok) const
{
    switch (m_type) {
    case Map:
        *ok = typeMap() == QJsonValue::Bool;
        return map().value(m_property).toBool();
    case List:
        *ok = typeList() == QJsonValue::Bool;
        return list().at(m_index).toBool();
    case RootMap:
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return false;
}

inline void QJsonObjectTypes::Value::toNull(bool *ok) const
{
    switch (m_type) {
    case Map:
        *ok = typeMap() == QJsonValue::Null;
    case List:
        *ok = typeList() == QJsonValue::Null;
    case RootMap:
    default:
        Q_ASSERT(false);
    }
    *ok = false;
}

inline QJsonObjectTypes::Object QJsonObjectTypes::Value::toObject(bool *ok) const
{
    switch (m_type) {
    case Map:
        *ok = typeMap() == QJsonValue::Object;
        return map().value(m_property).toObject();
    case List:
        *ok = typeList() == QJsonValue::Object;
        return list().at(m_index).toObject();
    case RootMap:
        *ok =  true;
        return static_cast<Object>(m_value.toObject());
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return Object(QJsonObject());
}

inline const QJsonObject QJsonObjectTypes::Value::map() const
{
    Q_ASSERT(m_type == Map);
    Q_ASSERT(!m_property.isEmpty());
    return m_value.toObject();
}

inline const QJsonArray QJsonObjectTypes::Value::list() const
{
    Q_ASSERT(m_type == List);
    Q_ASSERT(m_index >= 0);
    return m_value.toArray();
}

inline QJsonValue::Type QJsonObjectTypes::Value::typeMap() const
{
    if (m_qsonTypeCache.isValid())
        return m_qsonTypeCache.value();
    QJsonValue::Type result = map().value(m_property).type();
    m_qsonTypeCache.set(true, result);
    return result;
}

inline QJsonValue::Type QJsonObjectTypes::Value::typeList() const
{
    if (m_qsonTypeCache.isValid())
        return m_qsonTypeCache.value();
    QJsonValue::Type result = list().at(m_index).type();
    m_qsonTypeCache.set(true, result);
    return result;
}

inline QJsonObjectTypes::Object::Object()
{}

inline QJsonObjectTypes::Object::Object(const QJsonObject &map)
    : QJsonObject(map)
{}

inline QJsonObjectTypes::Value QJsonObjectTypes::Object::property(const QJsonObjectTypes::Key& name) const
{
    return Value(name, *this);
}

inline QList<QJsonObjectTypes::Key> QJsonObjectTypes::Object::propertyNames() const { return QJsonObject::keys(); }

inline QJsonObjectTypes::Service::Service(SchemaManager *schemas)
    : m_schemas(schemas)
{}

inline QJsonObject QJsonObjectTypes::Service::error() const
{
    return m_errorMap;
}

inline void QJsonObjectTypes::Service::setError(const QString &message)
{
    m_errorMap.insert(JsonDbString::kCodeStr, JsonDbError::FailedSchemaValidation);
    m_errorMap.insert(JsonDbString::kMessageStr, message);
}

inline SchemaValidation::Schema<QJsonObjectTypes> QJsonObjectTypes::Service::loadSchema(const QString &schemaName)
{
    return m_schemas->schema(schemaName, this);
}

QT_END_NAMESPACE_JSONDB

#endif // QSONOBJECTTYPES_IMPL_P_H
