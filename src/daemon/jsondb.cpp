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

#include <QtJsonDbQson/private/qson_p.h>
#include <QtJsonDbQson/private/qsonstrings_p.h>
#include <QtJsonDbQson/private/qsonparser_p.h>

#include "jsondb.h"
#include "jsondb-proxy.h"
#include "jsondb-map-reduce.h"
#include "jsondbbtreestorage.h"
#include "schemamanager_impl_p.h"
#include "qsonobjecttypes_impl_p.h"

#include "aodb.h"

namespace QtAddOn { namespace JsonDb {

bool gUseQsonInDb = true;
bool gUseJsonInDb = false;
bool gValidateSchemas = (::getenv("JSONDB_VALIDATE_SCHEMAS") ? (QLatin1String(::getenv("JSONDB_VALIDATE_SCHEMAS")) == "true") : false);
bool gRejectStaleUpdates = (::getenv("JSONDB_REJECT_STALE_UPDATES") ? (QLatin1String(::getenv("JSONDB_REJECT_STALE_UPDATES")) == "true") : false);
bool gVerbose = (::getenv("JSONDB_VERBOSE") ? (QLatin1String(::getenv("JSONDB_VERBOSE")) == "true") : false);
bool gShowErrors = (::getenv("JSONDB_SHOW_ERRORS") ? (QLatin1String(::getenv("JSONDB_SHOW_ERRORS")) == "true") : false);
#ifndef QT_NO_DEBUG_OUTPUT
bool gDebug = (::getenv("JSONDB_DEBUG") ? (QLatin1String(::getenv("JSONDB_DEBUG")) == "true") : false);
bool gDebugRecovery = (::getenv("JSONDB_DEBUG_RECOVERY") ? (QLatin1String(::getenv("JSONDB_DEBUG_RECOVERY")) == "true") : false);
bool gPerformanceLog = (::getenv("JSONDB_PERFORMANCE_LOG") ? (QLatin1String(::getenv("JSONDB_PERFORMANCE_LOG")) == "true") : false);
#endif

const QString kSortKeysStr = QLatin1String("sortKeys");
const QString kStateStr = QLatin1String("state");

#ifndef QT_NO_DEBUG_OUTPUT
#define DBG() if (gDebug) qDebug() << Q_FUNC_INFO
#else
#define DBG() if (0) qDebug() << Q_FUNC_INFO
#endif

#define RETURN_IF_ERROR(errmap, toCheck) \
    ({ \
        errmap = toCheck; \
        if (!errmap.isEmpty()) { \
            QsonMap _r; \
            return makeResponse(_r, errmap); \
        } \
    })

void JsonDb::setError(QsonMap &map, int code, const QString &message)
{
    map.insert(JsonDbString::kCodeStr, code);
    map.insert(JsonDbString::kMessageStr, message);
}

QsonMap JsonDb::makeError(int code, const QString &message)
{
    QsonMap map;
    setError(map, code, message);
    return map;
}

QsonMap JsonDb::makeResponse( QsonMap& resultmap, QsonMap& errormap, bool silent )
{
    QsonMap map;
    if (gVerbose && !silent && !errormap.isEmpty()) {
        qCritical() << errormap;
    }
    if (!resultmap.isEmpty())
        map.insert( JsonDbString::kResultStr, resultmap);
    else
        map.insert( JsonDbString::kResultStr, QsonObject::NullValue);

    if (!errormap.isEmpty())
        map.insert( JsonDbString::kErrorStr, errormap );
    else
        map.insert( JsonDbString::kErrorStr, QsonObject::NullValue);
    return map;
}

QsonMap JsonDb::makeErrorResponse(QsonMap &resultmap,
                                  int code, const QString &message, bool silent)
{
    QsonMap errormap;
    setError(errormap, code, message);
    return makeResponse(resultmap, errormap, silent);
}

bool JsonDb::responseIsError( QsonMap responseMap )
{
    return responseMap.contains(JsonDbString::kErrorStr)
        && !responseMap.isNull(JsonDbString::kErrorStr);
}

//bool JsonDb::responseIsGood( QsonObject responseMap )
//{
//    return !responseMap.contains(JsonDbString::kErrorStr);
//}

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
  This function takes a single QsonObject,
  adds a "uuid" field to it with a unique UUID (overwriting a
  previous "uuid" field if necessary), and stores it in the object
  database.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "uuid", STRING } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

  This method must be overridden in a subclass.
*/

/*
  QsonObject JsonDb::create(const JsonDbOwner *owner, QsonObject object)
  {
  }
*/

/*!
  This function takes a single QsonObject with a valid "uuid" field.
  It updates the database to match the new object.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : NULL }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

  This method must be overridden in a subclass.
*/

/*
  QsonObject JsonDb::update(QsonObject object)
  {
  }
*/


/*!
  This function takes a QsonList of objects. It creates
  the items in the database, assigning each a unique "uuid" field.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }


  This class should be optimized by a JsonDb subclass.
*/

QsonMap JsonDb::createList(const JsonDbOwner *owner, QsonList& list, const QString &partition)
{
    int count = 0;
    QsonList resultList;

    JsonDbBtreeStorage *storage = findPartition(partition);
    if (!storage) {
        QsonMap resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partition));
        return makeResponse(resultmap, errormap);
    }

    WithTransaction lock(storage);
    CHECK_LOCK_RETURN(lock, "createList");

    for (int i = 0; i < list.size(); ++i) {
        QsonMap o = list.at<QsonMap>(i);
        QsonMap r = create(owner, o, partition);
        if (responseIsError(r))
            return r;
        count += 1;
        resultList.append(r.subObject("result"));
    }
    lock.commit();

    QsonMap resultmap, errormap;
    resultmap.insert( JsonDbString::kDataStr, resultList );
    resultmap.insert( JsonDbString::kCountStr, count );
    return makeResponse( resultmap, errormap );
}

/*!
  This function takes a QsonList of objects each with a valid
  "uuid" field.  It updates the items in the database.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

  This class should be optimized by a JsonDb subclass.
*/

QsonMap JsonDb::updateList(const JsonDbOwner *owner, QsonList& list, const QString &partition, bool replication)
{
    JsonDbBtreeStorage *storage = findPartition(partition);
    if (!storage) {
        QsonMap resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partition));
        return makeResponse(resultmap, errormap);
    }

    WithTransaction transaction(storage);
    CHECK_LOCK_RETURN(transaction, "updateList");
    int count = 0;
    QsonList resultList;

    for (int i = 0; i < list.size(); ++i) {
        QsonMap o = list.at<QsonMap>(i);
        QsonMap r = update(owner, o, partition, replication);
        if (responseIsError(r))
            return r;
        count += r.subObject(JsonDbString::kResultStr).valueInt(JsonDbString::kCountStr);
        resultList.append(r.subObject("result"));
    }
    transaction.commit();

    QsonMap resultmap, errormap;
    resultmap.insert( JsonDbString::kDataStr, resultList );
    resultmap.insert( JsonDbString::kCountStr, count );
    return makeResponse( resultmap, errormap );
}

/*!
  This function takes a QsonList of objects each with a valid
  "uuid" field.  It removes the items from the database.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

  This class should be optimized by a JsonDb subclass.
*/

