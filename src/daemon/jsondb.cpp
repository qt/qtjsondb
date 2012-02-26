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

#include <QFile>
#include <QString>
#include <QStringList>
#include <QStringBuilder>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <QJSValue>
#include <QJSValueIterator>
#include <QElapsedTimer>
#include <QtAlgorithms>
#include <QDebug>

#include "jsondb-strings.h"
#include "jsondb-error.h"

#include <json.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef Q_OS_UNIX
#include <pwd.h>
#endif

#include "jsondb.h"
#include "jsondbproxy.h"
#include "jsondbpartition.h"
#include "jsondbephemeralpartition.h"
#include "jsondbsettings.h"
#include "jsondbview.h"
#include "jsondbschemamanager_impl_p.h"
#include "jsondbobjecttypes_impl_p.h"

QT_BEGIN_NAMESPACE_JSONDB

const QString kSortKeysStr = QLatin1String("sortKeys");
const QString kStateStr = QLatin1String("state");
const QString kIdStr = QLatin1String("_id");

#define RETURN_IF_ERROR(errmap, toCheck) \
    ({ \
        errmap = toCheck; \
        if (!errmap.isEmpty()) { \
            QJsonObject _r; \
            return makeResponse(_r, errmap); \
        } \
    })

void JsonDb::setError(QJsonObject &map, int code, const QString &message)
{
    map.insert(JsonDbString::kCodeStr, code);
    map.insert(JsonDbString::kMessageStr, message);
}

QJsonObject JsonDb::makeError(int code, const QString &message)
{
    QJsonObject map;
    setError(map, code, message);
    return map;
}

QJsonObject JsonDb::makeResponse( QJsonObject& resultmap, QJsonObject& errormap, bool silent )
{
    QJsonObject map;
    if (jsondbSettings->verbose() && !silent && !errormap.isEmpty()) {
        qCritical() << errormap;
    }
    if (!resultmap.isEmpty())
        map.insert( JsonDbString::kResultStr, resultmap);
    else
        map.insert( JsonDbString::kResultStr, QJsonValue());

    if (!errormap.isEmpty())
        map.insert( JsonDbString::kErrorStr, errormap );
    else
        map.insert( JsonDbString::kErrorStr, QJsonValue());
    return map;
}

QJsonObject JsonDb::makeErrorResponse(QJsonObject &resultmap,
                                  int code, const QString &message, bool silent)
{
    QJsonObject errormap;
    setError(errormap, code, message);
    return makeResponse(resultmap, errormap, silent);
}

bool JsonDb::responseIsError( QJsonObject responseMap )
{
    return responseMap.contains(JsonDbString::kErrorStr)
            && responseMap.value(JsonDbString::kErrorStr).isObject();
}

QString JsonDb::uuidhex(uint data, int digits)
{
    return QString::number(data, 16).rightJustified(digits, QLatin1Char('0'));
}

QString JsonDb::createDatabaseId()
{
    QFile devUrandom;
    uint data[3];
    devUrandom.setFileName(QLatin1String("/dev/urandom"));
    if (!devUrandom.open(QIODevice::ReadOnly)) {
    } else {
        qint64 numToRead = 3 * sizeof(uint);
        devUrandom.read((char *) data, numToRead); // should read 128-bits of data
    }
    return (QString("%1-%2-%3")
            .arg(uuidhex(data[0], 8))
            .arg(uuidhex(data[1], 8))
            .arg(uuidhex(data[2], 8)));
}

/*!
  This function takes a single QJsonValue,
  adds a "uuid" field to it with a unique UUID (overwriting a
  previous "uuid" field if necessary), and stores it in the object
  database.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "uuid", STRING } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

  This method must be overridden in a subclass.
*/

/*
  QJsonValue JsonDb::create(const JsonDbOwner *owner, QJsonValue object)
  {
  }
*/

/*!
  This function takes a single QJsonValue with a valid "uuid" field.
  It updates the database to match the new object.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : NULL }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

  This method must be overridden in a subclass.
*/

/*
  QJsonValue JsonDb::update(QJsonValue object)
  {
  }
*/


/*!
  This function takes a QJsonArray of objects. It creates
  the items in the database, assigning each a unique "uuid" field.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }


  This class should be optimized by a JsonDb subclass.
*/

QJsonObject JsonDb::createList(const JsonDbOwner *owner, JsonDbObjectList& list, const QString &partitionName, WriteMode writeMode)
{
    int count = 0;
    QJsonArray resultList;

    JsonDbPartition *partition = findPartition(partitionName);
    if (!partition) {
        QJsonObject resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partitionName));
        return makeResponse(resultmap, errormap);
    }

    quint32 stateNumber = 0;

    int size = list.size();
    int transactionSize = jsondbSettings->transactionSize() ? jsondbSettings->transactionSize() : size;
    for (int k = 0; k < size; k += transactionSize) {
        WithTransaction lock(partition);
        CHECK_LOCK_RETURN(lock, "createList");
        for (int i = k; i < qMin(k+transactionSize, size); ++i) {
            JsonDbObject o = list.at(i);
            QJsonObject r = create(owner, o, partitionName, writeMode);
            stateNumber = r.value(JsonDbString::kStateNumberStr).toDouble();
            if (responseIsError(r))
                return r;
            count += 1;
            resultList.append(r.value("result").toObject());
        }
        lock.commit();
    }

    QJsonObject resultmap, errormap;
    resultmap.insert( JsonDbString::kDataStr, resultList );
    resultmap.insert( JsonDbString::kCountStr, count );
    if (stateNumber > 0)
        resultmap.insert( JsonDbString::kStateNumberStr, (int)stateNumber);
    return makeResponse( resultmap, errormap );
}

/*!
  This function takes a QJsonArray of objects each with a valid
  "uuid" field.  It updates the items in the database.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

  This class should be optimized by a JsonDb subclass.
*/

QJsonObject JsonDb::updateList(const JsonDbOwner *owner, JsonDbObjectList& list, const QString &partitionName, WriteMode writeMode)
{
    JsonDbPartition *partition = findPartition(partitionName);
    if (!partition) {
        QJsonObject resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partitionName));
        return makeResponse(resultmap, errormap);
    }

    int count = 0;
    QJsonArray resultList;
    quint32 stateNumber = 0;

    int size = list.size();
    int transactionSize = jsondbSettings->transactionSize() ? jsondbSettings->transactionSize() : size;
    for (int k = 0; k < size; k += transactionSize) {
        WithTransaction transaction(partition);
        CHECK_LOCK_RETURN(transaction, "updateList");
        for (int i = k; i < qMin(k+transactionSize, size); ++i) {
            JsonDbObject o = list.at(i);
            QJsonObject r = update(owner, o, partitionName, writeMode);
            stateNumber = r.value(JsonDbString::kStateNumberStr).toDouble();
            if (responseIsError(r))
                return r;
            count += r.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kCountStr).toDouble();
            resultList.append(r.value("result").toObject());
        }
        transaction.commit();
    }
    QJsonObject resultmap, errormap;
    resultmap.insert( JsonDbString::kDataStr, resultList );
    resultmap.insert( JsonDbString::kCountStr, count );
    if (stateNumber > 0)
        resultmap.insert( JsonDbString::kStateNumberStr, (int)stateNumber );
    return makeResponse( resultmap, errormap );
}

/*!
  This function takes a QJsonArray of objects each with a valid
  "uuid" field.  It removes the items from the database.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

  This class should be optimized by a JsonDb subclass.
*/

