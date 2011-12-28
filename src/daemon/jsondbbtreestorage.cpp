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

#include "jsondb-trace.h"

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <QString>
#include <QElapsedTimer>
#include <QUuid>
#include <QtAlgorithms>
#include <QtEndian>
#include <QStringBuilder>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <QtJsonDbQson/private/qson_p.h>
#include <QtJsonDbQson/private/qsonparser_p.h>

#include "jsondb.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"
#include "aodb.h"
#include "jsondbbtreestorage.h"
#include "jsondbindex.h"
#include "objecttable.h"

QT_ADDON_JSONDB_BEGIN_NAMESPACE

//#define QT_NO_DEBUG_OUTPUT
#ifndef QT_NO_DEBUG_OUTPUT
extern bool gDebug;
#define DBG() if (gDebug) qDebug() << Q_FUNC_INFO << __LINE__
#else
#define DBG() if (0) qDebug() << Q_FUNC_INFO
#endif
bool gVerboseCheckValidity = false;

const QString kDbidTypeStr("DatabaseId");
const QString kIndexTypeStr("Index");
const QString kPropertyNameStr("propertyName");
const QString kPropertyTypeStr("propertyType");
const QString kNameStr("name");
const QString kObjectTypeStr("objectType");
const QString kDatabaseSchemaVersionStr("databaseSchemaVersion");
const QString kPropertyFunctionStr("propertyFunction");
const QString gDatabaseSchemaVersion = "0.2";

int forwardKeyCmp(const char *aptr, size_t asiz, const char *bptr, size_t bsiz, void *op);

JsonDbBtreeStorage::JsonDbBtreeStorage(const QString &filename, const QString &name, JsonDb *jsonDb)
    : QObject(jsonDb)
    , mJsonDb(jsonDb)
    , mObjectTable(0)
    , mBdbIndexes(0)
    , mPartitionName(name)
    , mFilename(filename)
    , mTransactionDepth(0)
    , mWildCardPrefixRegExp("([^*?\\[\\]\\\\]+).*")
{
}

JsonDbBtreeStorage::~JsonDbBtreeStorage()
{
    if (mTransactionDepth) {
        qCritical() << "JsonDbBtreeStorage::~JsonDbBtreeStorage"
                    << "closing while transaction open" << "mTransactionDepth" << mTransactionDepth;
    }
    close();
}

bool JsonDbBtreeStorage::close()
{
    foreach (ObjectTable *table, mViews.values()) {
        delete table;
    }
    mViews.clear();

    delete mObjectTable;
    mObjectTable = 0;

    delete mBdbIndexes;
    mBdbIndexes = 0;

    return true;
}

bool JsonDbBtreeStorage::open()
{
    DBG() << "JsonDbBtree::open" << mPartitionName << mFilename;

    QFileInfo fi(mFilename);

    mObjectTable = new ObjectTable(this);
    mBdbIndexes = new AoDb();

    mObjectTable->open(mFilename, AoDb::NoSync | AoDb::UseSyncMarker);

#if 0
    if (!mBdbIndexes->setCmpFunc(objectKeyCmp, 0)) {
        qCritical() << "mBdbIndexes->setCmpFunc" << mBdb->errorMessage();
        return false;
    }
#endif

    QString dir = fi.dir().path();
    QString basename = fi.fileName();
    if (basename.endsWith(".db"))
        basename.chop(3);
    if (!mBdbIndexes->open(QString("%1/%2-Indexes.db").arg(dir).arg(basename),
                           AoDb::NoSync | AoDb::UseSyncMarker)) {
        qCritical() << "mBdbIndexes->open" << mBdbIndexes->errorMessage();
        mBdbIndexes->close();
        return false;
    }

    if (!checkStateConsistency()) {
        qCritical() << "JsonDbBtreeStorage::open()" << "Unable to recover database";
        return false;
    }

    bool rebuildingDatabaseMetadata = false;

    QString partitionId;
    if (mBdbIndexes->count()) {
        QByteArray baKey, baValue;
        AoDbCursor cursor(mBdbIndexes);
        if (cursor.first()) {
            cursor.current(baKey, baValue);
            if (baValue.size() > 4) {
                QsonMap object = QsonParser::fromRawData(baValue).toMap();
                if (object.valueString(JsonDbString::kTypeStr) != kDbidTypeStr) {
                    qCritical() << __FUNCTION__ << __LINE__ << "no dbid in indexes table";
                } else {
                    partitionId = object.valueString("id");
                    QString partitionName = object.value<QString>(QLatin1String("name"));
                    if (partitionName != mPartitionName || !object.contains(kDatabaseSchemaVersionStr)
                        || object.valueString(kDatabaseSchemaVersionStr) != gDatabaseSchemaVersion) {
                        if (gVerbose) qDebug() << "Rebuilding database metadata";
                        rebuildingDatabaseMetadata = true;
                    }
                }
            }
        }
    }
    if (rebuildingDatabaseMetadata) {
        mBdbIndexes->clearData();
    }
    if (partitionId.isEmpty() || rebuildingDatabaseMetadata) {
        if (partitionId.isEmpty())
            partitionId = QUuid::createUuid().toString();

        QByteArray baKey(4, 0);
        qToBigEndian(0, (uchar *)baKey.data());

        QsonMap object;
        object.insert(JsonDbString::kTypeStr, kDbidTypeStr);
        object.insert(QLatin1String("id"), partitionId);
        object.insert(QLatin1String("name"), mPartitionName);
        object.insert(kDatabaseSchemaVersionStr, gDatabaseSchemaVersion);
        QByteArray baObject = object.data();
        bool ok = mBdbIndexes->put(baKey, baObject);
        Q_ASSERT(ok);
    }
    if (gVerbose) qDebug() << "partition" << mPartitionName << "id" << partitionId;

    initIndexes();

    return true;
}

bool JsonDbBtreeStorage::clear()
{
    if (mObjectTable->bdb()) {
        qCritical() << "Cannot clear database while it is open.";
        return false;
    }
    QStringList filters;
    QFileInfo fi(mFilename);
    filters << QString::fromLatin1("%1*.db").arg(fi.baseName());
    QDir dir(fi.absolutePath());
    QStringList lst = dir.entryList(filters);
    foreach (const QString &fileName, lst) {
        if (gVerbose)
            qDebug() << "removing" << fileName;
        if (!dir.remove(fileName)) {
            qCritical() << "Failed to remove" << fileName;
            return false;
        }
    }
    return true;
}


enum FieldValueKind {
    FvkVoid,
    FvkString,
    FvkInt,
    FvkDouble,

    FvkLastKind = FvkDouble
};

inline FieldValueKind qVariantFieldValueKind(const QVariant &v)
{
    if (!v.isValid())
        return FvkVoid;
    if (v.type() == QVariant::String) // this test here because String canConvert to Int
        return FvkString;
    else if (v.canConvert(QVariant::Int))
        return FvkInt;
    else if (v.canConvert(QVariant::Double))
        return FvkDouble;
    else
        return FvkString;
}

inline quint16 fieldValueSize(FieldValueKind fvk, const QVariant &fieldValue)
{
    switch (fvk) {
    case FvkVoid:
        return 0;
    case FvkInt:
        return 4;
    case FvkDouble:
        return 8;
    case FvkString:
        return 2*fieldValue.toString().count();
    }
    return 0;
}

void memcpyFieldValue(char *data, FieldValueKind fvk, const QVariant &fieldValue)
{
    switch (fvk) {
    case FvkVoid:
        break;
    case FvkInt: {
        qint32 value = fieldValue.toInt();
        qToBigEndian(value, (uchar *)data);
    } break;
    case FvkDouble: {
        double value = fieldValue.toDouble();
        memcpy(data, &value, 8);
    } break;
    case FvkString: {
        QString str = fieldValue.toString();
        memcpy(data, (const char *)str.constData(), 2*str.count());
    }
    }
}