QsonMap JsonDb::removeList(const JsonDbOwner *owner, QsonList list, const QString &partition)
{
    JsonDbBtreeStorage *storage = findPartition(partition);
    if (!storage) {
        QsonMap resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partition));
        return makeResponse(resultmap, errormap);
    }

    WithTransaction transaction(storage);
    CHECK_LOCK_RETURN(transaction, "removeList");
    int count = 0;
    QsonList removedList, errorsList;
    for (int i = 0; i < list.size(); ++i) {
        QsonMap o = list.at<QsonMap>(i);
        QString uuid = o.valueString(JsonDbString::kUuidStr);
        QsonMap r = remove(owner, o, partition);
        QsonMap error = r.subObject(JsonDbString::kErrorStr);
        if (!error.isEmpty()) {
            QsonMap obj;
            obj.insert(JsonDbString::kUuidStr, uuid);
            obj.insert(JsonDbString::kErrorStr, error);
            errorsList.append(obj);
            continue;
        }
        count += r.subObject(JsonDbString::kResultStr).valueInt(JsonDbString::kCountStr);
        QsonMap item;
        item.insert(JsonDbString::kUuidStr, uuid);
        removedList.append(item);
    }
    transaction.commit();

    QsonMap resultmap;
    resultmap.insert(JsonDbString::kCountStr, count);
    resultmap.insert(JsonDbString::kDataStr, removedList);
    resultmap.insert(JsonDbString::kErrorStr, errorsList);
    QsonMap errormap;
    return makeResponse(resultmap, errormap);
}

QsonMap JsonDb::changesSince(const JsonDbOwner * /* owner */, QsonMap object, const QString &partition)
{
    JsonDbBtreeStorage *storage = findPartition(partition);
    if (!storage) {
        QsonMap resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partition));
        return makeResponse(resultmap, errormap);
    }

    int stateNumber = object.valueInt(JsonDbString::kStateNumberStr, 0);
    if (object.contains(JsonDbString::kTypesStr)) {
        QSet<QString> limitTypes;
        QsonList l = object.subList(JsonDbString::kTypesStr);
        for (int i = 0; i < l.size(); i++)
            limitTypes.insert(l.at<QString>(i));
        return storage->changesSince(stateNumber, limitTypes);
    } else {
        return storage->changesSince(stateNumber);
    }
}

QVariant JsonDb::propertyLookup(QsonMap v, const QString &path)
{
    return propertyLookup(v, path.split('.'));
}

QVariant JsonDb::propertyLookup(QVariantMap object, const QStringList &path)
{
    QVariant v = object;
    for (int i = 0; i < path.size(); i++) {
        object = v.toMap();
        QString key = path[i];
        if (object.contains(key))
            v = object.value(key);
        else
            return QVariant();
    }
    return v;
}

QVariant JsonDb::propertyLookup(QsonMap object, const QStringList &path)
{
    if (!path.size()) {
        qCritical() << "JsonDb::propertyLookup empty path";
        abort();
        return QVariant();
    }
    QsonMap emptyMap;
    QsonList emptyList;
    QsonList objectList;
    for (int i = 0; i < path.size() - 1; i++) {
        const QString &key = path.at(i);
        // this part of the property is a list
        if (!objectList.isEmpty()) {
            bool ok = false;
            int index = key.toInt(&ok);
            if (ok && (index >= 0) && (objectList.count() > index)) {
                if (objectList.typeAt(index) == QsonObject::ListType) {
                    objectList = objectList.listAt(index);
                    object = emptyMap;
                } else  {
                    object = objectList.objectAt(index);
                    objectList = emptyList;
                }
                continue;
            }
        }
        // this part is a map
        if (object.contains(key)) {
            if (object.valueType(key) == QsonObject::ListType) {
                objectList = object.subList(key);
                object = emptyMap;
            } else  {
                object = object.subObject(key);
                objectList = emptyList;
            }
        } else {
            return QVariant();
        }
    }
    const QString &key = path.last();
    // get the last part from the list
    if (!objectList.isEmpty()) {
        bool ok = false;
        int index = key.toInt(&ok);
        if (ok && (index >= 0) && (objectList.count() > index)) {
            if (objectList.typeAt(index) == QsonObject::ListType) {
                return qsonToVariant(objectList.listAt(index));
            } else  {
                return qsonToVariant(objectList.objectAt(index));
            }
        }
    }
    // if the last part is in a map
    if (object.valueType(key) == QsonObject::ListType)
        return qsonToVariant(object.subList(key));
    else
        return qsonToVariant(object.value<QsonElement>(key));
}

JsonDb::JsonDb(const QString &path, QObject *parent)
    : QObject(parent)
    , mOwner(0)
    , mScriptEngine(new QJSEngine(this))
    , mJsonDbProxy(new JsonDbProxy(0, this, this))
    , mOpen(false)
{
    QFileInfo fi(path);
    if (fi.isDir())
        mFilePath = fi.filePath();
    mFilePath = fi.dir().path();
    if (mFilePath.at(mFilePath.size()-1) != QLatin1Char('/'))
        mFilePath += QLatin1Char('/');

    mOwner = new JsonDbOwner(this);
    mOwner->setOwnerId("com.nrcc.noklab.JsonDb");

    QJSValue globalObject = mScriptEngine->globalObject();
    globalObject.setProperty("jsondb", mScriptEngine->newQObject(mJsonDbProxy));
    globalObject.setProperty("console", mScriptEngine->newQObject( new Console));
    mJsonDbProxy->setOwner(owner());

}

JsonDb::~JsonDb()
{
    close();
}

bool JsonDb::open()
{
    // open system partition
    QString systemFileName = mFilePath + JsonDbString::kSystemPartitionName + QLatin1String(".db");
    JsonDbBtreeStorage *storage = new JsonDbBtreeStorage(systemFileName, JsonDbString::kSystemPartitionName, this);
    if (!storage->open()) {
        qDebug() << "Cannot open system partition at" << systemFileName;
        return false;
    }
    mStorages.insert(JsonDbString::kSystemPartitionName, storage);

    // read partition information from the db
    QsonMap result = storage->getObjects(JsonDbString::kTypeStr, JsonDbString::kPartitionStr);
    QsonList partitions = result.value<QsonList>("result");
    if (partitions.isEmpty()) {
        WithTransaction transaction(storage);
        ObjectTable *objectTable = storage->mainObjectTable();
        transaction.addObjectTable(objectTable);

        // make a system partition
        QsonMap partition;
        partition.insert(JsonDbString::kTypeStr, JsonDbString::kPartitionStr);
        partition.insert(QLatin1String("name"), JsonDbString::kSystemPartitionName);
        partition.insert(QLatin1String("file"), systemFileName);
        result = storage->createPersistentObject(partition);
        if (responseIsError(result)) {
            qCritical() << "Cannot create a system partition";
            return false;
        }
    }

    for (int i = 0; i < partitions.size(); ++i) {
        QsonMap part = partitions.at<QsonMap>(i);
        QString filename = part.value<QString>(QLatin1String("file"));
        QString name = part.value<QString>(QLatin1String("name"));

        if (name == JsonDbString::kSystemPartitionName)
            continue;

        if (mStorages.contains(name)) {
            qWarning() << "Duplicate partition found, ignoring" << name << "at" << filename;
            continue;
        }

        JsonDbBtreeStorage *storage = new JsonDbBtreeStorage(filename, name, this);
        if (gVerbose) qDebug() << "Opening partition" << name;

        if (!storage->open()) {
            qWarning() << "Failed to initialize partition" << name << "at" << filename;
            continue;
        }
        mStorages.insert(name, storage);
    }

    mViewTypes.clear();
    mEagerViewSourceTypes.clear();
    mMapDefinitionsBySource.clear();
    mMapDefinitionsByTarget.clear();
    mReduceDefinitionsBySource.clear();
    mReduceDefinitionsByTarget.clear();

    initSchemas();

    foreach (const QString &partition, mStorages.keys())
        initMap(partition);

    mOpen = true;
    return true;
}
bool JsonDb::clear()
{
    bool fail = false;
    foreach(JsonDbBtreeStorage *storage, mStorages)
        fail = !storage->clear() || fail;
    return !fail;
}

bool JsonDb::checkValidity()
{
    bool fail = false;
    foreach(JsonDbBtreeStorage *storage, mStorages)
        fail = !storage->checkValidity() || fail;
    return !fail;
}

