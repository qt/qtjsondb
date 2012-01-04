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

#ifndef JSONDB_OWNER_H
#define JSONDB_OWNER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QSet>

#include "jsondb-global.h"
#include "jsondbquery.h"

#include <QtJsonDbQson/private/qson_p.h>

QT_BEGIN_HEADER

class TestJsonDb;

QT_BEGIN_NAMESPACE_JSONDB

extern bool gEnforceAccessControlPolicies;

class JsonDb;

class JsonDbOwner : public QObject
{
    Q_OBJECT
public:
    JsonDbOwner( QObject *parent = 0 );
    ~JsonDbOwner();
    QString ownerId() const { return mOwnerId; }
    void setOwnerId(const QString &ownerId) {  mOwnerId = ownerId; }
    QString domain() const { return mDomain; }
    void setDomain(const QString &domain) {  mDomain = domain; }

    QList<QString> allowedObjects(const QString &op) const;

    void setAllowedObjects(const QString &op, const QList<QString> &queries);
    void setCapabilities(QsonMap &capabilities, JsonDb *jsondb);
    void setAllowAll(bool allowAll) { mAllowAll = allowAll; }

    bool isAllowed(QsonObject &object, const QString &op) const;
    int storageQuota() const { return mStorageQuota; }
    void setStorageQuota(int storageQuota) { mStorageQuota = storageQuota; }

private:
    QString mOwnerId; // from security object
    QString mDomain;  // from security object
    QMap<QString, QList<QString> > mAllowedObjects; // list of querie strings
    QMap<QString, QList<JsonDbQuery> > mAllowedObjectQueries;
    int mStorageQuota;
    bool mAllowAll;
    friend class ::TestJsonDb;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_OWNER_H