QJsonObject JsonDb::removeList(const JsonDbOwner *owner, JsonDbObjectList list, const QString &partitionName, WriteMode writeMode)
{
    JsonDbPartition *partition = findPartition(partitionName);
    if (!partition) {
        QJsonObject resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partitionName));
        return makeResponse(resultmap, errormap);
    }

    int count = 0;
    quint32 stateNumber = 0;
    QJsonArray removedList, errorsList;

    int size = list.size();
    int transactionSize = jsondbSettings->transactionSize() ? jsondbSettings->transactionSize() : size;
    for (int k = 0; k < size; k += transactionSize) {
        WithTransaction transaction(partition);
        CHECK_LOCK_RETURN(transaction, "removeList");
        for (int i = k; i < qMin(k+transactionSize, size); ++i) {
            JsonDbObject o = list.at(i);
            QString uuid = o.value(JsonDbString::kUuidStr).toString();
            QJsonObject r = remove(owner, o, partitionName, writeMode);
            stateNumber = r.value(JsonDbString::kStateNumberStr).toDouble();
            QJsonObject error = r.value(JsonDbString::kErrorStr).toObject();
            if (!error.isEmpty()) {
                QJsonObject obj;
                obj.insert(JsonDbString::kUuidStr, uuid);
                obj.insert(JsonDbString::kErrorStr, error);
                errorsList.append(obj);
                continue;
            }
            count += r.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kCountStr).toDouble();
            QJsonObject item;
            item.insert(JsonDbString::kUuidStr, uuid);
            removedList.append(item);
        }
        transaction.commit();
    }

    QJsonObject resultmap;
    resultmap.insert(JsonDbString::kCountStr, count);
    resultmap.insert(JsonDbString::kDataStr, removedList);
    resultmap.insert(JsonDbString::kErrorStr, errorsList);
    if (stateNumber > 0)
        resultmap.insert(JsonDbString::kStateNumberStr, (int)stateNumber);
    QJsonObject errormap;
    return makeResponse(resultmap, errormap);
}

QJsonObject JsonDb::changesSince(const JsonDbOwner * /* owner */, QJsonObject object, const QString &partitionName)
{
    JsonDbPartition *partition = findPartition(partitionName);
    if (!partition) {
        QJsonObject resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partitionName));
        return makeResponse(resultmap, errormap);
    }

    int stateNumber = object.value(JsonDbString::kStateNumberStr).toDouble();
    if (object.contains(JsonDbString::kTypesStr)) {
        QSet<QString> limitTypes;
        QJsonArray l = object.value(JsonDbString::kTypesStr).toArray();
        for (int i = 0; i < l.size(); i++)
            limitTypes.insert(l.at(i).toString());
        return partition->changesSince(stateNumber, limitTypes);
    } else {
        return partition->changesSince(stateNumber);
    }
}

QJsonValue JsonDb::propertyLookup(QJsonObject v, const QString &path)
{
    return propertyLookup(v, path.split('.'));
}

QJsonValue JsonDb::propertyLookup(const JsonDbObject &document, const QString &path)
{
    return propertyLookup(document, path.split('.'));
}

QJsonValue JsonDb::propertyLookup(QJsonObject object, const QStringList &path)
{
    if (!path.size()) {
        qCritical() << "JsonDb::propertyLookup empty path";
        abort();
        return QJsonValue(QJsonValue::Undefined);
    }
    // TODO: one malloc here
    QJsonValue value(object);
    for (int i = 0; i < path.size(); i++) {
        const QString &key = path.at(i);
        // this part of the property is a list
        if (value.isArray()) {
            QJsonArray objectList = value.toArray();
            bool ok = false;
            int index = key.toInt(&ok);
            if (ok && (index >= 0) && (objectList.size() > index))
                value = objectList.at(index);
            else
                value = QJsonValue(QJsonValue::Undefined);
        } else if (value.isObject()) {
            QJsonObject o = value.toObject();
            if (o.contains(key))
                value = o.value(key);
            else
                value = QJsonValue(QJsonValue::Undefined);
        } else {
            value = QJsonValue(QJsonValue::Undefined);
        }
    }
    return value;
}

QJsonValue JsonDb::fromJSValue(const QJSValue &v)
{
    if (v.isNull())
        return QJsonValue(QJsonValue::Null);
    if (v.isNumber())
        return QJsonValue(v.toNumber());
    if (v.isString())
        return QJsonValue(v.toString());
    if (v.isBool())
        return QJsonValue(v.toBool());
    if (v.isArray()) {
        QJsonArray a;
        int size = v.property("length").toInt();
        for (int i = 0; i < size; i++) {
            a.append(fromJSValue(v.property(i)));
        }
        return a;
    }
    if (v.isObject()) {
        QJSValueIterator it(v);
        QJsonObject o;
        while (it.hasNext()) {
            it.next();
            QString name = it.name();
            QJSValue value = it.value();
            o.insert(name, fromJSValue(value));
        }
        return o;
    }
    return QJsonValue(QJsonValue::Undefined);
}

JsonDb::JsonDb(const QString &filePath, const QString &baseName, const QString &username, QObject *parent)
    : QObject(parent)
    , mOwner(0)
    , mOpen(false)
    , mFilePath(filePath)
    , mCompactOnClose(false)
{
    mSystemPartitionName = baseName+QStringLiteral(".System");
    mEphemeralPartitionName = QStringLiteral("Ephemeral");
    if (mFilePath.isEmpty()) mFilePath = QStringLiteral(".");
    if (mFilePath.at(mFilePath.size()-1) != QLatin1Char('/'))
        mFilePath += QLatin1Char('/');
    qDebug () << mFilePath << mSystemPartitionName << mEphemeralPartitionName;

    mOwner = new JsonDbOwner(this);
    mOwner->setOwnerId(username);
}

JsonDb::~JsonDb()
{
    close();
}

bool JsonDb::open()
{
    // init ephemeral partition
    mEphemeralPartition = new JsonDbEphemeralPartition(this);

    // open system partition
    QString systemFileName = mFilePath + mSystemPartitionName + QLatin1String(".db");
    JsonDbPartition *partition = new JsonDbPartition(systemFileName, mSystemPartitionName, this);
    if (!partition->open()) {
        qDebug() << "Cannot open system partition at" << systemFileName;
        return false;
    }
    mPartitions.insert(mSystemPartitionName, partition);

    // read partition information from the db
    QJsonObject result;
    JsonDbObjectList partitions = partition->getObjects(JsonDbString::kTypeStr, JsonDbString::kPartitionTypeStr).data;
    if (partitions.isEmpty()) {
        WithTransaction transaction(partition);
        JsonDbObjectTable *objectTable = partition->mainObjectTable();
        transaction.addObjectTable(objectTable);

        // make a system partition
        JsonDbObject partitionDefinition;
        partitionDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kPartitionTypeStr);
        partitionDefinition.insert(QLatin1String("name"), mSystemPartitionName);
        result = partition->createPersistentObject(partitionDefinition);
        if (responseIsError(result)) {
            qCritical() << "Cannot create a system partition";
            return false;
        }
    }

    for (int i = 0; i < partitions.size(); ++i) {
        JsonDbObject part = partitions.at(i);
        QString filename = mFilePath + part.value(QLatin1String("file")).toString();
        QString name = part.value(QLatin1String("name")).toString();

        if (name == mSystemPartitionName)
            continue;

        if (mPartitions.contains(name)) {
            qWarning() << "Duplicate partition found, ignoring" << name << "at" << filename;
            continue;
        }

        JsonDbPartition *partition = new JsonDbPartition(filename, name, this);
        if (jsondbSettings->verbose())
            qDebug() << "Opening partition" << name;

        if (!partition->open()) {
            qWarning() << "Failed to initialize partition" << name << "at" << filename;
            continue;
        }
        mPartitions.insert(name, partition);
    }

    mViewTypes.clear();
    mEagerViewSourceTypes.clear();

    initSchemas();

    foreach (JsonDbPartition *partition, mPartitions)
        JsonDbView::initViews(partition, partition->name());

    mOpen = true;
    return true;
}
bool JsonDb::clear()
{
    bool fail = false;
    foreach (JsonDbPartition *partition, mPartitions)
        fail = !partition->clear() || fail;
    return !fail;
}

/*!
  This function frees all unnecessary memory, for example flushing the btree caches.
 */
void JsonDb::reduceMemoryUsage()
{
    for (QHash<QString, JsonDbPartition *>::iterator it = mPartitions.begin();
         it != mPartitions.end(); ++it) {
        JsonDbPartition *partition = it.value();
        partition->flushCaches();
    }
}

JsonDbStat JsonDb::stat() const
{
    JsonDbStat result;
    foreach (JsonDbPartition *partition, mPartitions)
        result += partition->stat();
    return result;
}

