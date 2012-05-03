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

#include "jsondbindex.h"
#include "jsondbindexquery.h"
#include "jsondbobjecttable.h"
#include "jsondbpartition.h"
#include "jsondbsettings.h"
#include "qbtree.h"
#include "qbtreecursor.h"
#include "qbtreetxn.h"

#include <QJsonDocument>

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

JsonDbIndexQuery *JsonDbIndexQuery::indexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                                   const QString &propertyName, const QString &propertyType,
                                   const JsonDbOwner *owner, bool ascending)
{
    if (propertyName == JsonDbString::kUuidStr)
        return new JsonDbUuidQuery(partition, table, propertyName, owner, ascending);
    else
        return new JsonDbIndexQuery(partition, table, propertyName, propertyType, owner, ascending);
}

JsonDbUuidQuery::JsonDbUuidQuery(JsonDbPartition *partition, JsonDbObjectTable *table, const QString &propertyName, const JsonDbOwner *owner, bool ascending)
    : JsonDbIndexQuery(partition, table, propertyName, QLatin1String("string"), owner, ascending)
{
}

JsonDbIndexQuery::JsonDbIndexQuery(JsonDbPartition *partition, JsonDbObjectTable *table,
                       const QString &propertyName, const QString &propertyType,
                       const JsonDbOwner *owner, bool ascending)
    : mPartition(partition)
    , mObjectTable(table)
    , mBdbIndex(0)
    , mCursor(0)
    , mOwner(owner)
    , mMin(QJsonValue::Undefined)
    , mMax(QJsonValue::Undefined)
    , mAscending(ascending)
    , mPropertyName(propertyName)
    , mPropertyType(propertyType)
    , mSparseMatchPossible(false)
    , mResidualQuery(0)
{
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
    delete mResidualQuery;
    if (isOwnTransaction)
        mTxn->abort();
    delete mCursor;
    for (int i = 0; i < mQueryConstraints.size(); i++)
        delete mQueryConstraints[i];
}

QString JsonDbIndexQuery::partition() const
{
    return mPartition->name();
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
    mMin = makeFieldValue(value, mPropertyType);
    if (mPropertyName != JsonDbString::kUuidStr)
        truncateFieldValue(&mMin, mPropertyType);
}

void JsonDbIndexQuery::setMax(const QJsonValue &value)
{
    mMax = makeFieldValue(value, mPropertyType);
    if (mPropertyName != JsonDbString::kUuidStr)
        truncateFieldValue(&mMax, mPropertyType);
}

bool JsonDbIndexQuery::seekToStart(QJsonValue &fieldValue)
{
    QByteArray forwardKey;
    if (mAscending) {
        forwardKey = makeForwardKey(mMin, ObjectKey());
        if (jsondbSettings->debugQuery())
            qDebug() << __FUNCTION__ << __LINE__ << "mMin" << mMin << "key" << forwardKey.toHex();
    } else {
        forwardKey = makeForwardKey(mMax, ObjectKey());
        if (jsondbSettings->debugQuery())
            qDebug() << __FUNCTION__ << __LINE__ << "mMax" << mMin << "key" << forwardKey.toHex();
    }

    bool ok = false;
    if (mAscending) {
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
        QByteArray baKey;
        mCursor->current(&baKey, 0);
        forwardKeySplit(baKey, fieldValue);
    }
    //qDebug() << "IndexQuery::seekToStart" << (mAscending ? mMin : mMax) << "ok" << ok << fieldValue;
    return ok;
}

bool JsonDbIndexQuery::seekToNext(QJsonValue &fieldValue)
{
    bool ok = mAscending ? mCursor->next() : mCursor->previous();
    if (ok) {
        QByteArray baKey;
        mCursor->current(&baKey, 0);
        forwardKeySplit(baKey, fieldValue);
    }
    //qDebug() << "IndexQuery::seekToNext" << "ok" << ok << fieldValue;
    return ok;
}

