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

#ifndef SCHEMAMANAGER_IMPL_P_H
#define SCHEMAMANAGER_IMPL_P_H

#include "schemamanager_p.h"
#include "schema-validation/object.h"

namespace QtAddOn { namespace JsonDb {

bool SchemaManager::contains(const QString &name) const
{
    return m_schemas.contains(name);
}

QsonMap SchemaManager::value(const QString &name) const
{
    return m_schemas.value(name).first;
}

SchemaValidation::Schema<QsonObjectTypes> SchemaManager::schema(const QString &schemaName, QsonObjectTypes::Service *service)
{
    QsonMapSchemaPair schemaPair = m_schemas.value(schemaName);
    ensureCompiled(schemaName, &schemaPair, service);
    return schemaPair.second;
}

QsonMap SchemaManager::take(const QString &name)
{
    return m_schemas.take(name).first;
}

QsonMap SchemaManager::insert(const QString &name, QsonMap &schema)
{
    m_schemas.insert(name, qMakePair(schema, SchemaValidation::Schema<QsonObjectTypes>()));
    return QsonMap();
}

inline QsonMap SchemaManager::ensureCompiled(const QString &schemaName, QsonMapSchemaPair *pair, QsonObjectTypes::Service *callbacks)
{
    SchemaValidation::Schema<QsonObjectTypes> schema(pair->second);
    if (!schema.isValid()) {
        // Try to compile schema
        QsonObjectTypes::Object schemaObject(pair->first);
        SchemaValidation::Schema<QsonObjectTypes> compiledSchema(schemaObject, callbacks);
        pair->second = compiledSchema;
        m_schemas.insert(schemaName, *pair);
        return callbacks->error();
    }
    return QsonMap();
}

inline QsonMap SchemaManager::validate(const QString &schemaName, QsonMap object)
{
    if (!contains(schemaName))
        return QsonMap();

    QsonObjectTypes::Service callbacks(this);
    QsonMapSchemaPair schemaPair = m_schemas.value(schemaName);
    ensureCompiled(schemaName, &schemaPair, &callbacks);
    SchemaValidation::Schema<QsonObjectTypes> schema(schemaPair.second);
    QsonObjectTypes::Value rootObject(QString(), object);
    /*bool result = */ schema.check(rootObject, &callbacks);
    return callbacks.error();
}

} } // end namespace QtAddOn::JsonDb

#endif // SCHEMAMANAGER_IMPL_P_H