void JsonDb::close()
{
    if (mOpen) {
        foreach (JsonDbBtreeStorage *storage, mStorages)
            storage->close();
    }
    mOpen = false;
}

void JsonDb::load(const QString &jsonFileName)
{
    QFile jsonFile(jsonFileName);
    if (!jsonFile.exists()) {
        qCritical() << QString("File %1 does not exist").arg(jsonFileName);
        return;
    }
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        qCritical() << QString("Cannot open file %1").arg(jsonFileName);
        return;
    }

    QByteArray json = jsonFile.readAll();
    QJSValue sv = scriptEngine()->evaluate(QString::fromUtf8(json.constData(), json.size()), jsonFileName);
    if (sv.isError()) {
        qWarning() << QString("DbServer::Load load %1: error:\n").arg(jsonFileName) << sv.toVariant();
    } else if (sv.isValid()) {
        if (gDebug)
            qDebug() << QString("DbServer::Load load %1: result:\n").arg(jsonFileName) << sv.toVariant();
    }
}

QsonMap JsonDb::find(const JsonDbOwner *owner, QsonMap obj, const QString &partition)
{
//    QElapsedTimer time;
//    QsonMap times;
//    time.start();
    QsonMap resultmap, errormap;

    QString query = obj.valueString(JsonDbString::kQueryStr);
    QsonMap bindings = obj.subObject("bindings");

    int limit = obj.valueInt(JsonDbString::kLimitStr, -1);
    int offset = obj.valueInt(JsonDbString::kOffsetStr, 0);

    JsonDbBtreeStorage *storage = findPartition(partition);
    if (!storage) {
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partition));
        return makeResponse(resultmap, errormap);
    }

    if ( limit < -1 )
        setError( errormap, JsonDbError::InvalidLimit, "Invalid limit" );
    else if ( offset < 0 )
        setError( errormap, JsonDbError::InvalidOffset, "Invalid offset" );
    else if ( query.isEmpty() )
        setError( errormap, JsonDbError::MissingQuery, "Missing query string");
    else {
//        times.insert("time0 before parse", time.elapsed());
        JsonDbQuery parsedQuery  = JsonDbQuery::parse(query, bindings);
//        times.insert("time1 after parse", time.elapsed());

        if (!parsedQuery.queryTerms.size() && !parsedQuery.orderTerms.size()) {
            setError( errormap, JsonDbError::MissingQuery, QString("Missing query: ") + parsedQuery.queryExplanation.join("\n"));
            return makeResponse( resultmap, errormap );
        }

        QStringList explanation = parsedQuery.queryExplanation;
        QVariant::Type resultType = parsedQuery.resultType;
        QStringList mapExpressions = parsedQuery.mapExpressionList;
        QStringList mapKeys = parsedQuery.mapKeyList;

        QsonMap queryResult;
        if (partition.isEmpty() && (mStorages.size() > 1))
            queryResult = storage->queryPersistentObjects(owner, parsedQuery, limit, offset, mStorages.values());
        else
            queryResult = storage->queryPersistentObjects(owner, parsedQuery, limit, offset);
        QsonList results = queryResult.subList("data");
        if (gDebug) {
            const QList<OrQueryTerm> &orQueryTerms = parsedQuery.queryTerms;
            for (int i = 0; i < orQueryTerms.size(); i++) {
                const OrQueryTerm orQueryTerm = orQueryTerms[i];
                foreach (const QueryTerm &queryTerm, orQueryTerm.terms()) {
                    if (gVerbose) {
                        qDebug() << __FILE__ << __LINE__
                                 << (QString("    %1%4%5 %2 %3    ")
                                     .arg(queryTerm.fieldName())
                                     .arg(queryTerm.op())
                                     .arg(JsonWriter().toString(queryTerm.value()))
                                     .arg(queryTerm.joinField().size() ? "->" : "").arg(queryTerm.joinField()));
                    }
                }
            }
            QList<OrderTerm> &orderTerms = parsedQuery.orderTerms;
            for (int i = 0; i < orderTerms.size(); i++) {
                const OrderTerm &orderTerm = orderTerms[i];
                if (gVerbose) qDebug() << __FILE__ << __LINE__ << QString("    %1 %2    ").arg(orderTerm.fieldName).arg(orderTerm.ascending ? "ascending" : "descending");
            }
        }
        DBG() << endl
#if 0
              << "  orderTerms: " << serializer.serialize(orderTerms) << endl
              << "  queryTerms: " << serializer.serialize(queryTerms) << endl
#endif
              << "  limit:      " << limit << endl
              << "  offset:     " << offset << endl
              << "  results:    " << results;

        int length = results.count();

        if (mapExpressions.length() && parsedQuery.mAggregateOperation.compare("count")) {
            QMap<QString, QsonMap> objectCache;
            int nExpressions = mapExpressions.length();
            QVector<QVector<QStringList> > joinPaths(nExpressions);
            for (int i = 0; i < nExpressions; i++) {
                QString fieldName = mapExpressions[i];
                QStringList joinPath = fieldName.split("->");
                int joinPathSize = joinPath.size();
                QVector<QStringList> fieldPaths(joinPathSize);
                for (int j = 0; j < joinPathSize; j++) {
                    QString joinField = joinPath[j];
                    fieldPaths[j] = joinField.split('.');
                }
                joinPaths[i] = fieldPaths;
            }

            QsonList mappedResult;
            for (int r = 0; r < results.count(); r++) {
                const QsonMap obj = results.at<QsonMap>(r);
                QsonList list;
                QsonMap map;
                for (int i = 0; i < nExpressions; i++) {
                    QVariant v;

                    QVector<QStringList> &joinPath = joinPaths[i];
                    int joinPathSize = joinPath.size();
                    if (joinPathSize == 1) {
                        v = propertyLookup(obj, joinPath[joinPathSize-1]);
                    } else {
                        QsonMap baseObject = obj;
                        for (int j = 0; j < joinPathSize-1; j++) {
                            QString uuid = propertyLookup(baseObject, joinPath[j]).toString();
                            if (uuid.isEmpty()) {
                                baseObject = QsonMap();
                            } else if (objectCache.contains(uuid)) {
                                baseObject = objectCache.value(uuid);
                            } else {
                                ObjectKey objectKey(uuid);
                                bool gotBaseObject = storage->getObject(objectKey, baseObject);
                                if (gotBaseObject)
                                    objectCache.insert(uuid, baseObject);
                            }
                        }
                        v = propertyLookup(baseObject, joinPath[joinPathSize-1]);
                    }

                    if (resultType == QVariant::Map)
                        map.insert(mapKeys[i], variantToQson(v));
                    else if (resultType == QVariant::List)
                        list.append(variantToQson(v));
                    else
                        mappedResult.append(variantToQson(v));
                }
                if (resultType == QVariant::Map)
                    mappedResult.append(map);
                else if (resultType == QVariant::List)
                    mappedResult.append(list);
            }

            resultmap.insert(JsonDbString::kDataStr, mappedResult);
        } else {
            resultmap.insert(JsonDbString::kDataStr, results);
        }

        resultmap.insert(JsonDbString::kLengthStr, length);
        resultmap.insert(JsonDbString::kOffsetStr, offset);
        resultmap.insert(JsonDbString::kExplanationStr, variantToQson(explanation));
        if (queryResult.contains(kSortKeysStr))
            resultmap.insert(kSortKeysStr, queryResult.subList(kSortKeysStr));
        if (queryResult.contains(kStateStr))
            resultmap.insert(kStateStr, queryResult.valueInt(kStateStr));
    }

    return makeResponse( resultmap, errormap );
}

