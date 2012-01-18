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
#include "jsondbbtreestorage.h"
#include "jsondb-map-reduce.h"
#include "notification.h"
#include "schemamanager_p.h"

QT_BEGIN_HEADER

class TestJsonDb;
class TestJsonDbQueries;

QT_BEGIN_NAMESPACE_JSONDB

extern bool gValidateSchemas;
extern bool gRejectStaleUpdates;
extern bool gUseQsonInDb;
extern bool gUseJsonInDb;
extern bool gVerbose;
extern bool gPrintErrors;
extern QMap<QString, int> gTouchedFiles;
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

    JsonDbQueryResult find(const JsonDbOwner *owner, QJsonObject object, const QString &partition = QString());
    QJsonObject create(const JsonDbOwner *owner, JsonDbObject&, const QString &partition = QString(), bool viewObject=false);
    QJsonObject update(const JsonDbOwner *owner, JsonDbObject&, const QString &partition = QString(), bool viewObject=false);
    QJsonObject remove(const JsonDbOwner *owner, const JsonDbObject&, const QString &partition = QString(), bool viewObject=false);

    QJsonObject createList(const JsonDbOwner *owner, JsonDbObjectList&, const QString &partition = QString());
    QJsonObject updateList(const JsonDbOwner *owner, JsonDbObjectList&, const QString &partition = QString());
    QJsonObject removeList(const JsonDbOwner *owner, JsonDbObjectList, const QString &partition = QString());

    QJsonObject createViewObject(const JsonDbOwner *owner, JsonDbObject &, const QString &partition = QString());
    QJsonObject updateViewObject(const JsonDbOwner *owner, JsonDbObject&, const QString &partition = QString());
    QJsonObject removeViewObject(const JsonDbOwner *owner, JsonDbObject, const QString &partition = QString());

    QJsonObject changesSince(const JsonDbOwner *owner, QJsonObject object, const QString &partition = QString());

    JsonDbOwner *owner() const { return mOwner; }
    bool load(const QString &jsonFileName);

    GetObjectsResult getObjects(const QString &keyName, const QJsonValue &key, const QString &type = QString(), const QString &partition = QString()) const;

    QString getTablePrefix();
    void setTablePrefix(const QString &prefix);

    bool compactOnClose() const { return mCompactOnClose; }
    void setCompactOnClose(bool compact) { mCompactOnClose = compact; }

    static QJsonValue propertyLookup(const JsonDbObject &document, const QString &path);
    static QJsonValue propertyLookup(QJsonObject v, const QString &path);
    static QJsonValue propertyLookup(QJsonObject o, const QStringList &path);

    static QJsonValue fromJSValue(const QJSValue &v);
    static QJSValue toJSValue(const QJsonObject &object, QJSEngine *mScriptEngine);

    void updateView(const QString &viewType, const QString &partitionName = JsonDbString::kSystemPartitionName);

protected:
    bool populateIdBySchema(const JsonDbOwner *owner, JsonDbObject &object);

    void initSchemas();
    void updateSchemaIndexes(const QString &schemaName, QJsonObject object, const QStringList &path=QStringList());
    void setSchema(const QString &schemaName, QJsonObject object);
    void removeSchema(const QString &schemaName);

    QJsonObject validateSchema(const QString &schemaName, JsonDbObject object);
    QJsonObject validateMapObject(JsonDbObject map);
    QJsonObject validateReduceObject(JsonDbObject reduce);
    QJsonObject checkPartitionPresent(const QString &partition);
    QJsonObject checkUuidPresent(JsonDbObject object, QString &uuid);
    QJsonObject checkTypePresent(JsonDbObject, QString &type);
    QJsonObject checkNaturalObjectType(JsonDbObject object, QString &type);
    QJsonObject checkAccessControl(const JsonDbOwner *owner, JsonDbObject object, const QString &op);
    QJsonObject checkQuota(const JsonDbOwner *owner, int size, JsonDbBtreeStorage *partition);
    QJsonObject checkCanAddSchema(JsonDbObject schema, JsonDbObject oldSchema = QJsonObject());
    QJsonObject checkCanRemoveSchema(JsonDbObject schema);
    QJsonObject validateAddIndex(const JsonDbObject &newIndex, const JsonDbObject &oldIndex) const;

    enum Action { Create, Remove };

    bool addIndex(JsonDbObject indexObject, const QString &partition);
    bool removeIndex(const QString &propertyName,
                     const QString &objectType = QString(),
                     const QString &partition = QString());
    bool removeIndex(JsonDbObject indexObject, const QString &partition);

    void initMap(const QString &partition);
    void createMapDefinition(QJsonObject mapDefinition, bool firstTime, const QString &partition);
    void removeMapDefinition(QJsonObject mapDefinition, const QString &partition);

    void createReduceDefinition(QJsonObject reduceDefinition, bool firstTime, const QString &partition);
    void removeReduceDefinition(QJsonObject reduceDefinition, const QString &partition);
    void removeReduceDefinition(QJsonObject *def);

    quint32 findUpdatedMapReduceDefinitions(JsonDbBtreeStorage *partition, const QString &definitionType, const QString &viewType, quint32 targetStateNumber,
                                         QMap<QString, QJsonObject> &removedDefinitions, QMap<QString, QJsonObject> &addedDefinitions) const;

    void updateMap(const QString &viewType, const QString &partitionName);
    void updateReduce(const QString &viewType, const QString &partitionName);
    void updateEagerViewTypes(const QString &objectType);

    JsonDbQuery parseJsonQuery(const QString &query, QJsonValue &bindings) const;

    void checkNotifications(const QString &partition, JsonDbObject obj, Notification::Action action);

    const Notification *createNotification(const JsonDbOwner *owner, JsonDbObject object);
    void removeNotification(const QString &uuid);

    QString filePath() const { return mFilePath; }

    static void setError( QJsonObject& map, int code, const QString &message );
    static QJsonObject makeError(int code, const QString &message);
    static QJsonObject makeResponse( QJsonObject& resultmap, QJsonObject& errormap, bool silent = false );
    static QJsonObject makeErrorResponse(QJsonObject &resultmap, int code, const QString &message, bool silent = false );
    static bool responseIsError( QJsonObject responseMap );
//    static bool responseIsGood( QJsonObject responseMap );
    static QString uuidhex(uint data, int digits);
    static QString createDatabaseId();

    JsonDbBtreeStorage *findPartition(const QString &name) const;
    QJsonObject createPartition(const JsonDbObject &object);

Q_SIGNALS:
    void notified(const QString &id, JsonDbObject, const QString &action);
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
    bool                  mCompactOnClose;

    friend class ::TestJsonDb;
    friend class ::TestJsonDbQueries;
    friend class JsonDbBtreeStorage;
    friend class JsonDbMapDefinition;
    friend class JsonDbReduceDefinition;
    friend class JsonDbQuery;
    friend class ObjectTable;
    friend class JsonDbQueryResult;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif /* JSONDB_H */