void memcpyFieldValue(FieldValueKind fvk, QVariant &fieldValue, const char *data, quint16 size)
{
    switch (fvk) {
    case FvkVoid:
        break;
    case FvkInt: {
        fieldValue = qFromBigEndian<qint32>((const uchar *)data);
    } break;
    case FvkDouble: {
        double value;
        memcpy(&value, data, size);
        fieldValue = value;
    } break;
    case FvkString: {
        fieldValue = QString((const QChar *)data, size/2);
    }
    }
}

int objectKeyCmp(const char *aptr, size_t alen, const char *bptr, size_t blen, void *)
{
    Q_UNUSED(alen);
    Q_UNUSED(blen);
    quint32 a = qFromBigEndian<quint32>((const uchar *)aptr);
    quint32 b = qFromBigEndian<quint32>((const uchar *)bptr);
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

int intcmp(const uchar *aptr, const uchar *bptr)
{
    qint32 a = qFromBigEndian<qint32>((const uchar *)aptr);
    qint32 b = qFromBigEndian<qint32>((const uchar *)bptr);
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}
int doublecmp(const uchar *aptr, const uchar *bptr)
{
    double a, b;
    memcpy(&a, aptr, 8);
    memcpy(&b, bptr, 8);
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}
int qstringcmp(const quint16 *achar, quint32 acount, const quint16 *bchar, quint32 bcount)
{
    int rv = 0;
    quint32 minCount = qMin(acount, bcount);
    for (quint32 i = 0; i < minCount; i++) {
        if ((rv = (achar[i] - bchar[i])) != 0)
            return rv;
    }
    return acount-bcount;
}

int fvkCmp(FieldValueKind afvk, const char *aData, quint16 aSize,
           FieldValueKind bfvk, const char *bData, quint16 bSize)
{
    if (afvk != bfvk)
        return afvk - bfvk;
    switch (afvk) {
    case FvkInt:
        return intcmp((const uchar *)aData, (const uchar *)bData);
    case FvkDouble:
        return doublecmp((const uchar *)aData, (const uchar *)bData);
    case FvkString:
        return qstringcmp((const quint16 *)aData, aSize/2, (const quint16 *)bData, bSize/2);
        break;
    case FvkVoid:
        return 0;
    }
    return 0;
}

QByteArray makeForwardKey(const QVariant &fieldValue, const ObjectKey &objectKey)
{
    FieldValueKind fvk = qVariantFieldValueKind(fieldValue);
    Q_ASSERT(fvk <= FvkLastKind);
    quint32 size = fieldValueSize(fvk, fieldValue);

    QByteArray forwardKey(4+size+16, 0);
    char *data = forwardKey.data();
    qToBigEndian<quint32>(fvk, (uchar *)&data[0]);
    memcpyFieldValue(data+4, fvk, fieldValue);
    qToBigEndian(objectKey, (uchar *)&data[4+size]);

    return forwardKey;
}
void forwardKeySplit(const QByteArray &forwardKey, QVariant &fieldValue)
{
    const char *data = forwardKey.constData();
    FieldValueKind fvk = (FieldValueKind)qFromBigEndian<quint32>((const uchar *)&data[0]);
    Q_ASSERT(fvk <= FvkLastKind);
    quint32 fvSize = forwardKey.size()-4-16;
    memcpyFieldValue(fvk, fieldValue, data+4, fvSize);
}
void forwardKeySplit(const QByteArray &forwardKey, QVariant &fieldValue, ObjectKey &objectKey)
{
    const char *data = forwardKey.constData();
    FieldValueKind fvk = (FieldValueKind)qFromBigEndian<quint32>((const uchar *)&data[0]);
    Q_ASSERT(fvk <= FvkLastKind);
    quint32 fvSize = forwardKey.size()-4-16;
    memcpyFieldValue(fvk, fieldValue, data+4, fvSize);
    objectKey = qFromBigEndian<ObjectKey>((const uchar *)&data[4+fvSize]);
}
int forwardKeyCmp(const char *aptr, size_t asiz, const char *bptr, size_t bsiz, void *)
{
    int rv = 0;
    FieldValueKind afvk = (FieldValueKind)qFromBigEndian<quint32>((const uchar *)&aptr[0]);
    FieldValueKind bfvk = (FieldValueKind)qFromBigEndian<quint32>((const uchar *)&bptr[0]);
    if (afvk > FvkLastKind) {
        qDebug() << "forwardKeyCmp" << "afvk" << afvk;
    }
    if (bfvk > FvkLastKind) {
        qDebug() << "forwardKeyCmp" << "bfvk" << bfvk;
    }
    Q_ASSERT(afvk <= FvkLastKind);
    Q_ASSERT(bfvk <= FvkLastKind);
    quint32 asize = asiz - 4 - 16;
    quint32 bsize = bsiz - 4 - 16;
    const char *aData = aptr + 4;
    const char *bData = bptr + 4;
    rv = fvkCmp(afvk, aData, asize, bfvk, bData, bsize);
    if (rv != 0)
        return rv;
    ObjectKey aObjectKey = qFromBigEndian<ObjectKey>((const uchar *)aptr+4+asize);
    ObjectKey bObjectKey = qFromBigEndian<ObjectKey>((const uchar *)bptr+4+bsize);
    if (aObjectKey == bObjectKey)
        return 0;
    return aObjectKey < bObjectKey ? -1 : 1;
}

QByteArray makeForwardValue(const ObjectKey &objectKey)
{
    QByteArray forwardValue(16, 0);
    char *data = forwardValue.data();
    qToBigEndian(objectKey,  (uchar *)&data[0]);
    return forwardValue;
}
void forwardValueSplit(const QByteArray &forwardValue, ObjectKey &objectKey)
{
    const uchar *data = (const uchar *)forwardValue.constData();
    objectKey = qFromBigEndian<ObjectKey>(&data[0]);
}

QsonMap JsonDbBtreeStorage::createPersistentObject(QsonMap &object)
{
    QsonMap resultmap, errormap;

    if (!object.isDocument()) {
        object.generateUuid();
        object.computeVersion();
    }

    QString uuid = object.valueString(JsonDbString::kUuidStr);
    QString version = object.valueString(JsonDbString::kVersionStr);
    QString objectType = object.valueString(JsonDbString::kTypeStr);

    ObjectKey objectKey(object.uuid());
    ObjectTable *table = findObjectTable(objectType);

    bool ok = table->put(objectKey, object);
    if (!ok) {
        return JsonDb::makeErrorResponse(resultmap, JsonDbError::DatabaseError,
                                         table->errorMessage());
    }

    table->storeStateChange(objectKey, ObjectChange::Created);
    table->indexObject(objectKey, object, table->stateNumber());

    resultmap.insert(JsonDbString::kUuidStr, uuid);
    resultmap.insert(JsonDbString::kVersionStr, version);
    resultmap.insert(JsonDbString::kCountStr, 1);

    return JsonDb::makeResponse( resultmap, errormap );
}

void JsonDbBtreeStorage::addView(const QString &viewType)
{
    if (!mViews.contains(viewType)) {
        ObjectTable *viewObjectTable = new ObjectTable(this);
        QFileInfo fi(mFilename);
        QString dirName = fi.dir().path();
        QString baseName = fi.fileName();
        baseName.replace(".db", "");
        if (!viewObjectTable->open(QString("%1/%2-%3-View.db")
                                   .arg(dirName)
                                   .arg(baseName)
                                   .arg(viewType),
                                   AoDb::NoSync | AoDb::UseSyncMarker)) {
            qCritical() << "viewDb->open" << viewObjectTable->errorMessage();
            return;
        }
        mViews.insert(viewType, viewObjectTable);
        viewObjectTable->addIndex(JsonDbString::kUuidStr, "string", viewType);
        // TODO: special case for the following
        viewObjectTable->addIndex(JsonDbString::kTypeStr, "string", viewType);
    }
}

void JsonDbBtreeStorage::updateView(const QString &objectType)
{
    if (!mViews.contains(objectType))
        return;
    // TODO partition name
    mJsonDb->updateView(objectType);
}

void JsonDbBtreeStorage::updateView(ObjectTable *objectTable)
{
    QString targetType = mViews.key(objectTable, QString());
    // TODO partition name
    if (!targetType.isEmpty())
        mJsonDb->updateView(targetType);
}

ObjectTable *JsonDbBtreeStorage::findObjectTable(const QString &objectType) const
{
        if (mViews.contains(objectType))
            return mViews.value(objectType);
        else
            return mObjectTable;
}

bool JsonDbBtreeStorage::beginTransaction()
{
    if (mTransactionDepth++ == 0) {
        if (!mBdbIndexes->begin()) {
            qCritical() << __FILE__ << __LINE__ << mBdbIndexes->errorMessage();
            mTransactionDepth--;
            return false;
        }
        Q_ASSERT(mTableTransactions.isEmpty());
    }
    return true;
}

bool JsonDbBtreeStorage::commitTransaction(quint32 stateNumber)
{
    if (--mTransactionDepth == 0) {
        bool ret = true;
        quint32 nextStateNumber = stateNumber ? stateNumber : (mObjectTable->stateNumber() + 1);
        if (gDebug) qDebug() << "commitTransaction" << stateNumber;
        if (!stateNumber && (mTableTransactions.size() == 1))
            nextStateNumber = mTableTransactions.at(0)->stateNumber() + 1;

        if (!mBdbIndexes->commit(nextStateNumber)) {
            qCritical() << __FILE__ << __LINE__ << mBdbIndexes->errorMessage();
            ret = false;
        }

        for (int i = 0; i < mTableTransactions.size(); i++) {
            ObjectTable *table = mTableTransactions.at(i);
            if (!table->commit(nextStateNumber)) {
                qCritical() << __FILE__ << __LINE__ << "Failed to commit transaction on object table";
                ret = false;
            }
        }
        mTableTransactions.clear();
        return ret;
    }
    return true;
}

bool JsonDbBtreeStorage::abortTransaction()
{
    if (--mTransactionDepth == 0) {
        if (gVerbose) qDebug() << "JsonDbBtreeStorage::abortTransaction()";
        bool ret = true;

        if (!mBdbIndexes->abort()) {
            qCritical() << __FILE__ << __LINE__ << mBdbIndexes->errorMessage();
            ret = false;
        }

        for (int i = 0; i < mTableTransactions.size(); i++) {
            ObjectTable *table = mTableTransactions.at(i);
            if (!table->abort()) {
                qCritical() << __FILE__ << __LINE__ << "Failed to abort transaction";
                ret = false;
            }
        }
        mTableTransactions.clear();
        return ret;
    }
    return true;
}

QsonMap JsonDbBtreeStorage::updatePersistentObject(QsonMap &object)
{
    QsonMap resultmap, errormap;

    QString uuid = object.valueString(JsonDbString::kUuidStr);
    QString version = object.valueString(JsonDbString::kVersionStr);
    QString objectType = object.valueString(JsonDbString::kTypeStr);
    ObjectTable *table = findObjectTable(objectType);
    ObjectKey objectKey(uuid);

    QsonMap oldObject;
    bool exists = table->get(objectKey, oldObject);

    objectKey = ObjectKey(object.uuid());

    if (exists) {
        table->deindexObject(objectKey, oldObject, table->stateNumber());
    }

    if (!table->put(objectKey, object)) {
        return JsonDb::makeErrorResponse(resultmap, JsonDbError::DatabaseError,
                                         table->errorMessage());
    }

    if (gDebug) qDebug() << "updateObject" << objectKey << endl << object << endl << oldObject;

    if (exists) {
        table->storeStateChange(objectKey, ObjectChange::Updated, oldObject);
    } else {
        table->storeStateChange(objectKey, ObjectChange::Created);
    }

    table->indexObject(objectKey, object, table->stateNumber());

    resultmap.insert( JsonDbString::kCountStr, 1 );
    resultmap.insert( JsonDbString::kUuidStr, uuid );
    resultmap.insert( JsonDbString::kVersionStr, version );

    return JsonDb::makeResponse( resultmap, errormap );
}

QsonMap JsonDbBtreeStorage::removePersistentObject(QsonMap object, const QsonMap &ts)
{
    if (gDebug) qDebug() << "removePersistentObject" << object;

    QsonMap resultmap, errormap;
    QString uuid = object.valueString(JsonDbString::kUuidStr);
    QString objectType = object.valueString(JsonDbString::kTypeStr);

    ObjectTable *table = findObjectTable(objectType);

    ObjectKey objectKey(uuid);

    QsonMap oldObject;
    bool ok = table->get(objectKey, oldObject);
    if (!ok) {
        qDebug() << "Failed to get object" << objectKey << table->errorMessage();
        JsonDb::setError( errormap, JsonDbError::InvalidRequest, QString("No object with _uuid %1 in database").arg(uuid));
        QsonList errors;
        errors.append(errormap);
        resultmap.insert(JsonDbString::kCountStr, 0);
        resultmap.insert(JsonDbString::kDataStr, QsonObject::NullValue);
        resultmap.insert(JsonDbString::kErrorStr, errors);
        return JsonDb::makeResponse(resultmap, errormap);
    }
    QString version = oldObject.valueString(JsonDbString::kVersionStr);

    QsonMap tombstone(ts);
    if (!tombstone.valueBool(JsonDbString::kDeletedStr, false))
        tombstone.insert(JsonDbString::kDeletedStr, true);
    tombstone.insert(JsonDbString::kUuidStr, uuid);
    tombstone.insert(JsonDbString::kVersionStr, version);

    ok = table->put(objectKey, tombstone);
    if (!ok) {
        return JsonDb::makeErrorResponse(resultmap,
                                         JsonDbError::DatabaseError,
                                         table->errorMessage());
    }

    table->storeStateChange(objectKey, ObjectChange::Deleted, oldObject);
    table->deindexObject(objectKey, oldObject, table->stateNumber());

    QsonMap item;
    item.insert(JsonDbString::kUuidStr, uuid);

    QsonList data;
    data.append(item);
    resultmap.insert(JsonDbString::kCountStr, 1);
    resultmap.insert(JsonDbString::kDataStr, data);
    resultmap.insert(JsonDbString::kErrorStr, QsonObject::NullValue);
    QsonMap result = JsonDb::makeResponse( resultmap, errormap );
    return result;
}

bool JsonDbBtreeStorage::getObject(const QString &uuid, QsonMap &object, const QString &objectType) const
{
    ObjectKey objectKey(uuid);
    return getObject(objectKey, object, objectType);
}

bool JsonDbBtreeStorage::getObject(const ObjectKey &objectKey, QsonMap &object, const QString &objectType) const
{
    ObjectTable *table = findObjectTable(objectType);

    bool ok = table->get(objectKey, object);
    if (ok)
        return ok;
    QHash<QString,QPointer<ObjectTable> >::const_iterator it = mViews.begin();
    for (; it != mViews.end(); ++it) {
        bool ok = it.value()->get(objectKey, object);
        if (ok)
            return ok;
    }
    return false;
}

QsonMap JsonDbBtreeStorage::getObjects(const QString &keyName, const QVariant &keyValue, const QString &_objectType)
{
    QsonMap resultmap;
    QsonList objectList;
    const QString &objectType = (keyName == JsonDbString::kTypeStr) ? keyValue.toString() : _objectType;
    bool typeSpecified = !objectType.isEmpty();

    ObjectTable *table = findObjectTable(objectType);

    updateView(objectType);

    if (keyName == JsonDbString::kUuidStr) {
        ObjectKey objectKey(keyValue.toString());
        QsonList objectList;
        QsonMap object;
        bool ok = table->get(objectKey, object);
        if (ok)
            objectList.append(object);
        resultmap.insert(QLatin1String("result"), objectList);
        resultmap.insert(JsonDbString::kCountStr, objectList.size());
        return resultmap;
    }

    const IndexSpec *indexSpec = table->indexSpec(keyName);
    if (!indexSpec) {
        qDebug() << "getObject" << "no index for" << objectType << keyName;
        resultmap.insert(JsonDbString::kCountStr, 0);
        return resultmap;
    }

    if (indexSpec->lazy)
        updateIndex(table, indexSpec->index);
    JsonDbIndexCursor cursor(indexSpec->index);
    if (cursor.seekRange(keyValue)) {
        do {
            QVariant key;
            ObjectKey objectKey;
            if (!cursor.current(key, objectKey))
                continue;
            if (key != keyValue)
                break;

            QsonMap map;
            if (table->get(objectKey, map)) {
                if (map.contains(JsonDbString::kDeletedStr) && map.valueBool(JsonDbString::kDeletedStr, false))
                    continue;
                if (typeSpecified && (map.valueString(JsonDbString::kTypeStr) != objectType))
                    continue;

                objectList.append(map);
            } else {
              DBG() << "Failed to get object" << objectKey << table->errorMessage();
            }
        } while (cursor.next());
    }
    resultmap.insert(QLatin1String("result"), objectList);
    resultmap.insert(JsonDbString::kCountStr, objectList.size());
    return resultmap;
}

QsonMap JsonDbBtreeStorage::changesSince(quint32 stateNumber, const QSet<QString> &limitTypes)
{
    return mObjectTable->changesSince(stateNumber, limitTypes);
}

bool JsonDbBtreeStorage::checkValidity()
{
    bool noErrors = true;
#if 0
    if (gVerboseCheckValidity) qDebug() << "JsonDbBtreeStorage::checkValidity {";
    QMap<quint32, QString> keyUuids; // objectKey -> uuid
    QMap<QString, quint32> uuidKeys; // objectKey -> uuid
    QMap<QString, QsonObject> objects; // uuid -> object

    AoDbCursor cursor(mObjectTable->bdb());
    for (bool ok = cursor.first(); ok; ok = cursor.next()) {
        QByteArray baKey, baValue;
        if (!cursor.current(baKey, baValue))
            continue;
        if (baValue.size() == 0)
            continue;
        quint32 objectKey = qFromBigEndian<quint32>((const uchar *)baKey.data());
        if (baValue.size() == 4)
            continue;
        QsonMap object = QsonParser::fromRawData(baValue).toMap();
        QString uuid = object.valueString(JsonDbString::kUuidStr);
        QString typeName = object.valueString(JsonDbString::kTypeStr);
        if (uuidKeys.contains(uuid)) {
            quint32 previousKey = uuidKeys.value(uuid);
            keyUuids.remove(previousKey);
        }
        if (deleted) {
            objects.remove(uuid);
            uuidKeys.remove(uuid);
        } else {
            objects.insert(uuid, object);
            uuidKeys.insert(uuid, objectKey);
            keyUuids.insert(objectKey, uuid);
        }

        if (gVerboseCheckValidity || gDebug) {
            qDebug() << objectKey << uuid << object.valueString(JsonDbString::kTypeStr);
            qDebug() << object;
        }
        //Q_ASSERT(objectKey > lastObjectKey);
    }
    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        const IndexSpec &indexSpec = it.value();
        if (!indexSpec.index.isNull()) {
            if (!indexSpec.index->checkValidity(objects, keyUuids, uuidKeys, this))
                noErrors = false;
        }
    }
    if (gVerboseCheckValidity) qDebug() << "} JsonDbBtreeStorage::checkValidity done";
#endif
    return noErrors;
}