/*!
  This function takes a single QsonObject,
  adds a "_uuid" field to it with a unique UUID (overwriting a
  previous "_uuid" field if necessary), and stores it in the object
  database.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "_uuid", STRING } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/

QsonMap JsonDb::create(const JsonDbOwner *owner, QsonMap& object, const QString &partition)
{
    QsonMap resultmap, errormap;

    if (object.isDocument()) {
        setError(errormap, JsonDbError::InvalidRequest, "New object should not have _uuid");
        return makeResponse(resultmap, errormap);
    }

    populateIdBySchema(owner, object);
    object.generateUuid();

    return update(owner, object, partition);
}

QsonMap JsonDb::createViewObject(const JsonDbOwner *owner, QsonMap& object, const QString &partition)
{
    QsonMap resultmap, errormap;

    if (object.contains(JsonDbString::kUuidStr)) {
        setError(errormap, JsonDbError::InvalidRequest, "New object should not have _uuid");
        return makeResponse(resultmap, errormap);
    }
    QString objectType;
    RETURN_IF_ERROR(errormap, checkTypePresent(object, objectType));
    RETURN_IF_ERROR(errormap, checkAccessControl(owner, object, "write"));
    RETURN_IF_ERROR(errormap, validateSchema(objectType, object));

    JsonDbBtreeStorage *storage = findPartition(partition);
    if (!storage) {
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partition));
        return makeResponse(resultmap, errormap);
    }

    populateIdBySchema(owner, object);
    object.generateUuid();
    object.computeVersion();

    int dataSize = object.dataSize();
    RETURN_IF_ERROR(errormap, checkQuota(owner, dataSize, storage));

    QsonMap response = storage->createPersistentObject(object);

    if (responseIsError(response))
        return response;

    storage->addToQuota(owner, dataSize);

    checkNotifications( object, Notification::Create );

    return response;
}

/*!
  This function takes a single QsonObject with a valid "_uuid" field.
  It updates the database to match the new object.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "count" : NUMBER } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/

QsonMap JsonDb::update(const JsonDbOwner *owner, QsonMap& object, const QString &partition, bool replication)
{
    QsonMap resultmap, errormap;
    QString uuid, objectType;

    RETURN_IF_ERROR(errormap, checkUuidPresent(object, uuid));
    RETURN_IF_ERROR(errormap, checkTypePresent(object, objectType));
    RETURN_IF_ERROR(errormap, checkNaturalObjectType(object, objectType));
    if (!object.contains(JsonDbString::kDeletedStr) || !object.valueBool(JsonDbString::kDeletedStr, false))
        RETURN_IF_ERROR(errormap, validateSchema(objectType, object));
    RETURN_IF_ERROR(errormap, checkAccessControl(owner, object, "write"));

    // don't repopulate if deleting object, if it's a schema type then _id is altered.
    if (!object.contains(JsonDbString::kDeletedStr)) {
        if (populateIdBySchema(owner, object)) {
            object.generateUuid();

            if (object.valueString(JsonDbString::kUuidStr) != uuid) {
                setError(errormap, JsonDbError::InvalidRequest, "_uuid mismatch, use create()");
                return makeResponse(resultmap, errormap);
            }
        }
    }
    object.computeVersion();

    JsonDbBtreeStorage *storage = findPartition(partition);
    if (!storage) {
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partition));
        return makeResponse(resultmap, errormap);
    }

    WithTransaction transaction(storage);
    ObjectTable *objectTable = storage->findObjectTable(objectType);
    transaction.addObjectTable(objectTable);

    // retrieve record we are updating to use
    QsonMap master;
    bool forCreation = true;

    QsonMap _delrec;
    bool gotDelrec = storage->getObject(uuid, _delrec, objectType);
    if (gotDelrec) {
        master = _delrec;
        forCreation = false;
        Q_ASSERT(master.isDocument());
    } else if (!replication && object.valueBool(JsonDbString::kDeletedStr, false)) {
        setError(errormap, JsonDbError::MissingObject, QLatin1String("Cannot remove non-existing object"));
        return makeResponse(resultmap, errormap);
    }

    if (forCreation || !gRejectStaleUpdates) {
        master = object;
        // hack to make sure _lastVersion equals _version
        master.insert(JsonDbString::kVersionStr, master.valueString(JsonDbString::kVersionStr));
    } else {
        int conflictsBefore = master.subObject(QsonStrings::kMetaStr).subList(QsonStrings::kConflictsStr).size();
        if (!master.mergeVersions(object, replication)) {
            // replay
            resultmap.insert(JsonDbString::kUuidStr, uuid);
            resultmap.insert(JsonDbString::kVersionStr, object.valueString(JsonDbString::kVersionStr));
            resultmap.insert(JsonDbString::kCountStr, 1);

            return makeResponse(resultmap, errormap);
        }
        int conflictsAfter = master.subObject(QsonStrings::kMetaStr).subList(QsonStrings::kConflictsStr).size();

        if (!replication && (conflictsBefore < conflictsAfter)) {
            setError( errormap, JsonDbError::UpdatingStaleVersion, "Updating stale version of object.");
            return makeResponse(resultmap, errormap);
        }
    }
    objectType = master.valueString(JsonDbString::kTypeStr);
    bool forRemoval = master.valueBool(JsonDbString::kDeletedStr, false);

    if (forCreation && forRemoval && !replication) {
        setError( errormap, JsonDbError::MissingObject, "Can not create a deleted object.");
        return makeResponse(resultmap, errormap);
    }

    if (objectType == "Partition") {
        if (!forCreation) {
            setError(errormap, JsonDbError::InvalidPartition, "Updates to partition objects not supported yet!");
            return makeResponse(resultmap, errormap);
        }

        if (!partition.isEmpty() && partition != JsonDbString::kSystemPartitionName) {
            setError(errormap, JsonDbError::InvalidPartition, "Partition objects can only be created in system partition");
            return makeResponse(resultmap, errormap);
        }

        return createPartition(object);
    }

    // if the old object is a schema, make sure it's ok to remove it
    if (_delrec.valueString(JsonDbString::kTypeStr) == JsonDbString::kSchemaTypeStr)
        RETURN_IF_ERROR(errormap, checkCanRemoveSchema(_delrec));

    // validate the new object
    if (objectType == JsonDbString::kSchemaTypeStr)
        RETURN_IF_ERROR(errormap, checkCanAddSchema(master, _delrec));
    else if (objectType == JsonDbString::kMapTypeStr)
        RETURN_IF_ERROR(errormap, validateMapObject(master));
    else if (objectType == JsonDbString::kReduceTypeStr)
        RETURN_IF_ERROR(errormap, validateReduceObject(master));

    // If index, make sure it's ok to update/create (update not supported yet)
    if (!forRemoval && objectType == kIndexTypeStr)
        RETURN_IF_ERROR(errormap, validateAddIndex(master, _delrec));

    int dataSize = master.dataSize() - _delrec.dataSize();
    if (!forRemoval)
        RETURN_IF_ERROR(errormap, checkQuota(owner, dataSize, storage));

    QsonMap response;

    Notification::Action action;
    if (forRemoval) {
        action = Notification::Delete;
        response = storage->removePersistentObject(_delrec, master);
    } else if (!_delrec.isDocument()) {
        action = Notification::Create;
        response = storage->createPersistentObject(master);
    } else {
        action = Notification::Update;
        response = storage->updatePersistentObject(master);
    }

    if (responseIsError(response))
        return response;

    if (forRemoval)
        storage->addToQuota(owner, -_delrec.dataSize());
    else
        storage->addToQuota(owner, dataSize);

    // replication might do a write not changing the head version
    bool headVersionUpdated =
            !forCreation && QsonVersion::version(_delrec) != QsonVersion::version(master);


    // handle removing old schema, map, etc.
    if (headVersionUpdated) {
        QString oldType = _delrec.valueString(JsonDbString::kTypeStr);
        if (oldType == JsonDbString::kSchemaTypeStr)
            removeSchema(_delrec.valueString("name"));
        else if (oldType == JsonDbString::kNotificationTypeStr)
            removeNotification(uuid);
    }

    // create new schema, map, etc.
    if (!forRemoval && (forCreation || headVersionUpdated)) {
        if (objectType == JsonDbString::kSchemaTypeStr)
            setSchema(object.valueString("name"), object.subObject("schema"));
        else if (objectType == JsonDbString::kNotificationTypeStr)
            createNotification(owner, master);
        else if (objectType == kIndexTypeStr)
            addIndex(master, partition);
    }

    if (forRemoval) {
        if (objectType == kIndexTypeStr)
            removeIndex(_delrec, partition);
    }

    // only notify if there is a visible change
    // TODO: fix tests/benchmarks/jsondb-listmodel before re-enabling this
    //    if (forCreation || headVersionUpdated)
    if (!forRemoval)
        checkNotifications( object, action );
    else
        checkNotifications( _delrec, action );

    return response;
}

/*!
  This function takes a single QsonObject with a valid "_uuid" field.
  It updates the database to match the new object.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "count" : NUMBER } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/
QsonMap JsonDb::updateViewObject(const JsonDbOwner *owner, QsonMap& object, const QString &partition)
{
    QsonMap resultmap, errormap;
    QString uuid, objectType;
    bool replication = false;

    RETURN_IF_ERROR(errormap, checkUuidPresent(object, uuid));
    RETURN_IF_ERROR(errormap, checkTypePresent(object, objectType));
    RETURN_IF_ERROR(errormap, validateSchema(objectType, object));
    RETURN_IF_ERROR(errormap, checkAccessControl(owner, object, "write"));

    if (populateIdBySchema(owner, object)) {
        object.generateUuid();

        if (object.valueString(JsonDbString::kUuidStr) != uuid) {
            setError(errormap, JsonDbError::InvalidRequest, "_uuid mismatch, use create()");
            return makeResponse(resultmap, errormap);
        }
    }
    object.computeVersion();

    JsonDbBtreeStorage *storage = findPartition(partition);
    if (!storage) {
        setError(errormap, JsonDbError::InvalidPartition, QString("Invalid partition '%1'").arg(partition));
        return makeResponse(resultmap, errormap);
    }

    // retrieve record we are updating to use
    QsonMap master;

    QsonMap _delrec;
    bool gotDelrec = storage->getObject(uuid, _delrec, objectType);
    if (gotDelrec) {
        master =_delrec;
    } else if (!replication && object.valueBool(JsonDbString::kDeletedStr, false)) {
        setError(errormap, JsonDbError::MissingObject, QLatin1String("Cannot remove non-existing object"));
        return makeResponse(resultmap, errormap);
    }

    if (master.isDocument() && gRejectStaleUpdates) {
        int conflictsBefore = master.subObject(QsonStrings::kMetaStr).subList(QsonStrings::kConflictsStr).size();
        if (!master.mergeVersions(object)) {
            // replay
            resultmap.insert(JsonDbString::kUuidStr, uuid);
            resultmap.insert(JsonDbString::kVersionStr, object.valueString(JsonDbString::kVersionStr));
            resultmap.insert(JsonDbString::kCountStr, 1);

            return makeResponse(resultmap, errormap);
        }
        int conflictsAfter = master.subObject(QsonStrings::kMetaStr).subList(QsonStrings::kConflictsStr).size();

        if (!replication && (conflictsBefore < conflictsAfter)) {
            setError( errormap, JsonDbError::UpdatingStaleVersion, "Updating stale version of object.");
            return makeResponse(resultmap, errormap);
        }
    } else {
        master = object;
        // hack to make sure _lastVersion equals _version
        master.insert(JsonDbString::kVersionStr, master.valueString(JsonDbString::kVersionStr));
    }
    objectType = master.valueString(JsonDbString::kTypeStr);

    bool forRemoval = object.valueBool(JsonDbString::kDeletedStr, false);

    // if the old object is a schema, make sure it's ok to remove it
    if (_delrec.valueString(JsonDbString::kTypeStr) == JsonDbString::kSchemaTypeStr)
        RETURN_IF_ERROR(errormap, checkCanRemoveSchema(_delrec));

    // validate the new object
    if (objectType == JsonDbString::kSchemaTypeStr)
        RETURN_IF_ERROR(errormap, checkCanAddSchema(master, _delrec));
    else if (objectType == JsonDbString::kMapTypeStr)
        RETURN_IF_ERROR(errormap, validateMapObject(master));
    else if (objectType == JsonDbString::kReduceTypeStr)
        RETURN_IF_ERROR(errormap, validateReduceObject(master));

    int dataSize = master.dataSize() - _delrec.dataSize();
    if (!forRemoval)
        RETURN_IF_ERROR(errormap, checkQuota(owner, dataSize, storage));

    QsonMap response;

    Notification::Action action;
    if (forRemoval) {
        action = Notification::Delete;
        response = storage->removePersistentObject(_delrec, master);
    } else if (!_delrec.isDocument()) {
        action = Notification::Create;
        response = storage->createPersistentObject(master);
    } else {
        action = Notification::Update;
        response = storage->updatePersistentObject(master);
    }

    if (responseIsError(response))
        return response;

    if (forRemoval)
        storage->addToQuota(owner, -_delrec.dataSize());
    else
        storage->addToQuota(owner, dataSize);

    QString oldType = _delrec.valueString(JsonDbString::kTypeStr);
    // handle removing old schema, map, etc.
    if (oldType == JsonDbString::kSchemaTypeStr)
        removeSchema(_delrec.valueString("name"));
    else if (oldType == JsonDbString::kNotificationTypeStr)
        removeNotification(uuid);

    // create new schema, map, etc.
    if (objectType == JsonDbString::kSchemaTypeStr)
        setSchema(object.valueString("name"), object.subObject("schema"));
    else if (objectType == JsonDbString::kNotificationTypeStr)
        createNotification(owner, master);

    checkNotifications( (action == Notification::Delete ? _delrec : object), action );

    return response;
}


/*!
  This function takes a single QsonObject with valid "_uuid" and "_version" fields.
  It removes the item's verstion from the database by replacing it with a tombstone.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/

QsonMap JsonDb::remove(const JsonDbOwner *owner, const QsonMap &object, const QString &partition)
{
    QsonMap errormap;
    QString uuid;
    RETURN_IF_ERROR(errormap, checkUuidPresent(object, uuid));
    QString objectType = object.valueString(JsonDbString::kTypeStr);

    // quick fix for the case when object only contains _uuid
    // TODO: refactor
    JsonDbBtreeStorage *storage = findPartition(partition);
    QsonMap delrec;
    bool gotDelrec = storage->getObject(uuid, delrec, objectType);
    if (!gotDelrec)
        delrec = object;

    QsonMap tombstone;
    tombstone.insert(JsonDbString::kUuidStr, uuid);
    tombstone.insert(JsonDbString::kDeletedStr, true);
    if (delrec.contains(JsonDbString::kTypeStr))
        tombstone.insert(JsonDbString::kTypeStr, delrec.valueString(JsonDbString::kTypeStr));
    if (delrec.contains(JsonDbString::kVersionStr))
        tombstone.insert(JsonDbString::kVersionStr, delrec.valueString(JsonDbString::kVersionStr));
    return update(owner, tombstone, partition);
}

/*!
  This function takes a single QsonObject with a valid "_uuid" field.
  It removes the item from the database.

  On success it returns a QsonObject of the form:
  { "error": NULL, "result" : { "count": NUMBER } }

  On failure it returns a QsonObject of the form:
  { "error": { "code": VALUE, "message": STRING }, "result": NULL }

*/

