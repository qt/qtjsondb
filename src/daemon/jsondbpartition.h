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

#include "jsondb.h"
#include "jsondbmanagedbtreetxn.h"
#include "jsondbobjectkey.h"
#include "jsondbowner.h"
#include "jsondbstat.h"
#include "qbtree.h"

QT_BEGIN_HEADER

class TestJsonDb;
class JsonDbManagedBtree;
class QBtreeCursor;
QT_BEGIN_NAMESPACE_JSONDB

class JsonDbOwner;
class JsonDbObjectTable;
class JsonDbIndex;
class JsonDbView;

extern const QString kDbidTypeStr;
extern const QString kIndexTypeStr;
extern const QString kPropertyNameStr;
extern const QString kPropertyTypeStr;
extern const QString kNameStr;
extern const QString kObjectTypeStr;
extern const QString kDatabaseSchemaVersionStr;
extern const QString kPropertyFunctionStr;
extern const QString gDatabaseSchemaVersion;

class QueryConstraint {
public:
    virtual ~QueryConstraint() { }
    virtual bool matches(const QJsonValue &value) = 0;
    virtual bool sparseMatchPossible() const
        { return false; }
};

bool lessThan(const QJsonValue &a, const QJsonValue &b);
bool greaterThan(const QJsonValue &a, const QJsonValue &b);

class IndexQuery {
protected:
    IndexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
               const QString &propertyName, const QString &propertyType,
               const JsonDbOwner *owner, bool ascending = true);
public:
    static IndexQuery *indexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                                  const QString &propertyName, const QString &propertyType,
                                  const JsonDbOwner *owner, bool ascending = true);
    ~IndexQuery();

    JsonDbObjectTable *objectTable() const { return mObjectTable; }
    QString partition() const;
    void addConstraint(QueryConstraint *qc) { mQueryConstraints.append(qc); }
    bool ascending() const { return mAscending; }
    QString propertyName() const { return mPropertyName; }
    void setTypeNames(const QSet<QString> typeNames) { mTypeNames = typeNames; }
    void setMin(const QJsonValue &minv);
    void setMax(const QJsonValue &maxv);
    QString aggregateOperation() const { return mAggregateOperation; }
    void setAggregateOperation(QString op) { mAggregateOperation = op; }

    JsonDbObject first(); // returns first matching object
    JsonDbObject next(); // returns next matching object
    bool matches(const QJsonValue &value);
    QJsonValue fieldValue() const { return mFieldValue; }
    JsonDbQuery *residualQuery() const { return mResidualQuery; }
    void setResidualQuery(JsonDbQuery *residualQuery) { mResidualQuery = residualQuery; }
    virtual quint32 stateNumber() const;

protected:
    virtual bool seekToStart(QJsonValue &fieldValue);
    virtual bool seekToNext(QJsonValue &fieldValue);
    virtual JsonDbObject currentObjectAndTypeNumber(ObjectKey &objectKey);

protected:
    JsonDbPartition *mPartition;
    JsonDbObjectTable   *mObjectTable;
    JsonDbManagedBtree *mBdbIndex;
    QBtreeCursor  *mCursor;
    const JsonDbOwner *mOwner;
    QJsonValue      mMin, mMax;
    QSet<QString> mTypeNames;
    bool          mAscending;
    QString       mUuid;
    QVector<QueryConstraint*> mQueryConstraints;
    QString       mAggregateOperation;
    QString       mPropertyName;
    QString       mPropertyType;
    QJsonValue     mFieldValue; // value of field for the object the cursor is pointing at
    bool          mSparseMatchPossible;
    QHash<QString, JsonDbObject> mObjectCache;
    JsonDbQuery  *mResidualQuery;
};

class UuidQuery : public IndexQuery {
protected:
    UuidQuery(JsonDbPartition *partition, JsonDbObjectTable *table, const QString &propertyName, const JsonDbOwner *owner, bool ascending = true);
    virtual bool seekToStart(QJsonValue &fieldValue);
    virtual bool seekToNext(QJsonValue &fieldValue);
    virtual JsonDbObject currentObjectAndTypeNumber(ObjectKey &objectKey);
    virtual quint32 stateNumber() const;
    friend class IndexQuery;
};

class JsonDbPartition : public QObject
{
    Q_OBJECT
public:
    JsonDbPartition(const QString &filename, const QString &name, JsonDb *jsonDb);
    ~JsonDbPartition();
    QString filename() const { return mFilename; }
    bool open();
    bool close();
    bool clear();
    bool beginTransaction();
    bool commitTransaction(quint32 stateNumber = 0);
    bool abortTransaction();

