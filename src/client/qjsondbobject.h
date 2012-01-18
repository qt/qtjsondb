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

#ifndef JSONDB_OBJECT_H
#define JSONDB_OBJECT_H

#include <QtCore/QJsonObject>
#include <QtCore/QUuid>

#include <QtJsonDb/qjsondbglobal.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class Q_JSONDB_EXPORT QJsonDbObject : public QJsonObject
{
public:
    QJsonDbObject();
    QJsonDbObject(const QJsonObject &other);

    QJsonDbObject &operator=(const QJsonObject &other);

    QUuid uuid() const;
    void setUuid(const QUuid &uuid);

    static QUuid createUuid();
    static QUuid createUuidFromString(const QString &id);

// ### TODO: put me back after QUuid has explicit constructors
//    QJsonObject::iterator insert(const QString &key, const QUuid &value);
//#if !defined(Q_NO_USING_KEYWORD)
//    using QJsonObject::insert;
//#else
//    inline QJsonObject::iterator insert(const QString &key, const QJsonValue &value)
//    { return QJsonObject::insert(key, value); }
//#endif
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_OBJECT_H
