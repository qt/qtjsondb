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

#include "jsondbowner.h"
#include "jsondbpartition.h"
#include "jsondbpartition_p.h"
#include "jsondbsettings.h"
#include "jsondbqueryparser.h"
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

JsonDbOwner::~JsonDbOwner()
{
}

void JsonDbOwner::setAllowedObjects(const QString &partition, const QString &op,
                                    const QList<QString> &queries)
{
    mAllowedObjects[partition][op] = queries;
    mAllowedObjectQueries[partition][op].clear();
    QMap<QString, QJsonValue> bindings;
    bindings.insert(QStringLiteral("domain"), domain());
    bindings.insert(QStringLiteral("owner"), ownerId());
    foreach (const QString &query, queries) {
        JsonDbQueryParser parser;
        parser.setQuery(query);
        parser.setBindings(bindings);
        if (!parser.parse())
            Q_ASSERT(false);
        mAllowedObjectQueries[partition][op].append(parser.result());
    }
}

void JsonDbOwner::setCapabilities(QJsonObject &applicationCapabilities, JsonDbPartition *partition)
{
    GetObjectsResult result = partition->d_func()->getObjects(JsonDbString::kTypeStr, JsonDbString::kCapabilityTypeStr);
    JsonDbObjectList translations = result.data;
    //qDebug() << "JsonDbOwner::setCapabilities" << "translations" << translations;

    QMap<QString, QSet<QString> > allowedObjects;
    const QStringList ops = (QStringList() << QStringLiteral("read") << QStringLiteral("write") << QStringLiteral("setOwner"));

    for (int i = 0; i < translations.size(); ++i) {
        JsonDbObject translation = translations.at(i);
        QString name = translation.value(QStringLiteral("name")).toString();
        QString partitionName = translation.value(QStringLiteral("partition")).toString();
        partitionName = partitionName.replace(QStringLiteral("%owner"), ownerId());
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
                setAllowedObjects(partitionName, op, allowedObjects.value(op).toList());
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

    QString _type = object[QStringLiteral("_type")].toString();
    QStringList domainParts = _type.split(QLatin1Char('.'), QString::SkipEmptyParts);
    QString typeDomain;
    // TODO: handle reverse domains like co.uk.foo, co.au.bar, co.nz.foo , .... correctly
    if (domainParts.count() > 2)
        typeDomain = _type.left(_type.lastIndexOf(QLatin1Char('.'))+1);
    else
        // Capability queries with _typeDomain should fail, if there is not long enough domain
        typeDomain = QStringLiteral("public.domain.fail.");
    QJsonValue tdval = QJsonValue(typeDomain);

    QList<JsonDbQuery> queries = mAllowedObjectQueries[partition][op];
    for (int i = 0; i < queries.size(); ++i) {
        JsonDbQuery query = queries.at(i);
        query.bindings.insert(QStringLiteral("typeDomain"), tdval);
        if (query.match(object, NULL, NULL))
            return true;
    }
    queries = mAllowedObjectQueries[QStringLiteral("all")][op];
    for (int i = 0; i < queries.size(); ++i) {
        JsonDbQuery query = queries.at(i);
        query.bindings.insert(QStringLiteral("typeDomain"), tdval);
        if (query.match(object, NULL, NULL))
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

#ifndef Q_OS_WIN32
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
#endif

#include "moc_jsondbowner.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
