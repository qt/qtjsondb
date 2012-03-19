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

#ifndef JSONDB_OWNER_H
#define JSONDB_OWNER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QSet>

#include "jsondb-global.h"
#include "jsondbquery.h"
#include "jsondbobject.h"


QT_BEGIN_HEADER

class TestJsonDb;
struct passwd;

QT_BEGIN_NAMESPACE_JSONDB

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

    void setAllowedObjects(const QString &partition, const QString &op,
                           const QList<QString> &queries);
    void setCapabilities(QJsonObject &capabilities, JsonDbPartition *partition);
    void setAllowAll(bool allowAll) { mAllowAll = allowAll; }

    bool isAllowed(JsonDbObject &object, const QString &partition, const QString &op) const;
    int storageQuota() const { return mStorageQuota; }
    void setStorageQuota(int storageQuota) { mStorageQuota = storageQuota; }
    bool setOwnerCapabilities(uid_t uid, JsonDbPartition *partition);
    bool setOwnerCapabilities(QString username, JsonDbPartition *partition);

private:
    QString mOwnerId; // from security object
    QString mDomain;  // from security object
    QMap<QString, QMap<QString, QList<QString> > > mAllowedObjects; // list of querie strings per partition
    QMap<QString, QMap<QString, QList<JsonDbQuery *> > > mAllowedObjectQueries;
    int mStorageQuota;
    bool mAllowAll;
    friend class ::TestJsonDb;
    bool _setOwnerCapabilities(struct passwd *pwd, JsonDbPartition *partition);
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_OWNER_H
