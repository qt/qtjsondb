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

#ifndef JSONDB_H
#define JSONDB_H

#include <QDebug>
#include <QObject>
#include <QHash>
#include <QJSEngine>
#include <QStringList>
#include <QVariant>
#include <QMap>

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <qjsondocument.h>

#include "jsondbobject.h"
#include "jsondbquery.h"
#include "jsondbpartition.h"
#include "jsondbstat.h"
#include "jsondbnotification.h"
#include "jsondbschemamanager_p.h"

QT_BEGIN_HEADER

class TestJsonDb;
class TestJsonDbQueries;

QT_BEGIN_NAMESPACE_JSONDB

extern const QString kSortKeysStr;
extern const QString kStateStr;

class JsonDbProxy;
class JsonDbQuery;
class JsonDbObjectTable;
class JsonDbPartition;
class JsonDbEphemeralPartition;

class JsonDb : public QObject
{
    Q_OBJECT

public:
    JsonDb( const QString &filePath, const QString &baseName, const QString &username,
            QObject *parent=0 );
    ~JsonDb();

    bool open();
    void close();
    bool clear();
    void reduceMemoryUsage();
    JsonDbStat stat() const;

    enum WriteMode {
        DefaultWrite,       // legacy, let gRejectStaleUpdate decide
        OptimisticWrite,    // write must not introduce a conflict
        ForcedWrite,        // accept write as is (almost no matter what)
        ReplicatedWrite,    // master/master replication, may create obj._meta.conflicts
        ViewObject,         // internal for view object
        EphemeralObject     // internal for ephemeral, just likely go away in future refactor
    };

    JsonDbQueryResult find(const JsonDbOwner *owner, QJsonObject object, const QString &partitionName = QString());
    QJsonObject create(const JsonDbOwner *owner, JsonDbObject&, const QString &partitionName = QString(), WriteMode writeMode = DefaultWrite);
    QJsonObject update(const JsonDbOwner *owner, JsonDbObject&, const QString &partitionName = QString(), WriteMode writeMode = DefaultWrite);
    QJsonObject remove(const JsonDbOwner *owner, const JsonDbObject&, const QString &partitionName = QString(), WriteMode writeMode = DefaultWrite);

    QJsonObject createList(const JsonDbOwner *owner, JsonDbObjectList&, const QString &partitionName = QString(), WriteMode writeMode = DefaultWrite);
    QJsonObject updateList(const JsonDbOwner *owner, JsonDbObjectList&, const QString &partitionName = QString(), WriteMode writeMode = DefaultWrite);
    QJsonObject removeList(const JsonDbOwner *owner, JsonDbObjectList, const QString &partitionName = QString(), WriteMode writeMode = DefaultWrite);

    QJsonObject createViewObject(const JsonDbOwner *owner, JsonDbObject &, const QString &partitionName = QString());
    QJsonObject updateViewObject(const JsonDbOwner *owner, JsonDbObject&, const QString &partitionName = QString());
    QJsonObject removeViewObject(const JsonDbOwner *owner, JsonDbObject, const QString &partitionName = QString());

    QJsonObject changesSince(const JsonDbOwner *owner, QJsonObject object, const QString &partitionName = QString());
    Q_INVOKABLE QJsonObject log(JsonDbOwner *owner, QJsonValue data);

    QJsonObject flush(const JsonDbOwner *owner, const QString &partition);

    JsonDbOwner *owner() const { return mOwner; }
    const JsonDbOwner *findOwner(const QString &ownerId) const;

    GetObjectsResult getObjects(const QString &keyName, const QJsonValue &key, const QString &type = QString(), const QString &partitionName = QString()) const;

    QString ephemeralPartitionName() const { return mEphemeralPartitionName; }
    QString setEphemeralPartitionName(const QString &name) { mEphemeralPartitionName = name; return mEphemeralPartitionName; }
    QString systemPartitionName() const { return mSystemPartitionName; }
    QString setSystemPartitionName(const QString &name) { mSystemPartitionName = name; return mSystemPartitionName; }

    bool compactOnClose() const { return mCompactOnClose; }
    void setCompactOnClose(bool compact) { mCompactOnClose = compact; }

    static QJsonValue propertyLookup(const JsonDbObject &document, const QString &path);
    static QJsonValue propertyLookup(QJsonObject v, const QString &path);
    static QJsonValue propertyLookup(QJsonObject o, const QStringList &path);