void JsonDbBtreeStorage::initIndexes()
{
    QByteArray baPropertyName;
    QByteArray baIndexObject;

    AoDbCursor cursor(mBdbIndexes);
    for (bool ok = cursor.first(); ok; ok = cursor.next()) {
        if (!cursor.current(baPropertyName, baIndexObject))
            break;

        if (baIndexObject.size() == 4)
            continue;
        QsonObject object = QsonParser::fromRawData(baIndexObject);
        if (object.isNull())
            continue;
        QsonMap indexObject = object.toMap();
        if (gVerbose) qDebug() << "initIndexes" << "index" << indexObject;
        QString indexObjectType = indexObject.valueString(JsonDbString::kTypeStr);
        if (indexObjectType == kIndexTypeStr) {
            QString propertyName = indexObject.valueString(kPropertyNameStr);
            QString propertyType = indexObject.valueString(kPropertyTypeStr);
            QString objectType = indexObject.valueString(kObjectTypeStr);
            QString propertyFunction = indexObject.valueString(kPropertyFunctionStr);
            QStringList path = propertyName.split('.');

            ObjectTable *table = findObjectTable(objectType);
            table->addIndex(propertyName, propertyType, objectType, propertyFunction);
            //checkIndexConsistency(index);
        }
    }


    beginTransaction();
    mObjectTable->addIndex(JsonDbString::kUuidStr, "string", QString());
    mObjectTable->addIndex(JsonDbString::kTypeStr, "string", QString());
    commitTransaction();
}

