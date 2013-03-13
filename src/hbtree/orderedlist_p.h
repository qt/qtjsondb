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

#ifndef ORDERED_LIST_P_H
#define ORDERED_LIST_P_H

#include "hbtreeglobal.h"

#include <QList>
#include <QPair>
#include <QtAlgorithms>
#include <QDebug>

QT_BEGIN_NAMESPACE_HBTREE

template <typename Key, typename Value, typename LessThan = qLess<Key> >
class OrderedList
{
public:
    typedef Key key_type;
    typedef Value value_type;
    typedef QPair<Key, Value> pair_type;

    class const_iterator
    {
        typename QList<pair_type>::const_iterator i;

    public:
        typedef Value value_type;

        inline const_iterator() { }
        inline const_iterator(typename QList<pair_type>::const_iterator ii) : i(ii) { }

        inline bool operator==(const const_iterator &o) const
        { return i == o.i; }
        inline bool operator!=(const const_iterator &o) const
        { return i != o.i; }

        inline const_iterator &operator++()
        {
            i.operator ++();
            return *this;
        }
        inline const_iterator operator++(int)
        {
            const_iterator r = *this;
            i.operator ++();
            return r;
        }

        inline const_iterator &operator--()
        {
            i.operator --();
            return *this;
        }
        inline const_iterator operator--(int)
        {
            const_iterator r = *this;
            i.operator --();
            return r;
        }
        inline const_iterator operator+(int j) const
        {
            const_iterator r = *this;
            r.i += j;
            return r;
        }
        inline const_iterator operator-(int j) const
        {
            const_iterator r = *this;
            r.i -= j;
            return r;
        }
        inline const_iterator &operator+=(int j)
        {
            i += j;
            return *this;
        }
        inline const_iterator &operator-=(int j)
        {
            i -= j;
            return *this;
        }

        inline int operator-(const const_iterator &o) const
        { return i - o.i; }

        inline const key_type &key() const { return (*i).first; }
        inline const value_type &value() const { return (*i).second; }
        inline const key_type &operator*() const { return (*i).first; }
        inline const key_type *operator->() const { return &(*i).first; }
    };

    inline const_iterator constBegin() const
    { return const_iterator(list_.constBegin()); }
    inline const_iterator constEnd() const
    { return const_iterator(list_.constEnd()); }

    inline const_iterator find(const key_type &key) const
    {
        return qBinaryFind(constBegin(), constEnd(), key, cmp_);
    }

    inline const_iterator lowerBound(const key_type &key) const
    {
        return qLowerBound(constBegin(), constEnd(), key, cmp_);
    }

    inline const_iterator upperBound(const key_type &key) const
    {
        return qUpperBound(constBegin(), constEnd(), key, cmp_);
    }

    inline void insert(const key_type &key, const value_type &value)
    {
        const_iterator i = lowerBound(key);
        if (i == constEnd()) {
            list_.append(pair_type(key, value));
        } else if (i.key() == key) {
            list_.replace(i - constBegin(), pair_type(key, value));
        } else {
            list_.insert(i - constBegin(), pair_type(key, value));
        }
    }

    inline bool contains(const key_type &key) const
    { return find(key) != constEnd(); }

    inline value_type &operator[](const key_type &key)
    {
        const_iterator i = lowerBound(key);
        if (i == constEnd()) {
            list_.append(pair_type(key, value_type()));
            return list_.last().second;
        }
        int idx = i - constBegin();
        if (i.key() == key)
            return list_[idx].second;
        list_.insert(idx, pair_type(key, value_type()));
        return list_[idx].second;
    }

    inline value_type value(const key_type &key) const
    {
        const_iterator i = find(key);
        return i == constEnd() ? value_type() : i.value();
    }

    inline value_type value(const key_type &key, const value_type &defaultValue) const
    {
        const_iterator i = find(key);
        return i == constEnd() ? defaultValue: i.value();
    }

    inline void remove(const key_type &key)
    {
        const_iterator i = find(key);
        if (i != constEnd())
            list_.removeAt(i - constBegin());
    }

    inline void reserve(int size) { list_.reserve(size); }
    inline int size() const { return list_.size(); }

    inline void clear()
    { list_.clear(); }

    QList<key_type> keys() const
    {
        QList<key_type> lst;
        lst.reserve(list_.size());
        for (int i = 0; i < list_.size(); ++i)
            lst.append(list_.at(i).first);
        return lst;
    }

    void setLessThan(LessThan cmp)
    { cmp_ = cmp; }
    LessThan lessThan() const
    { return cmp_; }

    inline void uncheckedAppend(const key_type &key, const value_type &value)
    { list_.append(pair_type(key, value)); }

    const QList<pair_type> &rawList() const { return list_; }

    friend QDebug operator << (QDebug dbg, const OrderedList<Key, Value, LessThan>::const_iterator &it)
    {
        dbg.nospace() << "(" << it.key() << "," << it.value() << ")";
        return dbg.space();
    }

private:
    QList<pair_type> list_;
    LessThan cmp_;
};

template <typename Key, typename Value, typename LessThan>
inline QDebug operator << (QDebug dbg, const OrderedList<Key, Value, LessThan> &list)
{
    dbg.nospace() << list.rawList();
    return dbg.space();
}

QT_END_NAMESPACE_HBTREE

#endif // ORDERED_LIST_P_H
