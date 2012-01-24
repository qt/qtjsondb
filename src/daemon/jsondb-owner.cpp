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

#include "jsondb-owner.h"
#include "jsondb.h"
#include "jsondb-strings.h"
#include <qdebug.h>

QT_BEGIN_NAMESPACE_JSONDB

bool gEnforceAccessControlPolicies = false;

JsonDbOwner::JsonDbOwner( QObject *parent )
    : QObject(parent), mStorageQuota(-1), mAllowAll(false)
{
    QList<QString> defaultQueries;
    defaultQueries.append(".*");
    setAllowedObjects("read", defaultQueries);
    setAllowedObjects("write", defaultQueries);
}

JsonDbOwner::~JsonDbOwner()
{
}

void JsonDbOwner::setAllowedObjects(const QString &op, const QList<QString> &queries)
{
    mAllowedObjects[op] = queries;
    mAllowedObjectQueries[op].clear();
    QJsonObject nullBindings;
    foreach (const QString &query, queries) {
        mAllowedObjectQueries[op].append(JsonDbQuery::parse(query, nullBindings));
    }
}

void JsonDbOwner::setCapabilities(QJsonObject &applicationCapabilities, JsonDb *jsondb)
{
    QJsonObject request;
    GetObjectsResult result = jsondb->getObjects(JsonDbString::kTypeStr, QString("Capability"));
    JsonDbObjectList translations = result.data;
    //qDebug() << "JsonDbOwner::setCapabilities" << "translations" << translations;

    QMap<QString, QSet<QString> > allowedObjects;
    const QStringList ops = (QStringList() << "read" << "write" << "setOwner");

    for (int i = 0; i < translations.size(); ++i) {
        JsonDbObject translation = translations.at(i);
        QString name = translation.value("name").toString();
        if (applicationCapabilities.contains(name)) {
            QJsonObject accessRules = translation.value("accessRules").toObject();
            QVariantList accessTypesAllowed =
                applicationCapabilities.value(name).toArray().toVariantList();
            foreach (QVariant accessTypeAllowed, accessTypesAllowed) {
                QJsonObject accessTypeTranslation =
                    accessRules.value(accessTypeAllowed.toString()).toObject();
                foreach (const QString op, ops) {
                    if (accessTypeTranslation.contains(op)) {
                        QStringList rules;
                        QJsonArray jsonRules = accessTypeTranslation.value(op).toArray();
                        for (int r = 0; r < jsonRules.size(); r++)
                            rules.append(jsonRules[r].toString());
                        allowedObjects[op].unite(QSet<QString>::fromList(rules));
                    }
                }
            }
        }
    }
    foreach (const QString op, ops) {
        setAllowedObjects(op, allowedObjects.value(op).toList());
    }
    if (gVerbose) {
        qDebug() << "setCapabilities" << mOwnerId;
        qDebug() << mAllowedObjects << endl;
    }
}

bool JsonDbOwner::isAllowed(JsonDbObject &object, const QString &op) const
{
    if (mAllowAll || !gEnforceAccessControlPolicies)
        return true;

    QList<JsonDbQuery> queries = mAllowedObjectQueries[op];
    foreach (const JsonDbQuery &query, queries) {
        if (query.match(object, NULL, NULL))
            return true;
    }
    return false;
}

QT_END_NAMESPACE_JSONDB
