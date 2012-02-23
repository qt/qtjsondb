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

#ifndef JSONDB_OBJECTTYPES_P_H
#define JSONDB_OBJECTTYPES_P_H

#include "jsondb-global.h"
#include "jsondb-strings.h"

#include "schema-validation/object.h"

#include <QPair>

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbSchemaManager;

/**
  \internal
  This is type definition for schema validation framework. It was
  created because of planed change of data representation in jsondb
  (Bson -> Qson -> QtJson). Essentially schema validation is
  independent from data representation. The performance cost of this
  indirection is about 0. We can consider removing it in future or
  leave it and check different data representation (think about
  QJSValue).

  These types define the simplest types in JSON.
  */
class QJsonObjectTypes {
public:
    typedef QString Key;

    class Value;
    class ValueList : protected QJsonArray
    {
    public:
        inline ValueList(const QJsonArray &list);

        // interface
        class const_iterator;
        inline uint size() const;
        inline const_iterator constBegin() const;
        inline const_iterator constEnd() const;
        class const_iterator
        {
            friend class ValueList;
        public:
            inline const_iterator();
            inline Value operator *() const;
            inline bool operator !=(const const_iterator &other) const;
            inline const_iterator& operator ++();

        private:
            inline const_iterator(int begin, const QJsonArray *list);
            inline bool isValid() const;

            int m_index;
            const QJsonArray *m_list;
        };
    };

    class Object;
    class Value
    {
        enum Type {List, Map, RootMap};

        template<class T>
        class Cache : protected QPair<bool, T>
        {
        public:
            Cache()
                : QPair<bool, T>(false, T())
            {}
            bool isValid() const { return QPair<bool, T>::first; }
            T value() const { Q_ASSERT(isValid()); return QPair<bool, T>::second; }
            void set(const bool ok, const T &value) { QPair<bool, T>::first = ok; QPair<bool, T>::second = value; }
        };

    public:
        inline Value(Key propertyName, const QJsonObject &map);
        inline Value(const int index, const QJsonArray &list);

        // interface
        inline int toInt(bool *ok) const;
        inline double toDouble(bool *ok) const;
        inline ValueList toList(bool *ok) const;
        inline QString toString(bool *ok) const;
        inline bool toBool(bool *ok) const;
        inline void toNull(bool *ok) const;
        inline Object toObject(bool *ok) const;

    private:
        inline const QJsonObject map() const;
        inline const QJsonArray list() const;
        inline QJsonValue::Type typeMap() const;
        inline QJsonValue::Type typeList() const;

        const int m_index;
        const Key m_property;

        const QJsonValue m_value;
        const Type m_type;

        mutable Cache<QJsonValue::Type> m_qsonTypeCache;
        mutable Cache<int> m_intCache;
        mutable Cache<double> m_doubleCache;
    };

    class Object : public QJsonObject
    {
    public:
        inline Object(const QJsonObject &map);

        // interface
        inline Object();
        inline Value property(const Key& name) const;
        inline QList<Key> propertyNames() const;
    };

    class Service {
    public:
        inline Service(JsonDbSchemaManager *schemas);
        inline QJsonObject error() const;

        // interface
        inline void setError(const QString &message);
        inline SchemaValidation::Schema<QJsonObjectTypes> loadSchema(const QString &schemaName);

    private:
        JsonDbSchemaManager *m_schemas;
        QJsonObject m_errorMap;
    };
};

QT_END_NAMESPACE_JSONDB

#endif // JSONDB_OBJECTTYPES_P_H