QsonMap JsonDb::removeViewObject(const JsonDbOwner *owner, QsonMap object, const QString &partition)
{
    QsonMap resultmap, errormap;
    QString uuid;

    RETURN_IF_ERROR(errormap, checkUuidPresent(object, uuid));

    QString objectType = object.valueString(JsonDbString::kTypeStr);

    QsonMap tombstone;
    tombstone.insert(JsonDbString::kTypeStr, object.valueString(JsonDbString::kTypeStr));
    tombstone.insert(JsonDbString::kUuidStr, object.valueString(JsonDbString::kUuidStr));
    tombstone.insert(JsonDbString::kVersionStr, object.valueString(JsonDbString::kVersionStr));
    tombstone.insert(JsonDbString::kDeletedStr, true);

    QsonMap result = updateViewObject(owner, tombstone, partition);
    return result;
}

QsonMap JsonDb::getObjects(const QString &keyName, const QVariant &key, const QString &type, const QString &partition) const
{
    const QString &objectType = (keyName == JsonDbString::kTypeStr) ? key.toString() : type;
    if (JsonDbBtreeStorage *storage = findPartition(partition))
        return storage->getObjects(keyName, key, objectType);
    return QsonMap();
}

void JsonDb::checkNotifications( QsonMap object, Notification::Action action )
{
    //DBG() << object;

    QStringList notificationKeys;
    if (object.contains(JsonDbString::kTypeStr)) {
        QString objectType = object.valueString(JsonDbString::kTypeStr);
        notificationKeys << objectType;
        if (mEagerViewSourceTypes.contains(objectType)) {
            const QSet<QString> &targetTypes = mEagerViewSourceTypes[objectType];
            for (QSet<QString>::const_iterator it = targetTypes.begin(); it != targetTypes.end(); ++it)
                emit requestViewUpdate(*it, JsonDbString::kSystemPartitionName);
        }
    }
    if (object.contains(JsonDbString::kUuidStr))
        notificationKeys << object.valueString(JsonDbString::kUuidStr);
    notificationKeys << "__generic_notification__";

    QHash<QString, QsonMap> objectCache;
    for (int i = 0; i < notificationKeys.size(); i++) {
        QString key = notificationKeys[i];
        QMultiMap<QString, Notification *>::const_iterator it = mKeyedNotifications.find(key);
        while ((it != mKeyedNotifications.end()) && (it.key() == key)) {
            Notification *n = it.value();
            DBG() << "Notification" << n->query() << n->actions();
            if ( n->actions() & action ) {
                QsonMap r;
                if (!n->query().isEmpty()) {
                    DBG() << "Checking notification" << n->query() << endl
                          << "    for object" << object;
                    JsonDbQuery *query  = n->parsedQuery();
                    if (query->match(object, &objectCache, 0/*mStorage*/))
                        r = object;
                    DBG() << "Got result" << r;
                } else {
                    r = object;
                }
                if (!r.isEmpty()) {
                    QString actionStr = ( action == Notification::Create ? JsonDbString::kCreateStr :
                                          (action == Notification::Update ? JsonDbString::kUpdateStr :
                                           JsonDbString::kRemoveStr ));
                    emit notified(n->uuid(), r, actionStr);
                }
            }

            ++it;
        }
    }
}

