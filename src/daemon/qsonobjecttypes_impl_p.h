/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef QSONOBJECTTYPES_IMPL_P_H
#define QSONOBJECTTYPES_IMPL_P_H

#include "jsondb-global.h"
#include "qsonobjecttypes_p.h"
#include "schemamanager_p.h"

QT_BEGIN_NAMESPACE_JSONDB

inline QsonObjectTypes::ValueList::ValueList(const QsonList list) : QsonList(list)
{}

inline uint QsonObjectTypes::ValueList::count() const
{
    return QsonList::count();
}

inline QsonObjectTypes::ValueList::const_iterator QsonObjectTypes::ValueList::constBegin() const
{
    return const_iterator(0, this);
}

inline QsonObjectTypes::ValueList::const_iterator QsonObjectTypes::ValueList::constEnd() const
{
    return const_iterator(count(), this);
}

inline QsonObjectTypes::ValueList::const_iterator::const_iterator()
    : m_index(-1)
    , m_list(0)
{}

inline QsonObjectTypes::Value QsonObjectTypes::ValueList::const_iterator::operator *() const
{
    Q_ASSERT(isValid());
    return Value(m_index, *m_list);
}

inline bool QsonObjectTypes::ValueList::const_iterator::operator !=(const const_iterator &other) const
{
    return m_index != other.m_index || m_list != other.m_list;
}

inline QsonObjectTypes::ValueList::const_iterator& QsonObjectTypes::ValueList::const_iterator::operator ++()
{
    m_index++;
    return *this;
}

inline QsonObjectTypes::ValueList::const_iterator::const_iterator(int begin, const QsonList *list)
    : m_index(begin)
    , m_list(list)
{}

inline bool QsonObjectTypes::ValueList::const_iterator::isValid() const
{
    return m_index != -1 && m_index < m_list->count();
}

inline QsonObjectTypes::Value::Value(Key propertyName, const QsonMap &map)
    : m_index(-1)
    , m_property(propertyName)
    , m_object(map)
    , m_type(propertyName.isEmpty() ? RootMap : Map)
{}

inline QsonObjectTypes::Value::Value(const int index, const QsonList &list)
    : m_index(index)
    , m_object(list)
    , m_type(List)
{}

