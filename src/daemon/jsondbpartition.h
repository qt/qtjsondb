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

#ifndef JSONDB_PARTITION_H
#define JSONDB_PARTITION_H

#include <QStringList>
#include <QRegExp>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QPointer>

#include "jsondbmanagedbtreetxn.h"
#include "jsondbobjectkey.h"
#include "jsondbnotification.h"
#include "jsondbowner.h"
#include "jsondbstat.h"
#include "jsondbschemamanager_p.h"
#include "qbtree.h"

QT_BEGIN_HEADER

class TestJsonDb;

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbManagedBtree;
class JsonDbOwner;
class JsonDbObjectTable;
class JsonDbIndex;
class JsonDbIndexQuery;
class JsonDbView;

struct JsonDbUpdate {
    JsonDbUpdate(const JsonDbObject &oldObj, const JsonDbObject &newObj, JsonDbNotification::Action act) :
        oldObject(oldObj), newObject(newObj), action(act) { }
    JsonDbObject oldObject;
    JsonDbObject newObject;
    JsonDbNotification::Action action;
};

typedef QList<JsonDbUpdate> JsonDbUpdateList;

struct JsonDbWriteResult {
    JsonDbWriteResult() : state(0), code(JsonDbError::NoError) { }
    JsonDbObjectList objectsWritten;
    quint32 state;
    JsonDbError::ErrorCode code;
    QString message;
};

class JsonDbPartition : public QObject
{
    Q_OBJECT
public:

    enum WriteMode {
        OptimisticWrite,    // write must not introduce a conflict
        ForcedWrite,        // accept write as is (almost no matter what)
        ReplicatedWrite,    // master/master replication, may create obj._meta.conflicts
        ViewObject          // internal for view object
    };

    JsonDbPartition(const QString &filename, const QString &name, JsonDbOwner *owner, QObject *parent = 0);
    ~JsonDbPartition();
    QString filename() const { return mFilename; }
    bool open();
    bool close();
    bool clear();

    bool beginTransaction();
    bool commitTransaction(quint32 stateNumber = 0);
    bool abortTransaction();

    void initIndexes();
    void flushCaches();
    bool addIndex(const QString &indexName,
                  const QString &propertyName,
                  const QString &propertyType = QString("string"),
                  const QString &objectType = QString(),
                  const QString &propertyFunction = QString(),
                  const QString &locale = QString(),
                  const QString &collation = QString());
    bool addIndexOnProperty(const QString &propertyName,
                            const QString &propertyType = QString("string"),
                            const QString &objectType = QString())
    { return addIndex(propertyName, propertyName, propertyType, objectType); }
    bool removeIndex(const QString &indexName, const QString &objectType = QString());

    bool checkQuota(const JsonDbOwner *owner, int size) const;
    bool addToQuota(const JsonDbOwner *owner, int size);

    JsonDbQueryResult queryObjects(const JsonDbOwner *owner, const JsonDbQuery *query, int limit=-1, int offset=0);
    JsonDbWriteResult updateObjects(const JsonDbOwner *owner, const JsonDbObjectList &objects, WriteMode mode = OptimisticWrite);
    JsonDbWriteResult updateObject(const JsonDbOwner *owner, const JsonDbObject &object, WriteMode mode = OptimisticWrite);

    QJsonObject createPersistentObject(JsonDbObject & );
    QJsonObject updatePersistentObject(const JsonDbObject& oldObject, const JsonDbObject& object);
    QJsonObject removePersistentObject(const JsonDbObject& oldObject, const JsonDbObject &tombStone );

    QJsonObject flush();

    JsonDbView *addView(const QString &viewType);
    void removeView(const QString &viewType);
    JsonDbObjectTable *mainObjectTable() const { return mObjectTable; }
    JsonDbObjectTable *findObjectTable(const QString &objectType) const;
    JsonDbView *findView(const QString &objectType) const;

    bool getObject(const QString &uuid, JsonDbObject &object, const QString &objectType = QString()) const;
    bool getObject(const ObjectKey & objectKey, JsonDbObject &object, const QString &objectType = QString()) const;

    GetObjectsResult getObjects(const QString &keyName, const QJsonValue &key, const QString &type = QString(),
                                bool updateViews = true);

    QJsonObject changesSince(quint32 stateNumber, const QSet<QString> &limitTypes = QSet<QString>());

    inline QString name() const { return mPartitionName; }
    inline void setName(const QString &name) { mPartitionName = name; }
    inline JsonDbOwner *defaultOwner() { return mDefaultOwner; }

    void checkIndex(const QString &propertyName);
    bool compact();
    struct JsonDbStat stat() const;

    QHash<QString, qint64> fileSizes() const;