QsonMap JsonDb::addIndex(const QString &fieldName, const QString &fieldType, const QString &objectType, const QString &partition)
{
    if (JsonDbBtreeStorage *storage = findPartition(partition))
        return storage->addIndex(fieldName, fieldType, objectType);
    return QsonMap();
}

QsonMap JsonDb::removeIndex(const QString &fieldName, const QString &fieldType, const QString &objectType, const QString &partition)
{
    if (JsonDbBtreeStorage *storage = findPartition(partition))
        return storage->removeIndex(fieldName, fieldType, objectType);
    return QsonMap();
}

void JsonDb::addIndex(QsonMap indexObject, const QString &partition)
{
    QString fieldName = indexObject.valueString(kFieldStr);
    QString fieldType = indexObject.valueString(kFieldTypeStr);
    QString objectType = indexObject.valueString(kObjectTypeStr);

    if (JsonDbBtreeStorage *storage = findPartition(partition))
        storage->addIndex(fieldName, fieldType, objectType);
}

void JsonDb::removeIndex(QsonMap indexObject, const QString &partition)
{
    QString fieldName = indexObject.valueString(kFieldStr);
    QString fieldType = indexObject.valueString(kFieldTypeStr);
    QString objectType = indexObject.valueString(kObjectTypeStr);

    if (JsonDbBtreeStorage *storage = findPartition(partition))
        storage->removeIndex(fieldName, fieldType, objectType);
}

void JsonDb::updateSchemaIndexes(const QString &schemaName, QsonMap object, const QStringList &path)
{
    QsonMap properties = object.subObject("properties");
    const QList<QString> keys = properties.keys();
    for (int i = 0; i < keys.size(); i++) {
        const QString &k = keys[i];
        QsonMap propertyInfo = properties.subObject(k);
        if (propertyInfo.contains("indexed")) {
            QString propertyType = (propertyInfo.contains("type") ? propertyInfo.valueString("type") : "string");
            QStringList kpath = path;
            kpath << k;
            JsonDbBtreeStorage *storage = findPartition(JsonDbString::kSystemPartitionName);
            storage->addIndex(kpath.join("."), propertyType, schemaName);
        }
        if (propertyInfo.contains("properties")) {
            updateSchemaIndexes(schemaName, propertyInfo, path + (QStringList() << k));
        }
    }
}

bool JsonDb::populateIdBySchema(const JsonDbOwner *owner, QsonMap &object)
{
    // minimize inserts
    if (!object.contains(JsonDbString::kOwnerStr)
        || ((object.valueString(JsonDbString::kOwnerStr) != owner->ownerId())
            && !owner->isAllowed(object, "setOwner")))
        object.insert(JsonDbString::kOwnerStr, owner->ownerId());

    QString objectType = object.valueString(JsonDbString::kTypeStr);
    if (mSchemas.contains(objectType)) {
        QsonMap schema = mSchemas.value(objectType);
        if (schema.contains("primaryKey")) {
            QsonObject::Type pkType = schema.valueType("primaryKey");
            QString _id = QString("primaryKey::%1:").arg(objectType);

            if (pkType == QsonObject::StringType) {
                QString keyName = schema.valueString("primaryKey");
                _id.append(keyName).append('=').append(object.valueString(keyName));
            } else if (pkType == QsonObject::ListType) {
                QsonList keyList = schema.subList("primaryKey");
                for (int i = 0; i < keyList.size(); i++) {
                    QString keyName = keyList.at<QString>(i);
                    _id.append(":").append(keyName).append('=').append(object.valueString(keyName));
                }
            }
            // minimize inserts
            if (object.valueString("_id") != _id)
                object.insert("_id", _id);

            return true;
        }
    }
    return false;
}

void JsonDb::initSchemas()
{
    if (gVerbose) qDebug() << "initSchemas";
    {
        QsonMap getObjectResponse = getObjects(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr,
                                              QString(), JsonDbString::kSystemPartitionName);
        QsonList schemas = getObjectResponse.subList("result");
        for (int i = 0; i < schemas.size(); ++i) {
            QsonMap schemaObject = schemas.at<QsonMap>(i);
            QString schemaName = schemaObject.valueString("name");
            QsonMap schema = schemaObject.subObject("schema");
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
            QsonMap schema = variantToQson(parser.result().toMap());
            QsonMap schemaObject;
            schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
            schemaObject.insert("name", schemaName);
            schemaObject.insert("schema", schema);
            create(mOwner, schemaObject);
        }
    }
    {
        addIndex("name", "string", "Capability");
        const QString capabilityName("RootCapability");
        QFile capabilityFile(QString(":schema/%1.json").arg(capabilityName));
        capabilityFile.open(QIODevice::ReadOnly);
        JsonReader parser;
        bool ok = parser.parse(capabilityFile.readAll());
        if (!ok) {
            qWarning() << "Parsing " << capabilityName << " capability" << parser.errorString();
            return;
        }
        QsonMap capability = variantToQson(parser.result().toMap());
        QString name = capability.valueString("name");
        QsonMap getObjectResponse = getObjects("name", name, "Capability");
        int count = getObjectResponse.valueInt("count", 0);
        if (!count) {
            if (gVerbose)
                qDebug() << "Creating capability" << capability;
            create(mOwner, capability);
        } else {
            QsonMap currentCapability = getObjectResponse.subList("result").at<QsonMap>(0);
            if (currentCapability.value<QsonElement>("accessRules") != capability.value<QsonElement>("accessRules")) {
                currentCapability.insert("accessRules", capability.value<QsonElement>("accessRules"));
                update(mOwner, currentCapability);
            }
        }
    }
}

