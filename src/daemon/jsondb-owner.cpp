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

#include "jsondb-owner.h"
#include "jsondb.h"
#include "jsondb-strings.h"
#include <qdebug.h>

QT_ADDON_JSONDB_BEGIN_NAMESPACE

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
    QsonMap nullBindings;
    foreach (const QString &query, queries) {
        mAllowedObjectQueries[op].append(JsonDbQuery::parse(query, nullBindings));
    }
}

void JsonDbOwner::setCapabilities(QsonMap &applicationCapabilities, JsonDb *jsondb)
{
    QsonMap request;
    request.insert(JsonDbString::kQueryStr, (QString("[?%1=\"%2\"]").arg(JsonDbString::kTypeStr).arg("Capability")));
    QsonMap result = jsondb->find(jsondb->owner(), request);
    QsonList translations = result.subObject("result").subList("data");
    //qDebug() << "JsonDbOwner::setCapabilities" << "translations" << translations;

    QMap<QString, QSet<QString> > allowedObjects;
    const QStringList ops = (QStringList() << "read" << "write" << "setOwner");

    for (int i = 0; i < translations.size(); ++i) {
        QsonMap translation = translations.at<QsonMap>(i);
        QString name = translation.valueString("name");
        if (applicationCapabilities.contains(name)) {
            QsonMap accessRules = translation.subObject("accessRules");
            QStringList accessTypesAllowed = applicationCapabilities.value<QsonList>(name).toStringList();
            foreach (QString accessTypeAllowed, accessTypesAllowed) {
                QsonMap accessTypeTranslation = accessRules.subObject(accessTypeAllowed);
                foreach (const QString op, ops) {
                    if (accessTypeTranslation.contains(op)) {
                        allowedObjects[op].unite(QSet<QString>::fromList(accessTypeTranslation.value<QsonList>(op).toStringList()));
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

bool JsonDbOwner::isAllowed (QsonObject &object, const QString &op) const
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

QT_ADDON_JSONDB_END_NAMESPACE