inline int QsonObjectTypes::Value::toInt(bool *ok) const
{
    if (m_intCache.isValid()) {
        *ok = true;
        return m_intCache.value();
    }
    int result;
    QsonObject::Type type;
    switch (m_type) {
    case Map:
        type = typeMap();
        *ok =  type == QsonObject::IntType || type == QsonObject::UIntType;
        result = map()->valueInt(m_property);
        m_intCache.set(*ok, result);
        return result;
    case List:
        type = typeList();
        *ok = type == QsonObject::IntType || type == QsonObject::UIntType;
        result = list()->intAt(m_index);
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

inline double QsonObjectTypes::Value::toDouble(bool *ok) const
{
    if (m_doubleCache.isValid()) {
        *ok = true;
        return m_doubleCache.value();
    }
    double result;
    QsonObject::Type type;
    switch (m_type) {
    case Map:
        type = typeMap();
        *ok = type == QsonObject::DoubleType
                || type == QsonObject::IntType
                || type == QsonObject::UIntType;
        result = map()->valueDouble(m_property);
        m_doubleCache.set(*ok, result);
        return result;
    case List:
        type = typeList();
        *ok = type == QsonObject::DoubleType
                || type == QsonObject::IntType
                || type == QsonObject::UIntType;
        result = list()->doubleAt(m_index);
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

inline QsonObjectTypes::ValueList QsonObjectTypes::Value::toList(bool *ok) const
{
    *ok = true;
    switch (m_type) {
    case Map:
        return map()->subList(m_property);
    case List:
        return list()->listAt(m_index);
    case RootMap:
        return m_object.toList();
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return ValueList(QsonList());
}

inline QString QsonObjectTypes::Value::toString(bool *ok) const
{
    switch (m_type) {
    case Map:
        *ok = typeMap() == QsonObject::StringType;
        return map()->valueString(m_property);
    case List:
        *ok = typeList() == QsonObject::StringType;
        return list()->stringAt(m_index);
    case RootMap:
        *ok = true;
        return QString(); // useful for debugging
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return QString();
}

inline bool QsonObjectTypes::Value::toBoolean(bool *ok) const
{
    switch (m_type) {
    case Map:
        *ok = typeMap() == QsonObject::BoolType;
        return map()->valueBool(m_property);
    case List:
        *ok = typeList() == QsonObject::BoolType;
        return list()->boolAt(m_index);
    case RootMap:
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return false;
}

inline void QsonObjectTypes::Value::toNull(bool *ok) const
{
    switch (m_type) {
    case Map:
        *ok = typeMap() == QsonObject::NullType;
    case List:
        *ok = typeList() == QsonObject::NullType;
    case RootMap:
    default:
        Q_ASSERT(false);
    }
    *ok = false;
}

inline QsonObjectTypes::Object QsonObjectTypes::Value::toObject(bool *ok) const
{
    switch (m_type) {
    case Map:
        *ok = typeMap() == QsonObject::MapType;
        return map()->subObject(m_property);
    case List:
        *ok = typeList() == QsonObject::MapType;
        return list()->objectAt(m_index);
    case RootMap:
        *ok =  true;
        Q_ASSERT_X(sizeof(Object) == sizeof(m_object), Q_FUNC_INFO, "We are assuming that Object and QsonObject has the same binary representation");
        return static_cast<Object>(m_object);
    default:
        Q_ASSERT(false);
    }
    *ok = false;
    return Object(QsonMap());
}

inline const QsonMap *QsonObjectTypes::Value::map() const
{
    Q_ASSERT(m_type == Map);
    Q_ASSERT(!m_property.isEmpty());
    Q_ASSERT_X(sizeof(QsonMap) == sizeof(m_object), Q_FUNC_INFO, "We are assuming that QsonMap and QsonObject has the same binary representation");
    return static_cast<const QsonMap*>(&m_object);
}

inline const QsonList *QsonObjectTypes::Value::list() const
{
    Q_ASSERT(m_type == List);
    Q_ASSERT(m_index >= 0);
    Q_ASSERT_X(sizeof(QsonList) == sizeof(m_object), Q_FUNC_INFO, "We are assuming that QsonList and QsonObject has the same binary representation");
    return static_cast<const QsonList*>(&m_object);
}

inline QsonObject::Type QsonObjectTypes::Value::typeMap() const
{
    if (m_qsonTypeCache.isValid())
        return m_qsonTypeCache.value();
    QsonObject::Type result = map()->valueType(m_property);
    m_qsonTypeCache.set(true, result);
    return result;
}

inline QsonObject::Type QsonObjectTypes::Value::typeList() const
{
    if (m_qsonTypeCache.isValid())
        return m_qsonTypeCache.value();
    QsonObject::Type result = list()->typeAt(m_index);
    m_qsonTypeCache.set(true, result);
    return result;
}

inline QsonObjectTypes::Object::Object()
{}

inline QsonObjectTypes::Object::Object(const QsonMap &map)
    : QsonMap(map)
{}

inline QsonObjectTypes::Value QsonObjectTypes::Object::property(const QsonObjectTypes::Key& name) const
{
    return Value(name, *this);
}

inline QList<QsonObjectTypes::Key> QsonObjectTypes::Object::propertyNames() const { return QsonMap::keys(); }

inline QsonObjectTypes::Service::Service(SchemaManager *schemas)
    : m_schemas(schemas)
{}

inline QsonMap QsonObjectTypes::Service::error() const
{
    return m_errorMap;
}

inline void QsonObjectTypes::Service::setError(const QString &message)
{
    m_errorMap.insert(JsonDbString::kCodeStr, JsonDbError::FailedSchemaValidation);
    m_errorMap.insert(JsonDbString::kMessageStr, message);
}

inline SchemaValidation::Schema<QsonObjectTypes> QsonObjectTypes::Service::loadSchema(const QString &schemaName)
{
    return m_schemas->schema(schemaName, this);
}

QT_END_NAMESPACE_JSONDB

#endif // QSONOBJECTTYPES_IMPL_P_H
