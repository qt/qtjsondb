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

#ifndef QSONLIST_H
#define QSONLIST_H

#include <QtJsonDbQson/qsonglobal.h>
#include <QtJsonDbQson/private/qsonobject_p.h>

QT_BEGIN_NAMESPACE_JSONDB

class Q_ADDON_JSONDB_QSON_EXPORT QsonList : public QsonObject
{
public:
    QsonList() : QsonObject(QsonObject::ListType) { }
    QsonList(const QsonObject &object); //### remove me

protected:
    QsonList(const QsonContent pages) : QsonObject(pages) {}

public:
    int size() const;
    int count() const;

    template<typename T>
    T at(int pos) const;

    QsonObject::Type typeAt(int pos) const;
    bool isNull(int pos) const;
    bool boolAt(int pos) const;
    quint64 uintAt(int pos, quint64 fallback = 0) const;
    qint64 intAt(int pos, qint64 fallback = 0) const;
    double doubleAt(int pos, double fallback = 0) const;
    QString stringAt(int pos) const;

    QsonMap objectAt(int pos) const;
    QsonList listAt(int pos) const;

    QStringList toStringList() const;

    QsonList& append(QsonObject::Special value);
    QsonList& append(bool value);
    QsonList& append(quint64 value);
    inline QsonList& append(quint32 value)
    { return append(quint64(value)); }
    QsonList& append(qint64 value);
    inline QsonList& append(qint32 value)
    { return append(qint64(value)); }
    QsonList& append(double value);
    QsonList& append(const QString &value);
    QsonList& append(const QsonObject &value);

    // declared but not defined.
    // This function is here only to disambiguate with append(quint64) overload
    Q_DECL_DEPRECATED QsonList& append(const char *value);

#ifndef QT_TESTLIB_LIB
protected:
#endif
    QsonObject::CachedIndex *index() const;

    friend class QsonMap;
};

template <> inline bool QsonList::at(int pos) const
{ return boolAt(pos); }
template <> inline quint64 QsonList::at(int pos) const
{ return uintAt(pos); }
template <> inline qint64 QsonList::at(int pos) const
{ return intAt(pos); }
template <> inline quint32 QsonList::at(int pos) const
{ return (quint32)uintAt(pos); }
template <> inline qint32 QsonList::at(int pos) const
{ return (qint32)intAt(pos); }
template <> inline double QsonList::at(int pos) const
{ return doubleAt(pos); }
template <> inline QString QsonList::at(int pos) const
{ return stringAt(pos); }
template <> inline QsonList QsonList::at(int pos) const
{ return listAt(pos); }

QT_END_NAMESPACE_JSONDB

#endif // QSONLIST_H
