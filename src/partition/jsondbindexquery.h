/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDB_INDEXQUERY_H
#define JSONDB_INDEXQUERY_H

#include <QJsonValue>
#include <QSet>
#include <QVector>
#include <QStringList>

#include "jsondbpartitionglobal.h"
#include "jsondbobject.h"
#include "jsondbobjectkey.h"
#include "jsondbquery.h"
#include "jsondbbtree.h"
#include "jsondbquery.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbObjectTable;
class JsonDbOwner;
class JsonDbPartition;
class JsonDbQuery;

class Q_JSONDB_PARTITION_EXPORT JsonDbQueryConstraint {
public:
    virtual ~JsonDbQueryConstraint() { }
    virtual bool matches(const QJsonValue &value) = 0;
    virtual bool sparseMatchPossible() const { return false; }
};

class Q_JSONDB_PARTITION_EXPORT JsonDbIndexQuery
{
protected:
    JsonDbIndexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
               const QString &propertyName, const QString &propertyType,
               const JsonDbOwner *owner, const JsonDbQuery &query);
public:
    static JsonDbIndexQuery *indexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                                  const QString &propertyName, const QString &propertyType,
                                  const JsonDbOwner *owner, const JsonDbQuery &query);
    virtual ~JsonDbIndexQuery();

    JsonDbObjectTable *objectTable() const { return mObjectTable; }
    QString partition() const;
    void addConstraint(JsonDbQueryConstraint *qc) { mQueryConstraints.append(qc); }
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
    JsonDbObject first(QByteArray *key); // returns first matching object and its key
    JsonDbObject next(QByteArray *key); // returns next matching object and its keyb
    JsonDbObject seek(const QByteArray &key); // returns the object matching key
    bool matches(const QJsonValue &value);
    QJsonValue fieldValue() const { return mFieldValue; }

    const JsonDbQuery &query() const { return mQuery; }

    const JsonDbQuery &residualQuery() const { return mResidualQuery; }
    void setResidualQuery(const JsonDbQuery &residualQuery)
    { Q_ASSERT(!residualQuery.query.isEmpty()); mResidualQuery = residualQuery; }

    virtual quint32 stateNumber() const;

    void compileOrQueryTerm(const JsonDbQueryTerm &queryTerm);
    JsonDbObject resultObject(const JsonDbObject &object);

    static bool lessThan(const QJsonValue &a, const QJsonValue &b);
    static bool greaterThan(const QJsonValue &a, const QJsonValue &b);

protected:
    virtual bool seekToStart(QJsonValue &fieldValue);
    virtual bool seekToStart(QJsonValue &fieldValue, QByteArray *key);
    virtual bool seekToNext(QJsonValue &fieldValue);
    virtual bool seekToNext(QJsonValue &fieldValue, QByteArray *key);
    virtual bool seekTo(const QByteArray &key, QJsonValue &fieldValue);
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
    QString       mUuid;
    QVector<JsonDbQueryConstraint*> mQueryConstraints;
    QString       mAggregateOperation;
    QString       mPropertyName;
    QString       mPropertyType;
    QJsonValue     mFieldValue; // value of field for the object the cursor is pointing at
    bool          mSparseMatchPossible;
    QHash<QString, JsonDbObject> mObjectCache;
    QStringList  mResultExpressionList;
    QStringList  mResultKeyList;
    QVector<QVector<QStringList> > mJoinPaths;
    JsonDbQuery  mQuery;
    JsonDbQuery  mResidualQuery;

    Q_DISABLE_COPY(JsonDbIndexQuery)
};

class JsonDbUuidQuery : public JsonDbIndexQuery {
protected:
    JsonDbUuidQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                    const QString &propertyName, const JsonDbOwner *owner,
                    const JsonDbQuery &query);
    virtual bool seekToStart(QJsonValue &fieldValue);
    virtual bool seekToStart(QJsonValue &fieldValue, QByteArray *key);
    virtual bool seekToNext(QJsonValue &fieldValue);
    virtual bool seekToNext(QJsonValue &fieldValue, QByteArray *key);
    virtual bool seekTo(const QByteArray &key, QJsonValue &fieldValue) { return false; }
    virtual JsonDbObject currentObjectAndTypeNumber(ObjectKey &objectKey);
    virtual quint32 stateNumber() const;
    friend class JsonDbIndexQuery;
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_INDEXQUERY_H
