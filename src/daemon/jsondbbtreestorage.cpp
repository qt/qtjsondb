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
#include <QMap>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "jsondb.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"
#include "jsondbbtreestorage.h"
#include "jsondbindex.h"
#include "objecttable.h"
#include "qbtreetxn.h"
#include "qmanagedbtree.h"

QT_BEGIN_NAMESPACE_JSONDB

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
    mBdbIndexes = new QManagedBtree();

    mObjectTable->open(mFilename, QBtree::NoSync | QBtree::UseSyncMarker);

    QString dir = fi.dir().path();
    QString basename = fi.fileName();
    if (basename.endsWith(".db"))
        basename.chop(3);
    if (!mBdbIndexes->open(QString("%1/%2-Indexes.db").arg(dir).arg(basename),
                           QBtree::NoSync | QBtree::UseSyncMarker)) {
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
        QBtreeCursor cursor(mBdbIndexes->btree());
        if (cursor.first()) {
            cursor.current(&baKey, &baValue);
            if (baValue.size() > 4) {
                QJsonObject object(QJsonDocument::fromBinaryData(baValue).object());
                if (object.value(JsonDbString::kTypeStr).toString() != kDbidTypeStr) {
                    qCritical() << __FUNCTION__ << __LINE__ << "no dbid in indexes table";
                } else {
                    partitionId = object.value("id").toString();
                    QString partitionName = object.value("name").toString();
                    if (partitionName != mPartitionName || !object.contains(kDatabaseSchemaVersionStr)
                        || object.value(kDatabaseSchemaVersionStr).toString() != gDatabaseSchemaVersion) {
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

        QJsonObject object;
        object.insert(JsonDbString::kTypeStr, kDbidTypeStr);
        object.insert(QLatin1String("id"), partitionId);
        object.insert(QLatin1String("name"), mPartitionName);
        object.insert(kDatabaseSchemaVersionStr, gDatabaseSchemaVersion);
        QByteArray baObject = QJsonDocument(object).toBinaryData();
        bool ok = mBdbIndexes->putOne(baKey, baObject);
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


inline quint16 fieldValueSize(QJsonValue::Type vt, const QJsonValue &fieldValue)
{
    switch (vt) {
    case QJsonValue::Undefined:
    case QJsonValue::Null:
    case QJsonValue::Array:
    case QJsonValue::Object:
        return 0;
    case QJsonValue::Bool:
        return 4;
    case QJsonValue::Double:
        return 8;
    case QJsonValue::String:
        return 2*fieldValue.toString().count();
    }
    return 0;
}

void memcpyFieldValue(char *data, QJsonValue::Type vt, const QJsonValue &fieldValue)
{
    switch (vt) {
    case QJsonValue::Undefined:
    case QJsonValue::Null:
    case QJsonValue::Array:
    case QJsonValue::Object:
        break;
    case QJsonValue::Bool: {
        quint32 value = fieldValue.toBool() ? 1 : 0;
        qToBigEndian(value, (uchar *)data);
    } break;
    case QJsonValue::Double: {
        union {
            double d;
            quint64 ui;
        };
        d = fieldValue.toDouble();
        qToBigEndian<quint64>(ui, (uchar *)data);
    } break;
    case QJsonValue::String: {
        QString str = fieldValue.toString();
        memcpy(data, (const char *)str.constData(), 2*str.count());
    }
    }
}

void memcpyFieldValue(QJsonValue::Type vt, QJsonValue &fieldValue, const char *data, quint16 size)
{
    switch (vt) {
    case QJsonValue::Undefined:
    case QJsonValue::Array:
    case QJsonValue::Object:
        break;
    case QJsonValue::Null:
        fieldValue = QJsonValue();
        break;
    case QJsonValue::Bool: {
        fieldValue = qFromBigEndian<qint32>((const uchar *)data) == 1 ? true : false;
    } break;
    case QJsonValue::Double: {
        union {
            double d;
            quint64 ui;
        };
        ui = qFromBigEndian<quint64>((const uchar *)data);
        fieldValue = d;
    } break;
    case QJsonValue::String: {
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
    union {
        double d;
        quint64 ui;
    } a, b;
    a.ui = qFromBigEndian<quint64>((const uchar *)aptr);
    b.ui = qFromBigEndian<quint64>((const uchar *)bptr);
    if (a.d < b.d)
        return -1;
    if (a.d > b.d)
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

QJsonValue makeFieldValue(const QJsonValue &value, const QString &type)
{
    if (type.isEmpty() || type == QLatin1String("string")) {
        switch (value.type()) {
        case QJsonValue::Null: return QLatin1String("null");
        case QJsonValue::Bool: return QLatin1String(value.toBool() ? "true" : "false");
        case QJsonValue::Double: return QString::number(value.toDouble());
        case QJsonValue::String: return value.toString();
        case QJsonValue::Array: {
            QJsonArray array = value.toArray();
            if (array.size() == 1)
                return makeFieldValue(array.at(0), type);
            return QJsonValue(QJsonValue::Undefined);
        }
        case QJsonValue::Object: break;
        case QJsonValue::Undefined: break;
        }
    } else if ((type == QLatin1String("number"))
               || (type == QLatin1String("integer"))) {
        switch (value.type()) {
        case QJsonValue::Null: return 0;
        case QJsonValue::Bool: return value.toBool() ? 1 : 0;
        case QJsonValue::Double: return value.toDouble();
        case QJsonValue::String: {
            QString str = value.toString();
            bool ok = false;
            double dval = str.toDouble(&ok);
            if (ok)
                return dval;
            int ival = str.toInt(&ok);
            if (ok)
                return ival;
            break;
        }
        case QJsonValue::Array: {
            QJsonArray array = value.toArray();
            if (array.size() == 1)
                return makeFieldValue(array.at(0), type);
            return QJsonValue(QJsonValue::Undefined);
        }
        case QJsonValue::Object: break;
        case QJsonValue::Undefined: break;
        }
    } else {
        qWarning() << "qtjsondb: makeFieldValue: unsupported index type" << type;
    }
    return QJsonValue(QJsonValue::Undefined);
}

static const int VtLastKind = QJsonValue::Undefined;
QByteArray makeForwardKey(const QJsonValue &fieldValue, const ObjectKey &objectKey)
{
    QJsonValue::Type vt = fieldValue.type();
    Q_ASSERT(vt <= VtLastKind);
    quint32 size = fieldValueSize(vt, fieldValue);

    QByteArray forwardKey(4+size+16, 0);
    char *data = forwardKey.data();
    qToBigEndian<quint32>(vt, (uchar *)&data[0]);
    memcpyFieldValue(data+4, vt, fieldValue);
    qToBigEndian(objectKey, (uchar *)&data[4+size]);

    return forwardKey;
}

void forwardKeySplit(const QByteArray &forwardKey, QJsonValue &fieldValue)
{
    const char *data = forwardKey.constData();
    QJsonValue::Type vt = (QJsonValue::Type)qFromBigEndian<quint32>((const uchar *)&data[0]);
    Q_ASSERT(vt <= VtLastKind);
    quint32 fvSize = forwardKey.size()-4-16;
    memcpyFieldValue(vt, fieldValue, data+4, fvSize);
}
void forwardKeySplit(const QByteArray &forwardKey, QJsonValue &fieldValue, ObjectKey &objectKey)
{
    const char *data = forwardKey.constData();
    QJsonValue::Type vt = (QJsonValue::Type)qFromBigEndian<quint32>((const uchar *)&data[0]);
    Q_ASSERT(vt <= VtLastKind);
    quint32 fvSize = forwardKey.size()-4-16;
    memcpyFieldValue(vt, fieldValue, data+4, fvSize);
    objectKey = qFromBigEndian<ObjectKey>((const uchar *)&data[4+fvSize]);
}
int forwardKeyCmp(const char *aptr, size_t asiz, const char *bptr, size_t bsiz, void *)
{
    int rv = 0;
    QJsonValue::Type avt = (QJsonValue::Type)qFromBigEndian<quint32>((const uchar *)&aptr[0]);
    QJsonValue::Type bvt = (QJsonValue::Type)qFromBigEndian<quint32>((const uchar *)&bptr[0]);
    Q_ASSERT(avt <= VtLastKind);
    Q_ASSERT(bvt <= VtLastKind);
    quint32 asize = asiz - 4 - 16;
    quint32 bsize = bsiz - 4 - 16;
    if (avt != bvt)
        return avt - bvt;

    const char *aData = aptr + 4;
    const char *bData = bptr + 4;
    switch (avt) {
    case QJsonValue::Bool:
        rv = intcmp((const uchar *)aData, (const uchar *)bData);
        break;
    case QJsonValue::Double:
        rv = doublecmp((const uchar *)aData, (const uchar *)bData);
        break;
    case QJsonValue::String:
        rv = qstringcmp((const quint16 *)aData, asize/2, (const quint16 *)bData, bsize/2);
        break;
    case QJsonValue::Undefined:
    case QJsonValue::Null:
    case QJsonValue::Array:
    case QJsonValue::Object:
        rv = 0;
        break;
    }
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

QJsonObject JsonDbBtreeStorage::createPersistentObject(JsonDbObject &object)
{
    QJsonObject resultmap, errormap;

    if (!object.contains(JsonDbString::kUuidStr)) {
        object.generateUuid();
        object.computeVersion();
    }

    QString uuid = object.value(JsonDbString::kUuidStr).toString();
    QString version = object.value(JsonDbString::kVersionStr).toString();
    QString objectType = object.value(JsonDbString::kTypeStr).toString();

    ObjectKey objectKey(object.value(JsonDbString::kUuidStr).toString());
    ObjectTable *table = findObjectTable(objectType);

    bool ok = table->put(objectKey, object);
    if (!ok) {
        return JsonDb::makeErrorResponse(resultmap, JsonDbError::DatabaseError,
                                         table->errorMessage());
    }

    quint32 stateNumber = table->storeStateChange(objectKey, ObjectChange::Created);
    table->indexObject(objectKey, object, table->stateNumber());

    resultmap.insert(JsonDbString::kUuidStr, uuid);
    resultmap.insert(JsonDbString::kVersionStr, version);
    resultmap.insert(JsonDbString::kCountStr, 1);
    resultmap.insert(JsonDbString::kStateNumberStr, (int)stateNumber);

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
                                   QBtree::NoSync | QBtree::UseSyncMarker)) {
            qCritical() << "viewDb->open" << viewObjectTable->errorMessage();
            return;
        }
        mViews.insert(viewType, viewObjectTable);
        viewObjectTable->addIndexOnProperty(JsonDbString::kUuidStr, "string", viewType);
        // TODO: special case for the following
        viewObjectTable->addIndexOnProperty(JsonDbString::kTypeStr, "string", viewType);
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
        Q_ASSERT(!mIndexTxn);
        mIndexTxn = mBdbIndexes->beginWrite();
        if (!mIndexTxn) {
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
        Q_ASSERT(mIndexTxn);
        bool ret = true;
        quint32 nextStateNumber = stateNumber ? stateNumber : (mObjectTable->stateNumber() + 1);
        if (gDebug) qDebug() << "commitTransaction" << stateNumber;
        if (!stateNumber && (mTableTransactions.size() == 1))
            nextStateNumber = mTableTransactions.at(0)->stateNumber() + 1;

        if (!mIndexTxn.commit(nextStateNumber)) {
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

        mIndexTxn.abort();

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

QJsonObject JsonDbBtreeStorage::updatePersistentObject(const JsonDbObject &oldObject, const JsonDbObject &object)
{
    QJsonObject resultmap, errormap;

    QString uuid = object.value(JsonDbString::kUuidStr).toString();
    QString version = object.value(JsonDbString::kVersionStr).toString();
    QString objectType = object.value(JsonDbString::kTypeStr).toString();
    ObjectTable *table = findObjectTable(objectType);
    ObjectKey objectKey(uuid);

    objectKey = ObjectKey(object.value(JsonDbString::kUuidStr).toString());

    if (!oldObject.isEmpty())
        table->deindexObject(objectKey, oldObject, table->stateNumber());

    if (!table->put(objectKey, object)) {
        return JsonDb::makeErrorResponse(resultmap, JsonDbError::DatabaseError,
                                         table->errorMessage());
    }

    if (gDebug) qDebug() << "updateObject" << objectKey << endl << object << endl << oldObject;

    quint32 stateNumber;
    if (!oldObject.isEmpty()) {
        stateNumber = table->storeStateChange(objectKey, ObjectChange::Updated, oldObject);
    } else {
        stateNumber = table->storeStateChange(objectKey, ObjectChange::Created);
    }

    table->indexObject(objectKey, object, table->stateNumber());

    resultmap.insert( JsonDbString::kCountStr, 1 );
    resultmap.insert( JsonDbString::kUuidStr, uuid );
    resultmap.insert( JsonDbString::kVersionStr, version );
    resultmap.insert( JsonDbString::kStateNumberStr, (int)stateNumber );

    return JsonDb::makeResponse( resultmap, errormap );
}

QJsonObject JsonDbBtreeStorage::removePersistentObject(const JsonDbObject &oldObject, const JsonDbObject &ts)
{
    if (gDebug) qDebug() << "removePersistentObject" << endl << oldObject << endl << "    tombstone" << ts;

    QJsonObject resultmap, errormap;
    QString uuid = oldObject.value(JsonDbString::kUuidStr).toString();
    QString version = oldObject.value(JsonDbString::kVersionStr).toString();
    QString objectType = oldObject.value(JsonDbString::kTypeStr).toString();

    ObjectTable *table = findObjectTable(objectType);

    ObjectKey objectKey(uuid);

    Q_ASSERT(ts.contains(JsonDbString::kUuidStr) && (oldObject.value(JsonDbString::kUuidStr) == ts.value(JsonDbString::kUuidStr)));
    Q_ASSERT(ts.contains(JsonDbString::kVersionStr));
    Q_ASSERT(ts.contains(JsonDbString::kDeletedStr) && ts.value(JsonDbString::kDeletedStr).toBool());

    bool ok = table->put(objectKey, ts);
    Q_ASSERT(ok);
    if (!ok) {
        return JsonDb::makeErrorResponse(resultmap,
                                         JsonDbError::DatabaseError,
                                         table->errorMessage());
    }
    JsonDbObject testing;
    ok = table->get(objectKey, &testing, true);
    Q_ASSERT(ok);
    Q_ASSERT(testing == ts);

    quint32 stateNumber = table->storeStateChange(objectKey, ObjectChange::Deleted, oldObject);
    table->deindexObject(objectKey, oldObject, table->stateNumber());

    QJsonObject item;
    item.insert(JsonDbString::kUuidStr, uuid);

    QJsonArray data;
    data.append(item);
    resultmap.insert(JsonDbString::kCountStr, 1);
    resultmap.insert(JsonDbString::kDataStr, data);
    resultmap.insert(JsonDbString::kErrorStr, QJsonValue());
    resultmap.insert(JsonDbString::kStateNumberStr, (int)stateNumber);
    QJsonObject result = JsonDb::makeResponse( resultmap, errormap );
    return result;
}

bool JsonDbBtreeStorage::getObject(const QString &uuid, JsonDbObject &object, const QString &objectType) const
{
    ObjectKey objectKey(uuid);
    return getObject(objectKey, object, objectType);
}

bool JsonDbBtreeStorage::getObject(const ObjectKey &objectKey, JsonDbObject &object, const QString &objectType) const
{
    ObjectTable *table = findObjectTable(objectType);

    bool ok = table->get(objectKey, &object);
    if (ok)
        return ok;
    QHash<QString,QPointer<ObjectTable> >::const_iterator it = mViews.begin();
    for (; it != mViews.end(); ++it) {
        bool ok = it.value()->get(objectKey, &object);
        if (ok)
            return ok;
    }
    return false;
}

GetObjectsResult JsonDbBtreeStorage::getObjects(const QString &keyName, const QJsonValue &keyValue, const QString &_objectType)
{
    const QString &objectType = (keyName == JsonDbString::kTypeStr) ? keyValue.toString() : _objectType;
    ObjectTable *table = findObjectTable(objectType);

    updateView(objectType);
    return table->getObjects(keyName, keyValue, objectType);
}

QJsonObject JsonDbBtreeStorage::changesSince(quint32 stateNumber, const QSet<QString> &limitTypes)
{
    ObjectTable *objectTable = 0;
    if (!limitTypes.size())
        objectTable = mObjectTable;
    else
        foreach (const QString &limitType, limitTypes) {
            ObjectTable *ot = findObjectTable(limitType);
            qDebug() << "changesSince" << limitType << QString::number((long long)ot, 16);
            if (!objectTable)
                objectTable = ot;
            else if (ot == objectTable)
                continue;
            else
                return JsonDb::makeError(JsonDbError::InvalidRequest, "limit types must be from the same object table");
        }
    Q_ASSERT(objectTable);
    return objectTable->changesSince(stateNumber, limitTypes);
}

bool JsonDbBtreeStorage::checkValidity()
{
    bool noErrors = true;
#if 0
    if (gVerboseCheckValidity) qDebug() << "JsonDbBtreeStorage::checkValidity {";
    QMap<quint32, QString> keyUuids; // objectKey -> uuid
    QMap<QString, quint32> uuidKeys; // objectKey -> uuid
    QMap<QString, QJsonValue> objects; // uuid -> object

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
        QJsonObject object = QsonParser::fromRawData(baValue).toMap();
        QString uuid = object.value(JsonDbString::kUuidStr).toString();
        QString typeName = object.value(JsonDbString::kTypeStr).toString();
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
            qDebug() << objectKey << uuid << object.value(JsonDbString::kTypeStr).toString();
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

void JsonDbBtreeStorage::flushCaches()
{
    mObjectTable->flushCaches();
    if (mBdbIndexes && mBdbIndexes->handle()) {
        mBdbIndexes->setCacheSize(1);
        mBdbIndexes->setCacheSize(gCacheSize);
    }
    for (QHash<QString,QPointer<ObjectTable> >::const_iterator it = mViews.begin();
         it != mViews.end();
         ++it)
        it.value()->flushCaches();
}

void JsonDbBtreeStorage::initIndexes()
{
    QByteArray baPropertyName;
    QByteArray baIndexObject;

    QBtreeCursor cursor(mBdbIndexes->btree());
    for (bool ok = cursor.first(); ok; ok = cursor.next()) {
        if (!cursor.current(&baPropertyName, &baIndexObject))
            break;

        if (baIndexObject.size() == 4)
            continue;
        QJsonObject indexObject = QJsonDocument::fromBinaryData(baIndexObject).object();
        if (!indexObject.size())
            continue;
        if (gVerbose) qDebug() << "initIndexes" << "index" << indexObject;
        QString indexObjectType = indexObject.value(JsonDbString::kTypeStr).toString();
        if (indexObjectType == kIndexTypeStr) {
            QString indexName = indexObject.value(kNameStr).toString();
            QString propertyName = indexObject.value(kPropertyNameStr).toString();
            QString propertyType = indexObject.value(kPropertyTypeStr).toString();
            QString objectType = indexObject.value(kObjectTypeStr).toString();
            QString propertyFunction = indexObject.value(kPropertyFunctionStr).toString();

            ObjectTable *table = findObjectTable(objectType);
            table->addIndex(indexName, propertyName, propertyType, objectType, propertyFunction);
            //checkIndexConsistency(index);
        }
    }

    beginTransaction();
    mObjectTable->addIndexOnProperty(JsonDbString::kUuidStr, "string");
    mObjectTable->addIndexOnProperty(JsonDbString::kTypeStr, "string");
    commitTransaction();
}

bool JsonDbBtreeStorage::addIndex(const QString &indexName, const QString &propertyName,
                                  const QString &propertyType, const QString &objectType, const QString &propertyFunction)
{
    Q_ASSERT(!indexName.isEmpty());
    //qDebug() << "JsonDbBtreeStorage::addIndex" << propertyName << objectType;
    ObjectTable *table = findObjectTable(objectType);
    const IndexSpec *indexSpec = table->indexSpec(indexName);
    if (indexSpec)
        return true;
    //if (gVerbose) qDebug() << "JsonDbBtreeStorage::addIndex" << propertyName << objectType;
    return table->addIndex(indexName, propertyName, propertyType, objectType, propertyFunction);
}

bool JsonDbBtreeStorage::removeIndex(const QString &indexName, const QString &objectType)
{
    ObjectTable *table = findObjectTable(objectType);
    const IndexSpec *indexSpec = table->indexSpec(indexName);
    if (!indexSpec)
        return false;
    return table->removeIndex(indexName);
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
            int rc = index->bdb()->rollback();
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

QHash<QString, qint64> JsonDbBtreeStorage::fileSizes() const
{
    QList<QFileInfo> fileInfo;
    fileInfo << mBdbIndexes->fileName();
    fileInfo << mObjectTable->bdb()->fileName();

    foreach (const IndexSpec &spec, mObjectTable->indexSpecs().values()) {
        if (spec.index->bdb())
            fileInfo << spec.index->bdb()->fileName();
    }

    foreach (ObjectTable *view, mViews) {
        fileInfo << view->bdb()->fileName();
        foreach (const IndexSpec &spec, view->indexSpecs().values()) {
            if (spec.index->bdb())
                fileInfo << spec.index->bdb()->fileName();
        }
    }

    QHash<QString, qint64> result;
    foreach (const QFileInfo &info, fileInfo)
        result.insert(info.fileName(), info.size());
    return result;
}

static bool sDebugQuery = (::getenv("JSONDB_DEBUG_QUERY") ? (QLatin1String(::getenv("JSONDB_DEBUG_QUERY")) == "true") : false);

IndexQuery *IndexQuery::indexQuery(JsonDbBtreeStorage *storage, ObjectTable *table,
                                   const QString &propertyName, const QString &propertyType,
                                   const JsonDbOwner *owner, bool ascending)
{
    if (propertyName == JsonDbString::kUuidStr)
        return new UuidQuery(storage, table, propertyName, owner, ascending);
    else
        return new IndexQuery(storage, table, propertyName, propertyType, owner, ascending);
}

UuidQuery::UuidQuery(JsonDbBtreeStorage *storage, ObjectTable *table, const QString &propertyName, const JsonDbOwner *owner, bool ascending)
    : IndexQuery(storage, table, propertyName, QString(), owner, ascending)
{
}

IndexQuery::IndexQuery(JsonDbBtreeStorage *storage, ObjectTable *table,
                       const QString &propertyName, const QString &propertyType,
                       const JsonDbOwner *owner, bool ascending)
    : mStorage(storage)
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
        mBdbIndex = table->indexSpec(propertyName)->index->bdb();
        mCursor = new QBtreeCursor(mBdbIndex->btree());
    } else {
        mCursor = new QBtreeCursor(table->bdb()->btree(), true);
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

QString IndexQuery::partition() const
{
    return mStorage->name();
}

quint32 IndexQuery::stateNumber() const
{
    return mBdbIndex->tag();
}

bool IndexQuery::matches(const QJsonValue &fieldValue)
{
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        if (!mQueryConstraints[i]->matches(fieldValue))
            return false;
    }
    return true;
}

void IndexQuery::setMin(const QJsonValue &value)
{
    mMin = makeFieldValue(value, mPropertyType);
}

void IndexQuery::setMax(const QJsonValue &value)
{
    mMax = makeFieldValue(value, mPropertyType);
}

bool IndexQuery::seekToStart(QJsonValue &fieldValue)
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
        if (!mMin.isUndefined()) {
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
        mCursor->current(&baKey, 0);
        forwardKeySplit(baKey, fieldValue);
    }
    //qDebug() << "IndexQuery::seekToStart" << (mAscending ? mMin : mMax) << "ok" << ok << fieldValue;
    return ok;
}

bool IndexQuery::seekToNext(QJsonValue &fieldValue)
{
    bool ok = mAscending ? mCursor->next() : mCursor->prev();
    if (ok) {
        QByteArray baKey;
        mCursor->current(&baKey, 0);
        forwardKeySplit(baKey, fieldValue);
    }
    //qDebug() << "IndexQuery::seekToNext" << "ok" << ok << fieldValue;
    return ok;
}

JsonDbObject IndexQuery::currentObjectAndTypeNumber(ObjectKey &objectKey)
{
    QByteArray baValue;
    mCursor->current(0, &baValue);
    forwardValueSplit(baValue, objectKey);

    if (sDebugQuery) qDebug() << __FILE__ << __LINE__ << "objectKey" << objectKey << baValue.toHex();
    JsonDbObject object;
    mObjectTable->get(objectKey, &object);
    return object;
}

quint32 UuidQuery::stateNumber() const
{
    return mObjectTable->stateNumber();
}

bool UuidQuery::seekToStart(QJsonValue &fieldValue)
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
            ok = mCursor->prev();
    }
    if (ok) {
        QUuid quuid(QUuid::fromRfc4122(baKey));
        ObjectKey objectKey(quuid);
        fieldValue = objectKey.key.toString();
    }
    return ok;
}

bool UuidQuery::seekToNext(QJsonValue &fieldValue)
{
    bool ok = mAscending ? mCursor->next() : mCursor->prev();
    QByteArray baKey;
    while (ok) {
        mCursor->current(&baKey, 0);
        if (baKey.size() == 16)
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
    return ok;
}

JsonDbObject UuidQuery::currentObjectAndTypeNumber(ObjectKey &objectKey)
{
    QByteArray baKey, baValue;
    mCursor->current(&baKey, &baValue);
    objectKey = ObjectKey(baKey);

    if (sDebugQuery) qDebug() << __FILE__ << __LINE__ << "objectKey" << objectKey << baKey.toHex();
    JsonDbObject object(QJsonDocument::fromBinaryData(baValue).object());
    return object;
}

JsonDbObject IndexQuery::first()
{
    mSparseMatchPossible = false;
    for (int i = 0; i < mQueryConstraints.size(); i++) {
        mSparseMatchPossible |= mQueryConstraints[i]->sparseMatchPossible();
    }

    QJsonValue fieldValue;
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
        JsonDbObject object(currentObjectAndTypeNumber(objectKey));
        if (sDebugQuery) qDebug() << "IndexQuery::first()" << __LINE__ << "objectKey" << objectKey << object.value(JsonDbString::kDeletedStr).toBool();
        if (object.contains(JsonDbString::kDeletedStr) && object.value(JsonDbString::kDeletedStr).toBool())
            continue;

        if (!mTypeNames.isEmpty() && !mTypeNames.contains(object.value(JsonDbString::kTypeStr).toString()))
            continue;
        if (sDebugQuery) qDebug() << "mTypeName" << mTypeNames << "!contains" << object << "->" << object.value(JsonDbString::kTypeStr);

        if (mResidualQuery && !mResidualQuery->match(object, &mObjectCache, mStorage))
            continue;

        if (sDebugQuery) qDebug() << "IndexQuery::first()" << "returning objectKey" << objectKey;

        return object;
    }
    mUuid.clear();
    return QJsonObject();
}
JsonDbObject IndexQuery::next()
{
    QJsonValue fieldValue;
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
        JsonDbObject object(currentObjectAndTypeNumber(objectKey));
        if (object.contains(JsonDbString::kDeletedStr) && object.value(JsonDbString::kDeletedStr).toBool())
            continue;

        if (!mTypeNames.isEmpty() && !mTypeNames.contains(object.value(JsonDbString::kTypeStr).toString()))
            continue;

        if (sDebugQuery) qDebug() << "IndexQuery::next()" << "objectKey" << objectKey;

        if (mResidualQuery && !mResidualQuery->match(object, &mObjectCache, mStorage))
            continue;

        return object;
    }
    mUuid.clear();
    return QJsonObject();
}

bool lessThan(const QJsonValue &a, const QJsonValue &b)
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

bool greaterThan(const QJsonValue &a, const QJsonValue &b)
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

class QueryConstraintGt: public QueryConstraint {
public:
    QueryConstraintGt(const QJsonValue &v) { mValue = v; }
    bool matches(const QJsonValue &v) { return greaterThan(v, mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintGe: public QueryConstraint {
public:
    QueryConstraintGe(const QJsonValue &v) { mValue = v; }
    bool matches(const QJsonValue &v) { return greaterThan(v, mValue) || (v == mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintLt: public QueryConstraint {
public:
    QueryConstraintLt(const QJsonValue &v) { mValue = v; }
    bool matches(const QJsonValue &v) { return lessThan(v, mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintLe: public QueryConstraint {
public:
    QueryConstraintLe(const QJsonValue &v) { mValue = v; }
    bool matches(const QJsonValue &v) { return lessThan(v, mValue) || (v == mValue); }
private:
    QJsonValue mValue;
};
class QueryConstraintEq: public QueryConstraint {
public:
    QueryConstraintEq(const QJsonValue &v) { mValue = v; }
    bool matches(const QJsonValue &v) { return v == mValue; }
private:
    QJsonValue mValue;
};
class QueryConstraintNe: public QueryConstraint {
public:
    QueryConstraintNe(const QJsonValue &v) { mValue = v; }
    bool sparseMatchPossible() const { return true; }
    bool matches(const QJsonValue &v) { return v != mValue; }
private:
    QJsonValue mValue;
};
class QueryConstraintExists: public QueryConstraint {
public:
    QueryConstraintExists() { }
    bool matches(const QJsonValue &v) { return !v.isUndefined(); }
private:
};
class QueryConstraintNotExists: public QueryConstraint {
public:
    QueryConstraintNotExists() { }
    // this will never match
    bool matches(const QJsonValue &v) { return v.isUndefined(); }
private:
};
class QueryConstraintIn: public QueryConstraint {
public:
    QueryConstraintIn(const QJsonValue &v) { mList = v.toArray();}
    bool sparseMatchPossible() const { return true; }

    bool matches(const QJsonValue &v) {
        return mList.contains(v);
    }
private:
    QJsonArray mList;
};
class QueryConstraintNotIn: public QueryConstraint {
public:
    QueryConstraintNotIn(const QJsonValue &v) { mList = v.toArray();}
    bool sparseMatchPossible() const { return true; }
    bool matches(const QJsonValue &v) {
        return !mList.contains(v);
    }
private:
    QJsonArray mList;
};
class QueryConstraintContains: public QueryConstraint {
public:
    QueryConstraintContains(const QJsonValue &v) { mValue = v;}
    bool sparseMatchPossible() const { return true; }
    bool matches(const QJsonValue &v) {
        return v.toArray().contains(mValue);
    }
private:
    QJsonValue mValue;
};
class QueryConstraintStartsWith: public QueryConstraint {
public:
    QueryConstraintStartsWith(const QString &v) { mValue = v;}
    bool sparseMatchPossible() const { return true; }
    bool matches(const QJsonValue &v) {
        return (v.type() == QJsonValue::String) && v.toString().startsWith(mValue);
    }
private:
    QString mValue;
};
class QueryConstraintRegExp: public QueryConstraint {
public:
    QueryConstraintRegExp(const QRegExp &regexp) : mRegExp(regexp) {}
    bool matches(const QJsonValue &v) { return mRegExp.exactMatch(v.toString()); }
    bool sparseMatchPossible() const { return true; }
private:
    QString mValue;
    QRegExp mRegExp;
};

void JsonDbBtreeStorage::compileOrQueryTerm(IndexQuery *indexQuery, const QueryTerm &queryTerm)
{
    QString op = queryTerm.op();
    QJsonValue fieldValue = queryTerm.value();
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
        const QRegExp &re = queryTerm.regExpConst();
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
        indexQuery->addConstraint(new QueryConstraintExists);
    } else if (op == "notExists") {
        indexQuery->addConstraint(new QueryConstraintNotExists);
    } else if (op == "in") {
        QJsonArray value = queryTerm.value().toArray();
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
                        if (op == "=") {
                            types << fieldValue;
                            for (int i = 1; i < queryTerms.size(); ++i) {
                                if (queryTerms[i].propertyName() == JsonDbString::kTypeStr && queryTerms[i].op() == "=")
                                    types << queryTerms[i].value().toString();
                            }
                        } else {
                            QJsonArray array = queryTerm.value().toArray();
                            types.clear();
                            for (int t = 0; t < array.size(); t++)
                                types << array.at(t).toString();
                        }

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
                        else {
                            QJsonArray array = queryTerm.value().toArray();
                            for (int t = 0; t < array.size(); t++)
                                types << array.at(t).toString();
                        }
                    }
                }
            }
        }
    }
    if ((typeNames.size() == 1)
        && mViews.contains(typeNames.toList()[0]))
        table = mViews[typeNames.toList()[0]];
    if (!indexedQueryTermCount && !indexCandidate.isEmpty()) {
            if (gDebug)
                qDebug() << "adding index" << indexCandidate;
            table->addIndexOnProperty(indexCandidate);
    }

    for (int i = 0; i < orderTerms.size(); i++) {
        const OrderTerm &orderTerm = orderTerms[i];
        QString propertyName = orderTerm.propertyName;
        if (!table->indexSpec(propertyName)) {
            if (gVerbose || gPerformanceLog) qDebug() << "Unindexed sort term" << propertyName << orderTerm.ascending;
            if (0) {
                if (gVerbose) qDebug() << "adding index for sort term" << propertyName;
                Q_ASSERT(table);
                table->addIndexOnProperty(propertyName);
                Q_ASSERT(table->indexSpec(propertyName));
                if (gVerbose) qDebug() << "done adding index" << propertyName;
            } else {
                residualQuery->orderTerms.append(orderTerm);
                continue;
            }
        }
        if (!indexQuery) {
            orderField = propertyName;
            const IndexSpec *indexSpec = table->indexSpec(propertyName);
            updateView(table);

            if (indexSpec->lazy)
                updateIndex(table, indexSpec->index);
            indexQuery = IndexQuery::indexQuery(this, table, propertyName, indexSpec->propertyType,
                                                owner, orderTerm.ascending);
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
                indexQuery = IndexQuery::indexQuery(this, table, propertyName, indexSpec->propertyType, owner);
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
        indexQuery = IndexQuery::indexQuery(this, table, defaultIndex, indexSpec->propertyType, owner);
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

void JsonDbBtreeStorage::doIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                                      IndexQuery *indexQuery)
{
    if (sDebugQuery) qDebug() << "doIndexQuery" << "limit" << limit << "offset" << offset;
    bool countOnly = (indexQuery->aggregateOperation() == "count");
    int count = 0;
    for (JsonDbObject object = indexQuery->first();
         !object.isEmpty();
         object = indexQuery->next()) {
        if (!owner->isAllowed(object, indexQuery->partition(), QString("read")))
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
        QJsonObject countObject;
        countObject.insert(QLatin1String("count"), count);
        results.append(countObject);
    }
}

static int findMinHead(QList<JsonDbObject> &heads, QStringList path, bool ascending)
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

void JsonDbBtreeStorage::doMultiIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                                           const QList<IndexQuery *> &indexQueries)
{
    Q_ASSERT(indexQueries.size());
    if (indexQueries.size() == 1)
        return doIndexQuery(owner, results, limit, offset, indexQueries[0]);

    QList<JsonDbObject> heads;
    QList<IndexQuery*> queries;
    int nPartitions = indexQueries.size();

    QString field0 = indexQueries[0]->propertyName();
    QStringList path0 = field0.split('.');
    bool ascending = indexQueries[0]->ascending();

    if (sDebugQuery) qDebug() << "doMultiIndexQuery" << "limit" << limit << "offset" << offset << "propertyName" << indexQueries[0]->propertyName();

    for (int i = 0; i < nPartitions; i++) {
        JsonDbObject object = indexQueries[i]->first();
        if (!object.isEmpty()) {
            heads.append(object);
            queries.append(indexQueries[i]);
        }
    }

    bool countOnly = (indexQueries[0]->aggregateOperation() == "count");
    int count = 0;
    int s = 0;
    while (queries.size()) {
        JsonDbObject object;
        // if this is an ordered query, should take the minimum of the values
        s = findMinHead(heads, path0, ascending);
        object = heads[s];
        heads[s] = queries[s]->next();
        QString partition = queries[s]->partition();
        if (heads[s].isEmpty()) {
            heads.takeAt(s);
            queries.takeAt(s);
        }
        if (object.isEmpty())
            break;

        if (!owner->isAllowed(object, partition, QString("read")))
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
        QJsonObject countObject;
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
    bool ok = mBdbIndexes->getOne(baKey, &baValue);
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
    if (mBdbIndexes->getOne(baKey, &baValue) && baValue.size() == 4)
        value = qFromLittleEndian<quint32>((const uchar *)baValue.constData());
    value += size;
    baValue.resize(4);
    qToLittleEndian(value, (uchar *)baValue.data());
    mBdbIndexes->putOne(baKey, baValue);
    return true;
}

JsonDbQueryResult JsonDbBtreeStorage::queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit, int offset)
{
    JsonDbObjectList results;
    JsonDbObjectList joinedResults;

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

    QJsonArray sortKeys;
    sortKeys.append(indexQuery->propertyName());
    sortKeys.append(indexQuery->objectTable()->filename());

    delete indexQuery;

    JsonDbQueryResult result;
    result.length = length;
    result.offset = offset;
    result.data = results;
    result.state = (qint32)stateNumber;
    result.sortKeys = sortKeys;
    int elapsedToDone = time.elapsed();
    if (gVerbose)
        qDebug() << "elapsed" << elapsedToCompile << elapsedToQuery << elapsedToDone << query.query;
    return result;
}

JsonDbQueryResult JsonDbBtreeStorage::queryPersistentObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit, int offset, QList<JsonDbBtreeStorage *> partitions)
{
    Q_ASSERT(partitions.size());
    JsonDbObjectList results;
    JsonDbObjectList joinedResults;

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

    QJsonArray sortKeys;
    sortKeys.append(indexQueries[0]->propertyName());

    for (int i = 0; i < indexQueries.size(); i++)
        delete indexQueries[i];

    JsonDbQueryResult result;
    result.length = length;
    result.offset = offset;
    result.data = results;
    result.sortKeys = sortKeys;
    int elapsedToDone = time.elapsed();
    if (gVerbose)
        qDebug() << "elapsed" << elapsedToCompile << elapsedToQuery << elapsedToDone << query.query;
    return result;
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
    bool result = true;
    result &= mObjectTable->compact();
    if (mBdbIndexes)
        result &= mBdbIndexes->compact();
    return result;
}

QString JsonDbBtreeStorage::name() const
{
    return mPartitionName;
}
void JsonDbBtreeStorage::setName(const QString &name)
{
    mPartitionName = name;
}

struct QJsonSortable {
    QJsonValue key;
    QJsonObject result;
    QJsonObject joinedResult;
};

bool sortableLessThan(const QJsonSortable &a, const QJsonSortable &b)
{
    return lessThan(a.key, b.key);
}
bool sortableGreaterThan(const QJsonSortable &a, const QJsonSortable &b)
{
    return greaterThan(a.key, b.key);
}

void JsonDbBtreeStorage::sortValues(const JsonDbQuery *parsedQuery, JsonDbObjectList &results, JsonDbObjectList &joinedResults)
{
    const QList<OrderTerm> &orderTerms = parsedQuery->orderTerms;
    if (!orderTerms.size() || (results.size() < 2))
        return;
    const OrderTerm &orderTerm0 = orderTerms[0];
    QString field0 = orderTerm0.propertyName;
    bool ascending = orderTerm0.ascending;
    QStringList path0 = field0.split('.');

    if (orderTerms.size() == 1) {
        QVector<QJsonSortable> valuesToSort(results.size());
        int resultsSize = results.size();
        int joinedResultsSize = joinedResults.size();

        for (int i = 0; i < resultsSize; i++) {
            QJsonSortable *p = &valuesToSort[i];
            JsonDbObject r = results.at(i);
            p->key = JsonDb::propertyLookup(r, path0);
            p->result = r;
            if (joinedResultsSize > i)
                p->joinedResult = joinedResults.at(i);
        }

        if (ascending)
            qStableSort(valuesToSort.begin(), valuesToSort.end(), sortableLessThan);
        else
            qStableSort(valuesToSort.begin(), valuesToSort.end(), sortableGreaterThan);

        results = JsonDbObjectList();
        joinedResults = JsonDbObjectList();
        for (int i = 0; i < resultsSize; i++) {
            QJsonSortable *p = &valuesToSort[i];
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

QT_END_NAMESPACE_JSONDB