void JsonDb::setSchema(const QString &schemaName, QsonMap schema)
{
    if (gVerbose)
        qDebug() << "setSchema" << schemaName << schema;
    QsonMap errors = mSchemas.insert(schemaName, schema);

    if (!errors.isEmpty()) {
        //if (gVerbose) {
            qDebug() << "setSchema failed because of errors" << schemaName << schema;
            qDebug() << errors;
        //}
        // FIXME should we accept broken schemas?
        // return;
    }
    if (schema.contains("extends")) {
        QString extendedSchemaName = schema.valueString("extends");
        if (extendedSchemaName == JsonDbString::kViewTypeStr) {
            mViewTypes.insert(schemaName);
            //TODO fix call to findPartition
            JsonDbBtreeStorage *storage = findPartition(JsonDbString::kSystemPartitionName);
            storage->addView(schemaName);
            if (gVerbose) qDebug() << "viewTypes" << mViewTypes;
        }
    }
    if (schema.contains("properties"))
        updateSchemaIndexes(schemaName, schema);
}

void JsonDb::removeSchema(const QString &schemaName)
{
    if (gVerbose)
        qDebug() << "removeSchema" << schemaName;

    if (mSchemas.contains(schemaName)) {
        QsonMap schema = mSchemas.take(schemaName);

        if (schema.valueString("extends") == JsonDbString::kViewTypeStr)
            mViewTypes.remove(schemaName);
    }
}

QsonMap JsonDb::validateSchema(const QString &schemaName, QsonMap object)
{
    if (!gValidateSchemas) {
        DBG() << "Not validating schemas";
        return QsonMap();
    }

    return mSchemas.validate(schemaName, object);
}

QsonMap JsonDb::validateMapObject(QsonMap map)
{
    QString targetType = map.valueString("targetType");

    if (map.valueBool(JsonDbString::kDeletedStr, false))
        return QsonMap();
    if (targetType.isEmpty())
        return makeError(JsonDbError::InvalidMap, "targetType property for Map not specified");
    if (!mViewTypes.contains(targetType))
        return makeError(JsonDbError::InvalidMap, "targetType must be of a type that extends View");
    if (map.contains("join")) {
        QsonMap sourceFunctions = map.subObject("join").toMap();
        if (sourceFunctions.isEmpty())
            return makeError(JsonDbError::InvalidMap, "sourceTypes and functions for Map with join not specified");
        QStringList sourceTypes = sourceFunctions.keys();
        for (int i = 0; i < sourceTypes.size(); i++)
            if (sourceFunctions.valueString(sourceTypes[i]).isEmpty())
                return makeError(JsonDbError::InvalidMap,
                                 QString("join function for source type '%1' not specified for Map")
                                 .arg(sourceTypes[i]));
        if (map.contains("map"))
            return makeError(JsonDbError::InvalidMap, "Map 'join' and 'map' options are mutually exclusive");
        if (map.contains("sourceType"))
            return makeError(JsonDbError::InvalidMap, "Map 'join' and 'sourceType' options are mutually exclusive");

    } else {
        if (map.valueString("sourceType").isEmpty())
            return makeError(JsonDbError::InvalidMap, "sourceType property for Map not specified");
        if (map.valueString("map").isEmpty())
            return makeError(JsonDbError::InvalidMap, "map function for Map not specified");
    }

    return QsonMap();
}

QsonMap JsonDb::validateReduceObject(QsonMap reduce)
{
    QString targetType = reduce.valueString("targetType");
    QString sourceType = reduce.valueString("sourceType");

    if (reduce.valueBool(JsonDbString::kDeletedStr, false))
        return QsonMap();
    if (targetType.isEmpty())
        return makeError(JsonDbError::InvalidReduce, "targetType property for Reduce not specified");
    if (!mViewTypes.contains(targetType))
        return makeError(JsonDbError::InvalidReduce, "targetType must be of a type that extends View");
    if (sourceType.isEmpty())
        return makeError(JsonDbError::InvalidReduce, "sourceType property for Reduce not specified");
    if (reduce.valueString("sourceKeyName").isEmpty())
        return makeError(JsonDbError::InvalidReduce, "sourceKeyName property for Reduce not specified");
    if (reduce.valueString("add").isEmpty())
        return makeError(JsonDbError::InvalidReduce, "add function for Reduce not specified");
    if (reduce.valueString("subtract").isEmpty())
        return makeError(JsonDbError::InvalidReduce, "subtract function for Reduce not specified");

    return QsonMap();
}

QsonMap JsonDb::checkUuidPresent(QsonMap object, QString &uuid)
{
    if (!object.isDocument())
        return makeError(JsonDbError::MissingUUID, "Missing '_uuid' field in object");
    uuid = object.valueString(JsonDbString::kUuidStr);
    return QsonMap();
}

QsonMap JsonDb::checkTypePresent(QsonMap object, QString &type)
{
    type = object.valueString(JsonDbString::kTypeStr);
    if (type.isEmpty()) {
        QString str = JsonWriter().toString(qsonToVariant(object));
        return makeError(JsonDbError::MissingType, QString("Missing '_type' field in object '%1'").arg(str));
    }
    return QsonMap();
}

QsonMap JsonDb::checkNaturalObjectType(QsonMap object, QString &type)
{
    type = object.valueString(JsonDbString::kTypeStr);
    if (mViewTypes.contains(type)) {
        QString str = JsonWriter().toString(qsonToVariant(object));
        return makeError(JsonDbError::MissingType, QString("Cannot create/remove object of view type '%1': '%2'").arg(type).arg(str));
    }
    return QsonMap();
}

QsonMap JsonDb::checkAccessControl(const JsonDbOwner *owner, QsonMap object,
                                   const QString &op)
{
    if (!owner->isAllowed(object, op))
        return makeError(JsonDbError::OperationNotPermitted, "Access denied");
    return QsonMap();
}

QsonMap JsonDb::checkQuota(const JsonDbOwner *owner, int size, JsonDbBtreeStorage *partition)
{
    if (!partition->checkQuota(owner, size))
        return makeError(JsonDbError::QuotaExceeded, "Quota exceeded.");
    return QsonMap();
}

QsonMap JsonDb::checkCanAddSchema(QsonMap schema, QsonMap oldSchema)
{
    if (schema.valueBool(JsonDbString::kDeletedStr, false))
        return QsonMap();
    if (!schema.contains("name")
            || !schema.contains("schema"))
        return makeError(JsonDbError::InvalidSchemaOperation,
                         "_schemaType objects must specify both name and schema properties");

    QString schemaName = schema.valueString("name");

    if (schemaName.isEmpty())
        return makeError(JsonDbError::InvalidSchemaOperation,
                         "name property of _schemaType object must be specified");

    if (mSchemas.contains(schemaName) && oldSchema.valueString("name") != schemaName)
        return makeError(JsonDbError::InvalidSchemaOperation,
                         QString("A schema with name %1 already exists").arg(schemaName));

    return QsonMap();
}