void JsonDb::close()
{
    if (mOpen) {
        foreach (JsonDbPartition *partition, mPartitions) {
            JsonDbStat stat = partition->stat();
            qDebug() << "Partition"
                     << "read" << stat.reads << "hits" << stat.hits << "writes" << stat.writes
                     << partition->name();
            if (mCompactOnClose)
                partition->compact();
            partition->close();
        }
    }
    mOpen = false;
}

const JsonDbOwner *JsonDb::findOwner(const QString &ownerId) const
{
    const JsonDbOwner *owner = mOwners.value(ownerId);
    if (owner)
        return owner;
    else
        // security hole
        return mOwner;
}

JsonDbQueryResult JsonDb::find(const JsonDbOwner *owner, QJsonObject obj, const QString &partitionName)
{
//    QElapsedTimer time;
//    QJsonObject times;
//    time.start();
    QJsonObject resultmap, errormap;
    JsonDbQueryResult result;

    QString query = obj.value(JsonDbString::kQueryStr).toString();
    QJsonObject bindings = obj.value("bindings").toObject();

    int limit = obj.contains(JsonDbString::kLimitStr) ? obj.value(JsonDbString::kLimitStr).toDouble() : -1;
    int offset = obj.value(JsonDbString::kOffsetStr).toDouble();

    if ( limit < -1 )
        setError( errormap, JsonDbError::InvalidLimit, "Invalid limit" );
    else if ( offset < 0 )
        setError( errormap, JsonDbError::InvalidOffset, "Invalid offset" );
    else if ( query.isEmpty() )
        setError( errormap, JsonDbError::MissingQuery, "Missing query string");
    else {
//        times.insert("time0 before parse", time.elapsed());
        QScopedPointer<JsonDbQuery> parsedQuery(JsonDbQuery::parse(query, bindings));
//        times.insert("time1 after parse", time.elapsed());

        if (!parsedQuery->queryTerms.size() && !parsedQuery->orderTerms.size()) {
            return JsonDbQueryResult::makeErrorResponse(JsonDbError::MissingQuery, QString("Missing query: ") + parsedQuery->queryExplanation.join("\n"));
        }

        if (partitionName == mEphemeralPartitionName)
            return mEphemeralPartition->query(parsedQuery.data(), limit, offset);

        JsonDbPartition *partition = findPartition(partitionName);
        if (!partition) {
            return JsonDbQueryResult::makeErrorResponse(JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partitionName));
        }

        QStringList explanation = parsedQuery->queryExplanation;
        QJsonValue::Type resultType = parsedQuery->resultType;
        QStringList mapExpressions = parsedQuery->mapExpressionList;
        QStringList mapKeys = parsedQuery->mapKeyList;

        JsonDbQueryResult queryResult = partition->queryPersistentObjects(owner, parsedQuery.data(), limit, offset);
        JsonDbObjectList results = queryResult.data;
        if (jsondbSettings->debug()) {
            const QList<OrQueryTerm> &orQueryTerms = parsedQuery->queryTerms;
            for (int i = 0; i < orQueryTerms.size(); i++) {
                const OrQueryTerm &orQueryTerm = orQueryTerms[i];
                foreach (const QueryTerm &queryTerm, orQueryTerm.terms()) {
                    if (jsondbSettings->verbose())
                        qDebug() << __FILE__ << __LINE__
                                 << (QString("    %1%4%5 %2 %3    ")
                                     .arg(queryTerm.propertyName())
                                     .arg(queryTerm.op())
                                     .arg(JsonWriter().toString(queryTerm.value().toVariant()))
                                     .arg(queryTerm.joinField().size() ? "->" : "").arg(queryTerm.joinField()));
                }
            }
            QList<OrderTerm> &orderTerms = parsedQuery->orderTerms;
            for (int i = 0; i < orderTerms.size(); i++) {
                const OrderTerm &orderTerm = orderTerms[i];
                if (jsondbSettings->verbose())
                    qDebug() << __FILE__ << __LINE__ << QString("    %1 %2    ").arg(orderTerm.propertyName).arg(orderTerm.ascending ? "ascending" : "descending");
            }
        }
        if (jsondbSettings->debug())
            qDebug() << endl
#if 0
              << "  orderTerms: " << serializer.serialize(orderTerms) << endl
              << "  queryTerms: " << serializer.serialize(queryTerms) << endl
#endif
              << "  limit:      " << limit << endl
              << "  offset:     " << offset << endl
              << "  results:    " << results;

        int length = results.size();

        if (mapExpressions.length() && parsedQuery->mAggregateOperation.compare("count")) {
            QMap<QString, JsonDbObject> objectCache;
            int nExpressions = mapExpressions.length();
            QVector<QVector<QStringList> > joinPaths(nExpressions);
            for (int i = 0; i < nExpressions; i++) {
                QString propertyName = mapExpressions[i];
                QStringList joinPath = propertyName.split("->");
                int joinPathSize = joinPath.size();
                QVector<QStringList> fieldPaths(joinPathSize);
                for (int j = 0; j < joinPathSize; j++) {
                    QString joinField = joinPath[j];
                    fieldPaths[j] = joinField.split('.');
                }
                joinPaths[i] = fieldPaths;
            }

            QList<JsonDbObject> mappedResult;
            QJsonArray valueArray;
            for (int r = 0; r < results.size(); r++) {
                const JsonDbObject obj = results.at(r);
                QJsonArray list;
                QJsonObject map;
                for (int i = 0; i < nExpressions; i++) {
                    QJsonValue v;

                    QVector<QStringList> &joinPath = joinPaths[i];
                    int joinPathSize = joinPath.size();
                    if (joinPathSize == 1) {
                        v = propertyLookup(obj, joinPath[0]);
                        QJsonObject vObj;
                        vObj.insert("v", v);
                    } else {
                        JsonDbObject baseObject(obj);
                        for (int j = 0; j < joinPathSize-1; j++) {
                            QJsonValue uuidQJsonValue = propertyLookup(baseObject, joinPath[j]).toString();
                            QString uuid = uuidQJsonValue.toString();
                            if (uuid.isEmpty()) {
                                baseObject = JsonDbObject();
                            } else if (objectCache.contains(uuid)) {
                                baseObject = objectCache.value(uuid);
                            } else {
                                ObjectKey objectKey(uuid);
                                bool gotBaseObject = partition->getObject(objectKey, baseObject);
                                if (gotBaseObject)
                                    objectCache.insert(uuid, baseObject);
                            }
                        }
                        v = propertyLookup(baseObject, joinPath[joinPathSize-1]);
                    }

                    if (resultType == QJsonValue::Object)
                        map.insert(mapKeys[i], v);
                    else if (resultType == QJsonValue::Array)
                        list.append(v);
                    else
                        valueArray.append(v);
                }
                if (resultType == QJsonValue::Object) {
                    mappedResult.append(map);
                } else if (resultType == QJsonValue::Array) {
                    valueArray.append(list);
                }
            }

            result.data = mappedResult;
            result.values = valueArray;
        } else {
            result.data = results;
        }

        result.length = length;
        result.offset = offset;
        result.explanation = QJsonValue::fromVariant(explanation);
        result.sortKeys = queryResult.sortKeys;
        result.state = queryResult.state;
    }

    return result;
}