JsonDbObject JsonDbIndexQuery::currentObjectAndTypeNumber(ObjectKey &objectKey)
{
    QByteArray baValue;
    mCursor->current(0, &baValue);
    forwardValueSplit(baValue, objectKey);

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
    if (mAscending) {
        if (!mMin.isUndefined()) {
            ObjectKey objectKey(mMin.toString());
            ok = mCursor->seekRange(objectKey.toByteArray());
        } else {
            ok = mCursor->first();
        }
    } else {
        if (!mMax.isUndefined()) {
            ObjectKey objectKey(mMax.toString());
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
        if (mAscending)
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

bool JsonDbUuidQuery::seekToNext(QJsonValue &fieldValue)
{
    bool ok = mAscending ? mCursor->next() : mCursor->previous();
    QByteArray baKey;
    while (ok) {
        mCursor->current(&baKey, 0);
        if (baKey.size() == 16)
            break;
        if (mAscending)
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

JsonDbObject JsonDbIndexQuery::first()
{
    mSparseMatchPossible = false;
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        mSparseMatchPossible |= mQueryConstraints[i]->sparseMatchPossible();
    }

    QJsonValue fieldValue;
    bool ok = seekToStart(fieldValue);
    if (jsondbSettings->debugQuery())
        qDebug() << "IndexQuery::first" << __LINE__ << "ok after first/last()" << ok;
    for (; ok; ok = seekToNext(fieldValue)) {
        mFieldValue = fieldValue;
        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::first()"
                     << "mPropertyName" << mPropertyName
                     << "fieldValue" << fieldValue
                     << (mAscending ? "ascending" : "descending");

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

        if (mResidualQuery && !mResidualQuery->match(object, &mObjectCache, mPartition))
            continue;

        if (jsondbSettings->debugQuery())
            qDebug() << "IndexQuery::first()" << "returning objectKey" << objectKey;

        return object;
    }
    mUuid.clear();
    return QJsonObject();
}

JsonDbObject JsonDbIndexQuery::next()
{
    QJsonValue fieldValue;
    while (seekToNext(fieldValue)) {
        mFieldValue = fieldValue;
        if (jsondbSettings->debugQuery()) {
            qDebug() << "IndexQuery::next()" << "mPropertyName" << mPropertyName
                     << "fieldValue" << fieldValue
                     << (mAscending ? "ascending" : "descending");
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

        if (mResidualQuery && !mResidualQuery->match(object, &mObjectCache, mPartition))
            continue;

        return object;
    }
    mUuid.clear();
    return QJsonObject();
}

JsonDbObject JsonDbIndexQuery::resultObject(const JsonDbObject &object)
{
    int numExpressions = mResultExpressionList.length();
    QJsonObject result;
    JsonDbObject baseObject(object);

    // insert the computed index value
    baseObject.insert(JsonDbString::kIndexValueStr, mFieldValue);

    if (mResultKeyList.isEmpty())
        result = baseObject;

    if (!numExpressions)
        return result;

    for (int i = 0; i < numExpressions; i++) {
        QJsonValue v;

        QVector<QStringList> &joinPath = mJoinPaths[i];
        int joinPathSize = joinPath.size();
        for (int j = 0; j < joinPathSize-1; j++) {
            QJsonValue uuidQJsonValue = baseObject.propertyLookup(joinPath[j]).toString();
            QString uuid = uuidQJsonValue.toString();
            if (uuid.isEmpty()) {
                baseObject = JsonDbObject();
            } else if (mObjectCache.contains(uuid)) {
                baseObject = mObjectCache.value(uuid);
            } else {
                ObjectKey objectKey(uuid);
                bool gotBaseObject = mPartition->getObject(objectKey, baseObject);
                if (gotBaseObject)
                    mObjectCache.insert(uuid, baseObject);
            }
        }
        v = baseObject.propertyLookup(joinPath[joinPathSize-1]);
        result.insert(mResultKeyList[i], v);
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
