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

#ifndef QSONOBJECTTYPES_P_H
#define QSONOBJECTTYPES_P_H

#include "jsondb-global.h"

#include "jsondb-strings.h"
#include <QtJsonDbQson/private/qsonmap_p.h>
#include <QtJsonDbQson/private/qsonlist_p.h>
#include <QtJsonDbQson/private/qsonobject_p.h>

#include "schema-validation/object.h"

#include <QPair>

QT_BEGIN_NAMESPACE_JSONDB

class SchemaManager;

/**
  \internal
  This is type definition for schema validation framework. It was created because of planed change
  of data representation in jsondb (Bson -> Qson). Essentially schema validation is independent from
  data representation. The performance cost of this indirection is about 0. We can consider removing
  it in future or leave it and check different data representation (think about QJSValue).

  These types define the simplest types in JSON.
  */
class QsonObjectTypes {
public:
    typedef QString Key;

    class Value;
    class ValueList : protected QsonList
    {
    public:
        inline ValueList(const QsonList list);

        // interface
        class const_iterator;
        inline uint count() const;
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
            inline const_iterator(int begin, const QsonList *list);
            inline bool isValid() const;

            int m_index;
            const QsonList *m_list;
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
        inline Value(Key propertyName, const QsonMap &map);
        inline Value(const int index, const QsonList &list);

        // interface
        inline int toInt(bool *ok) const;
        inline double toDouble(bool *ok) const;
        inline ValueList toList(bool *ok) const;
        inline QString toString(bool *ok) const;
        inline bool toBoolean(bool *ok) const;
        inline void toNull(bool *ok) const;
        inline Object toObject(bool *ok) const;

    private:
        inline const QsonMap *map() const;
        inline const QsonList *list() const;
        inline QsonObject::Type typeMap() const;
        inline QsonObject::Type typeList() const;

        const int m_index;
        const Key m_property;

        const QsonObject m_object;
        const Type m_type;

        mutable Cache<QsonObject::Type> m_qsonTypeCache;
        mutable Cache<int> m_intCache;
        mutable Cache<double> m_doubleCache;
    };

    class Object : public QsonMap
    {
    public:
        inline Object(const QsonMap &map);

        // interface
        inline Object();
        inline Value property(const Key& name) const;
        inline QList<Key> propertyNames() const;
    };

    class Service {
    public:
        inline Service(SchemaManager *schemas);
        inline QsonMap error() const;

        // interface
        inline void setError(const QString &message);
        inline SchemaValidation::Schema<QsonObjectTypes> loadSchema(const QString &schemaName);

    private:
        SchemaManager *m_schemas;
        QsonMap m_errorMap;
    };
};

QT_END_NAMESPACE_JSONDB

#endif // QSONOBJECTTYPES_P_H