bool JsonDbBtreeStorage::addIndex(const QString &propertyName, const QString &propertyType,
                                  const QString &objectType, const QString &propertyFunction)
{
    //qDebug() << "JsonDbBtreeStorage::addIndex" << propertyName << objectType;
    ObjectTable *table = findObjectTable(objectType);
    const IndexSpec *indexSpec = table->indexSpec(propertyName);
    if (indexSpec)
        return true;
    //if (gVerbose) qDebug() << "JsonDbBtreeStorage::addIndex" << propertyName << objectType;
    return table->addIndex(propertyName, propertyType, objectType, propertyFunction);
}

bool JsonDbBtreeStorage::removeIndex(const QString &propertyName, const QString &objectType)
{
    ObjectTable *table = findObjectTable(objectType);
    const IndexSpec *indexSpec = table->indexSpec(propertyName);
    if (!indexSpec)
        return false;
    return table->removeIndex(propertyName);
}

bool JsonDbBtreeStorage::checkStateConsistency()
{
    return true;
}

void JsonDbBtreeStorage::checkIndexConsistency(ObjectTable *objectTable, JsonDbIndex *index)
{
    quint32 indexStateNumber = index->bdb()->tag();
    quint32 objectStateNumber = objectTable->stateNumber();
    if (indexStateNumber > objectTable->stateNumber()) {
        qCritical() << "reverting index" << index->propertyName() << indexStateNumber << objectStateNumber;
        while (indexStateNumber > objectTable->stateNumber()) {
            int rc = index->bdb()->revert();
            quint32 newIndexStateNumber = index->bdb()->tag();
            if (newIndexStateNumber == indexStateNumber) {
                qDebug() << "failed to revert. clearing" << rc;
                index->bdb()->clearData();
                break;
            }
            qCritical() << "   reverted index to state" << indexStateNumber;
        }
    }
    if (indexStateNumber < objectTable->stateNumber())
        updateIndex(objectTable, index);
}

static bool sDebugQuery = (::getenv("JSONDB_DEBUG_QUERY") ? (QLatin1String(::getenv("JSONDB_DEBUG_QUERY")) == "true") : false);

IndexQuery *IndexQuery::indexQuery(JsonDbBtreeStorage *storage, ObjectTable *table, const QString &propertyName, const JsonDbOwner *owner, bool ascending)
{
    if (propertyName == JsonDbString::kUuidStr)
        return new UuidQuery(storage, table, propertyName, owner, ascending);
    else
        return new IndexQuery(storage, table, propertyName, owner, ascending);
}

UuidQuery::UuidQuery(JsonDbBtreeStorage *storage, ObjectTable *table, const QString &propertyName, const JsonDbOwner *owner, bool ascending)
    : IndexQuery(storage, table, propertyName, owner, ascending)
{
}

IndexQuery::IndexQuery(JsonDbBtreeStorage *storage, ObjectTable *table, const QString &propertyName, const JsonDbOwner *owner, bool ascending)
    : mStorage(storage)
    , mObjectTable(table)
    , mBdbIndex(0)
    , mCursor(0)
    , mOwner(owner)
    , mAscending(ascending)
    , mPropertyName(propertyName)
    , mSparseMatchPossible(false)
    , mResidualQuery(0)
{
    if (propertyName != JsonDbString::kUuidStr) {
        mBdbIndex = table->indexSpec(propertyName)->index->bdb();
        mCursor = mBdbIndex->cursor();
    } else {
        mCursor = new AoDbCursor(table->bdb(), true);
    }
}
IndexQuery::~IndexQuery()
{
    if (mResidualQuery) {
        delete mResidualQuery;
        mResidualQuery = 0;
    }
    if (mCursor) {
        delete mCursor;
        mCursor = 0;
    }
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        delete mQueryConstraints[i];
    }
}

