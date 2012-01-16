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

#ifndef SCHEMAMANAGER_IMPL_P_H
#define SCHEMAMANAGER_IMPL_P_H

#include "schemamanager_p.h"
#include "schema-validation/object.h"

QT_BEGIN_NAMESPACE_JSONDB

bool SchemaManager::contains(const QString &name) const
{
    return m_schemas.contains(name);
}

QJsonObject SchemaManager::value(const QString &name) const
{
    return m_schemas.value(name).first;
}

SchemaValidation::Schema<QJsonObjectTypes> SchemaManager::schema(const QString &schemaName, QJsonObjectTypes::Service *service)
{
    QJsonObjectSchemaPair schemaPair = m_schemas.value(schemaName);
    ensureCompiled(schemaName, &schemaPair, service);
    return schemaPair.second;
}

QJsonObject SchemaManager::take(const QString &name)
{
    return m_schemas.take(name).first;
}

QJsonObject SchemaManager::insert(const QString &name, QJsonObject &schema)
{
    m_schemas.insert(name, qMakePair(schema, SchemaValidation::Schema<QJsonObjectTypes>()));
    return QJsonObject();
}

inline QJsonObject SchemaManager::ensureCompiled(const QString &schemaName, QJsonObjectSchemaPair *pair, QJsonObjectTypes::Service *callbacks)
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

inline QJsonObject SchemaManager::validate(const QString &schemaName, JsonDbObject object)
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

QT_END_NAMESPACE_JSONDB

#endif // SCHEMAMANAGER_IMPL_P_H