/*!
  This function takes a single QJsonValue,
  adds a "_uuid" field to it with a unique UUID (overwriting a
  previous "_uuid" field if necessary), and stores it in the object
  database.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "_uuid", STRING } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/

QJsonObject JsonDb::create(const JsonDbOwner *owner, JsonDbObject& object, const QString &partitionName, WriteMode writeMode)
{
    QJsonObject resultmap, errormap;
    if (object.contains(JsonDbString::kUuidStr)) {
        setError(errormap, JsonDbError::InvalidRequest, "New object should not have _uuid");
        return makeResponse(resultmap, errormap);
    }

    populateIdBySchema(owner, object, partitionName);
    object.generateUuid();

    return update(owner, object, partitionName, writeMode);
}

QJsonObject JsonDb::createViewObject(const JsonDbOwner *owner, JsonDbObject& object, const QString &partitionName)
{
    return create(owner, object, partitionName, ViewObject);
}


/*!
  This function takes a single QJsonValue with a valid "_uuid" field.
  It updates the database to match the new object.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "count" : NUMBER } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/

QJsonObject JsonDb::update(const JsonDbOwner *owner, JsonDbObject& object, const QString &partitionName, WriteMode writeMode)
{
    if (writeMode == DefaultWrite)
        writeMode = jsondbSettings->rejectStaleUpdates() ? OptimisticWrite : ForcedWrite;

    QJsonObject resultmap, errormap;
    QString uuid, objectType;
    bool forRemoval = object.contains(JsonDbString::kDeletedStr) ? object.value(JsonDbString::kDeletedStr).toBool() : false;

    RETURN_IF_ERROR(errormap, checkUuidPresent(object, uuid));
    if (!forRemoval) {
        RETURN_IF_ERROR(errormap, checkTypePresent(object, objectType));
    } else {
        objectType = object.type();
        if (objectType.isEmpty()) {
            JsonDbObject testEphemeral;
            if (mEphemeralPartition->get(uuid, &testEphemeral)) {
                objectType = testEphemeral.type();
                writeMode = EphemeralObject;
            }
        }
    }

    QString partitionId = partitionName.isEmpty() ? mSystemPartitionName : partitionName;
    bool isNotification = objectType == JsonDbString::kNotificationTypeStr;

    if (writeMode != EphemeralObject && (isNotification || partitionId == mEphemeralPartitionName))
        writeMode = EphemeralObject;
    if (writeMode == EphemeralObject)
        partitionId = mEphemeralPartitionName;

    if (writeMode != ViewObject)
        RETURN_IF_ERROR(errormap, checkNaturalObjectType(object, objectType));
    if (!forRemoval)
        RETURN_IF_ERROR(errormap, validateSchema(objectType, object));

    RETURN_IF_ERROR(errormap, checkPartitionPresent(partitionId));

    // don't repopulate if deleting object, if it's a schema type then _id is altered.
    if (!forRemoval) {
        if (populateIdBySchema(owner, object, partitionId)) {
            object.generateUuid();

            if (object.value(JsonDbString::kUuidStr).toString() != uuid) {
                setError(errormap, JsonDbError::InvalidRequest, "_uuid mismatch, use create()");
                return makeResponse(resultmap, errormap);
            }
        }
    }

    JsonDbPartition *partition = 0;
    WithTransaction transaction;
    JsonDbObjectTable *objectTable = 0;

    JsonDbObject master;
    bool forCreation;

    if (writeMode == EphemeralObject) {
        forCreation = !mEphemeralPartition->get(object.uuid(), &master);

        // ensure this objects belongs to this connection so that other clients
        // cannot update / remove someone else's notifications
        if (!forCreation && isNotification && owner->ownerId() != master.value(JsonDbString::kOwnerStr).toString()) {
            setError(errormap, JsonDbError::MismatchedNotifyId, "Cannot touch notification objects that doesn't belong to you");
            return makeResponse(resultmap, errormap);
        }
    } else {
        partition = findPartition(partitionId);
        if (!partition) {
            setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partitionId));
            return makeResponse(resultmap, errormap);
        }
        objectTable = partition->findObjectTable(objectType);
        if (writeMode != ViewObject) {
            transaction.setPartition(partition);
            transaction.addObjectTable(objectTable);
        }

        forCreation = !partition->getObject(uuid, master, objectType);
    }

    if (writeMode != ReplicatedWrite && forCreation && forRemoval) {
        setError(errormap, JsonDbError::MissingObject, QLatin1String("Cannot remove non-existing object"));
        return makeResponse(resultmap, errormap);
    }

    if (!forCreation)
        RETURN_IF_ERROR(errormap, checkAccessControl(owner, master, partitionId, "write"));

    if (!master.value(JsonDbString::kOwnerStr).toString().isEmpty())
        object.insert(JsonDbString::kOwnerStr, master.value(JsonDbString::kOwnerStr));
    else if (object.value(JsonDbString::kOwnerStr).toString().isEmpty())
        object.insert(JsonDbString::kOwnerStr, owner->ownerId());

    if (!forRemoval)
        RETURN_IF_ERROR(errormap, checkAccessControl(owner, object, partitionId, "write"));

    bool validWrite = false;
    QString versionWritten;

    JsonDbObject oldMaster = master;

    // if the old object is a schema, make sure it's ok to remove it
    if (oldMaster.type() == JsonDbString::kSchemaTypeStr)
        RETURN_IF_ERROR(errormap, checkCanRemoveSchema(oldMaster));

    switch (writeMode) {
    case OptimisticWrite:
        validWrite = master.updateVersionOptimistic(object, &versionWritten);
        break;
    case ViewObject:
    case ForcedWrite:
    case EphemeralObject:
        master = object;
        versionWritten = master.computeVersion();
        validWrite = true;
        break;
    case ReplicatedWrite:
        validWrite = master.updateVersionReplicating(object);
        versionWritten = master.version();
        break;
    default:
        setError(errormap, JsonDbError::InvalidRequest, "Missing writeMode implementation.");
        return makeResponse(resultmap, errormap);
    }

    if (!validWrite) {
        if (writeMode == ReplicatedWrite) {
            setError(errormap, JsonDbError::InvalidRequest, "Replication has reject your update for sanity reasons");
        } else {
            if (jsondbSettings->debug())
                qDebug() << "Stale update detected - expected version:" << oldMaster.version() << object;
            setError( errormap, JsonDbError::UpdatingStaleVersion, "Updating stale version of object. Expected version " + oldMaster.version() + ", is " + versionWritten);
        }
        return makeResponse(resultmap, errormap);
    }

    // recheck, it might just be a conflict removal
    forRemoval = master.isDeleted();
    objectType = master.type();
    isNotification = objectType == JsonDbString::kNotificationTypeStr;

    if (objectType.isEmpty()) {
        // we skipped the objectType check for removals, let's add it back in
        objectType = oldMaster.type();
        master.insert(JsonDbString::kTypeStr, objectType);
    }

    bool isVisibleWrite = oldMaster.version() != master.version();

    if (objectType == "Partition") {
        if (!forCreation) {
            setError(errormap, JsonDbError::InvalidPartition, "Updates to partition objects not supported yet!");
            return makeResponse(resultmap, errormap);
        }
        return createPartition(master);
    }

    // TODO: ephemeral objects?
    if (isVisibleWrite && !forRemoval) {
        // validate the new object
        if (objectType == JsonDbString::kSchemaTypeStr)
            RETURN_IF_ERROR(errormap, checkCanAddSchema(master, oldMaster));
        else if (objectType == JsonDbString::kMapTypeStr)
            RETURN_IF_ERROR(errormap, validateMapObject(master));
        else if (objectType == JsonDbString::kReduceTypeStr)
            RETURN_IF_ERROR(errormap, validateReduceObject(master));
        else if (!forRemoval && objectType == kIndexTypeStr)
            RETURN_IF_ERROR(errormap, validateAddIndex(master, oldMaster));
    }

    QJsonObject response;
    if (writeMode != EphemeralObject) {
        int dataSize = 0;

        if (forCreation) {
            dataSize = master.toBinaryData().size();
            RETURN_IF_ERROR(errormap, checkQuota(owner, dataSize, partition));
            response = partition->createPersistentObject(master);
        } else if (forRemoval) {
            dataSize = -oldMaster.toBinaryData().size();
            response = partition->removePersistentObject(oldMaster, master);
        } else {
            dataSize = master.toBinaryData().size() - oldMaster.toBinaryData().size();
            RETURN_IF_ERROR(errormap, checkQuota(owner, dataSize, partition));
            response = partition->updatePersistentObject(oldMaster, master);
        }

        if (responseIsError(response))
            return response;

        partition->addToQuota(owner, dataSize);

    } else {
        if (forRemoval) {
            response = mEphemeralPartition->remove(master);
            if (isNotification)
                removeNotification(uuid);
        } else if (forCreation) {
            response = mEphemeralPartition->create(master);
            if (isNotification)
                createNotification(owner, master);
        } else {
            response = mEphemeralPartition->update(master);
        }
    }

    // handle schemata and indices
    if (isVisibleWrite && writeMode != EphemeralObject) {
        QString oldType = oldMaster.type();
        if (oldType == JsonDbString::kSchemaTypeStr)
            removeSchema(oldMaster.value("name").toString());
        else if (oldType == kIndexTypeStr)
            removeIndex(oldMaster, partitionId);

        if (!forRemoval) {
            if (objectType == JsonDbString::kSchemaTypeStr)
                setSchema(master.value("name").toString(), object.value("schema").toObject());
            else if (objectType == kIndexTypeStr)
                addIndex(master, partitionId);
        }
    }

    if (!isVisibleWrite) {
        // make sure, the actual version is returned, not the head version
        QJsonObject result = response.value("result").toObject();
        result.insert(JsonDbString::kVersionStr, versionWritten);
        response.insert("result", result);
    }

    if (forCreation)
        checkNotifications(partitionId, master, JsonDbNotification::Create);
    else if (forRemoval)
        checkNotifications(partitionId, master, JsonDbNotification::Delete);
    else
        checkNotifications(partitionId, master, JsonDbNotification::Update);

    // method used to be non-const, so writing back
    // TODO: fixme
    object.insert(JsonDbString::kVersionStr, versionWritten);

    return response;
}

/*!
  This function takes a single QJsonValue with a valid "_uuid" field.
  It updates the database to match the new object.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "count" : NUMBER } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/
QJsonObject JsonDb::updateViewObject(const JsonDbOwner *owner, JsonDbObject &object, const QString &partitionName)
{
    return update(owner, object, partitionName, ViewObject);
}


/*!
  This function takes a single QJsonValue with valid "_uuid" and "_version" fields.
  It removes the item's verstion from the database by replacing it with a tombstone.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/

QJsonObject JsonDb::remove(const JsonDbOwner *owner, const JsonDbObject &object, const QString &partitionName, WriteMode writeMode)
{
    JsonDbObject tombstone;
    tombstone.insert(JsonDbString::kUuidStr, object.uuid().toString());
    tombstone.insert(JsonDbString::kVersionStr, object.version());
    tombstone.insert(JsonDbString::kTypeStr, object.type());
    tombstone.insert(JsonDbString::kDeletedStr, true);
    tombstone.insert(JsonDbString::kOwnerStr, object.value(JsonDbString::kOwnerStr));

    return update(owner, tombstone, partitionName, writeMode);
}

/*!
  This function takes a single QJsonValue with a valid "_uuid" field.
  It removes the item from the database.

  On success it returns a QJsonValue of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QJsonValue of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/

QJsonObject JsonDb::removeViewObject(const JsonDbOwner *owner, JsonDbObject object, const QString &partitionName)
{
    return remove(owner, object, partitionName, ViewObject);
}

GetObjectsResult JsonDb::getObjects(const QString &keyName, const QJsonValue &key, const QString &type, const QString &partitionName) const
{
    const QString &objectType = (keyName == JsonDbString::kTypeStr) ? key.toString() : type;
    if (JsonDbPartition *partition = findPartition(partitionName))
        return partition->getObjects(keyName, key, objectType);
    return GetObjectsResult();
}

void JsonDb::checkNotifications(const QString &partitionName, JsonDbObject object, JsonDbNotification::Action action)
{
    //DBG() << object;

    QStringList notificationKeys;
    if (object.contains(JsonDbString::kTypeStr)) {
        QString objectType = object.value(JsonDbString::kTypeStr).toString();
        notificationKeys << objectType;
        if (mEagerViewSourceTypes.contains(objectType)) {
            const QSet<QString> &targetTypes = mEagerViewSourceTypes[objectType];
            for (QSet<QString>::const_iterator it = targetTypes.begin(); it != targetTypes.end(); ++it)
                emit requestViewUpdate(*it, mSystemPartitionName);
        }
    }
    if (object.contains(JsonDbString::kUuidStr))
        notificationKeys << object.value(JsonDbString::kUuidStr).toString();
    notificationKeys << "__generic_notification__";

    QHash<QString, JsonDbObject> objectCache;
    for (int i = 0; i < notificationKeys.size(); i++) {
        QString key = notificationKeys[i];
        QMultiMap<QString, JsonDbNotification *>::const_iterator it = mKeyedNotifications.find(key);
        while ((it != mKeyedNotifications.end()) && (it.key() == key)) {
            JsonDbNotification *n = it.value();
            if (jsondbSettings->debug())
                qDebug() << "Notification" << n->query() << n->actions();
            if (n->partition() == partitionName && n->actions() & action) {
                JsonDbObject r;
                if (!n->query().isEmpty()) {
                    if (jsondbSettings->debug())
                        qDebug() << "Checking notification" << n->query() << endl
                                 << "    for object" << object;
                    JsonDbQuery *query  = n->parsedQuery();
                    if (query->match(object, &objectCache, 0))
                        r = object;
                    if (jsondbSettings->debug())
                        qDebug() << "Got result" << r;
                } else {
                    r = object;
                }
                if (!r.isEmpty()) {
                    QString actionStr = (action == JsonDbNotification::Create ? JsonDbString::kCreateStr :
                                         (action == JsonDbNotification::Update ? JsonDbString::kUpdateStr : JsonDbString::kRemoveStr));
                    emit notified(n->uuid(), r, actionStr);
                }
            }

            ++it;
        }
    }
}

bool JsonDb::removeIndex(const QString &indexName, const QString &objectType, const QString &partitionName)
{
    if (JsonDbPartition *partition = findPartition(partitionName))
        return partition->removeIndex(indexName, objectType);
    return false;
}

bool JsonDb::addIndex(JsonDbObject indexObject, const QString &partitionName)
{
    // For propertyFunction, the indexName has to be set by user. Else it can
    // default to propertyType.
    QString indexName = indexObject.value(kNameStr).toString();
    QString propertyName = indexObject.value(kPropertyNameStr).toString();
    QString propertyType = indexObject.value(kPropertyTypeStr).toString();
    QString objectType = indexObject.value(kObjectTypeStr).toString();
    QString propertyFunction = indexObject.value(kPropertyFunctionStr).toString();

    Q_ASSERT(propertyName.isEmpty() ^ propertyFunction.isEmpty());
    Q_ASSERT(!propertyFunction.isEmpty() ? !indexName.isEmpty() : true);

    if (indexName.isEmpty())
        indexName = propertyName;

    if (JsonDbPartition *partition = findPartition(partitionName))
        return partition->addIndex(indexName, propertyName, propertyType, objectType, propertyFunction);
    qWarning() << "addIndex" << "did not find partition" << partitionName;
    return false;
}

bool JsonDb::removeIndex(JsonDbObject indexObject, const QString &partitionName)
{
    QString indexName = indexObject.value(kNameStr).toString();
    QString propertyName = indexObject.value(kPropertyNameStr).toString();
    QString objectType = indexObject.value(kObjectTypeStr).toString();

    if (indexName.isEmpty())
        indexName = propertyName;

    Q_ASSERT(!indexName.isEmpty());

    return removeIndex(indexName, objectType, partitionName);
}

void JsonDb::updateSchemaIndexes(const QString &schemaName, QJsonObject object, const QStringList &path)
{
    QJsonObject properties = object.value("properties").toObject();
    const QList<QString> keys = properties.keys();
    for (int i = 0; i < keys.size(); i++) {
        const QString &k = keys[i];
        QJsonObject propertyInfo = properties.value(k).toObject();
        if (propertyInfo.contains("indexed")) {
            QString propertyType = (propertyInfo.contains("type") ? propertyInfo.value("type").toString() : "string");
            QStringList kpath = path;
            kpath << k;
            JsonDbPartition *partition = findPartition(mSystemPartitionName);
            partition->addIndexOnProperty(kpath.join("."), propertyType, schemaName);
        }
        if (propertyInfo.contains("properties")) {
            updateSchemaIndexes(schemaName, propertyInfo, path + (QStringList() << k));
        }
    }
}

bool JsonDb::populateIdBySchema(const JsonDbOwner *owner, JsonDbObject &object,
                                const QString &partitionName)
{
    // minimize inserts
    if (!object.contains(JsonDbString::kOwnerStr)
        || ((object.value(JsonDbString::kOwnerStr).toString() != owner->ownerId())
            && !owner->isAllowed(object, partitionName, "setOwner")))
        object.insert(JsonDbString::kOwnerStr, owner->ownerId());

    QString objectType = object.value(JsonDbString::kTypeStr).toString();
    if (mSchemas.contains(objectType)) {
        QJsonObject schema = mSchemas.value(objectType);
        if (schema.contains("primaryKey")) {
            QJsonValue::Type pkType = schema.value("primaryKey").type();
            QString _id = QString("primaryKey::%1:").arg(objectType);

            if (pkType == QJsonValue::String) {
                QString keyName = schema.value("primaryKey").toString();
                _id.append(keyName).append('=').append(object.value(keyName).toString());
            } else if (pkType == QJsonValue::Array) {
                QJsonArray keyList = schema.value("primaryKey").toArray();
                for (int i = 0; i < keyList.size(); i++) {
                    QString keyName = keyList.at(i).toString();
                    _id.append(":").append(keyName).append('=').append(object.value(keyName).toString());
                }
            }
            // minimize inserts
            if (object.value("_id").toString() != _id)
                object.insert("_id", _id);

            return true;
        }
    }
    return false;
}

void JsonDb::initSchemas()
{
    if (jsondbSettings->verbose())
        qDebug() << "initSchemas";
    {
        JsonDbObjectList schemas = getObjects(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr,
                                                QString(), mSystemPartitionName).data;
        for (int i = 0; i < schemas.size(); ++i) {
            JsonDbObject schemaObject = schemas.at(i);
            QString schemaName = schemaObject.value("name").toString();
            QJsonObject schema = schemaObject.value("schema").toObject();
            setSchema(schemaName, schema);
        }
    }
    foreach (const QString &schemaName, (QStringList() << JsonDbString::kNotificationTypeStr << JsonDbString::kViewTypeStr
                                         << "Capability" << kIndexTypeStr)) {
        if (!mSchemas.contains(schemaName)) {
            QFile schemaFile(QString(":schema/%1.json").arg(schemaName));
            schemaFile.open(QIODevice::ReadOnly);
            JsonReader parser;
            bool ok = parser.parse(schemaFile.readAll());
            if (!ok) {
                qWarning() << "Parsing " << schemaName << " schema" << parser.errorString();
                return;
            }
            QJsonObject schema = QJsonObject::fromVariantMap(parser.result().toMap());
            JsonDbObject schemaObject;
            schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
            schemaObject.insert("name", schemaName);
            schemaObject.insert("schema", schema);
            create(mOwner, schemaObject);
        }
    }
    {
        JsonDbObject nameIndex;
        nameIndex.insert(JsonDbString::kTypeStr, kIndexTypeStr);
        nameIndex.insert(kPropertyNameStr, QLatin1String("name"));
        nameIndex.insert(kPropertyTypeStr, QLatin1String("string"));
        nameIndex.insert(kObjectTypeStr, QLatin1String("Capability"));
        create(mOwner, nameIndex);

        const QString capabilityName("RootCapability");
        QFile capabilityFile(QString(":schema/%1.json").arg(capabilityName));
        capabilityFile.open(QIODevice::ReadOnly);
        JsonReader parser;
        bool ok = parser.parse(capabilityFile.readAll());
        if (!ok) {
            qWarning() << "Parsing " << capabilityName << " capability" << parser.errorString();
            return;
        }
        JsonDbObject capability = QJsonObject::fromVariantMap(parser.result().toMap());
        QString name = capability.value("name").toString();
        GetObjectsResult getObjectResponse = getObjects("name", name, "Capability");
        int count = getObjectResponse.data.size();
        if (!count) {
            if (jsondbSettings->verbose())
                qDebug() << "Creating capability" << capability;
            create(mOwner, capability);
        } else {
            JsonDbObject currentCapability = getObjectResponse.data.at(0);
            if (currentCapability.value("accessRules") != capability.value("accessRules")) {
                currentCapability.insert("accessRules", capability.value("accessRules"));
                update(mOwner, currentCapability);
            }
        }
    }
}

void JsonDb::setSchema(const QString &schemaName, QJsonObject schema)
{
    if (jsondbSettings->verbose())
        qDebug() << "setSchema" << schemaName << schema;
    QJsonObject errors = mSchemas.insert(schemaName, schema);

    if (!errors.isEmpty()) {
        //if (gVerbose) {
            qDebug() << "setSchema failed because of errors" << schemaName << schema;
            qDebug() << errors;
        //}
        // FIXME should we accept broken schemas?
        // return;
    }
    if (schema.contains("extends")) {
        QJsonValue extendsValue = schema.value("extends");
        QString extendedSchemaName;
        if (extendsValue.type() == QJsonValue::String)
            extendedSchemaName = extendsValue.toString();
        else if ((extendsValue.type() == QJsonValue::Object)
                 && extendsValue.toObject().contains("$ref"))
            extendedSchemaName = extendsValue.toObject().value("$ref").toString();
        if (extendedSchemaName == JsonDbString::kViewTypeStr) {
            mViewTypes.insert(schemaName);
            //TODO fix call to findPartition
            JsonDbPartition *partition = findPartition(mSystemPartitionName);
            partition->addView(schemaName);
            if (jsondbSettings->verbose())
                qDebug() << "viewTypes" << mViewTypes;
        }
    }
    if (schema.contains("properties"))
        updateSchemaIndexes(schemaName, schema);
}

void JsonDb::removeSchema(const QString &schemaName)
{
    if (jsondbSettings->verbose())
        qDebug() << "removeSchema" << schemaName;

    if (mSchemas.contains(schemaName)) {
        QJsonObject schema = mSchemas.take(schemaName);

        if (schema.contains("extends")) {
            QJsonValue extendsValue = schema.value("extends");
            QString extendedSchemaName;
            if (extendsValue.type() == QJsonValue::String)
                extendedSchemaName = extendsValue.toString();
            else if ((extendsValue.type() == QJsonValue::Object)
                     && extendsValue.toObject().contains("$ref"))
                extendedSchemaName = extendsValue.toObject().value("$ref").toString();
            if (extendedSchemaName == JsonDbString::kViewTypeStr) {
                mViewTypes.remove(schemaName);
                JsonDbPartition *partition = findPartition(mSystemPartitionName);
                partition->removeView(schemaName);
            }
        }
    }
}

QJsonObject JsonDb::validateSchema(const QString &schemaName, JsonDbObject object)
{
    if (!jsondbSettings->validateSchemas()) {
        if (jsondbSettings->debug())
            qDebug() << "Not validating schemas";
        return QJsonObject();
    }

    QJsonObject result = mSchemas.validate(schemaName, object);
    if (jsondbSettings->debug() && !result.value(JsonDbString::kCodeStr).isNull())
        qDebug() << "Schema validation error: " << result.value(JsonDbString::kMessageStr).toString() << object;

    return result;
}

QJsonObject JsonDb::validateMapObject(JsonDbObject map)
{
    QString targetType = map.value("targetType").toString();

    if (map.value(JsonDbString::kDeletedStr).toBool())
        return QJsonObject();
    if (targetType.isEmpty())
        return makeError(JsonDbError::InvalidMap, "targetType property for Map not specified");
    if (!mViewTypes.contains(targetType))
        return makeError(JsonDbError::InvalidMap, "targetType must be of a type that extends View");
    if (map.contains("join")) {
        QJsonObject sourceFunctions = map.value("join").toObject();
        if (sourceFunctions.isEmpty())
            return makeError(JsonDbError::InvalidMap, "sourceTypes and functions for Map with join not specified");
        QStringList sourceTypes = sourceFunctions.keys();
        for (int i = 0; i < sourceTypes.size(); i++)
            if (sourceFunctions.value(sourceTypes[i]).toString().isEmpty())
                return makeError(JsonDbError::InvalidMap,
                                 QString("join function for source type '%1' not specified for Map")
                                 .arg(sourceTypes[i]));
        if (map.contains("map"))
            return makeError(JsonDbError::InvalidMap, "Map 'join' and 'map' options are mutually exclusive");
        if (map.contains("sourceType"))
            return makeError(JsonDbError::InvalidMap, "Map 'join' and 'sourceType' options are mutually exclusive");

    } else {
        QJsonValue mapValue = map.value("map");
        if (map.value("sourceType").toString().isEmpty() && !mapValue.isObject())
            return makeError(JsonDbError::InvalidMap, "sourceType property for Map not specified");
        if (!mapValue.isString() && !mapValue.isObject())
            return makeError(JsonDbError::InvalidMap, "map function for Map not specified");
    }

    return QJsonObject();
}

QJsonObject JsonDb::validateReduceObject(JsonDbObject reduce)
{
    QString targetType = reduce.value("targetType").toString();
    QString sourceType = reduce.value("sourceType").toString();

    if (reduce.value(JsonDbString::kDeletedStr).toBool())
        return QJsonObject();
    if (targetType.isEmpty())
        return makeError(JsonDbError::InvalidReduce, "targetType property for Reduce not specified");
    if (!mViewTypes.contains(targetType))
        return makeError(JsonDbError::InvalidReduce, "targetType must be of a type that extends View");
    if (sourceType.isEmpty())
        return makeError(JsonDbError::InvalidReduce, "sourceType property for Reduce not specified");
    if (reduce.value("sourceKeyName").toString().isEmpty())
        return makeError(JsonDbError::InvalidReduce, "sourceKeyName property for Reduce not specified");
    if (reduce.value("add").toString().isEmpty())
        return makeError(JsonDbError::InvalidReduce, "add function for Reduce not specified");
    if (reduce.value("subtract").toString().isEmpty())
        return makeError(JsonDbError::InvalidReduce, "subtract function for Reduce not specified");

    return QJsonObject();
}

QJsonObject JsonDb::checkPartitionPresent(const QString &partitionName)
{
    if (!mPartitions.contains(partitionName) && partitionName != mEphemeralPartitionName)
        return makeError(JsonDbError::InvalidPartition, QString::fromLatin1("Unknown partition '%1'").arg(partitionName));
    return QJsonObject();
}

QJsonObject JsonDb::checkUuidPresent(JsonDbObject object, QString &uuid)
{
    if (!object.contains(JsonDbString::kUuidStr))
        return makeError(JsonDbError::MissingUUID, "Missing '_uuid' field in object");

    QUuid id = object.uuid();
    if (id.isNull())
        return makeError(JsonDbError::MissingUUID, "'_uuid' must contain a valid UUID");

    uuid = id.toString();
    return QJsonObject();
}

QJsonObject JsonDb::checkTypePresent(JsonDbObject object, QString &type)
{
    type = object.type();
    if (type.isEmpty()) {
        QString str = JsonWriter().toString(object.toVariantMap());
        return makeError(JsonDbError::MissingType, QString("Missing '_type' field in object '%1'").arg(str));
    }
    return QJsonObject();
}

QJsonObject JsonDb::checkNaturalObjectType(JsonDbObject object, QString &type)
{
    type = object.value(JsonDbString::kTypeStr).toString();
    if (mViewTypes.contains(type)) {
        QString str = JsonWriter().toString(object.toVariantMap());
        return makeError(JsonDbError::MissingType, QString("Cannot create/remove object of view type '%1': '%2'").arg(type).arg(str));
    }
    return QJsonObject();
}

QJsonObject JsonDb::checkAccessControl(const JsonDbOwner *owner, JsonDbObject object,
                                   const QString &partitionName, const QString &op)
{
    if (!owner->isAllowed(object, partitionName, op))
        return makeError(JsonDbError::OperationNotPermitted, "Access denied");
    return QJsonObject();
}

QJsonObject JsonDb::checkQuota(const JsonDbOwner *owner, int size, JsonDbPartition *partition)
{
    if (!partition->checkQuota(owner, size))
        return makeError(JsonDbError::QuotaExceeded, "Quota exceeded.");
    return QJsonObject();
}

QJsonObject JsonDb::checkCanAddSchema(JsonDbObject schema, JsonDbObject oldSchema)
{
    if (schema.value(JsonDbString::kDeletedStr).toBool())
        return QJsonObject();
    if (!schema.contains("name")
            || !schema.contains("schema"))
        return makeError(JsonDbError::InvalidSchemaOperation,
                         "_schemaType objects must specify both name and schema properties");

    QString schemaName = schema.value("name").toString();

    if (schemaName.isEmpty())
        return makeError(JsonDbError::InvalidSchemaOperation,
                         "name property of _schemaType object must be specified");

    if (mSchemas.contains(schemaName) && oldSchema.value("name").toString() != schemaName)
        return makeError(JsonDbError::InvalidSchemaOperation,
                         QString("A schema with name %1 already exists").arg(schemaName));

    return QJsonObject();
}

QJsonObject JsonDb::checkCanRemoveSchema(JsonDbObject schema)
{
    QString schemaName = schema.value("name").toString();

    // check if any objects exist
    GetObjectsResult getObjectResponse = getObjects(JsonDbString::kTypeStr, schemaName);
    // for non-View types, if objects exist the schema cannot be removed
    if (!mViewTypes.contains(schemaName)) {
        if (getObjectResponse.data.size() != 0)
            return makeError(JsonDbError::InvalidSchemaOperation,
                             QString("%1 object(s) of type %2 exist. You cannot remove the schema")
                             .arg(getObjectResponse.data.size())
                             .arg(schemaName));
    }

    // for View types, make sure no Maps or Reduces point at this type
    // the call to getObject will have updated the view, so pending Map or Reduce object removes will be forced
    JsonDbView *view = mPartitions.value(mSystemPartitionName)->findView(schemaName);
    if (view && view->sourceTypes().size()) {
      return makeError(JsonDbError::InvalidSchemaOperation,
                       QString("A view with targetType of %2 exists. You cannot remove the schema")
                       .arg(schemaName));
    }

    return QJsonObject();
}



QJsonObject JsonDb::validateAddIndex(const JsonDbObject &newIndex, const JsonDbObject &oldIndex) const
{
    if (!newIndex.isEmpty() && !oldIndex.isEmpty()) {
        if (oldIndex.value(kPropertyNameStr).toString() != newIndex.value(kPropertyNameStr).toString())
            return makeError(JsonDbError::InvalidIndexOperation,
                             QString("Changing old index propertyName '%1' to '%2' not supported")
                             .arg(oldIndex.value(kPropertyNameStr).toString())
                             .arg(newIndex.value(kPropertyNameStr).toString()));
        if (oldIndex.value(kPropertyTypeStr).toString() != newIndex.value(kPropertyTypeStr).toString())
            return makeError(JsonDbError::InvalidIndexOperation,
                             QString("Changing old index propertyType from '%1' to '%2' not supported")
                             .arg(oldIndex.value(kPropertyTypeStr).toString())
                             .arg(newIndex.value(kPropertyTypeStr).toString()));
        if (oldIndex.value(kObjectTypeStr).toString() != newIndex.value(kObjectTypeStr).toString())
            return makeError(JsonDbError::InvalidIndexOperation,
                             QString("Changing old index objectType from '%1' to '%2' not supported")
                             .arg(oldIndex.value(kObjectTypeStr).toString())
                             .arg(newIndex.value(kObjectTypeStr).toString()));
        if (oldIndex.value(kPropertyFunctionStr).toString() != newIndex.value(kPropertyFunctionStr).toString())
            return makeError(JsonDbError::InvalidIndexOperation,
                             QString("Changing old index propertyFunction from '%1' to '%2' not supported")
                             .arg(oldIndex.value(kPropertyFunctionStr).toString())
                             .arg(newIndex.value(kPropertyFunctionStr).toString()));
    }

    if (!(newIndex.contains(kPropertyFunctionStr) ^ newIndex.contains(kPropertyNameStr)))
        return makeError(JsonDbError::InvalidIndexOperation,
                         QString("Index object must have one of propertyName or propertyFunction set"));

    if (newIndex.contains(kPropertyFunctionStr) && !newIndex.contains(kNameStr))
        return makeError(JsonDbError::InvalidIndexOperation,
                         QString("Index object with propertyFunction must have name"));

    return QJsonObject();
}

const JsonDbNotification *JsonDb::createNotification(const JsonDbOwner *owner, JsonDbObject object)
{
    QString        uuid = object.value(JsonDbString::kUuidStr).toString();
    QStringList actions = QVariant(object.value(JsonDbString::kActionsStr).toArray().toVariantList()).toStringList();
    QString       query = object.value(JsonDbString::kQueryStr).toString();
    QJsonObject bindings = object.value("bindings").toObject();
    QString partition = object.value(JsonDbString::kPartitionStr).toString();
    if (partition.isEmpty())
        partition = mSystemPartitionName;

    JsonDbNotification *n = new JsonDbNotification(owner, uuid, query, actions, partition);
    JsonDbQuery *parsedQuery = JsonDbQuery::parse(query, bindings);
    n->setCompiledQuery(parsedQuery);
    const QList<OrQueryTerm> &orQueryTerms = parsedQuery->queryTerms;

    bool generic = true;
    for (int i = 0; i < orQueryTerms.size(); i++) {
        const OrQueryTerm &orQueryTerm = orQueryTerms[i];
        const QList<QueryTerm> &terms = orQueryTerm.terms();
        if (terms.size() == 1) {
            const QueryTerm &term = terms[0];
            if (term.op() == "=") {
                if (term.propertyName() == JsonDbString::kUuidStr) {
                    mKeyedNotifications.insert(term.value().toString(), n);
                    generic = false;
                    break;
                } else if (term.propertyName() == JsonDbString::kTypeStr) {
                    QString objectType = term.value().toString();
                    mKeyedNotifications.insert(objectType, n);
                    updateEagerViewTypes(objectType);
                    generic = false;
                    break;
                }
            }
        }
    }
    if (generic) {
        mKeyedNotifications.insert("__generic_notification__", n);
    }

    mNotificationMap[uuid] = n;
    return n;
}

void JsonDb::removeNotification(const QString &uuid)
{
    if (mNotificationMap.contains(uuid)) {
        JsonDbNotification *n = mNotificationMap.value(uuid);
        mNotificationMap.remove(uuid);
        const JsonDbQuery *parsedQuery = n->parsedQuery();
        const QList<OrQueryTerm> &orQueryTerms = parsedQuery->queryTerms;
        for (int i = 0; i < orQueryTerms.size(); i++) {
            const OrQueryTerm &orQueryTerm = orQueryTerms[i];
            const QList<QueryTerm> &terms = orQueryTerm.terms();
            if (terms.size() == 1) {
                const QueryTerm &term = terms[0];
                if (term.op() == "=") {
                    if (term.propertyName() == JsonDbString::kTypeStr) {
                        mKeyedNotifications.remove(term.value().toString(), n);
                    } else if (term.propertyName() == JsonDbString::kUuidStr) {
                        QString objectType = term.value().toString();
                        mKeyedNotifications.remove(objectType, n);
                    }
                }
            }
        }

        mKeyedNotifications.remove("__generic_notification__", n);

        delete n;
    }
}

void JsonDb::updateEagerViewTypes(const QString &objectType)
{
    JsonDbPartition *partition = findPartition(mSystemPartitionName);
    JsonDbView *view = partition->findView(objectType);
    if (!view)
        return;
    foreach (const QString sourceType, view->sourceTypes()) {
        QSet<QString> &targetTypes = mEagerViewSourceTypes[sourceType];
        targetTypes.insert(objectType);
        // now recurse until we get to a non-view sourceType
        updateEagerViewTypes(sourceType);
    }
}

JsonDbPartition *JsonDb::findPartition(const QString &name) const
{
    if (name.isEmpty())
        return mPartitions.value(mSystemPartitionName, 0);
    return mPartitions.value(name, 0);
}

QJsonObject JsonDb::createPartition(const JsonDbObject &object)
{
    QString name = object.value("name").toString();
    if (mPartitions.contains(name)) {
        QJsonObject resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Already have a partition with a given name '%1'").arg(name));
        return makeResponse(resultmap, errormap);
    }
    // verify partition name - only alphanum characters allowed
    bool isAlNum = true;
    for (int i = 0; i < name.length(); ++i) {
        const QChar ch = name.at(i);
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('-') &&
                ch != QLatin1Char('.') && ch != QLatin1Char('_')) {
            isAlNum = false;
            break;
        }
    }
    if (!isAlNum) {
        QJsonObject resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Partition name can only be alphanumeric '%1'").arg(name));
        return makeResponse(resultmap, errormap);
    }

    QString filename = name + QLatin1String(".db");

    JsonDbObject partitionDefinition;
    partitionDefinition.insert(JsonDbString::kTypeStr, JsonDbString::kPartitionTypeStr);
    partitionDefinition.insert(QLatin1String("name"), name);
    partitionDefinition.insert(QLatin1String("file"), filename);
    QJsonObject result = mPartitions[mSystemPartitionName]->createPersistentObject(partitionDefinition);
    if (responseIsError(result))
        return result;

    filename = mFilePath + filename;
    JsonDbPartition *partition = new JsonDbPartition(filename, name, this);
    if (jsondbSettings->verbose())
        qDebug() << "Opening partition" << name;

    if (!partition->open()) {
        qWarning() << "Failed to initialize partition" << name << "at" << filename;
        QJsonObject resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Cannot initialize a partition '%1'").arg(name));
        return makeResponse(resultmap, errormap);
    }
    mPartitions.insert(name, partition);
    JsonDbView::initViews(partition, name);

    checkNotifications(mSystemPartitionName, partitionDefinition, JsonDbNotification::Create);

    return result;
}

QJSValue JsonDb::toJSValue(const QJsonValue &v, QJSEngine *scriptEngine)
{
    switch (v.type()) {
    case QJsonValue::Null:
        return QJSValue(QJSValue::NullValue);
    case QJsonValue::Undefined:
        return QJSValue(QJSValue::UndefinedValue);
    case QJsonValue::Double:
        return QJSValue(v.toDouble());
    case QJsonValue::String:
        return QJSValue(v.toString());
    case QJsonValue::Bool:
        return QJSValue(v.toBool());
    case QJsonValue::Array: {
        QJSValue jsArray = scriptEngine->newArray();
        QJsonArray array = v.toArray();
        for (int i = 0; i < array.size(); i++)
            jsArray.setProperty(i, toJSValue(array.at(i), scriptEngine));
        return jsArray;
    }
    case QJsonValue::Object:
        return toJSValue(v.toObject(), scriptEngine);
    }
    return QJSValue(QJSValue::UndefinedValue);
}

QJSValue JsonDb::toJSValue(const QJsonObject &object, QJSEngine *scriptEngine)
{
    QJSValue jsObject = scriptEngine->newObject();
    for (QJsonObject::const_iterator it = object.begin(); it != object.end(); ++it)
        jsObject.setProperty(it.key(), toJSValue(it.value(), scriptEngine));
    return jsObject;
}

void JsonDb::updateView(const QString &viewType, const QString &partitionName)
{
    if (viewType.isEmpty())
        return;
    JsonDbPartition *partition = findPartition(partitionName);
    JsonDbView *view = partition->findView(viewType);
    if (view)
        view->updateView();
}

QJsonObject JsonDb::log(JsonDbOwner *owner, QJsonValue data)
{
    Q_UNUSED(owner);
    if (jsondbSettings->debug() || jsondbSettings->performanceLog())
        qDebug() << data.toObject().value(JsonDbString::kMessageStr).toString();
    return QJsonObject();
}

QHash<QString, qint64> JsonDb::fileSizes(const QString &partitionName) const
{
    QString name = partitionName;
    if (name.isEmpty())
        name = mSystemPartitionName;

    JsonDbPartition *partition = findPartition(name);
    if (partition)
        return partition->fileSizes();

    return QHash<QString, qint64>();
}

QT_END_NAMESPACE_JSONDB
