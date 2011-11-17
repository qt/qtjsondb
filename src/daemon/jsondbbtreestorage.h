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

QT_BEGIN_HEADER

class TestJsonDb;
class AoDb;
class AoDbCursor;

namespace QtAddOn { namespace JsonDb {

class JsonDbOwner;
class ObjectTable;
class JsonDbIndex;

extern const QString kDbidTypeStr;
extern const QString kIndexTypeStr;
extern const QString kFieldStr;
extern const QString kFieldTypeStr;
extern const QString kObjectTypeStr;
extern const QString kDatabaseSchemaVersionStr;
extern const QString gDatabaseSchemaVersion;

class QueryConstraint {
public:
    virtual ~QueryConstraint() { }
    virtual bool matches(const QVariant &value) = 0;
    virtual bool sparseMatchPossible() const
        { return false; }
};

class IndexQuery {
protected:
    IndexQuery(JsonDbBtreeStorage *storage, ObjectTable *table, const QString &fieldName, const JsonDbOwner *owner, bool ascending = true);
public:
    static IndexQuery *indexQuery(JsonDbBtreeStorage *storage, ObjectTable *table, const QString &fieldName, const JsonDbOwner *owner, bool ascending = true);
    ~IndexQuery();

    void addConstraint(QueryConstraint *qc) { mQueryConstraints.append(qc); }
    bool ascending() const { return mAscending; }
    QString fieldName() const { return mFieldName; }
    void setTypeNames(const QSet<QString> typeNames) { mTypeNames = typeNames; }
    void setMin(const QVariant &minv) { mMin = minv; }
    void setMax(const QVariant &maxv) { mMax = maxv; }
    QString aggregateOperation() const { return mAggregateOperation; }
    void setAggregateOperation(QString op) { mAggregateOperation = op; }

    QsonMap first(); // returns first matching object
    QsonMap next(); // returns next matching object
    bool matches(const QVariant &value);
    QVariant fieldValue() const { return mFieldValue; }
    JsonDbQuery *residualQuery() const { return mResidualQuery; }
    void setResidualQuery(JsonDbQuery *residualQuery) { mResidualQuery = residualQuery; }
    virtual quint32 stateNumber() const;

protected:
    virtual bool seekToStart(QVariant &fieldValue);
    virtual bool seekToNext(QVariant &fieldValue);
    virtual QsonMap currentObjectAndTypeNumber(ObjectKey &objectKey);

protected:
    JsonDbBtreeStorage *mStorage;
    ObjectTable   *mObjectTable;
    AoDb          *mBdbIndex;
    AoDbCursor    *mCursor;
    const JsonDbOwner *mOwner;
    QVariant      mMin, mMax;
    QSet<QString> mTypeNames;
    bool          mAscending;
    QString       mUuid;
    QVector<QueryConstraint*> mQueryConstraints;
    QString       mAggregateOperation;
    QString       mFieldName;
    QVariant      mFieldValue; // value of field for the object the cursor is pointing at
    bool          mSparseMatchPossible;
    QHash<QString, QsonMap> mObjectCache;
    JsonDbQuery  *mResidualQuery;
};

class UuidQuery : public IndexQuery {
protected:
    UuidQuery(JsonDbBtreeStorage *storage, ObjectTable *table, const QString &fieldName, const JsonDbOwner *owner, bool ascending = true);
    virtual bool seekToStart(QVariant &fieldValue);
    virtual bool seekToNext(QVariant &fieldValue);
    virtual QsonMap currentObjectAndTypeNumber(ObjectKey &objectKey);
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
    QsonObject addIndex(const QString &fieldName,
                        const QString &fieldType = QString("string"),
                        const QString &objectType = QString(),
                        bool lazy=true);
    QsonObject removeIndex(const QString &fieldName,
                        const QString &fieldType = QString("string"),
                        const QString &objectType = QString());

    bool checkQuota(const JsonDbOwner *owner, int size) const;
    bool addToQuota(const JsonDbOwner *owner, int size);

    QsonMap queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit=-1, int offset=0);
    QsonMap queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit, int offset,
                                   QList<JsonDbBtreeStorage *> partitions);
    QsonMap createPersistentObject(QsonMap& );
    QsonMap updatePersistentObject(QsonMap& );
    QsonMap removePersistentObject(QsonMap object, const QsonMap &tombStone );

