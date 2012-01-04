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

#ifndef QSONOBJECT_H
#define QSONOBJECT_H

#include <QtJsonDbQson/qsonglobal.h>
#include <QtJsonDbQson/private/qsonpage_p.h>

#include <QString>
#include <QHash>
#include <QStringList>
#include <QPair>

QT_BEGIN_NAMESPACE_JSONDB

class QsonList;
class QsonMap;
class QsonElement;

class Q_ADDON_JSONDB_QSON_EXPORT QsonEntry
{
public:
    QsonEntry()
    {
        pageNumber = -1;
        valueOffset = 0;
        valueLength = 0;
    }

    QsonEntry(int special) {
        pageNumber = special;
        valueOffset = 0;
        valueLength = 0;
    }

    int pageNumber;
    qson_size valueOffset;
    int valueLength;
};


class Q_ADDON_JSONDB_QSON_EXPORT QsonObject
{
#ifndef QT_TESTLIB_LIB
protected:
#else
public:
#endif
    // FIXME QHash is not the best container for the cache
    class CachedIndex : protected QHash<QPair<QString, int>, QsonEntry>
    {
        typedef QHash<QPair<QString, int>, QsonEntry> CacheImpl;
        typedef QPair<QString, int> CacheKeyImpl;

    public:
        typedef CacheImpl::const_iterator const_iterator;
        class Cleaner
        {
        public:
            Cleaner(CachedIndex &index)
                : m_index(index)
            {}

            ~Cleaner()
            {
                // FIXME not always we need to clean the cache. Maybe we should implement incremental
                // udpates?
                m_index.clear();
            }
        private:
            CachedIndex &m_index;
        };

        CachedIndex() {}

        const QsonEntry &at(int i)
        {
            Q_ASSERT(i < size());
            return CacheImpl::operator [](CacheKeyImpl(QString(), i));
        }

        void clear()
        {
            // We use erase here, because it doesn't deallocate internal structures (as clear does)
            // There is high chance that we will need to recreate this hash in future.
            CacheImpl::iterator i = CacheImpl::begin();
            while (i != CacheImpl::end())
                i = CacheImpl::erase(i);
            Q_ASSERT(!size());
        }

        int size() const
        {
            return CacheImpl::size();
        }

        bool isEmpty() const
        {
            return !size();
        }

        void append(const QsonEntry &entry)
        {
            Q_ASSERT_X(CacheImpl::isEmpty() || CacheImpl::contains(CacheKeyImpl(QString(), size() - 1))
                       , Q_FUNC_INFO
                       , "Inconsisty in cache detected, this can happen if CachedIndex hasn't been cleaned correctly");
            CacheImpl::insert(CacheKeyImpl(QString(), size()), entry);
        }

        const_iterator constBegin() const
        {
            return CacheImpl::constBegin();
        }

        const_iterator begin() const
        {
            return CacheImpl::begin();
        }

        const_iterator constEnd() const
        {
            return CacheImpl::constEnd();
        }

        const_iterator end() const
        {
            return CacheImpl::end();
        }

        QStringList keys() const
        {
            QStringList result;
            foreach (const CacheKeyImpl &key, CacheImpl::keys())
                result.append(key.first);
            return result;
        }

        bool contains(const QString &key) const
        {
            return CacheImpl::contains(CacheKeyImpl(key, -1));
        }

        const QsonEntry value(const QString &key) const
        {
            return CacheImpl::value(CacheKeyImpl(key, -1));
        }

        QsonEntry &operator[](const QString &key)
        {
            return CacheImpl::operator [](CacheKeyImpl(key, -1));
        }
    };

public:
    enum Type {
        UnknownType,
        ListType,
        MapType,
        DocumentType,
        MetaType,

        NullType,
        BoolType,
        IntType,     // qint64
        UIntType,    // quint64
        DoubleType,
        StringType
    };

    QsonObject();
    explicit QsonObject(Type type);

    enum Special { NullValue = NullType }; // alias to NullType just to avoid typos

protected:
    QsonObject(const QsonContent &pages);

public:
    bool isNull() const
    {
        return mBody.isEmpty() && mHeader->type() == QsonPage::EMPTY_PAGE;
    }
    bool isEmpty() const
    {
        return mBody.isEmpty() && (mHeader->type() != QsonPage::DOCUMENT_HEADER_PAGE);
    }

    Type type() const;

    QsonList toList() const;
    QsonMap toMap() const;

    int dataSize() const;
    QByteArray data() const;

    inline bool operator==(const QsonObject &other) const
    {
        if (*mHeader != *other.mHeader)
            return false;
        if (mBody.size() != other.mBody.size())
            return false;
        for (int i = 0; i < mBody.size(); ++i)
            if (*mBody.at(i) != *other.mBody.at(i))
                return false;
        if (*mFooter != *other.mFooter)
            return false;
        return true;
    }
    inline bool operator!=(const QsonObject &other) const
    {
        return !operator==(other);
    }

#ifndef QT_TESTLIB_LIB
protected:
#endif
    inline void ensurePage(QsonPage::PageType type, bool force = false)
    {
        if (mBody.isEmpty() || force || mBody.last()->type() != type) {
            mBody.append(QsonPagePtr(new QsonPage(type)));
        }
    }

    QSharedDataPointer<QsonPage> mHeader;
    QsonContent mBody;
    QSharedDataPointer<QsonPage> mFooter;
    mutable CachedIndex mIndex;

    friend class QsonList;
    friend class QsonMap;
    friend class QsonParser;
    friend class QsonStream;
};

QT_END_NAMESPACE_JSONDB

#endif // QSONOBJECT_H