quint32 IndexQuery::stateNumber() const
{
    return mBdbIndex->tag();
}

bool IndexQuery::matches(const QVariant &fieldValue)
{
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        if (!mQueryConstraints[i]->matches(fieldValue))
            return false;
    }
    return true;
}

bool IndexQuery::seekToStart(QVariant &fieldValue)
{
    QByteArray forwardKey;
    if (mAscending) {
        forwardKey = makeForwardKey(mMin, ObjectKey());
        if (sDebugQuery) qDebug() << __FUNCTION__ << __LINE__ << "mMin" << mMin << "key" << forwardKey.toHex();
    } else {
        forwardKey = makeForwardKey(mMax, ObjectKey());
        if (sDebugQuery) qDebug() << __FUNCTION__ << __LINE__ << "mMax" << mMin << "key" << forwardKey.toHex();
    }

    bool ok = false;
    if (mAscending) {
        if (mMin.isValid()) {
            ok = mCursor->seekRange(forwardKey);
            if (sDebugQuery) qDebug() << "IndexQuery::first" << __LINE__ << "ok after seekRange" << ok;
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
        mCursor->currentKey(baKey);
        forwardKeySplit(baKey, fieldValue);
    }
    //qDebug() << "IndexQuery::seekToStart" << (mAscending ? mMin : mMax) << "ok" << ok << fieldValue;
    return ok;
}

bool IndexQuery::seekToNext(QVariant &fieldValue)
{
    bool ok = mAscending ? mCursor->next() : mCursor->prev();
    if (ok) {
        QByteArray baKey;
        mCursor->currentKey(baKey);
        forwardKeySplit(baKey, fieldValue);
    }
    //qDebug() << "IndexQuery::seekToNext" << "ok" << ok << fieldValue;
    return ok;
}

QsonMap IndexQuery::currentObjectAndTypeNumber(ObjectKey &objectKey)
{
    QByteArray baValue;
    mCursor->currentValue(baValue);
    forwardValueSplit(baValue, objectKey);

    if (sDebugQuery) qDebug() << __FILE__ << __LINE__ << "objectKey" << objectKey << baValue.toHex();
    QsonMap object;
    mObjectTable->get(objectKey, object);
    return object;
}

quint32 UuidQuery::stateNumber() const
{
    return mObjectTable->stateNumber();
}

bool UuidQuery::seekToStart(QVariant &fieldValue)
{
    bool ok;
    if (mAscending) {
        if (mMin.isValid()) {
            ObjectKey objectKey(mMin.toString());
            ok = mCursor->seekRange(objectKey.toByteArray());
        } else {
            ok = mCursor->first();
        }
    } else {
        if (mMax.isValid()) {
            ObjectKey objectKey(mMax.toString());
            ok = mCursor->seekRange(objectKey.toByteArray());
        } else {
            ok = mCursor->last();
        }
    }
    QByteArray baKey;
    while (ok) {
        mCursor->currentKey(baKey);
        if (!isStateKey(baKey))
            break;
        if (mAscending)
            ok = mCursor->next();
        else
            ok = mCursor->prev();
    }
    if (ok) {
        QUuid quuid(QUuid::fromRfc4122(baKey));
        ObjectKey objectKey(quuid);
        fieldValue = objectKey.key.toString();
    }
    //qDebug() << "seekToStart" << (mAscending ? mMin : mMax) << "ok" << ok << fieldValue;
    return ok;
}

bool UuidQuery::seekToNext(QVariant &fieldValue)
{
    bool ok = mAscending ? mCursor->next() : mCursor->prev();
    QByteArray baKey;
    while (ok) {
        mCursor->currentKey(baKey);
        if (!isStateKey(baKey))
            break;
        if (mAscending)
            ok = mCursor->next();
        else
            ok = mCursor->prev();
    }
    if (ok) {
        QUuid quuid(QUuid::fromRfc4122(baKey));
        ObjectKey objectKey(quuid);
        fieldValue = objectKey.key.toString();
    }
    //qDebug() << "seekToNext" << "ok" << ok << fieldValue;
    return ok;
}

QsonMap UuidQuery::currentObjectAndTypeNumber(ObjectKey &objectKey)
{
    QByteArray baKey;
    mCursor->currentKey(baKey);
    objectKey = ObjectKey(baKey);

    if (sDebugQuery) qDebug() << __FILE__ << __LINE__ << "objectKey" << objectKey << baKey.toHex();
    QsonMap object;
    mObjectTable->get(objectKey, object);
    return object;
}

QsonMap IndexQuery::first()
{
    mSparseMatchPossible = false;
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        mSparseMatchPossible |= mQueryConstraints[i]->sparseMatchPossible();
    }

    QVariant fieldValue;
    bool ok = seekToStart(fieldValue);
    if (sDebugQuery) qDebug() << "IndexQuery::first" << __LINE__ << "ok after first/last()" << ok;
    for (; ok; ok = seekToNext(fieldValue)) {
        mFieldValue = fieldValue;
        if (sDebugQuery) qDebug() << "IndexQuery::first()"
                                  << "mPropertyName" << mPropertyName
                                  << "fieldValue" << fieldValue
                                  << (mAscending ? "ascending" : "descending");

        if (sDebugQuery) qDebug() << "IndexQuery::first()" << "matches(fieldValue)" << matches(fieldValue);

        if (!matches(fieldValue))
            continue;

        ObjectKey objectKey;
        QsonMap object(currentObjectAndTypeNumber(objectKey));
        if (sDebugQuery) qDebug() << "IndexQuery::first()" << __LINE__ << "objectKey" << objectKey << object.valueBool(JsonDbString::kDeletedStr, false);
        if (object.contains(JsonDbString::kDeletedStr) && object.valueBool(JsonDbString::kDeletedStr, false))
            continue;

        if (!mTypeNames.isEmpty() && !mTypeNames.contains(object.valueString(JsonDbString::kTypeStr)))
            continue;

        if (mResidualQuery && !mResidualQuery->match(object, &mObjectCache, mStorage))
            continue;

        if (sDebugQuery) qDebug() << "IndexQuery::first()" << "returning objectKey" << objectKey;

        return object;
    }
    mUuid.clear();
    return QsonMap();
}
QsonMap IndexQuery::next()
{
    QVariant fieldValue;
    while (seekToNext(fieldValue)) {
        if (sDebugQuery) qDebug() << "IndexQuery::next()" << "mPropertyName" << mPropertyName
                                  << "fieldValue" << fieldValue
                                  << (mAscending ? "ascending" : "descending");
        if (sDebugQuery) qDebug() << "IndexQuery::next()" << "matches(fieldValue)" << matches(fieldValue);
        if (!matches(fieldValue)) {
            if (mSparseMatchPossible)
                continue;
            else
                break;
        }

        ObjectKey objectKey;
        QsonMap object(currentObjectAndTypeNumber(objectKey));
        if (object.contains(JsonDbString::kDeletedStr) && object.valueBool(JsonDbString::kDeletedStr, false))
            continue;

        if (!mTypeNames.isEmpty() && !mTypeNames.contains(object.valueString(JsonDbString::kTypeStr)))
            continue;

        if (sDebugQuery) qDebug() << "IndexQuery::next()" << "objectKey" << objectKey;

        if (mResidualQuery && !mResidualQuery->match(object, &mObjectCache, mStorage))
            continue;

        return object;
    }
    mUuid.clear();
    return QsonMap();
}