    void initIndexes();
    bool checkValidity();
    void flushCaches();
    bool addIndex(const QString &indexName,
                  const QString &propertyName,
                  const QString &propertyType = QString("string"),
                  const QString &objectType = QString(),
                  const QString &propertyFunction = QString());
    bool addIndexOnProperty(const QString &propertyName,
                            const QString &propertyType = QString("string"),
                            const QString &objectType = QString())
    { return addIndex(propertyName, propertyName, propertyType, objectType); }
    bool removeIndex(const QString &indexName, const QString &objectType);

    bool checkQuota(const JsonDbOwner *owner, int size) const;
    bool addToQuota(const JsonDbOwner *owner, int size);

    JsonDbQueryResult queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery *query, int limit=-1, int offset=0);
    JsonDbQueryResult queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery *query, int limit, int offset,
                                             QList<JsonDbPartition *> partitions);
    QJsonObject createPersistentObject(JsonDbObject & );
    QJsonObject updatePersistentObject(const JsonDbObject& oldObject, const JsonDbObject& object);
    QJsonObject removePersistentObject(const JsonDbObject& oldObject, const JsonDbObject &tombStone );

    JsonDbView *addView(const QString &viewType);
    void removeView(const QString &viewType);
    JsonDbObjectTable *mainObjectTable() const { return mObjectTable; }
    JsonDbObjectTable *findObjectTable(const QString &objectType) const;
    JsonDbView *findView(const QString &objectType) const;
    void updateEagerViewTypes(const QString &viewType) const;

    bool getObject(const QString &uuid, JsonDbObject &object, const QString &objectType = QString()) const;
    bool getObject(const ObjectKey & objectKey, JsonDbObject &object, const QString &objectType = QString()) const;

    GetObjectsResult getObjects(const QString &keyName, const QJsonValue &key, const QString &type = QString(),
                                bool updateViews = true);

    QJsonObject changesSince(quint32 stateNumber, const QSet<QString> &limitTypes = QSet<QString>());
    void dumpIndexes(QString label);

    QString name() const;
    void setName(const QString &name);

    QString getTablePrefix();
    void setTablePrefix(const QString &prefix);

    void checkIndex(const QString &propertyName);
    bool compact();
    struct JsonDbStat stat() const;

    QHash<QString, qint64> fileSizes() const;

    void updateView(const QString &objectType);

protected:
    void timerEvent(QTimerEvent *event);

    bool checkStateConsistency();
    void checkIndexConsistency(JsonDbObjectTable *table, JsonDbIndex *index);

    IndexQuery *compileIndexQuery(const JsonDbOwner *owner, const JsonDbQuery *query);
    void compileOrQueryTerm(IndexQuery *indexQuery, const QueryTerm &queryTerm);

    void doIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                      IndexQuery *indexQuery);
    void doMultiIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                           const QList<IndexQuery *> &indexQueries);

    static void sortValues(const JsonDbQuery *query, JsonDbObjectList &results, JsonDbObjectList &joinedResults);

private:
    JsonDb          *mJsonDb;
    JsonDbObjectTable     *mObjectTable;
    QVector<JsonDbObjectTable *> mTableTransactions;

    QString      mPartitionName;
    QString      mFilename;
    int          mTransactionDepth;
    bool         mTransactionOk;
    QHash<QString,QPointer<JsonDbView> > mViews;
    QRegExp      mWildCardPrefixRegExp;
    int          mMainSyncTimerId;
    int          mIndexSyncTimerId;
    int          mMainSyncInterval;
    int          mIndexSyncInterval;
    friend class IndexQuery;
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

#define CHECK_LOCK(lock, context) \
    if (!lock.hasBegin()) { \
        QJsonObject errormap;  \
        errormap.insert(JsonDbString::kCodeStr, (int)JsonDbError::DatabaseError); \
        QString error = QString("Failed to set begin transaction [%1:%2]") \
                                .arg(context, QString::number(__LINE__)); \
        qCritical() << error; \
    }

#define CHECK_LOCK_RETURN(lock, context) \
    if (!lock.hasBegin()) { \
        QJsonObject errormap; \
        errormap.insert(JsonDbString::kCodeStr, (int)JsonDbError::DatabaseError); \
        QString error = QString("Failed to set begin transaction [%1:%2]") \
                                .arg(context, QString::number(__LINE__)); \
        errormap.insert(JsonDbString::kMessageStr, error); \
        QJsonObject result; \
        result.insert(JsonDbString::kResultStr, QJsonValue()); \
        result.insert(JsonDbString::kErrorStr, errormap); \
        return result; \
    }

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
