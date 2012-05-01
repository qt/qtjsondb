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

#include "jsondbowner.h"
#include "jsondbpartition.h"
#include "jsondbsettings.h"
#include "jsondbstrings.h"
#include <qdebug.h>

#ifdef Q_OS_UNIX
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#endif

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

JsonDbOwner::JsonDbOwner( QObject *parent )
    : QObject(parent), mAllowAll(false)
{
}

void cleanQueryList(QList<JsonDbQuery *> &queryList)
{
    foreach (JsonDbQuery *q, queryList)
        delete q;
    queryList.clear();
}

JsonDbOwner::~JsonDbOwner()
{
    QMap<QString, QList<JsonDbQuery *> > list;
    foreach (list, mAllowedObjectQueries) {
        foreach (QList<JsonDbQuery *> queryList, list)
            cleanQueryList(queryList);
    }
}

void JsonDbOwner::setAllowedObjects(const QString &partition, const QString &op,
                                    const QList<QString> &queries)
{
    mAllowedObjects[partition][op] = queries;
    cleanQueryList(mAllowedObjectQueries[partition][op]);
    QJsonObject bindings;
    bindings.insert(QLatin1String("domain"), domain());
    bindings.insert(QLatin1String("owner"), ownerId());
    foreach (const QString &query, queries) {
        mAllowedObjectQueries[partition][op].append(JsonDbQuery::parse(query, bindings));
    }
}

void JsonDbOwner::setCapabilities(QJsonObject &applicationCapabilities, JsonDbPartition *partition)
{
    QJsonObject request;
    GetObjectsResult result = partition->getObjects(JsonDbString::kTypeStr, QStringLiteral("Capability"));
    JsonDbObjectList translations = result.data;
    //qDebug() << "JsonDbOwner::setCapabilities" << "translations" << translations;

    QMap<QString, QSet<QString> > allowedObjects;
    const QStringList ops = (QStringList() << QStringLiteral("read") << QStringLiteral("write") << QStringLiteral("setOwner"));

    for (int i = 0; i < translations.size(); ++i) {
        JsonDbObject translation = translations.at(i);
        QString name = translation.value(QStringLiteral("name")).toString();
        QString partition = translation.value(QStringLiteral("partition")).toString();
        partition = partition.replace(QStringLiteral("%owner"), ownerId());
        if (applicationCapabilities.contains(name)) {
            QJsonObject accessRules = translation.value(QStringLiteral("accessRules")).toObject();
            QVariantList accessTypesAllowed =
                applicationCapabilities.value(name).toArray().toVariantList();
            foreach (QVariant accessTypeAllowed, accessTypesAllowed) {
                QJsonObject accessTypeTranslation =
                    accessRules.value(accessTypeAllowed.toString()).toObject();
                foreach (const QString op, ops) {
                    if (accessTypeTranslation.contains(op)) {
                        QStringList rules;
                        QJsonArray jsonRules = accessTypeTranslation.value(op).toArray();
                        for (int r = 0; r < jsonRules.size(); r++) {
                            rules.append(jsonRules[r].toString());
                        }
                        allowedObjects[op].unite(QSet<QString>::fromList(rules));
                    }
                }
            }
        }
        foreach (const QString op, ops) {
            if (!allowedObjects.value(op).empty())
                setAllowedObjects(partition, op, allowedObjects.value(op).toList());
        }
        allowedObjects.clear();
    }
    if (jsondbSettings->verbose()) {
        qDebug() << "setCapabilities" << mOwnerId;
        qDebug() << mAllowedObjects << endl;
    }
}

bool JsonDbOwner::isAllowed(JsonDbObject &object, const QString &partition,
                            const QString &op) const
{
    if (mAllowAll || !jsondbSettings->enforceAccessControl())
        return true;

    QString _type = object[QLatin1String("_type")].toString();
    QStringList domainParts = _type.split(QLatin1Char('.'), QString::SkipEmptyParts);
    QString typeDomain;
    // TODO: handle reverse domains like co.uk.foo, co.au.bar, co.nz.foo , .... correctly
    if (domainParts.count() > 2)
        typeDomain = _type.left(_type.lastIndexOf(QLatin1Char('.'))+1);
    else
        // Capability queries with _typeDomain should fail, if there is not long enough domain
        typeDomain = QLatin1String("public.domain.fail.");
    QJsonValue tdval = QJsonValue(typeDomain);

    QList<JsonDbQuery *> queries = mAllowedObjectQueries[partition][op];
    foreach (JsonDbQuery *query, queries) {
        query->bind(QString(QLatin1String("typeDomain")), tdval);
        if (query->match(object, NULL, NULL))
            return true;
    }
    queries = mAllowedObjectQueries[QLatin1String("all")][op];
    foreach (JsonDbQuery *query, queries) {
        query->bind(QString(QLatin1String("typeDomain")), tdval);
        if (query->match(object, NULL, NULL))
            return true;
    }
    if (jsondbSettings->verbose()) {
        qDebug () << "Not allowed" << ownerId() << partition << _type << op;
    }
    return false;
}

