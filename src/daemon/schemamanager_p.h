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

#ifndef SCHEMAMANAGER_P_H
#define SCHEMAMANAGER_P_H

#include "jsondb-global.h"

#include <QtCore/qstring.h>
#include <QtCore/qpair.h>
#include <QtCore/qmap.h>

#include <QtJsonDbQson/private/qsonmap_p.h>

#include "schema-validation/object.h"
#include "qsonobjecttypes_p.h"

QT_BEGIN_NAMESPACE_JSONDB

//FIXME This can have better performance
class SchemaManager
{
public:
    inline bool contains(const QString &name) const;
    inline QsonMap value(const QString &name) const;
    inline SchemaValidation::Schema<QsonObjectTypes> schema(const QString &name, QsonObjectTypes::Service *service);
    inline QsonMap take(const QString &name);
    inline QsonMap insert(const QString &name, QsonMap &schema);

    inline QsonMap validate(const QString &schemaName, QsonMap object);

private:
    typedef QPair<QsonMap, SchemaValidation::Schema<QsonObjectTypes> > QsonMapSchemaPair;
    inline QsonMap ensureCompiled(const QString &schemaName, QsonMapSchemaPair *pair, QsonObjectTypes::Service *service);

    QMap<QString, QsonMapSchemaPair> m_schemas;
};

QT_END_NAMESPACE_JSONDB

#endif // SCHEMAMANAGER_P_H
