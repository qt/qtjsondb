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

#ifndef QSONMAP_H
#define QSONMAP_H

#include <QtJsonDbQson/qsonglobal.h>
#include <QtJsonDbQson/private/qsonobject_p.h>
#include <QtJsonDbQson/private/qsonversion_p.h>

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QSet>

QT_BEGIN_NAMESPACE_JSONDB

typedef QMap<QsonVersion, QsonMap> QsonVersionMap;
typedef QSet<QsonVersion> QsonVersionSet;

// api similar to QMap
class Q_ADDON_JSONDB_QSON_EXPORT QsonMap : public QsonObject
{
public:
    QsonMap() : QsonObject(QsonObject::MapType) { }
    QsonMap(const QsonObject &object); // ### removeme

protected:
    QsonMap(const QsonContent pages) : QsonObject(pages) {}

public:
    bool isDocument() const;
    bool isMeta() const;

    int size() const;

    QStringList keys() const;
    bool contains(const QString &key) const;

    QsonObject::Type valueType(const QString &key) const;
    using QsonObject::isNull;
    bool isNull(const QString &key) const;
    bool valueBool(const QString &key, bool fallback = false) const;
    quint64 valueUInt(const QString &key, quint64 fallback = 0) const;
    qint64 valueInt(const QString &key, qint64 fallback = 0) const;
    double valueDouble(const QString &key, double fallback = 0) const;
    QString valueString(const QString &key, const QString &fallback = QString()) const;

    QsonMap subObject(const QString &key) const;
    QsonList subList(const QString &key) const;
    template <typename T>
    T value(const QString &key) const;
    QUuid uuid() const;

    inline quint64 value(const QString &key, quint64 fallback) const
    { return valueUInt(key, fallback); }
    inline qint64 value(const QString &key, qint64 fallback) const
    { return valueInt(key, fallback); }
    inline quint32 value(const QString &key, quint32 fallback) const
    { return (quint32) valueUInt(key, quint64(fallback)); }
    inline qint32 value(const QString &key, qint32 fallback) const
    { return (qint32)valueInt(key, qint64(fallback)); }
    inline double value(const QString &key, double fallback) const
    { return valueDouble(key, fallback); }
    inline QString value(const QString &key, const QString &fallback) const
    { return valueString(key, fallback); }

    QsonMap& insert(const QString &key, QsonObject::Special value);
    QsonMap& insert(const QString &key, bool value);
    QsonMap& insert(const QString &key, quint64 value);
    inline QsonMap& insert(const QString &key, quint32 value)
    { return insert(key, quint64(value)); }
    QsonMap& insert(const QString &key, qint64 value);
    inline QsonMap& insert(const QString &key, qint32 value)
    { return insert(key, qint64(value)); }
    QsonMap& insert(const QString &key, double value);
    QsonMap& insert(const QString &key, const QString &value);
    QsonMap& insert(const QString &key, const QsonObject &value);

    QsonMap& remove(const QString &key);

    // declared but not defined.
    // This function is here only to disambiguate with insert(const QString &, quint64) overload
    Q_DECL_DEPRECATED QsonMap& insert(const QString &key, const char *value);

    bool ensureDocument();
    void generateUuid();
    void computeVersion(bool increaseCount = true);
    bool mergeVersions(const QsonMap &other, bool isReplication = false);

protected:
    void populateMerge(const QsonMap &document, const QsonVersion &version, QsonVersionMap &live, QsonVersionSet &ancestors);
    bool specialHandling(const QString &key, const QString *value = 0);

    template<typename T>
    inline void writeValue(T value)
    {
        if (!mBody.last()->writeValue(value)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeValue(value);
        }
    }

    QsonObject::CachedIndex *index() const;

    friend class QsonList;
    friend class QsonVersion;
};

template <> inline bool QsonMap::value(const QString &key) const
{ return valueBool(key); }
template <> inline quint64 QsonMap::value(const QString &key) const
{ return valueUInt(key); }
template <> inline qint64 QsonMap::value(const QString &key) const
{ return valueInt(key); }
template <> inline quint32 QsonMap::value(const QString &key) const
{ return (quint32) valueUInt(key); }
template <> inline qint32 QsonMap::value(const QString &key) const
{ return (qint32)valueInt(key); }

template <> inline double QsonMap::value(const QString &key) const
{ return valueDouble(key); }
template <> inline QString QsonMap::value(const QString &key) const
{ return valueString(key); }
template <> inline QsonMap QsonMap::value(const QString &key) const
{ return subObject(key); }

QT_END_NAMESPACE_JSONDB

#endif // QSONMAP_H
