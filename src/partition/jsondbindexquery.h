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

#ifndef JSONDB_INDEXQUERY_H
#define JSONDB_INDEXQUERY_H

#include <QJsonValue>
#include <QRegExp>
#include <QSet>
#include <QVector>
#include <QStringList>

#include "jsondbpartitionglobal.h"
#include "jsondbobject.h"
#include "jsondbobjectkey.h"
#include "jsondbbtree.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbObjectTable;
class JsonDbOwner;
class JsonDbPartition;
class JsonDbQuery;

class QueryConstraint {
public:
    virtual ~QueryConstraint() { }
    virtual bool matches(const QJsonValue &value) = 0;
    virtual bool sparseMatchPossible() const { return false; }
};

class Q_JSONDB_PARTITION_EXPORT JsonDbIndexQuery {
protected:
    JsonDbIndexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
               const QString &propertyName, const QString &propertyType,
               const JsonDbOwner *owner, bool ascending = true);
public:
    static JsonDbIndexQuery *indexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                                  const QString &propertyName, const QString &propertyType,
                                  const JsonDbOwner *owner, bool ascending = true);
    virtual ~JsonDbIndexQuery();

    JsonDbObjectTable *objectTable() const { return mObjectTable; }
    QString partition() const;
    void addConstraint(QueryConstraint *qc) { mQueryConstraints.append(qc); }
    bool ascending() const { return mAscending; }
    QString propertyName() const { return mPropertyName; }
    QString propertyType() const { return mPropertyType; }
    void setTypeNames(const QSet<QString> typeNames) { mTypeNames = typeNames; }
    void setMin(const QJsonValue &minv);
    void setMax(const QJsonValue &maxv);
    QString aggregateOperation() const { return mAggregateOperation; }
    void setAggregateOperation(QString op) { mAggregateOperation = op; }
    void setResultExpressionList(const QStringList &resultExpressionList);
    void setResultKeyList(QStringList resultKeyList) { mResultKeyList = resultKeyList; }

    JsonDbObject first(); // returns first matching object
    JsonDbObject next(); // returns next matching object
    bool matches(const QJsonValue &value);
    QJsonValue fieldValue() const { return mFieldValue; }
    JsonDbQuery *residualQuery() const { return mResidualQuery; }
    void setResidualQuery(JsonDbQuery *residualQuery) { mResidualQuery = residualQuery; }
    virtual quint32 stateNumber() const;

    JsonDbObject resultObject(const JsonDbObject &object);

    static bool lessThan(const QJsonValue &a, const QJsonValue &b);
    static bool greaterThan(const QJsonValue &a, const QJsonValue &b);

protected:
    virtual bool seekToStart(QJsonValue &fieldValue);
    virtual bool seekToNext(QJsonValue &fieldValue);
    virtual JsonDbObject currentObjectAndTypeNumber(ObjectKey &objectKey);

protected:
    JsonDbPartition *mPartition;
    JsonDbObjectTable   *mObjectTable;
    JsonDbBtree *mBdbIndex;
    bool isOwnTransaction;
    JsonDbBtree::Transaction *mTxn;
    JsonDbBtree::Cursor *mCursor;
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
    QStringList  mResultExpressionList;
    QStringList  mResultKeyList;
    QVector<QVector<QStringList> > mJoinPaths;
    JsonDbQuery  *mResidualQuery;
};

class JsonDbUuidQuery : public JsonDbIndexQuery {
protected:
    JsonDbUuidQuery(JsonDbPartition *partition, JsonDbObjectTable *table, const QString &propertyName, const JsonDbOwner *owner, bool ascending = true);
    virtual bool seekToStart(QJsonValue &fieldValue);
    virtual bool seekToNext(QJsonValue &fieldValue);
    virtual JsonDbObject currentObjectAndTypeNumber(ObjectKey &objectKey);
    virtual quint32 stateNumber() const;
    friend class JsonDbIndexQuery;
};

class QueryConstraintGt: public QueryConstraint {
public:
    QueryConstraintGt(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return JsonDbIndexQuery::greaterThan(v, mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintGe: public QueryConstraint {
public:
    QueryConstraintGe(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return JsonDbIndexQuery::greaterThan(v, mValue) || (v == mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintLt: public QueryConstraint {
public:
    QueryConstraintLt(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return JsonDbIndexQuery::lessThan(v, mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintLe: public QueryConstraint {
public:
    QueryConstraintLe(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return JsonDbIndexQuery::lessThan(v, mValue) || (v == mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintEq: public QueryConstraint {
public:
    QueryConstraintEq(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return v == mValue; }
private:
    QJsonValue mValue;
};
class QueryConstraintNe: public QueryConstraint {
public:
    QueryConstraintNe(const QJsonValue &v) { mValue = v; }
    inline bool sparseMatchPossible() const { return true; }
    inline bool matches(const QJsonValue &v) { return v != mValue; }
private:
    QJsonValue mValue;
};
class QueryConstraintExists: public QueryConstraint {
public:
    QueryConstraintExists() { }
    inline bool matches(const QJsonValue &v) { return !v.isUndefined(); }
};
class QueryConstraintNotExists: public QueryConstraint {
public:
    QueryConstraintNotExists() { }
    // this will never match
    inline bool matches(const QJsonValue &v) { return v.isUndefined(); }
};
class QueryConstraintIn: public QueryConstraint {
public:
    QueryConstraintIn(const QJsonValue &v) { mList = v.toArray();}
    inline bool sparseMatchPossible() const { return true; }
    inline bool matches(const QJsonValue &v) { return mList.contains(v); }
private:
    QJsonArray mList;
};
class QueryConstraintNotIn: public QueryConstraint {
public:
    QueryConstraintNotIn(const QJsonValue &v) { mList = v.toArray();}
    inline bool sparseMatchPossible() const { return true; }
    inline bool matches(const QJsonValue &v) { return !mList.contains(v); }
private:
    QJsonArray mList;
};
class QueryConstraintStartsWith: public QueryConstraint {
public:
    QueryConstraintStartsWith(const QString &v) { mValue = v;}
    inline bool sparseMatchPossible() const { return true; }
    inline bool matches(const QJsonValue &v) { return (v.type() == QJsonValue::String) && v.toString().startsWith(mValue); }
private:
    QString mValue;
};
class QueryConstraintRegExp: public QueryConstraint {
public:
    QueryConstraintRegExp(const QRegExp &regexp) : mRegExp(regexp) {}
    inline bool matches(const QJsonValue &v) { return mRegExp.exactMatch(v.toString()); }
    inline bool sparseMatchPossible() const { return true; }
private:
    QString mValue;
    QRegExp mRegExp;
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_INDEXQUERY_H