QsonMap JsonDb::checkCanRemoveSchema(QsonMap schema)
{
    QString schemaName = schema.valueString("name");

    // check if any objects exist
    QsonMap getObjectResponse = getObjects(JsonDbString::kTypeStr, schemaName);
    // for non-View types, if objects exist the schema cannot be removed
    if (!mViewTypes.contains(schemaName)) {
        if (getObjectResponse.valueInt("count") != 0)
            return makeError(JsonDbError::InvalidSchemaOperation,
                             QString("%1 object(s) of type %2 exist. You cannot remove the schema")
                             .arg(getObjectResponse.valueInt("count"))
                             .arg(schemaName));
    }

    // for View types, make sure no Maps or Reduces point at this type
    // the call to getObject will have updated the view, so pending Map or Reduce object removes will be forced
    if (mMapDefinitionsByTarget.contains(schemaName)) {
      return makeError(JsonDbError::InvalidSchemaOperation,
                       QString("A Map object with targetType of %2 exists. You cannot remove the schema")
                       .arg(schemaName));
    }

    if (mReduceDefinitionsByTarget.contains(schemaName)) {
      return makeError(JsonDbError::InvalidSchemaOperation,
                       QString("A Reduce object with targetType of %2 exists. You cannot remove the schema")
                       .arg(schemaName));
    }

    return QsonMap();
}



QsonMap JsonDb::validateAddIndex(const QsonMap &newIndex, const QsonMap &oldIndex) const
{
    if (!newIndex.isEmpty() && !oldIndex.isEmpty()) {
        if (oldIndex.valueString(kFieldStr) != newIndex.valueString(kFieldStr))
            return makeError(JsonDbError::InvalidIndexOperation,
                             QString("Changing old index field name %1 to %2 not supported")
                             .arg(oldIndex.valueString(kFieldStr))
                             .arg(newIndex.valueString(kFieldStr)));
        if (oldIndex.valueString(kFieldTypeStr) != newIndex.valueString(kFieldTypeStr))
            return makeError(JsonDbError::InvalidIndexOperation,
                             QString("Changing old index field type %1 to %2 not supported")
                             .arg(oldIndex.valueString(kFieldTypeStr))
                             .arg(newIndex.valueString(kFieldTypeStr)));
        if (oldIndex.valueString(kObjectTypeStr) != newIndex.valueString(kObjectTypeStr))
            return makeError(JsonDbError::InvalidIndexOperation,
                             QString("Changing old index object type %1 to %2 not supported")
                             .arg(oldIndex.valueString(kObjectTypeStr))
                             .arg(newIndex.valueString(kObjectTypeStr)));
    }

    if (!newIndex.contains(kFieldStr))
        return makeError(JsonDbError::InvalidIndexOperation,
                         QString("Index object must have field name"));

    return QsonMap();
}

const Notification *JsonDb::createNotification(const JsonDbOwner *owner, QsonMap object)
{
    QString        uuid = object.valueString(JsonDbString::kUuidStr);
    QStringList actions = object.value<QsonList>(JsonDbString::kActionsStr).toStringList();
    QString       query = object.valueString(JsonDbString::kQueryStr);
    QsonMap bindings = object.subObject("bindings");

    Notification *n = new Notification(owner, uuid, query, actions);
    JsonDbQuery parsedQuery = JsonDbQuery::parse(query, bindings);
    const QList<OrQueryTerm> &orQueryTerms = parsedQuery.queryTerms;
    n->setCompiledQuery(new JsonDbQuery(parsedQuery.queryTerms, parsedQuery.orderTerms));

    bool generic = true;
    for (int i = 0; i < orQueryTerms.size(); i++) {
        const OrQueryTerm &orQueryTerm = orQueryTerms[i];
        const QList<QueryTerm> &terms = orQueryTerm.terms();
        if (terms.size() == 1) {
            const QueryTerm &term = terms[0];
            if (term.op() == "=") {
                if (term.fieldName() == JsonDbString::kUuidStr) {
                    mKeyedNotifications.insert(term.value().toString(), n);
                    generic = false;
                    break;
                } else if (term.fieldName() == JsonDbString::kTypeStr) {
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
        Notification *n = mNotificationMap.value(uuid);
        mNotificationMap.remove(uuid);
        const JsonDbQuery *parsedQuery = n->parsedQuery();
        const QList<OrQueryTerm> &orQueryTerms = parsedQuery->queryTerms;
        for (int i = 0; i < orQueryTerms.size(); i++) {
            const OrQueryTerm &orQueryTerm = orQueryTerms[i];
            const QList<QueryTerm> &terms = orQueryTerm.terms();
            if (terms.size() == 1) {
                const QueryTerm &term = terms[0];
                if (term.op() == "=") {
                    if (term.fieldName() == JsonDbString::kTypeStr) {
                        mKeyedNotifications.remove(term.value().toString(), n);
                    } else if (term.fieldName() == JsonDbString::kUuidStr) {
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
    for (QMap<QString,JsonDbMapDefinition*>::const_iterator it = mMapDefinitionsByTarget.find(objectType);
         (it != mMapDefinitionsByTarget.end() && it.key() == objectType);
         ++it) {
        JsonDbMapDefinition *def = it.value();
        const QStringList &sourceTypes = def->sourceTypes();
        for (int i = 0; i < sourceTypes.size(); i++) {
            const QString &sourceType = sourceTypes[i];
            QSet<QString> &targetTypes = mEagerViewSourceTypes[sourceType];
            targetTypes.insert(objectType);
            // now recurse until we get to a non-view sourceType
            updateEagerViewTypes(sourceType);
        }
    }
    for (QMap<QString,JsonDbReduceDefinition*>::const_iterator it = mReduceDefinitionsByTarget.find(objectType);
         (it != mReduceDefinitionsByTarget.end() && it.key() == objectType);
         ++it) {
        JsonDbReduceDefinition *def = it.value();
        QString sourceType = def->sourceType();
        QSet<QString> &targetTypes = mEagerViewSourceTypes[sourceType];
        targetTypes.insert(objectType);
        // now recurse until we get to a non-view sourceType
        updateEagerViewTypes(sourceType);
    }
}

JsonDbBtreeStorage *JsonDb::findPartition(const QString &name) const
{
    if (name.isEmpty())
        return mStorages.value(JsonDbString::kSystemPartitionName, 0);
    return mStorages.value(name, 0);
}

QsonMap JsonDb::createPartition(const QsonMap &object)
{
    QString name = object.value<QString>("name");
    if (mStorages.contains(name)) {
        QsonMap resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Already have a partition with a given name '%1'").arg(name));
        return makeResponse(resultmap, errormap);
    }
    // verify partition name - only alphanum characters allowed
    bool isAlNum = true;
    for (int i = 0; i < name.length(); ++i) {
        if (!name.at(i).isLetterOrNumber()) {
            isAlNum = false;
            break;
        }
    }
    if (!isAlNum) {
        QsonMap resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Partition name can only be alphanumeric '%1'").arg(name));
        return makeResponse(resultmap, errormap);
    }

    QString filename = mFilePath + name + QLatin1String(".db");

    QsonMap partition;
    partition.insert(JsonDbString::kTypeStr, JsonDbString::kPartitionStr);
    partition.insert(QLatin1String("name"), name);
    partition.insert(QLatin1String("file"), filename);
    mStorages[JsonDbString::kSystemPartitionName]->createPersistentObject(partition);

    JsonDbBtreeStorage *storage = new JsonDbBtreeStorage(filename, name, this);
    if (gVerbose) qDebug() << "Opening partition" << name;

    if (!storage->open()) {
        qWarning() << "Failed to initialize partition" << name << "at" << filename;
        QsonMap resultmap, errormap;
        setError(errormap, JsonDbError::InvalidPartition, QString("Cannot initialize a partition '%1'").arg(name));
        return makeResponse(resultmap, errormap);
    }
    mStorages.insert(name, storage);
    initMap(name);

    QsonMap resultmap, errormap;
    return makeResponse(resultmap, errormap);
}

} } // end namespace QtAddOn::JsonDb