    void addView(const QString &viewType);
    void removeView(const QString &viewType);
    ObjectTable *mainObjectTable() const { return mObjectTable; }
    ObjectTable *findObjectTable(const QString &objectType) const;

    bool getObject(const QString &uuid, QsonMap &object, const QString &objectType = QString()) const;
    bool getObject(const ObjectKey & objectKey, QsonMap &object, const QString &objectType = QString()) const;

    QsonMap getObjects(const QString &keyName, const QVariant &key, const QString &type = QString());

    QsonMap changesSince(quint32 stateNumber, const QSet<QString> &limitTypes = QSet<QString>());
    void dumpIndexes(QString label);

    QString name() const;
    void setName(const QString &name);

    QString getTablePrefix();
    void setTablePrefix(const QString &prefix);

    void checkIndex(const QString &fieldName);
    bool compact();

protected:
    bool checkStateConsistency();
    void checkIndexConsistency(ObjectTable *table, JsonDbIndex *index);
    void updateIndex(ObjectTable *table, JsonDbIndex *index);
    void updateView(ObjectTable *table);
    void updateView(const QString &objectType);

    IndexQuery *compileIndexQuery(const JsonDbOwner *owner, const JsonDbQuery &query);
    void compileOrQueryTerm(IndexQuery *indexQuery, const QueryTerm &queryTerm);

    void doIndexQuery(const JsonDbOwner *owner, QsonList &results, int &limit, int &offset,
                      IndexQuery *indexQuery);
    void doMultiIndexQuery(const JsonDbOwner *owner, QsonList &results, int &limit, int &offset,
                           const QList<IndexQuery *> &indexQueries);

    static void sortValues(const JsonDbQuery *query, QsonList &results, QsonList &joinedResults);

private:
    JsonDb     *mJsonDb;
    ObjectTable *mObjectTable;
    AoDb        *mBdbIndexes;
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
    WithTransaction(JsonDbBtreeStorage *storage, QString name=QString())
        : mStorage(storage)
    {
        Q_UNUSED(name)
        //qDebug() << "WithTransaction" << "depth" << mStorage->mTransactionDepth << name;
        if (!mStorage->beginTransaction())
            mStorage = 0;
    }

    ~WithTransaction()
    {
        if (mStorage)
            mStorage->commitTransaction();
    }

    bool hasBegin() { return mStorage; }
    bool addObjectTable(ObjectTable *table);

    void abort()
    {
        mStorage->abortTransaction();
        mStorage = 0;
    }

    void commit(quint32 stateNumber = 0)
    {
        mStorage->commitTransaction(stateNumber);
        mStorage = 0;
    }

private:
    JsonDbBtreeStorage *mStorage;
};

#define CHECK_LOCK(lock, context) \
    if (!lock.hasBegin()) { \
        QsonMap errormap;  \
        errormap.insert(JsonDbString::kCodeStr, (int)JsonDbError::DatabaseError); \
        QString error = QString("Failed to set begin transaction [%1:%2]") \
                                .arg(context, QString::number(__LINE__)); \
        qCritical() << error; \
    }

#define CHECK_LOCK_RETURN(lock, context) \
    if (!lock.hasBegin()) { \
        QsonMap errormap; \
        errormap.insert(JsonDbString::kCodeStr, (int)JsonDbError::DatabaseError); \
        QString error = QString("Failed to set begin transaction [%1:%2]") \
                                .arg(context, QString::number(__LINE__)); \
        errormap.insert(JsonDbString::kMessageStr, error); \
        QsonMap result; \
        result.insert(JsonDbString::kResultStr, QsonObject::NullValue); \
        result.insert(JsonDbString::kErrorStr, errormap); \
        return result; \
    }

QByteArray makeForwardKey(const QVariant &fieldValue, const ObjectKey &objectKey);
int forwardKeyCmp(const char *aptr, size_t asiz, const char *bptr, size_t bsiz, void *op);
void forwardKeySplit(const QByteArray &forwardKey, QVariant &fieldValue);
void forwardKeySplit(const QByteArray &forwardKey, QVariant &fieldValue, ObjectKey &objectKey);
QByteArray makeForwardValue(const ObjectKey &);
void forwardValueSplit(const QByteArray &forwardValue, ObjectKey &objectKey);

QDebug &operator<<(QDebug &, const ObjectKey &);

} } // end namespace QtAddOn::JsonDb

QT_END_HEADER

#endif /* JSONDB_H */
