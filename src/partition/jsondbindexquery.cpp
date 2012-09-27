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

#include "jsondbindexquery.h"
#include "jsondbindex.h"
#include "jsondbindex_p.h"
#include "jsondbobjecttable.h"
#include "jsondbpartition.h"
#include "jsondbpartition_p.h"
#include "jsondbsettings.h"
#include "jsondbutils_p.h"

#include <QJsonDocument>
#include <QRegExp>

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class QueryConstraintGt: public JsonDbQueryConstraint {
public:
    QueryConstraintGt(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return JsonDbIndexQuery::greaterThan(v, mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintGe: public JsonDbQueryConstraint {
public:
    QueryConstraintGe(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return JsonDbIndexQuery::greaterThan(v, mValue) || (v == mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintLt: public JsonDbQueryConstraint {
public:
    QueryConstraintLt(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return JsonDbIndexQuery::lessThan(v, mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintLe: public JsonDbQueryConstraint {
public:
    QueryConstraintLe(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return JsonDbIndexQuery::lessThan(v, mValue) || (v == mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintEq: public JsonDbQueryConstraint {
public:
    QueryConstraintEq(const QJsonValue &v) { mValue = v; }
    inline bool matches(const QJsonValue &v) { return v == mValue; }
private:
    QJsonValue mValue;
};
class QueryConstraintNe: public JsonDbQueryConstraint {
public:
    QueryConstraintNe(const QJsonValue &v) { mValue = v; }
    inline bool sparseMatchPossible() const { return true; }
    inline bool matches(const QJsonValue &v) { return v != mValue; }
private:
    QJsonValue mValue;
};
class QueryConstraintExists: public JsonDbQueryConstraint {
public:
    QueryConstraintExists() { }
    inline bool matches(const QJsonValue &v) { return !v.isUndefined(); }
};
class QueryConstraintNotExists: public JsonDbQueryConstraint {
public:
    QueryConstraintNotExists() { }
    // this will never match
    inline bool matches(const QJsonValue &v) { return v.isUndefined(); }
};
class QueryConstraintIn: public JsonDbQueryConstraint {
public:
    QueryConstraintIn(const QJsonValue &v) { mList = v.toArray();}
    inline bool sparseMatchPossible() const { return true; }
    inline bool matches(const QJsonValue &v) { return mList.contains(v); }
private:
    QJsonArray mList;
};
class QueryConstraintNotIn: public JsonDbQueryConstraint {
public:
    QueryConstraintNotIn(const QJsonValue &v) { mList = v.toArray();}
    inline bool sparseMatchPossible() const { return true; }
    inline bool matches(const QJsonValue &v) { return !mList.contains(v); }
private:
    QJsonArray mList;
};
class QueryConstraintStartsWith: public JsonDbQueryConstraint {
public:
    QueryConstraintStartsWith(const QString &v) { mValue = v;}
    inline bool sparseMatchPossible() const { return true; }
    inline bool matches(const QJsonValue &v) { return (v.type() == QJsonValue::String) && v.toString().startsWith(mValue); }
private:
    QString mValue;
};
class QueryConstraintRegExp: public JsonDbQueryConstraint {
public:
    QueryConstraintRegExp(const QRegExp &regexp, bool negated=false) : mRegExp(regexp), mNegated(negated) {}
    inline void setNegated(bool negated) { mNegated = negated; }
    inline bool matches(const QJsonValue &v) { bool matches = mRegExp.exactMatch(v.toString()); if (mNegated) return !matches; else return matches; }
    inline bool sparseMatchPossible() const { return true; }
private:
    QString mValue;
    QRegExp mRegExp;
    bool mNegated;
};

JsonDbIndexQuery *JsonDbIndexQuery::indexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                                   const QString &propertyName, const QString &propertyType,
                                   const JsonDbOwner *owner, const JsonDbQuery &query)
{
    if (propertyName == JsonDbString::kUuidStr)
        return new JsonDbUuidQuery(partition, table, propertyName, owner, query);
    else
        return new JsonDbIndexQuery(partition, table, propertyName, propertyType, owner, query);
}

JsonDbUuidQuery::JsonDbUuidQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                                 const QString &propertyName, const JsonDbOwner *owner,
                                 const JsonDbQuery &query)
    : JsonDbIndexQuery(partition, table, propertyName, QStringLiteral("string"), owner, query)
{
}

JsonDbIndexQuery::JsonDbIndexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                       const QString &propertyName, const QString &propertyType,
                       const JsonDbOwner *owner, const JsonDbQuery &query)
    : mPartition(partition)
    , mObjectTable(table)
    , mBdbIndex(0)
    , mCursor(0)
    , mOwner(owner)
    , mMin(QJsonValue::Undefined)
    , mMax(QJsonValue::Undefined)
    , mPropertyName(propertyName)
    , mPropertyType(propertyType)
    , mSparseMatchPossible(false)
    , mQuery(query)
{
    mResidualQuery.query = mQuery.query;
    mResidualQuery.bindings = mQuery.bindings;

    if (propertyName != JsonDbString::kUuidStr) {
        mBdbIndex = table->index(propertyName)->bdb();
        isOwnTransaction = !mBdbIndex->writeTransaction();
        mTxn = isOwnTransaction ? mBdbIndex->beginWrite() : mBdbIndex->writeTransaction();
        mCursor = new JsonDbBtree::Cursor(mTxn);
    } else {
        isOwnTransaction = !table->bdb()->writeTransaction();
        mTxn = isOwnTransaction ? table->bdb()->beginWrite() : table->bdb()->writeTransaction();
        mCursor = new JsonDbBtree::Cursor(mTxn);
    }
}
JsonDbIndexQuery::~JsonDbIndexQuery()
{
    if (isOwnTransaction)
        mTxn->abort();
    delete mCursor;
    for (int i = 0; i < mQueryConstraints.size(); i++)
        delete mQueryConstraints[i];
}

QString JsonDbIndexQuery::partition() const
{
    return mPartition->partitionSpec().name;
}

quint32 JsonDbIndexQuery::stateNumber() const
{
    return mBdbIndex->tag();
}

bool JsonDbIndexQuery::matches(const QJsonValue &fieldValue)
{
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        if (!mQueryConstraints[i]->matches(fieldValue))
            return false;
    }
    return true;
}

void JsonDbIndexQuery::setMin(const QJsonValue &value)
{
    mMin = JsonDbIndexPrivate::makeFieldValue(value, mPropertyType);
    if (mPropertyName != JsonDbString::kUuidStr)
        JsonDbIndexPrivate::truncateFieldValue(&mMin, mPropertyType);
}

void JsonDbIndexQuery::setMax(const QJsonValue &value)
{
    mMax = JsonDbIndexPrivate::makeFieldValue(value, mPropertyType);
    if (mPropertyName != JsonDbString::kUuidStr)
        JsonDbIndexPrivate::truncateFieldValue(&mMax, mPropertyType);
}

bool JsonDbIndexQuery::seekToStart(QJsonValue &fieldValue, QByteArray *key)
{
    QByteArray forwardKey;
    if (mQuery.isAscending()) {
        forwardKey = JsonDbIndexPrivate::makeForwardKey(mMin, ObjectKey());
        if (jsondbSettings->debugQuery())
            qDebug() << __FUNCTION__ << __LINE__ << "mMin" << mMin << "key" << forwardKey.toHex();
    } else {
        forwardKey = JsonDbIndexPrivate::makeForwardKey(mMax, ObjectKey());
        if (jsondbSettings->debugQuery())
            qDebug() << __FUNCTION__ << __LINE__ << "mMax" << mMin << "key" << forwardKey.toHex();
    }

    bool ok = false;
    if (mQuery.isAscending()) {
        if (!mMin.isUndefined()) {
            ok = mCursor->seekRange(forwardKey);
            if (jsondbSettings->debugQuery())
                qDebug() << "IndexQuery::first" << __LINE__ << "ok after seekRange" << ok;
        }
        if (!ok) {
            ok = mCursor->first();
        }
    } else {
        // need a seekDescending
        ok = mCursor->last();
    }
    if (ok) {
        mCursor->current(key, 0);
        JsonDbIndexPrivate::forwardKeySplit(*key, fieldValue);
    }
    //qDebug() << "IndexQuery::seekToStart" << (mAscending ? mMin : mMax) << "ok" << ok << fieldValue;
    return ok;
}

bool JsonDbIndexQuery::seekToStart(QJsonValue &fieldValue)
{
    QByteArray baKey;
    return seekToStart(fieldValue, &baKey);
}

bool JsonDbIndexQuery::seekToNext(QJsonValue &fieldValue, QByteArray *key)
{
    bool ok = mQuery.isAscending() ? mCursor->next() : mCursor->previous();
    if (ok) {
        mCursor->current(key, 0);
        JsonDbIndexPrivate::forwardKeySplit(*key, fieldValue);
    }
    //qDebug() << "IndexQuery::seekToNext" << "ok" << ok << fieldValue;
    return ok;
}

bool JsonDbIndexQuery::seekToNext(QJsonValue &fieldValue)
{
    QByteArray baKey;
    return seekToNext(fieldValue, &baKey);
}

bool JsonDbIndexQuery::seekTo(const QByteArray &key, QJsonValue &fieldValue)
{
   bool ok = mCursor->seek(key);
   if (ok) {
       QByteArray baKey;
       mCursor->current(&baKey, 0);
       JsonDbIndexPrivate::forwardKeySplit(key, fieldValue);
   }
   return ok;
}

JsonDbObject JsonDbIndexQuery::currentObjectAndTypeNumber(ObjectKey &objectKey)
{
    QByteArray baValue;
    mCursor->current(0, &baValue);
    JsonDbIndexPrivate::forwardValueSplit(baValue, objectKey);

    if (jsondbSettings->debugQuery())
        qDebug() << __FILE__ << __LINE__ << "objectKey" << objectKey << baValue.toHex();
    JsonDbObject object;
    mObjectTable->get(objectKey, &object);
    return object;
}

quint32 JsonDbUuidQuery::stateNumber() const
{
    return mObjectTable->stateNumber();
}

bool JsonDbUuidQuery::seekToStart(QJsonValue &fieldValue)
{
    bool ok;
    if (mQuery.isAscending()) {
        if (!mMin.isUndefined()) {
            ObjectKey objectKey(mMin.toString());
            if (!objectKey.isValid())
                return false;
            ok = mCursor->seekRange(objectKey.toByteArray());
        } else {
            ok = mCursor->first();
        }
    } else {
        if (!mMax.isUndefined()) {
            ObjectKey objectKey(mMax.toString());
            if (objectKey.isValid())
                return false;
            ok = mCursor->seekRange(objectKey.toByteArray());
        } else {
            ok = mCursor->last();
        }
    }
    QByteArray baKey;
    while (ok) {
        mCursor->current(&baKey, 0);
        if (baKey.size() == 16)
            break;
        if (mQuery.isAscending())
            ok = mCursor->next();
        else
            ok = mCursor->previous();
    }
    if (ok) {
        QUuid quuid(QUuid::fromRfc4122(baKey));
        ObjectKey objectKey(quuid);
        fieldValue = objectKey.key.toString();
    }
    return ok;
}

bool JsonDbUuidQuery::seekToStart(QJsonValue &fieldValue, QByteArray *key)
{
    *key = QByteArray();
    return seekToStart (fieldValue);
}

bool JsonDbUuidQuery::seekToNext(QJsonValue &fieldValue)
{
    bool ok = mQuery.isAscending() ? mCursor->next() : mCursor->previous();
    QByteArray baKey;
    while (ok) {
        mCursor->current(&baKey, 0);
        if (baKey.size() == 16)
            break;
        if (mQuery.isAscending())
            ok = mCursor->next();
        else
            ok = mCursor->previous();
    }
    if (ok) {
        QUuid quuid(QUuid::fromRfc4122(baKey));
        ObjectKey objectKey(quuid);
        fieldValue = objectKey.key.toString();
    }
    return ok;
}

bool JsonDbUuidQuery::seekToNext(QJsonValue &fieldValue, QByteArray *key)
{
    *key = QByteArray();
    return seekToNext (fieldValue);
}

JsonDbObject JsonDbUuidQuery::currentObjectAndTypeNumber(ObjectKey &objectKey)
{
    QByteArray baKey, baValue;
    mCursor->current(&baKey, &baValue);
    objectKey = ObjectKey(baKey);

    if (jsondbSettings->debugQuery())
        qDebug() << __FILE__ << __LINE__ << "objectKey" << objectKey << baKey.toHex();
    JsonDbObject object(QJsonDocument::fromBinaryData(baValue).object());
    return object;
}

void JsonDbIndexQuery::setResultExpressionList(const QStringList &resultExpressionList)
{
    mResultExpressionList = resultExpressionList;
    int numExpressions = resultExpressionList.size();
    mJoinPaths.resize(numExpressions);
    for (int i = 0; i < numExpressions; i++) {
        const QString &propertyName = resultExpressionList.at(i);
        QStringList joinPath = propertyName.split(QStringLiteral("->"));
        int joinPathSize = joinPath.size();
        QVector<QStringList> fieldPaths(joinPathSize);
        for (int j = 0; j < joinPathSize; j++) {
            QString joinField = joinPath[j];
            fieldPaths[j] = joinField.split('.');
        }
        mJoinPaths[i] = fieldPaths;
    }
}

JsonDbObject JsonDbIndexQuery::first(QByteArray *key)
{
    mSparseMatchPossible = false;
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        mSparseMatchPossible |= mQueryConstraints[i]->sparseMatchPossible();
    }

    QJsonValue fieldValue;
    bool ok = seekToStart(fieldValue, key);
    if (jsondbSettings->debugQuery())
        qDebug() << "IndexQuery::first" << __LINE__ << "ok after first/last()" << ok;
    for (; ok; ok = seekToNext(fieldValue, key)) {
        mFieldValue = fieldValue;
        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::first()"
                     << "mPropertyName" << mPropertyName
                     << "fieldValue" << fieldValue
                     << (mQuery.isAscending() ? "ascending" : "descending");

        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::first()" << "matches(fieldValue)" << matches(fieldValue);

        if (!matches(fieldValue))
            continue;

        ObjectKey objectKey;
        JsonDbObject object(currentObjectAndTypeNumber(objectKey));
        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::first()" << __LINE__ << "objectKey" << objectKey << object.value(JsonDbString::kDeletedStr).toBool();
        if (object.contains(JsonDbString::kDeletedStr) && object.value(JsonDbString::kDeletedStr).toBool())
            continue;

        if (!mTypeNames.isEmpty() && !mTypeNames.contains(object.value(JsonDbString::kTypeStr).toString()))
            continue;
        if (jsondbSettings->debugQuery())
            qDebug() << "mTypeName" << mTypeNames << "!contains" << object << "->" << object.value(JsonDbString::kTypeStr);

        if (!mResidualQuery.isEmpty() && !mResidualQuery.match(object, &mObjectCache, mPartition))
            continue;

        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::first()" << "returning objectKey" << objectKey;

        return object;
    }
    mUuid.clear();
    return QJsonObject();
}

JsonDbObject JsonDbIndexQuery::first()
{
    QByteArray key;
    return first (&key);
}

JsonDbObject JsonDbIndexQuery::seek(const QByteArray &key)
{
    mSparseMatchPossible = false;
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        mSparseMatchPossible |= mQueryConstraints[i]->sparseMatchPossible();
    }

    QJsonValue fieldValue;
    bool ok = seekTo(key, fieldValue);
    if (jsondbSettings->debugQuery())
        qDebug() << "IndexQuery::seek" << __LINE__ << "ok after seekTo()" << ok;
    while (ok && matches(fieldValue)) {
        mFieldValue = fieldValue;
        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::seek()"
                     << "mPropertyName" << mPropertyName
                     << "fieldValue" << fieldValue
                     << (mQuery.isAscending() ? "ascending" : "descending");

        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::seek()" << "matches(fieldValue)" << matches(fieldValue);

        ObjectKey objectKey;
        JsonDbObject object(currentObjectAndTypeNumber(objectKey));
        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::first()" << __LINE__ << "objectKey" << objectKey << object.value(JsonDbString::kDeletedStr).toBool();
        if (object.contains(JsonDbString::kDeletedStr) && object.value(JsonDbString::kDeletedStr).toBool())
            break;

        if (!mTypeNames.isEmpty() && !mTypeNames.contains(object.value(JsonDbString::kTypeStr).toString()))
            break;
        if (jsondbSettings->debugQuery())
            qDebug() << "mTypeName" << mTypeNames << "!contains" << object << "->" << object.value(JsonDbString::kTypeStr);

        if (!mResidualQuery.isEmpty() && !mResidualQuery.match(object, &mObjectCache, mPartition))
            break;

        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::first()" << "returning objectKey" << objectKey;

        return object;
    }
    mUuid.clear();
    return QJsonObject();
}

JsonDbObject JsonDbIndexQuery::next(QByteArray *key)
{
    QJsonValue fieldValue;
    while (seekToNext(fieldValue, key)) {
        mFieldValue = fieldValue;
        if (jsondbSettings->debugQuery()) {
            qDebug() << "IndexQuery::next()" << "mPropertyName" << mPropertyName
                     << "fieldValue" << fieldValue
                     << (mQuery.isAscending() ? "ascending" : "descending");
            qDebug() << "IndexQuery::next()" << "matches(fieldValue)" << matches(fieldValue);
        }
        if (!matches(fieldValue)) {
            if (mSparseMatchPossible)
                continue;
            else
                break;
        }

        ObjectKey objectKey;
        JsonDbObject object(currentObjectAndTypeNumber(objectKey));
        if (object.contains(JsonDbString::kDeletedStr) && object.value(JsonDbString::kDeletedStr).toBool())
            continue;

        if (!mTypeNames.isEmpty() && !mTypeNames.contains(object.value(JsonDbString::kTypeStr).toString()))
            continue;

        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::next()" << "objectKey" << objectKey;

        if (!mResidualQuery.isEmpty() && !mResidualQuery.match(object, &mObjectCache, mPartition))
            continue;

        return object;
    }
    mUuid.clear();
    return QJsonObject();
}

JsonDbObject JsonDbIndexQuery::next()
{
    QByteArray key;
    return next (&key);
}

void JsonDbIndexQuery::compileOrQueryTerm(const JsonDbQueryTerm &queryTerm)
{
    static const QRegExp wildCardPrefixRegExp(QStringLiteral("([^*?\\[\\]\\\\]+).*"));

    QString op = queryTerm.op();
    QJsonValue fieldValue = mQuery.termValue(queryTerm);

    if (propertyName() != JsonDbString::kUuidStr)
        JsonDbIndexPrivate::truncateFieldValue(&fieldValue, propertyType());

    if (op == QLatin1Char('>')) {
        addConstraint(new QueryConstraintGt(fieldValue));
        setMin(fieldValue);
    } else if (op == QLatin1String(">=")) {
        addConstraint(new QueryConstraintGe(fieldValue));
        setMin(fieldValue);
    } else if (op == QLatin1Char('<')) {
        addConstraint(new QueryConstraintLt(fieldValue));
        setMax(fieldValue);
    } else if (op == QLatin1String("<=")) {
        addConstraint(new QueryConstraintLe(fieldValue));
        setMax(fieldValue);
    } else if (op == QLatin1Char('=')) {
        addConstraint(new QueryConstraintEq(fieldValue));
        setMin(fieldValue);
        setMax(fieldValue);
    } else if (op == QLatin1String("=~")
               || op == QLatin1String("!=~")) {
        const QRegExp &re = queryTerm.regExpConst();
        QRegExp::PatternSyntax syntax = re.patternSyntax();
        Qt::CaseSensitivity cs = re.caseSensitivity();
        QString pattern = re.pattern();
        addConstraint(new QueryConstraintRegExp(re, (op == QLatin1String("=~") ? false : true)));
        if (cs == Qt::CaseSensitive) {
            QString prefix;
            if ((syntax == QRegExp::Wildcard)
                && wildCardPrefixRegExp.exactMatch(pattern)) {
                prefix = wildCardPrefixRegExp.cap(1);
                if (jsondbSettings->debug())
                    qDebug() << "wildcard regexp prefix" << pattern << prefix;
            }
            setMin(prefix);
            setMax(prefix);
        }
    } else if (op == QLatin1String("!=")) {
        addConstraint(new QueryConstraintNe(fieldValue));
    } else if (op == QLatin1String("exists")) {
        addConstraint(new QueryConstraintExists);
    } else if (op == QLatin1String("notExists")) {
        addConstraint(new QueryConstraintNotExists);
    } else if (op == QLatin1String("in")) {
        QJsonArray value = mQuery.termValue(queryTerm).toArray();
        if (value.size() == 1)
            addConstraint(new QueryConstraintEq(value.at(0)));
        else
            addConstraint(new QueryConstraintIn(mQuery.termValue(queryTerm)));
    } else if (op == QLatin1String("notIn")) {
        addConstraint(new QueryConstraintNotIn(mQuery.termValue(queryTerm)));
    } else if (op == QLatin1String("startsWith")) {
        addConstraint(new QueryConstraintStartsWith(mQuery.termValue(queryTerm).toString()));
    }
}

JsonDbObject JsonDbIndexQuery::resultObject(const JsonDbObject &object)
{
    QJsonObject result;
    JsonDbObject baseObject(object);

    // insert the computed index value
    baseObject.insert(JsonDbString::kIndexValueStr, mFieldValue);

    Q_ASSERT(mResultKeyList.size() == mResultExpressionList.size());
    if (mResultKeyList.isEmpty())
        return baseObject;

    for (int i = 0; i < mResultExpressionList.size(); ++i) {
        JsonDbObject obj(baseObject);

        const QVector<QStringList> &joinPath = mJoinPaths.at(i);
        for (int j = 0; j < joinPath.size()-1; j++) {
            QString uuid = obj.valueByPath(joinPath.at(j)).toString();
            if (uuid.isEmpty()) {
                obj = JsonDbObject();
            } else if (mObjectCache.contains(uuid)) {
                obj = mObjectCache.value(uuid);
            } else {
                 if (mPartition->d_func()->getObject(ObjectKey(uuid), obj))
                    mObjectCache.insert(uuid, obj);
            }
        }
        QJsonValue v = obj.valueByPath(joinPath.last());
        result.insert(mResultKeyList.at(i), v);
    }

    return result;
}

bool JsonDbIndexQuery::lessThan(const QJsonValue &a, const QJsonValue &b)
{
    if (a.type() == b.type()) {
        if (a.type() == QJsonValue::Double) {
            return a.toDouble() < b.toDouble();
        } else if (a.type() == QJsonValue::String) {
            return a.toString() < b.toString();
        } else if (a.type() == QJsonValue::Bool) {
            return a.toBool() < b.toBool();
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool JsonDbIndexQuery::greaterThan(const QJsonValue &a, const QJsonValue &b)
{
    if (a.type() == b.type()) {
        if (a.type() == QJsonValue::Double) {
            return a.toDouble() > b.toDouble();
        } else if (a.type() == QJsonValue::String) {
            return a.toString() > b.toString();
        } else if (a.type() == QJsonValue::Bool) {
            return a.toBool() > b.toBool();
        } else {
            return false;
        }
    } else {
        return false;
    }
}

QT_END_NAMESPACE_JSONDB_PARTITION
