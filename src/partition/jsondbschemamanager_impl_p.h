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

#ifndef JSONDB_SCHEMA_MANAGER_IMPL_P_H
#define JSONDB_SCHEMA_MANAGER_IMPL_P_H

#include "jsondbschemamanager_p.h"
#include "jsondbschema_p.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

bool JsonDbSchemaManager::contains(const QString &name) const
{
    return m_schemas.contains(name);
}

QJsonObject JsonDbSchemaManager::value(const QString &name) const
{
    return m_schemas.value(name).first;
}

SchemaValidation::Schema<QJsonObjectTypes> JsonDbSchemaManager::schema(const QString &schemaName, QJsonObjectTypes::Service *service)
{
    QJsonObjectSchemaPair schemaPair = m_schemas.value(schemaName);
    ensureCompiled(schemaName, &schemaPair, service);
    return schemaPair.second;
}

QJsonObject JsonDbSchemaManager::take(const QString &name)
{
    return m_schemas.take(name).first;
}

QJsonObject JsonDbSchemaManager::insert(const QString &name, const QJsonObject &schema)
{
    m_schemas.insert(name, qMakePair(schema, SchemaValidation::Schema<QJsonObjectTypes>()));
    return QJsonObject();
}

inline QJsonObject JsonDbSchemaManager::ensureCompiled(const QString &schemaName, QJsonObjectSchemaPair *pair, QJsonObjectTypes::Service *callbacks)
{
    SchemaValidation::Schema<QJsonObjectTypes> schema(pair->second);
    if (!schema.isValid()) {
        // Try to compile schema
        QJsonObjectTypes::Object schemaObject(pair->first);
        SchemaValidation::Schema<QJsonObjectTypes> compiledSchema(schemaObject, callbacks);
        pair->second = compiledSchema;
        m_schemas.insert(schemaName, *pair);
        return callbacks->error();
    }
    return QJsonObject();
}

inline QJsonObject JsonDbSchemaManager::validate(const QString &schemaName, JsonDbObject object)
{
    if (!contains(schemaName))
        return QJsonObject();

    QJsonObjectTypes::Service callbacks(this);
    QJsonObjectSchemaPair schemaPair = m_schemas.value(schemaName);
    ensureCompiled(schemaName, &schemaPair, &callbacks);
    SchemaValidation::Schema<QJsonObjectTypes> schema(schemaPair.second);
    QJsonObjectTypes::Value rootObject(QString(), object);
    /*bool result = */ schema.check(rootObject, &callbacks);
    return callbacks.error();
}

inline void JsonDbSchemaManager::clear()
{
    m_schemas.clear();
}

QT_END_NAMESPACE_JSONDB_PARTITION

#endif // JSONDB_SCHEMA_MANAGER_IMPL_P_H
