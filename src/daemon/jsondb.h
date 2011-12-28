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

#ifndef JSONDB_H
#define JSONDB_H

#include <QDebug>
#include <QObject>
#include <QHash>
#include <QJSEngine>
#include <QStringList>
#include <QVariant>

#include <QtJsonDbQson/private/qson_p.h>

#include "jsondbquery.h"
#include "jsondbbtreestorage.h"
#include "jsondb-map-reduce.h"
#include "notification.h"
#include "schemamanager_p.h"

QT_BEGIN_HEADER

class TestJsonDb;
class AoDb;

QT_ADDON_JSONDB_BEGIN_NAMESPACE

extern bool gValidateSchemas;
extern bool gRejectStaleUpdates;
extern bool gUseQsonInDb;
extern bool gUseJsonInDb;
extern bool gVerbose;
extern bool gPrintErrors;
#ifndef QT_NO_DEBUG_OUTPUT
extern bool gDebug;
extern bool gDebugRecovery;
extern bool gPerformanceLog;
#else
static const bool gDebug = false;
static const bool gDebugRecovery = false;
static const bool gPerformanceLog = false;
#endif

extern const QString kSortKeysStr;
extern const QString kStateStr;

class JsonDbProxy;
class JsonDbQuery;
class ObjectTable;
class JsonDbBtreeStorage;
class JsonDbEphemeralStorage;

class JsonDb : public QObject
{
    Q_OBJECT

public:
    JsonDb( const QString& filename, QObject *parent=0 );
    ~JsonDb();

    bool open();
    void close();
    bool clear();
    bool checkValidity();

    QsonMap find(const JsonDbOwner *owner, QsonMap object, const QString &partition = QString());
    QsonMap create(const JsonDbOwner *owner, QsonMap&, const QString &partition = QString());
    QsonMap update(const JsonDbOwner *owner, QsonMap&, const QString &partition = QString(), bool replication = false);
    QsonMap remove(const JsonDbOwner *owner, const QsonMap&, const QString &partition = QString());

    QsonMap createList(const JsonDbOwner *owner, QsonList&, const QString &partition = QString());
    QsonMap updateList(const JsonDbOwner *owner, QsonList&, const QString &partition = QString(), bool replication = false);
    QsonMap removeList(const JsonDbOwner *owner, QsonList, const QString &partition = QString());

    QsonMap createViewObject(const JsonDbOwner *owner, QsonMap&, const QString &partition = QString());
    QsonMap updateViewObject(const JsonDbOwner *owner, QsonMap&, const QString &partition = QString());
    QsonMap removeViewObject(const JsonDbOwner *owner, QsonMap, const QString &partition = QString());

    QsonMap changesSince(const JsonDbOwner *owner, QsonMap object, const QString &partition = QString());

    JsonDbOwner *owner() const { return mOwner; }
    bool load(const QString &jsonFileName);

    QsonMap getObjects(const QString &keyName, const QVariant &key, const QString &type = QString(), const QString &partition = QString()) const;

    QString getTablePrefix();
    void setTablePrefix(const QString &prefix);

    static QVariant propertyLookup(QsonMap v, const QString &path);
    static QVariant propertyLookup(QVariantMap v, const QStringList &path);
    static QVariant propertyLookup(QsonMap o, const QStringList &path);

    void updateView(const QString &viewType, const QString &partitionName = JsonDbString::kSystemPartitionName);

protected:
    bool populateIdBySchema(const JsonDbOwner *owner, QsonMap &object);

    void initSchemas();
    void updateSchemaIndexes(const QString &schemaName, QsonMap object, const QStringList &path=QStringList());
    void setSchema(const QString &schemaName, QsonMap object);
    void removeSchema(const QString &schemaName);

    QsonMap validateSchema(const QString &schemaName, QsonMap object);
    QsonMap validateMapObject(QsonMap map);
    QsonMap validateReduceObject(QsonMap reduce);
    QsonMap checkPartitionPresent(const QString &partition);
    QsonMap checkUuidPresent(QsonMap object, QString &uuid);
    QsonMap checkTypePresent(QsonMap, QString &type);
    QsonMap checkNaturalObjectType(QsonMap object, QString &type);
    QsonMap checkAccessControl(const JsonDbOwner *owner, QsonMap object, const QString &op);
    QsonMap checkQuota(const JsonDbOwner *owner, int size, JsonDbBtreeStorage *partition);
    QsonMap checkCanAddSchema(QsonMap schema, QsonMap oldSchema = QsonMap());
    QsonMap checkCanRemoveSchema(QsonMap schema);
    QsonMap validateAddIndex(const QsonMap &newIndex, const QsonMap &oldIndex) const;