    // FIXME: copied from JsonDb class.
    // This is the protocol leaking into the lower layers and should be removed
    static void setError(QJsonObject &map, int code, const QString &message);
    static QJsonObject makeError(int code, const QString &message);
    static QJsonObject makeResponse(const QJsonObject &resultmap, const QJsonObject &errormap, bool silent = false);
    static QJsonObject makeErrorResponse(QJsonObject &resultmap, int code, const QString &message, bool silent = false);
    static bool responseIsError(const QJsonObject &responseMap);

public Q_SLOTS:
    void updateView(const QString &objectType);

Q_SIGNALS:
    void objectsUpdated(const JsonDbUpdateList &objects);

protected:
    void initSchemas();

    void timerEvent(QTimerEvent *event);

    bool checkStateConsistency();
    void checkIndexConsistency(JsonDbObjectTable *table, JsonDbIndex *index);

    JsonDbIndexQuery *compileIndexQuery(const JsonDbOwner *owner, const JsonDbQuery *query);
    void compileOrQueryTerm(JsonDbIndexQuery *indexQuery, const QueryTerm &queryTerm);

    void doIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                      JsonDbIndexQuery *indexQuery);

    static void sortValues(const JsonDbQuery *query, JsonDbObjectList &results, JsonDbObjectList &joinedResults);

    bool checkCanAddSchema(const JsonDbObject &schema, const JsonDbObject &oldSchema, QString &errorMsg);
    bool checkCanRemoveSchema(const JsonDbObject &schema, QString &errorMsg);
    bool validateSchema(const QString &schemaName, const JsonDbObject &object, QString &errorMsg);
    bool checkNaturalObjectType(const JsonDbObject &object, QString &errorMsg);

    JsonDbError::ErrorCode checkBuiltInTypeValidity(const JsonDbObject &object, const JsonDbObject &oldObject, QString &errorMsg);
    void updateBuiltInTypes(const JsonDbObject &object, const JsonDbObject &oldObject);
    void setSchema(const QString &schemaName, const QJsonObject &schema);
    void removeSchema(const QString &schemaName);
    void updateSchemaIndexes(const QString &schemaName, QJsonObject object, const QStringList &path=QStringList());

private:

    JsonDbObjectTable     *mObjectTable;
    QVector<JsonDbObjectTable *> mTableTransactions;

    QString      mPartitionName;
    QString      mFilename;
    int          mTransactionDepth;
    bool         mTransactionOk;
    QHash<QString,QPointer<JsonDbView> > mViews;
    QSet<QString> mViewTypes;
    JsonDbSchemaManager   mSchemas;
    QRegExp      mWildCardPrefixRegExp;
    int          mMainSyncTimerId;
    int          mIndexSyncTimerId;
    int          mMainSyncInterval;
    int          mIndexSyncInterval;
    JsonDbOwner *mDefaultOwner;

    friend class JsonDbIndexQuery;
    friend class JsonDbObjectTable;
    friend class JsonDbMapDefinition;
    friend class WithTransaction;
    friend class ::TestJsonDb;
};

class WithTransaction {
public:
    WithTransaction(JsonDbPartition *partition=0, QString name=QString())
        : mPartition(0)
    {
        Q_UNUSED(name)
        if (partition)
            setPartition(partition);
    }

    ~WithTransaction()
    {
        if (mPartition)
            mPartition->commitTransaction();
    }

    void setPartition(JsonDbPartition *partition)
    {
        Q_ASSERT(!mPartition);
        mPartition = partition;
        if (!mPartition->beginTransaction())
            mPartition = 0;
    }
    bool hasBegin() { return mPartition; }
    bool addObjectTable(JsonDbObjectTable *table);

    void abort()
    {
        if (mPartition)
            mPartition->abortTransaction();
        mPartition = 0;
    }

    void commit(quint32 stateNumber = 0)
    {
        if (mPartition)
            mPartition->commitTransaction(stateNumber);
        mPartition = 0;
    }

private:
    JsonDbPartition *mPartition;
};

QJsonValue makeFieldValue(const QJsonValue &value, const QString &type);
QByteArray makeForwardKey(const QJsonValue &fieldValue, const ObjectKey &objectKey);
int forwardKeyCmp(const char *aptr, size_t asiz, const char *bptr, size_t bsiz, void *op);
void forwardKeySplit(const QByteArray &forwardKey, QJsonValue &fieldValue);
void forwardKeySplit(const QByteArray &forwardKey, QJsonValue &fieldValue, ObjectKey &objectKey);
QByteArray makeForwardValue(const ObjectKey &);
void forwardValueSplit(const QByteArray &forwardValue, ObjectKey &objectKey);

QDebug &operator<<(QDebug &, const ObjectKey &);

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_PARTITION_H