class QueryConstraintGt: public QueryConstraint {
public:
    QueryConstraintGt(const QVariant &v) { mValue = v.toString(); }
    bool matches(const QVariant &v) { return v.toString() > mValue; }
private:
    QString mValue;
};
class QueryConstraintGe: public QueryConstraint {
public:
    QueryConstraintGe(const QVariant &v) { mValue = v.toString(); }
    bool matches(const QVariant &v) { return v.toString() >= mValue; }
private:
    QString mValue;
};
class QueryConstraintLt: public QueryConstraint {
public:
    QueryConstraintLt(const QVariant &v) { mValue = v.toString(); }
    bool matches(const QVariant &v) { return v.toString() < mValue; }
private:
    QString mValue;
};
class QueryConstraintLe: public QueryConstraint {
public:
    QueryConstraintLe(const QVariant &v) { mValue = v.toString(); }
    bool matches(const QVariant &v) { return v.toString() <= mValue; }
private:
    QString mValue;
};
class QueryConstraintEq: public QueryConstraint {
public:
    QueryConstraintEq(const QVariant &v) { mValue = v.toString(); }
    bool matches(const QVariant &v) { return v.toString() == mValue; }
private:
    QString mValue;
};
class QueryConstraintNe: public QueryConstraint {
public:
    QueryConstraintNe(const QVariant &v) { mValue = v.toString(); }
    bool sparseMatchPossible() const { return true; }
    bool matches(const QVariant &v) { return v.toString() != mValue; }
private:
    QString mValue;
};
class QueryConstraintHas: public QueryConstraint {
public:
    QueryConstraintHas() { }
    bool matches(const QVariant &v) { return v.isValid(); }
private:
};
class QueryConstraintIn: public QueryConstraint {
public:
    QueryConstraintIn(const QVariant &v) { mList = v.toList();}
    bool sparseMatchPossible() const { return true; }

    bool matches(const QVariant &v) {
        return mList.contains(v);
    }
private:
    QVariantList mList;
};
class QueryConstraintNotIn: public QueryConstraint {
public:
    QueryConstraintNotIn(const QVariant &v) { mList = v.toList();}
    bool sparseMatchPossible() const { return true; }
    bool matches(const QVariant &v) {
        return !mList.contains(v);
    }
private:
    QVariantList mList;
};
class QueryConstraintContains: public QueryConstraint {
public:
    QueryConstraintContains(const QVariant &v) { mValue = v;}
    bool sparseMatchPossible() const { return true; }
    bool matches(const QVariant &v) {
        return v.toList().contains(mValue);
    }
private:
    QVariant mValue;
};
class QueryConstraintStartsWith: public QueryConstraint {
public:
    QueryConstraintStartsWith(const QString &v) { mValue = v;}
    bool sparseMatchPossible() const { return true; }
    bool matches(const QVariant &v) {
        return v.toString().startsWith(mValue);
    }
private:
    QString mValue;
};
class QueryConstraintRegExp: public QueryConstraint {
public:
    QueryConstraintRegExp(const QRegExp &regexp) : mRegExp(regexp) {}
    bool matches(const QVariant &v) { return mRegExp.exactMatch(v.toString()); }
    bool sparseMatchPossible() const { return true; }
private:
    QString mValue;
    QRegExp mRegExp;
};

void JsonDbBtreeStorage::compileOrQueryTerm(IndexQuery *indexQuery, const QueryTerm &queryTerm)
{
    QString op = queryTerm.op();
    QVariant fieldValue = queryTerm.value();
    if (op == ">") {
        indexQuery->addConstraint(new QueryConstraintGt(fieldValue));
        indexQuery->setMin(fieldValue);
    } else if (op == ">=") {
        indexQuery->addConstraint(new QueryConstraintGe(fieldValue));
        indexQuery->setMin(fieldValue);
    } else if (op == "<") {
        indexQuery->addConstraint(new QueryConstraintLt(fieldValue));
        indexQuery->setMax(fieldValue);
    } else if (op == "<=") {
        indexQuery->addConstraint(new QueryConstraintLe(fieldValue));
        indexQuery->setMax(fieldValue);
    } else if (op == "=") {
        indexQuery->addConstraint(new QueryConstraintEq(fieldValue));
        indexQuery->setMin(fieldValue);
        indexQuery->setMax(fieldValue);
    } else if (op == "=~") {
        const QRegExp re = queryTerm.regExpConst();
        QRegExp::PatternSyntax syntax = re.patternSyntax();
        Qt::CaseSensitivity cs = re.caseSensitivity();
        QString pattern = re.pattern();
        indexQuery->addConstraint(new QueryConstraintRegExp(re));
        if (cs == Qt::CaseSensitive) {
            QString prefix;
            if ((syntax == QRegExp::Wildcard)
                && mWildCardPrefixRegExp.exactMatch(pattern)) {
                prefix = mWildCardPrefixRegExp.cap(1);
                if (gDebug) qDebug() << "wildcard regexp prefix" << pattern << prefix;
            }
            indexQuery->setMin(prefix);
            indexQuery->setMax(prefix);
        }
    } else if (op == "!=") {
        indexQuery->addConstraint(new QueryConstraintNe(fieldValue));
    } else if (op == "exists") {
        indexQuery->addConstraint(new QueryConstraintHas);
    } else if (op == "in") {
        QVariantList value = queryTerm.value().toList();
        //qDebug() << "in" << value << value.size();
        if (value.size() == 1)
            indexQuery->addConstraint(new QueryConstraintEq(value.at(0)));
        else
            indexQuery->addConstraint(new QueryConstraintIn(queryTerm.value()));
    } else if (op == "notIn") {
        indexQuery->addConstraint(new QueryConstraintNotIn(queryTerm.value()));
    } else if (op == "in") {
        indexQuery->addConstraint(new QueryConstraintContains(queryTerm.value()));
    } else if (op == "startsWith") {
        indexQuery->addConstraint(new QueryConstraintStartsWith(queryTerm.value().toString()));
    }
}

void JsonDbBtreeStorage::updateIndex(ObjectTable *table, JsonDbIndex *index)
{
    table->updateIndex(index);
}