    enum Action { Create, Remove };

    bool addIndex(QsonMap indexObject, const QString &partition);

    bool removeIndex(const QString &propertyName,
                     const QString &objectType = QString(),
                     const QString &partition = QString());
    bool removeIndex(QsonMap indexObject, const QString &partition);

    void initMap(const QString &partition);
    void createMapDefinition(QsonMap mapDefinition, bool firstTime, const QString &partition);
    void removeMapDefinition(QsonMap mapDefinition);

    void createReduceDefinition(QsonMap reduceDefinition, bool firstTime, const QString &partition);
    void removeReduceDefinition(QsonMap reduceDefinition);
    void removeReduceDefinition(JsonDbReduceDefinition *def);

    quint32 findUpdatedMapReduceDefinitions(JsonDbBtreeStorage *partition, const QString &definitionType, const QString &viewType, quint32 targetStateNumber,
                                         QMap<QString, QsonMap> &removedDefinitions, QMap<QString, QsonMap> &addedDefinitions) const;

    void updateMap(const QString &viewType, const QString &partitionName);
    void updateReduce(const QString &viewType, const QString &partitionName);
    void updateEagerViewTypes(const QString &objectType);

    JsonDbQuery parseJsonQuery(const QString &query, QsonObject &bindings) const;

    void checkNotifications(const QString &partition, QsonMap obj, Notification::Action action);

    const Notification *createNotification(const JsonDbOwner *owner, QsonMap object);
    void removeNotification(const QString &uuid);

    QString filePath() const { return mFilePath; }

    static void setError( QsonMap& map, int code, const QString &message );
    static QsonMap makeError(int code, const QString &message);
    static QsonMap makeResponse( QsonMap& resultmap, QsonMap& errormap, bool silent = false );
    static QsonMap makeErrorResponse(QsonMap &resultmap, int code, const QString &message, bool silent = false );
    static bool responseIsError( QsonMap responseMap );
//    static bool responseIsGood( QsonMap responseMap );
    static QString uuidhex(uint data, int digits);
    static QString createDatabaseId();

    JsonDbBtreeStorage *findPartition(const QString &name) const;
    QsonMap createPartition(const QsonMap &object);

Q_SIGNALS:
    void notified(const QString &id, QsonMap, const QString &action);
    void requestViewUpdate(QString viewType, QString partition);

protected:
    JsonDbOwner *mOwner;
    QHash<QString, JsonDbBtreeStorage *> mStorages;
    JsonDbEphemeralStorage *mEphemeralStorage;
    JsonDbProxy          *mJsonDbProxy;
    bool                  mOpen;
    QString               mFilePath;
    QString               mDatabaseId;
    SchemaManager         mSchemas;
    QMap<QString,Notification*> mNotificationMap;
    QMultiMap<QString,Notification*> mKeyedNotifications;
    QSet<QString>             mViewTypes;
    QMap<QString,QSet<QString> > mEagerViewSourceTypes; // set of eager view types dependent on this source type
    QMultiMap<QString,JsonDbMapDefinition*> mMapDefinitionsBySource; // maps map source type to view definition
    QMultiMap<QString,JsonDbMapDefinition*> mMapDefinitionsByTarget; // maps map target type to view definition
    QMultiMap<QString,JsonDbReduceDefinition*> mReduceDefinitionsBySource; // maps reduce source type to view definition
    QMultiMap<QString,JsonDbReduceDefinition*> mReduceDefinitionsByTarget; // maps reduce target type to view definition
    QSet<QString>         mViewsUpdating;

    friend class ::TestJsonDb;
    friend class JsonDbBtreeStorage;
    friend class JsonDbMapDefinition;
    friend class JsonDbReduceDefinition;
    friend class JsonDbQuery;
    friend class ObjectTable;
};

QT_ADDON_JSONDB_END_NAMESPACE

QT_END_HEADER

#endif /* JSONDB_H */
