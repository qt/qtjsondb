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

#ifndef JSONDB_SCHEMA_MANAGER_P_H
#define JSONDB_SCHEMA_MANAGER_P_H

#include "jsondb-global.h"

#include <QtCore/qstring.h>
#include <QtCore/qpair.h>
#include <QtCore/qmap.h>

#include "schema-validation/object.h"
#include "jsondbobjecttypes_p.h"
#include "jsondbobject.h"

QT_BEGIN_NAMESPACE_JSONDB

//FIXME This can have better performance
class JsonDbSchemaManager
{
public:
    inline bool contains(const QString &name) const;
    inline QJsonObject value(const QString &name) const;
    inline SchemaValidation::Schema<QJsonObjectTypes> schema(const QString &name, QJsonObjectTypes::Service *service);
    inline QJsonObject take(const QString &name);
    inline QJsonObject insert(const QString &name, QJsonObject &schema);

    inline QJsonObject validate(const QString &schemaName, JsonDbObject object);

private:
    typedef QPair<QJsonObject, SchemaValidation::Schema<QJsonObjectTypes> > QJsonObjectSchemaPair;
    inline QJsonObject ensureCompiled(const QString &schemaName, QJsonObjectSchemaPair *pair, QJsonObjectTypes::Service *service);

    QMap<QString, QJsonObjectSchemaPair> m_schemas;
};

QT_END_NAMESPACE_JSONDB

#endif // JSONDB_SCHEMA_MANAGER_P_H
