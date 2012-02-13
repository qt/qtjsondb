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

#ifndef JSONDB_BTREE_H
#define JSONDB_BTREE_H

#include <QStringList>
#include <QRegExp>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QPointer>

#include "jsondb.h"
#include "jsondb-owner.h"

#include "objectkey.h"
#include "qmanagedbtreetxn.h"

QT_BEGIN_HEADER

class TestJsonDb;
class QManagedBtree;
class QBtreeCursor;

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbOwner;
class ObjectTable;
class JsonDbIndex;

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
    IndexQuery(JsonDbBtreeStorage *storage, ObjectTable *table,
               const QString &propertyName, const QString &propertyType,
               const JsonDbOwner *owner, bool ascending = true);
public:
    static IndexQuery *indexQuery(JsonDbBtreeStorage *storage, ObjectTable *table,
                                  const QString &propertyName, const QString &propertyType,
                                  const JsonDbOwner *owner, bool ascending = true);
    ~IndexQuery();

    ObjectTable *objectTable() const { return mObjectTable; }
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
    JsonDbBtreeStorage *mStorage;
    ObjectTable   *mObjectTable;
    QManagedBtree *mBdbIndex;
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
    UuidQuery(JsonDbBtreeStorage *storage, ObjectTable *table, const QString &propertyName, const JsonDbOwner *owner, bool ascending = true);
    virtual bool seekToStart(QJsonValue &fieldValue);
    virtual bool seekToNext(QJsonValue &fieldValue);
    virtual JsonDbObject currentObjectAndTypeNumber(ObjectKey &objectKey);
    virtual quint32 stateNumber() const;
    friend class IndexQuery;
};

class JsonDbBtreeStorage : public QObject
{
    Q_OBJECT
public:

    JsonDbBtreeStorage(const QString &filename, const QString &name, JsonDb *jsonDb);
    ~JsonDbBtreeStorage();
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

    JsonDbQueryResult queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit=-1, int offset=0);
    JsonDbQueryResult queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit, int offset,
                                             QList<JsonDbBtreeStorage *> partitions);
    QJsonObject createPersistentObject(JsonDbObject & );
    QJsonObject updatePersistentObject(const JsonDbObject& oldObject, const JsonDbObject& object);
    QJsonObject removePersistentObject(const JsonDbObject& oldObject, const JsonDbObject &tombStone );

    void addView(const QString &viewType);
    void removeView(const QString &viewType);
    ObjectTable *mainObjectTable() const { return mObjectTable; }
    ObjectTable *findObjectTable(const QString &objectType) const;

    bool getObject(const QString &uuid, JsonDbObject &object, const QString &objectType = QString()) const;
    bool getObject(const ObjectKey & objectKey, JsonDbObject &object, const QString &objectType = QString()) const;

    GetObjectsResult getObjects(const QString &keyName, const QJsonValue &key, const QString &type = QString());

    QJsonObject changesSince(quint32 stateNumber, const QSet<QString> &limitTypes = QSet<QString>());
    void dumpIndexes(QString label);

    QString name() const;
    void setName(const QString &name);

    QString getTablePrefix();
    void setTablePrefix(const QString &prefix);

    void checkIndex(const QString &propertyName);
    bool compact();

    QHash<QString, qint64> fileSizes() const;

protected:
    bool checkStateConsistency();
    void checkIndexConsistency(ObjectTable *table, JsonDbIndex *index);
    void updateIndex(ObjectTable *table, JsonDbIndex *index);
    void updateView(ObjectTable *table);
    void updateView(const QString &objectType);

    IndexQuery *compileIndexQuery(const JsonDbOwner *owner, const JsonDbQuery &query);
    void compileOrQueryTerm(IndexQuery *indexQuery, const QueryTerm &queryTerm);

    void doIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                      IndexQuery *indexQuery);
    void doMultiIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                           const QList<IndexQuery *> &indexQueries);

    static void sortValues(const JsonDbQuery *query, JsonDbObjectList &results, JsonDbObjectList &joinedResults);

private:
    JsonDb          *mJsonDb;
    ObjectTable     *mObjectTable;
    QVector<ObjectTable *> mTableTransactions;

    QString      mPartitionName;
    QString      mFilename;
    int          mTransactionDepth;
    bool         mTransactionOk;
    QHash<QString,QPointer<ObjectTable> > mViews;
    QRegExp      mWildCardPrefixRegExp;

    friend class IndexQuery;
    friend class ObjectTable;
    friend class JsonDbMapDefinition;
    friend class WithTransaction;
    friend class ::TestJsonDb;
};

class WithTransaction {
public:
    WithTransaction(JsonDbBtreeStorage *storage=0, QString name=QString())
        : mStorage(0)
    {
        Q_UNUSED(name)
        //qDebug() << "WithTransaction" << "depth" << mStorage->mTransactionDepth << name;
        if (storage)
            setStorage(storage);
    }

    ~WithTransaction()
    {
        if (mStorage)
            mStorage->commitTransaction();
    }

    void setStorage(JsonDbBtreeStorage *storage)
    {
        Q_ASSERT(!mStorage);
        mStorage = storage;
        if (!mStorage->beginTransaction())
            mStorage = 0;
    }
    bool hasBegin() { return mStorage; }
    bool addObjectTable(ObjectTable *table);

    void abort()
    {
        if (mStorage)
            mStorage->abortTransaction();
        mStorage = 0;
    }

    void commit(quint32 stateNumber = 0)
    {
        if (mStorage)
            mStorage->commitTransaction(stateNumber);
        mStorage = 0;
    }

private:
    JsonDbBtreeStorage *mStorage;
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

#endif /* JSONDB_H */