IndexQuery *JsonDbBtreeStorage::compileIndexQuery(const JsonDbOwner *owner, const JsonDbQuery &query)
{
    IndexQuery *indexQuery = 0;
    JsonDbQuery *residualQuery = new JsonDbQuery();
    QString orderField;
    QSet<QString> typeNames;
    const QList<OrderTerm> &orderTerms = query.orderTerms;
    const QList<OrQueryTerm> &orQueryTerms = query.queryTerms;
    QString indexCandidate;
    ObjectTable *table = mObjectTable; //TODO fix me
    int indexedQueryTermCount = 0;
    if (orQueryTerms.size()) {
        for (int i = 0; i < orQueryTerms.size(); i++) {
            const OrQueryTerm orQueryTerm = orQueryTerms[i];
            const QList<QString> &querypropertyNames = orQueryTerm.propertyNames();
            if (querypropertyNames.size() == 1) {
                //QString fieldValue = queryTerm.value().toString();
                QString propertyName = querypropertyNames[0];

                const QList<QueryTerm> &queryTerms = orQueryTerm.terms();
                const QueryTerm &queryTerm = queryTerms[0];

                if ((typeNames.size() == 1)
                    && mViews.contains(typeNames.toList()[0]))
                    table = mViews[typeNames.toList()[0]];

                if (table->indexSpec(propertyName))
                    indexedQueryTermCount++;
                else if (indexCandidate.isEmpty() && (propertyName != JsonDbString::kTypeStr)) {
                    indexCandidate = propertyName;
                    if (!queryTerm.joinField().isEmpty())
                        indexCandidate = queryTerm.joinPaths()[0].join("->");

                }

                propertyName = queryTerm.propertyName();
                QString fieldValue = queryTerm.value().toString();
                QString op = queryTerm.op();
                if (propertyName == JsonDbString::kTypeStr) {
                    if ((op == "=") || (op == "in")) {
                        QSet<QString> types;
                        if (op == "=")
                            types << fieldValue;
                        else
                            types = QSet<QString>::fromList(queryTerm.value().toStringList());

                        if (typeNames.count()) {
                            typeNames.intersect(types);
                            if (!typeNames.count()) {
                                // make this a null query -- I really need a domain (partial order) here and not a set
                                typeNames = types;
                            }
                        } else {
                            typeNames = types;
                        }
                    } else if ((op == "!=") || (op == "notIn")) {
                        QSet<QString> types;
                        if (op == "!=")
                            types << fieldValue;
                        else
                            types = QSet<QString>::fromList(queryTerm.value().toStringList());
                    }
                }
            }
        }
    }
    if ((typeNames.size() == 1)
        && mViews.contains(typeNames.toList()[0]))
        table = mViews[typeNames.toList()[0]];
    if (!indexedQueryTermCount && !indexCandidate.isEmpty()) {
        if (gVerbose) qDebug() << "adding index" << indexCandidate;
        table->addIndex(indexCandidate);
        if (gVerbose) qDebug() << "done adding index" << indexCandidate;
    }

    for (int i = 0; i < orderTerms.size(); i++) {
        const OrderTerm &orderTerm = orderTerms[i];
        QString propertyName = orderTerm.propertyName;
        if (!table->indexSpec(propertyName)) {
            if (gVerbose) qDebug() << "Unindexed sort term" << propertyName << orderTerm.ascending;
            if (gVerbose) qDebug() << "adding index for sort term" << propertyName;
            Q_ASSERT(table);
            table->addIndex(propertyName);
            Q_ASSERT(table->indexSpec(propertyName));
            if (gVerbose) qDebug() << "done adding index" << propertyName;
            //residualQuery->orderTerms.append(orderTerm);
            //continue;
        }
        if (!indexQuery) {
            orderField = propertyName;
            const IndexSpec *indexSpec = table->indexSpec(propertyName);
            updateView(table);

            if (indexSpec->lazy)
                updateIndex(table, indexSpec->index);
            indexQuery = IndexQuery::indexQuery(this, table, propertyName, owner, orderTerm.ascending);
        } else if (orderField != propertyName) {
            qCritical() << QString("unimplemented: multiple order terms. Sorting on '%1'").arg(orderField);
            residualQuery->orderTerms.append(orderTerm);
        }
    }

    for (int i = 0; i < orQueryTerms.size(); i++) {
        const OrQueryTerm &orQueryTerm = orQueryTerms[i];
        const QList<QueryTerm> &queryTerms = orQueryTerm.terms();
        if (queryTerms.size() == 1) {
            QueryTerm queryTerm = queryTerms[0];
            QString propertyName = queryTerm.propertyName();
            QString fieldValue = queryTerm.value().toString();
            QString op = queryTerm.op();

            if (!queryTerm.joinField().isEmpty()) {
                residualQuery->queryTerms.append(queryTerm);
                propertyName = queryTerm.joinField();
                op = "exists";
                queryTerm.setPropertyName(propertyName);
                queryTerm.setOp(op);
                queryTerm.setJoinField(QString());
            }
            if (!table->indexSpec(propertyName)
                || (indexQuery
                    && (propertyName != orderField))) {
                if (gVerbose || gDebug) qDebug() << "residual query term" << propertyName << "orderField" << orderField;
                residualQuery->queryTerms.append(queryTerm);
                continue;
            }

            if (!indexQuery && (propertyName != JsonDbString::kTypeStr) && table->indexSpec(propertyName)) {
                orderField = propertyName;
                const IndexSpec *indexSpec = table->indexSpec(propertyName);
                updateView(table);
                if (indexSpec->lazy)
                    updateIndex(table, indexSpec->index);
                indexQuery = IndexQuery::indexQuery(this, table, propertyName, owner);
            }

            if (propertyName == orderField) {
                compileOrQueryTerm(indexQuery, queryTerm);
            } else {
                residualQuery->queryTerms.append(orQueryTerm);
            }
        } else {
            residualQuery->queryTerms.append(orQueryTerm);
        }
    }

    if (!indexQuery) {
        QString defaultIndex = JsonDbString::kUuidStr;
        if (typeNames.size()) {
            if ((typeNames.size() == 1)
                && mViews.contains(typeNames.toList()[0]))
                table = mViews[typeNames.toList()[0]];
            else
                defaultIndex = JsonDbString::kTypeStr;
        }
        const IndexSpec *indexSpec = table->indexSpec(defaultIndex);

        //qDebug() << "defaultIndex" << defaultIndex << "on table" << indexSpec->objectType;

        updateView(table);
        if (indexSpec->lazy)
            updateIndex(table, indexSpec->index);
        indexQuery = IndexQuery::indexQuery(this, table, defaultIndex, owner);
        if (typeNames.size() == 0)
            qCritical() << "searching all objects" << query.query;

        if (defaultIndex == JsonDbString::kTypeStr) {
            foreach (const OrQueryTerm &term, orQueryTerms) {
                QList<QueryTerm> terms = term.terms();
                if (terms.size() == 1 && terms[0].propertyName() == JsonDbString::kTypeStr) {
                    compileOrQueryTerm(indexQuery, terms[0]);
                    break;
                }
            }
        }
    }
    if (typeNames.count() > 0)
        indexQuery->setTypeNames(typeNames);
    if (residualQuery->queryTerms.size() || residualQuery->orderTerms.size())
        indexQuery->setResidualQuery(residualQuery);
    else
        delete residualQuery;
    indexQuery->setAggregateOperation(query.mAggregateOperation);
    return indexQuery;
}

void JsonDbBtreeStorage::doIndexQuery(const JsonDbOwner *owner, QsonList &results, int &limit, int &offset,
                                      IndexQuery *indexQuery)
{
    if (sDebugQuery) qDebug() << "doIndexQuery" << "limit" << limit << "offset" << offset;
    bool countOnly = (indexQuery->aggregateOperation() == "count");
    int count = 0;
    for (QsonMap object = indexQuery->first();
         !object.isEmpty();
         object = indexQuery->next()) {
        if (!owner->isAllowed(object, QString("read")))
            continue;
        if (limit && (offset <= 0)) {
            if (!countOnly) {
                if (sDebugQuery) qDebug() << "appending result" << object << endl;
                results.append(object);
            }
            limit--;
            count++;
        }
        offset--;
        if (limit == 0)
            break;
    }
    if (countOnly) {
        QsonMap countObject;
        countObject.insert(QLatin1String("count"), count);
        results.append(countObject);
    }
}

static int findMinHead(QList<QsonMap> &heads, QStringList path, bool ascending)
{
    int minIndex = 0;
    QString minVal = JsonDb::propertyLookup(heads[0], path).toString();
    for (int i = 1; i < heads.size(); i++) {
        QString val = JsonDb::propertyLookup(heads[i], path).toString();
        if (ascending ? (val < minVal) : (val > minVal)) {
            minVal = val;
            minIndex = i;
        }
    }
    return minIndex;
}

void JsonDbBtreeStorage::doMultiIndexQuery(const JsonDbOwner *owner, QsonList &results, int &limit, int &offset,
                                           const QList<IndexQuery *> &indexQueries)
{
    Q_ASSERT(indexQueries.size());
    if (indexQueries.size() == 1)
        return doIndexQuery(owner, results, limit, offset, indexQueries[0]);

    QList<QsonMap> heads;
    QList<IndexQuery*> queries;
    int nPartitions = indexQueries.size();

    QString field0 = indexQueries[0]->propertyName();
    QStringList path0 = field0.split('.');
    bool ascending = indexQueries[0]->ascending();

    if (sDebugQuery) qDebug() << "doMultiIndexQuery" << "limit" << limit << "offset" << offset << "propertyName" << indexQueries[0]->propertyName();

    for (int i = 0; i < nPartitions; i++) {
        QsonMap object = indexQueries[i]->first();
        qDebug() << i << "first" << object;
        if (!object.isEmpty()) {
            heads.append(object);
            queries.append(indexQueries[i]);
        }
    }

    bool countOnly = (indexQueries[0]->aggregateOperation() == "count");
    int count = 0;
    int s = 0;
    while (queries.size()) {
        QsonMap object;
        // if this is an ordered query, should take the minimum of the values
        s = findMinHead(heads, path0, ascending);
        object = heads[s];
        heads[s] = queries[s]->next();
        if (heads[s].isEmpty()) {
            heads.takeAt(s);
            queries.takeAt(s);
        }
        if (object.isEmpty())
            break;

        if (!owner->isAllowed(object, QString("read")))
            continue;
        if (limit && (offset <= 0)) {
            if (!countOnly) {
                if (sDebugQuery) qDebug() << "appending result" << object << endl;
                results.append(object);
            }
            limit--;
            count++;
        }
        offset--;
        if (limit == 0)
            break;
    }
    if (countOnly) {
        QsonMap countObject;
        countObject.insert(QLatin1String("count"), count);
        results.append(countObject);
    }
}