bool JsonDbOwner::_setOwnerCapabilities(struct passwd *pwd, JsonDbPartition *partition)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    mOwnerId = QString::fromLocal8Bit(pwd->pw_name);

    // Parse domain from username
    // TODO: handle reverse domains like co.uk.foo, co.au.bar, co.nz.foo , .... correctly
    QStringList domainParts = mOwnerId.split(QLatin1Char('.'), QString::SkipEmptyParts);
    if (domainParts.count() > 2)
        mDomain = domainParts.at(0)+QLatin1Char('.')+domainParts.at(1);
    else
        mDomain = QStringLiteral("public");

    if (jsondbSettings->debug())
        qDebug() << "username" << mOwnerId << "uid" << pwd->pw_uid << "domain set to" << mDomain;

    // Get capabilities from supplementary groups
    if (pwd->pw_uid) {
        int ngroups = 128;
        gid_t groups[128];
        bool setOwner = false;
        QJsonObject capabilities;
        if (::getgrouplist(pwd->pw_name, pwd->pw_gid, groups, &ngroups) != -1) {
            struct group *gr;
            for (int i = 0; i < ngroups; i++) {
                gr = ::getgrgid(groups[i]);
                if (gr && ::strcasecmp (gr->gr_name, "identity") == 0)
                    setOwner = true;
            }
            // Add the default group for everybody
            QJsonArray def;
            def.append(QJsonValue(QLatin1String("rw")));
            capabilities.insert(QLatin1String("default"), def);
            // Start from 1 to omit the primary group
            for (int i = 1; i < ngroups; i++) {
                gr = ::getgrgid(groups[i]);
                QJsonArray value;
                if (!gr || ::strcasecmp (gr->gr_name, "identity") == 0)
                    continue;
                if (setOwner)
                    value.append(QJsonValue(QLatin1String("setOwner")));
                value.append(QJsonValue(QLatin1String("rw")));
                capabilities.insert(QString::fromLocal8Bit(gr->gr_name), value);
                if (jsondbSettings->debug())
                    qDebug() << "Adding capability" << QString::fromLocal8Bit(gr->gr_name)
                             << "to user" << mOwnerId << "setOwner =" << setOwner;
            }
            if (ngroups)
                setCapabilities(capabilities, partition);
        } else {
            qWarning() << Q_FUNC_INFO << mOwnerId << "belongs to too many groups (>128)";
        }
    } else {
        // root can access all
        setAllowAll(true);
    }
#else
    Q_UNUSED(pwd);
    Q_UNUSED(partition);
#endif
    return true;
}

bool JsonDbOwner::setOwnerCapabilities(QString username, JsonDbPartition *partition)
{
    if (!jsondbSettings->enforceAccessControl()) {
        setAllowAll(true);
        return true;
    }
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    struct passwd *pwd = ::getpwnam(username.toLocal8Bit());
    if (!pwd) {
        qWarning() << Q_FUNC_INFO << "pwd entry for" << username <<
                      "not found" << strerror(errno);
        return false;
    }
    return _setOwnerCapabilities (pwd, partition);
#else
    Q_UNUSED(username);
    Q_UNUSED(partition);
    setAllowAll(true);
    return true;
#endif
}

bool JsonDbOwner::setOwnerCapabilities(uid_t uid, JsonDbPartition *partition)
{
    if (!jsondbSettings->enforceAccessControl()) {
        setAllowAll(true);
        return true;
    }
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    struct passwd *pwd = ::getpwuid(uid);
    if (!pwd) {
        qWarning() << Q_FUNC_INFO << "pwd entry for" << uid <<
                      "not found" << strerror(errno);
        return false;
    }
    return _setOwnerCapabilities (pwd, partition);
#else
    Q_UNUSED(uid);
    Q_UNUSED(partition);
    setAllowAll(true);
    return true;
#endif
}

#include "moc_jsondbowner.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
