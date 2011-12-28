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

#ifndef QSONELEMENT_H
#define QSONELEMENT_H

#include <QtJsonDbQson/qsonglobal.h>
#include <QtJsonDbQson/private/qsonobject_p.h>

QT_ADDON_JSONDB_BEGIN_NAMESPACE

class Q_ADDON_JSONDB_QSON_EXPORT QsonElement : public QsonObject
{
public:
    QsonElement() { }
    QsonElement(const QsonObject &object) : QsonObject(object) { } // ### removeme
    QsonElement(const QsonContent &body, const QsonEntry &entry);
    QsonElement(const QByteArray &content);

    inline QsonObject::Type valueType() const
    { return QsonObject::type(); }

    template <typename T>
    T value() const;

    void setValue(QsonObject::Special value);
    void setValue(bool value);
    void setValue(qint64 value);
    inline void setValue(qint32 value)
    { setValue(qint64(value)); }
    void setValue(quint64 value);
    inline void setValue(quint32 value)
    { setValue(quint64(value)); }
    void setValue(double value);
    void setValue(const QString &value);

    // declared but not defined.
    // This function is here only to disambiguate with setValue(quint64) overload
    Q_DECL_DEPRECATED QsonMap& setValue(const char *value);

    bool isNull() const;
};

template <> inline bool QsonElement::value() const
{ return mBody.at(0)->readBool(0); }

template <> inline qint64 QsonElement::value() const
{ return mBody.at(0)->readInt(0, 0); }

template <> inline quint64 QsonElement::value() const
{ return mBody.at(0)->readUInt(0, 0); }

template <> inline qint32 QsonElement::value() const
{ return (qint32)mBody.at(0)->readInt(0, 0); }

template <> inline quint32 QsonElement::value() const
{ return (quint32)mBody.at(0)->readUInt(0, 0); }

template <> inline double QsonElement::value() const
{ return mBody.at(0)->readDouble(0, 0); }

template <> inline QString QsonElement::value() const
{ return mBody.at(0)->readString(0); }

QT_ADDON_JSONDB_END_NAMESPACE

#endif // QSONELEMENT_H