    static QJsonValue fromJSValue(const QJSValue &v);
    static QJSValue toJSValue(const QJsonValue &v, QJSEngine *scriptEngine);
    static QJSValue toJSValue(const QJsonObject &object, QJSEngine *mScriptEngine);

    void updateView(const QString &viewType, const QString &partitionName = QString());

    QHash<QString, qint64> fileSizes(const QString &partitionName = QString()) const;

protected:
    bool populateIdBySchema(const JsonDbOwner *owner, JsonDbObject &object,
                            const QString &partitionName);

    void initSchemas();
    void updateSchemaIndexes(const QString &schemaName, QJsonObject object, const QStringList &path=QStringList());
    void setSchema(const QString &schemaName, QJsonObject object);
    void removeSchema(const QString &schemaName);

    QJsonObject validateSchema(const QString &schemaName, JsonDbObject object);
    QJsonObject checkPartitionPresent(const QString &partitionName);
    QJsonObject checkUuidPresent(JsonDbObject object, QString &uuid);
    QJsonObject checkTypePresent(JsonDbObject, QString &type);
    QJsonObject checkNaturalObjectType(JsonDbObject object, QString &type);
    QJsonObject checkAccessControl(const JsonDbOwner *owner, JsonDbObject object,
                                   const QString &partitionName, const QString &op);
    QJsonObject checkQuota(const JsonDbOwner *owner, int size, JsonDbPartition *partition);
    QJsonObject checkCanAddSchema(JsonDbObject schema, JsonDbObject oldSchema = QJsonObject());
    QJsonObject checkCanRemoveSchema(JsonDbObject schema);
    QJsonObject validateAddIndex(const JsonDbObject &newIndex, const JsonDbObject &oldIndex) const;

    enum Action { Create, Remove };

    bool addIndex(JsonDbObject indexObject, const QString &partitionName);
    bool removeIndex(const QString &indexName,
                     const QString &objectType = QString(),
                     const QString &partition = QString());
    bool removeIndex(JsonDbObject indexObject, const QString &partitionName);

    void updateEagerViewTypes(const QString &objectType);

    void checkNotifications(const QString &partitionName, JsonDbObject obj, JsonDbNotification::Action action);

    const JsonDbNotification *createNotification(const JsonDbOwner *owner, JsonDbObject object);
    void removeNotification(const QString &uuid);

    QString filePath() const { return mFilePath; }

    static void setError( QJsonObject& map, int code, const QString &message );
    static QJsonObject makeError(int code, const QString &message);
    static QJsonObject makeResponse(const QJsonObject& resultmap, const QJsonObject& errormap, bool silent = false);
    static QJsonObject makeErrorResponse(QJsonObject &resultmap, int code, const QString &message, bool silent = false );
    static bool responseIsError( QJsonObject responseMap );

    static QString uuidhex(uint data, int digits);
    static QString createDatabaseId();

    JsonDbPartition *findPartition(const QString &name) const;
    QJsonObject createPartition(const JsonDbObject &object);

Q_SIGNALS:
    void notified(const QString &id, JsonDbObject, const QString &action);
    void requestViewUpdate(QString viewType, QString partitionName);

protected:
    JsonDbOwner *mOwner;
    QHash<QString, JsonDbPartition *> mPartitions;
    JsonDbEphemeralPartition *mEphemeralPartition;
    bool                  mOpen;
    QString               mFilePath;
    QString               mSystemPartitionName;
    QString               mEphemeralPartitionName;
    QString               mDatabaseId;
    JsonDbSchemaManager   mSchemas;
    QMap<QString, JsonDbNotification *> mNotificationMap;
    QMultiMap<QString, JsonDbNotification *> mKeyedNotifications;
    QSet<QString>             mViewTypes;
    QMap<QString,QSet<QString> > mEagerViewSourceTypes; // set of eager view types dependent on this source type
    QSet<QString>         mViewsUpdating;
    QMap<QString,const JsonDbOwner*> mOwners;
    bool                  mCompactOnClose;
    friend class ::TestJsonDb;
    friend class ::TestJsonDbQueries;
    friend class JsonDbPartition;
    friend class JsonDbMapDefinition;
    friend class JsonDbReduceDefinition;
    friend class JsonDbQuery;
    friend class JsonDbObjectTable;
    friend class JsonDbQueryResult;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_H