bool JsonDbBtreeStorage::checkQuota(const JsonDbOwner *owner, int size) const
{
    int quota = owner->storageQuota();
    if (quota <= 0)
        return true;

    const QString ownerId(owner->ownerId());
    if (ownerId.isEmpty())
        return false;
    QByteArray baKey = QByteArray::fromRawData((const char *)ownerId.constData(), 2*ownerId.size());
    QByteArray baValue;
    bool ok = mBdbIndexes->get(baKey, baValue);
    int oldSize = (ok) ? qFromLittleEndian<quint32>((const uchar *)baValue.data()) : 0;
    if ((oldSize + size) <= quota)
        return true;
    else {
        DBG() << QString("Failed checkQuota: oldSize=%1 size=%2 quota=%3").arg(oldSize).arg(size).arg(quota);
        return false;
    }
}

bool JsonDbBtreeStorage::addToQuota(const JsonDbOwner *owner, int size)
{
    if (owner->storageQuota() <= 0)
        return true;

    const QString ownerId(owner->ownerId());
    if (ownerId.isEmpty())
        return true;
    WithTransaction transaction(this);
    CHECK_LOCK(transaction, "addToQuota");
    QByteArray baKey = QByteArray::fromRawData((const char *)ownerId.constData(), 2*ownerId.size());
    QByteArray baValue;
    quint32 value = 0;
    if (mBdbIndexes->get(baKey, baValue) && baValue.size() == 4)
        value = qFromLittleEndian<quint32>((const uchar *)baValue.constData());
    value += size;
    baValue.resize(4);
    qToLittleEndian(value, (uchar *)baValue.data());
    mBdbIndexes->put(baKey, baValue);
    return true;
}

QsonMap JsonDbBtreeStorage::queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit, int offset)
{
    QsonList results;
    QsonList joinedResults;

    QElapsedTimer time;
    time.start();
    IndexQuery *indexQuery = compileIndexQuery(owner, query);

    int elapsedToCompile = time.elapsed();
    doIndexQuery(owner, results, limit, offset, indexQuery);
    int elapsedToQuery = time.elapsed();
    quint32 stateNumber = indexQuery->stateNumber();
    int length = results.size();
    JsonDbQuery *residualQuery = indexQuery->residualQuery();
    if (residualQuery && residualQuery->orderTerms.size()) {
        if (gVerbose) qDebug() << "queryPersistentObjects" << "sorting";
        sortValues(residualQuery, results, joinedResults);
    }

    QsonList sortKeys;
    sortKeys.append(indexQuery->propertyName());

    delete indexQuery;

    QsonMap map;
    map.insert(JsonDbString::kLengthStr, length);
    map.insert(JsonDbString::kOffsetStr, offset);
    map.insert(JsonDbString::kDataStr, results);
    map.insert(kStateStr, stateNumber);
    map.insert(QByteArray("joinedData"), joinedResults);
    map.insert(kSortKeysStr, sortKeys);
    int elapsedToDone = time.elapsed();
    if (gVerbose)
        qDebug() << "elapsed" << elapsedToCompile << elapsedToQuery << elapsedToDone << query.query;
    return map;
}

QsonMap JsonDbBtreeStorage::queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit, int offset, QList<JsonDbBtreeStorage *> partitions)
{
    Q_ASSERT(partitions.size());
    QsonList results;
    QsonList joinedResults;

    QElapsedTimer time;
    time.start();
    QList<IndexQuery *> indexQueries;
    for (int i = 0; i < partitions.size(); i++)
        indexQueries.append(partitions[i]->compileIndexQuery(owner, query));

    int elapsedToCompile = time.elapsed();
    doMultiIndexQuery(owner, results, limit, offset, indexQueries);
    int elapsedToQuery = time.elapsed();

    int length = results.size();
    JsonDbQuery *residualQuery = indexQueries[0]->residualQuery();
    if (residualQuery && residualQuery->orderTerms.size()) {
        if (gVerbose) qDebug() << "queryPersistentObjects" << "sorting";
        sortValues(residualQuery, results, joinedResults);
    }

    QsonList sortKeys;
    sortKeys.append(indexQueries[0]->propertyName());

    for (int i = 0; i < indexQueries.size(); i++)
        delete indexQueries[i];

    QsonMap map;
    map.insert(JsonDbString::kLengthStr, length);
    map.insert(JsonDbString::kOffsetStr, offset);
    map.insert(JsonDbString::kDataStr, results);
    map.insert(kSortKeysStr, sortKeys);
    map.insert(QByteArray("joinedData"), joinedResults);
    int elapsedToDone = time.elapsed();
    if (gVerbose)
        qDebug() << "elapsed" << elapsedToCompile << elapsedToQuery << elapsedToDone << query.query;
    return map;
}

void JsonDbBtreeStorage::checkIndex(const QString &propertyName)
{
// TODO
    if (mObjectTable->indexSpec(propertyName))
        mObjectTable->indexSpec(propertyName)->index->checkIndex();
}

bool JsonDbBtreeStorage::compact()
{
    for (QHash<QString,QPointer<ObjectTable> >::const_iterator it = mViews.begin();
         it != mViews.end();
         ++it) {
        it.value()->compact();
    }
    return mObjectTable->compact()
        && mBdbIndexes->compact();
}

QString JsonDbBtreeStorage::name() const
{
    return mPartitionName;
}
void JsonDbBtreeStorage::setName(const QString &name)
{
    mPartitionName = name;
}

struct StringSortable {
    QString key;
    QsonMap result;
    QsonMap joinedResult;
};

bool lessThan(const StringSortable &a, const StringSortable &b)
{
    return a.key < b.key;
}
bool greaterThan(const StringSortable &a, const StringSortable &b)
{
    return a.key > b.key;
}

void JsonDbBtreeStorage::sortValues(const JsonDbQuery *parsedQuery, QsonList &results, QsonList &joinedResults)
{
    const QList<OrderTerm> &orderTerms = parsedQuery->orderTerms;
    if (!orderTerms.size() || (results.size() < 2))
        return;
    const OrderTerm &orderTerm0 = orderTerms[0];
    QString field0 = orderTerm0.propertyName;
    bool ascending = orderTerm0.ascending;
    QStringList path0 = field0.split('.');

    if (orderTerms.size() == 1) {
        QVector<StringSortable> valuesToSort(results.size());
        int resultsSize = results.size();
        int joinedResultsSize = joinedResults.size();

        for (int i = 0; i < resultsSize; i++) {
            StringSortable *p = &valuesToSort[i];
            QsonMap r = results.at<QsonMap>(i);
            p->key = JsonDb::propertyLookup(r, path0).toString();
            p->result = r;
            if (joinedResultsSize > i)
                p->joinedResult = joinedResults.at<QsonMap>(i);
        }

        if (ascending)
            qStableSort(valuesToSort.begin(), valuesToSort.end(), lessThan);
        else
            qStableSort(valuesToSort.begin(), valuesToSort.end(), greaterThan);

        results = QsonList();
        joinedResults = QsonList();
        for (int i = 0; i < resultsSize; i++) {
            StringSortable *p = &valuesToSort[i];
            results.append(p->result);
            if (joinedResultsSize > i)
                joinedResults.append(p->joinedResult);
        }
    } else {
        qCritical() << "Unimplemented: sorting on multiple keys or non-string keys";
    }
}

bool WithTransaction::addObjectTable(ObjectTable *table)
{
    if (!mStorage)
        return false;
    if (!mStorage->mTableTransactions.contains(table)) {
        bool ok = table->begin();
        mStorage->mTableTransactions.append(table);
        return ok;
    }
    return true;
}

#include "moc_jsondbbtreestorage.cpp"

QT_ADDON_JSONDB_END_NAMESPACE
