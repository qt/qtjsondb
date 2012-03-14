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

#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>

#include "jsondbobjecttable.h"
#include "jsondbindex.h"
#include "jsondb-strings.h"
#include "jsondbmanagedbtree.h"
#include "jsondbobject.h"
#include "jsondbsettings.h"
#include "qbtreetxn.h"

QT_BEGIN_NAMESPACE_JSONDB

void makeStateKey(QByteArray &baStateKey, quint32 stateNumber)
{
    baStateKey.resize(5);
    char *data = baStateKey.data();
    data[4] = 'S';
    qToBigEndian(stateNumber, (uchar *)baStateKey.data());
}

bool isStateKey(const QByteArray &baStateKey)
{
    return (baStateKey.size() == 5)
        && (baStateKey.constData()[4] == 'S');
}

JsonDbObjectTable::JsonDbObjectTable(JsonDbPartition *partition) :
    QObject(partition)
  , mPartition(partition)
  , mBdb(0)
{
    mBdb = new JsonDbManagedBtree();
}

JsonDbObjectTable::~JsonDbObjectTable()
{
    delete mBdb;
    mBdb = 0;
}

bool JsonDbObjectTable::open(const QString&fileName, QBtree::DbFlags flags)
{
    mFilename = fileName;
#if 0
    if (!mBdb->setCmpFunc(objectKeyCmp, 0)) {
        qCritical() << "mBdb->setCmpFunc" << mBdb->errorMessage();
        return false;
    }
#endif
    mBdb->setCacheSize(jsondbSettings->cacheSize());
    if (!mBdb->open(mFilename, flags)) {
        qCritical() << "mBdb->open" << mBdb->errorMessage();
        return false;
    }
    mStateNumber = mBdb->tag();
    if (jsondbSettings->verbose())
        qDebug() << "ObjectTable::open" << mStateNumber << mFilename;
    return true;
}

void JsonDbObjectTable::close()
{
    mBdb->close();
}

bool JsonDbObjectTable::begin()
{
    Q_ASSERT(!mWriteTxn);
    mWriteTxn = mBdb->beginWrite();
    Q_ASSERT(mBdbTransactions.isEmpty());
    return mWriteTxn;
}

void JsonDbObjectTable::begin(JsonDbIndex *index)
{
    if (!index->bdb()->isWriteTxnActive())
        mBdbTransactions.append(index->begin());
}

bool JsonDbObjectTable::commit(quint32 tag)
{
    Q_ASSERT(mWriteTxn);
    //qDebug() << "ObjectTable::commit" << tag << mFilename;

    QByteArray baStateKey(5, 0);
    makeStateKey(baStateKey, tag);
    bool ok = mWriteTxn.put(baStateKey, mStateChanges);
    if (!ok)
        qDebug() << "putting statekey ok" << ok << "baStateKey" << baStateKey.toHex();
    for (int i = 0; i < mStateObjectChanges.size(); ++i) {
        JsonDbObject object = mStateObjectChanges.at(i);
        bool ok = mWriteTxn.put(baStateKey + object.uuid().toRfc4122(), object.toBinaryData());
        if (!ok) {
            qDebug() << "putting state object ok" << ok << "baStateKey" << baStateKey.toHex()
                     << "object" << object;
        }
    }
    mStateChanges.clear();
    mStateObjectChanges.clear();
    mStateNumber = tag;

    for (int i = 0; i < mBdbTransactions.size(); i++) {
        JsonDbManagedBtreeTxn txn = mBdbTransactions.at(i);
        if (!txn.commit(tag)) {
            qCritical() << __FILE__ << __LINE__ << txn.btree()->errorMessage();
        }
    }
    mBdbTransactions.clear();
    return mWriteTxn.commit(tag);
}

bool JsonDbObjectTable::abort()
{
    Q_ASSERT(mWriteTxn);
    mStateChanges.clear();
    mStateObjectChanges.clear();
    for (int i = 0; i < mBdbTransactions.size(); i++) {
        JsonDbManagedBtreeTxn txn = mBdbTransactions.at(i);
        txn.abort();
    }
    mBdbTransactions.clear();
    mWriteTxn.abort();
    return true;
}

bool JsonDbObjectTable::compact()
{
    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        const IndexSpec &indexSpec = it.value();
        // _uuid index does not have bdb() because it is actually the object table itself
        if (!indexSpec.index->bdb())
            continue;
        if (!indexSpec.index->bdb()->compact())
            return false;
    }
    return mBdb->compact();
}

void JsonDbObjectTable::sync(JsonDbObjectTable::SyncFlags flags)
{
    if (flags & SyncObjectTable)
        mBdb->btree()->sync();

    if (flags & SyncIndexes) {
        foreach (const IndexSpec &spec, mIndexes.values()) {
            JsonDbIndex *index = spec.index;
            if (index->bdb()) {
                quint32 stateNumber = index->stateNumber();
                if (stateNumber != mStateNumber) {
                    index->begin();
                    index->commit(mStateNumber);
                }
                spec.index->bdb()->btree()->sync();
            }
        }
    }
}

JsonDbStat JsonDbObjectTable::stat() const
{
    JsonDbStat result;
    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        const IndexSpec &indexSpec = it.value();
        if (indexSpec.index->bdb()) {
            QBtree::Stat stat = indexSpec.index->bdb()->stat();
            result += JsonDbStat(stat.reads, stat.hits, stat.writes);
        }
        // _uuid index does not have bdb() because it is actually the object table itself
    }
    QBtree::Stat stat = mBdb->stat();
    result += JsonDbStat(stat.reads, stat.hits, stat.writes);
    return result;
}


void JsonDbObjectTable::flushCaches()
{
    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        const IndexSpec &indexSpec = it.value();
        // _uuid index does not have bdb() because it is actually the object table itself
        if (!indexSpec.index->bdb())
            continue;
        indexSpec.index->bdb()->setCacheSize(1);
        indexSpec.index->bdb()->setCacheSize(jsondbSettings->cacheSize());
    }
}

IndexSpec *JsonDbObjectTable::indexSpec(const QString &indexName)
{
    //qDebug() << "ObjectTable::indexSpec" << propertyName << mFilename << (mIndexes.contains(propertyName) ? "exists" : "missing") << (long)this << mIndexes.keys();
    if (mIndexes.contains(indexName))
        return &mIndexes[indexName];
    else
        return 0;
}

QHash<QString, IndexSpec> JsonDbObjectTable::indexSpecs() const
{
    return mIndexes;
}

bool JsonDbObjectTable::addIndex(const QString &indexName, const QString &propertyName,
                           const QString &propertyType, const QString &objectType, const QString &propertyFunction,
                           const QString &locale, const QString &collation, Qt::CaseSensitivity caseSensitivity)
{
    Q_ASSERT(propertyName.isEmpty() ^ propertyFunction.isEmpty());

    QString name = indexName.isEmpty() ? propertyName : indexName;
    //qDebug() << "ObjectTable::addIndex" << propertyName << mFilename << (mIndexes.contains(propertyName) ? "exists" : "to be created");
    if (mIndexes.contains(name))
        return true;

    //if (gVerbose) qDebug() << "ObjectTable::addIndex" << propertyName << mFilename;

    QStringList path = propertyName.split('.');

    IndexSpec &indexSpec = mIndexes[name];
    indexSpec.name = name;
    indexSpec.propertyName = propertyName;
    indexSpec.path = path;
    indexSpec.propertyType = propertyType;
    indexSpec.locale = locale;
    indexSpec.collation = collation;
    indexSpec.caseSensitivity = caseSensitivity;
    indexSpec.objectType = objectType;
    indexSpec.lazy = false; //lazy;
    indexSpec.index = new JsonDbIndex(mFilename, name, propertyName, propertyType, locale, collation, caseSensitivity, this);
    if (!propertyFunction.isEmpty() && propertyName.isEmpty()) // propertyName takes precedence
        indexSpec.index->setPropertyFunction(propertyFunction);
    indexSpec.index->setCacheSize(jsondbSettings->cacheSize());
    bool indexExists = indexSpec.index->exists();
    indexSpec.index->open();

    QJsonObject indexObject;
    indexObject.insert(JsonDbString::kTypeStr, JsonDbString::kIndexTypeStr);
    indexObject.insert(JsonDbString::kNameStr, name);
    indexObject.insert(JsonDbString::kPropertyNameStr, propertyName);
    indexObject.insert(JsonDbString::kPropertyTypeStr, propertyType);
    indexObject.insert(JsonDbString::kLocaleStr, locale);
    indexObject.insert(JsonDbString::kCollationStr, collation);
    indexObject.insert(JsonDbString::kCaseSensitiveStr, (bool)caseSensitivity);
    indexObject.insert(JsonDbString::kObjectTypeStr, objectType);
    indexObject.insert("lazy", false);
    indexObject.insert(JsonDbString::kPropertyFunctionStr, propertyFunction);
    Q_ASSERT(!name.isEmpty());
    Q_ASSERT(mIndexes.contains(name));

    QByteArray baIndexObject;
    bool needsReindexing = !indexExists;
    if (indexExists && (indexSpec.index->stateNumber() != mStateNumber)) {
        needsReindexing = true;
        if (jsondbSettings->verbose())
            qDebug() << "Index" << name << "stateNumber" << indexSpec.index->stateNumber() << "objectTable.stateNumber" << mStateNumber << "reindexing" << "clearing";
        indexSpec.index->clearData();
    }
    if (needsReindexing)
        reindexObjects(name, path, stateNumber());

    return true;
}

bool JsonDbObjectTable::removeIndex(const QString &indexName)
{
    IndexSpec *spec = indexSpec(indexName);
    if (!spec)
        return false;

    QString propertyName = spec->propertyName;
    if (propertyName == JsonDbString::kUuidStr || propertyName == JsonDbString::kTypeStr)
        return true;

    if (spec->index) {
        if (spec->index->bdb()
            && spec->index->bdb()->isWriteTxnActive()) { // Incase index is removed via Jdb::remove( _type=Index )
            mBdbTransactions.remove(mBdbTransactions.indexOf(spec->index->bdb()->existingWriteTxn()));
            spec->index->abort();
        }
        spec->index->close();
        if (spec->index->bdb())
            QFile::remove(spec->index->bdb()->fileName());
        delete spec->index;
        mIndexes.remove(indexName);
    }

    return true;
}

void JsonDbObjectTable::reindexObjects(const QString &indexName, const QStringList &path, quint32 stateNumber)
{
    Q_ASSERT(mIndexes.contains(indexName));

    if (jsondbSettings->verbose())
        qDebug() << "reindexObjects" << indexName << "{";
    if (indexName == JsonDbString::kUuidStr) {
        return;
    }

    IndexSpec &indexSpec = mIndexes[indexName];
    JsonDbIndex *index = indexSpec.index;
    bool inTransaction = index->bdb()->isWriteTxnActive();
    bool inObjectTableTransaction = mBdb->isWriteTxnActive();
    QBtreeTxn *bdbTxn = 0;
    if (!inObjectTableTransaction)
         bdbTxn = mBdb->btree()->beginRead();
    else
         bdbTxn = const_cast<QBtreeTxn *>(mBdb->existingWriteTxn().txn());
    QBtreeCursor cursor(bdbTxn);
    if (!inTransaction)
        index->begin();
    for (bool ok = cursor.first(); ok; ok = cursor.next()) {
        QByteArray baKey, baObject;
        bool rok = cursor.current(&baKey, &baObject);
        Q_ASSERT(rok);
        if (baKey.size() != 16) // state key is 5 bytes, or history key is 5 + 16 bytes
            continue;
        ObjectKey objectKey(baKey);
        JsonDbObject object = QJsonDocument::fromBinaryData(baObject).object();
        if (object.value(JsonDbString::kDeletedStr).toBool())
            continue;
        QJsonValue fieldValue = object.propertyLookup(path);
        if (!fieldValue.isNull())
            index->indexObject(objectKey, object, stateNumber);
    }
    if (!inTransaction)
        index->commit(stateNumber);
    if (!inObjectTableTransaction)
        bdbTxn->abort();
    if (jsondbSettings->verbose())
        qDebug() << "} reindexObjects";
}

void JsonDbObjectTable::indexObject(const ObjectKey &objectKey, JsonDbObject object, quint32 stateNumber)
{
    if (jsondbSettings->debug())
        qDebug() << "ObjectTable::indexObject" << objectKey << object.value(JsonDbString::kVersionStr).toString() << endl << mIndexes.keys();
    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        Q_ASSERT(mWriteTxn);
        const IndexSpec &indexSpec = it.value();
        if (indexSpec.propertyName == JsonDbString::kUuidStr)
            continue;
        if (indexSpec.lazy)
            continue;
        indexSpec.index->indexObject(objectKey, object, stateNumber);
    }
}

void JsonDbObjectTable::deindexObject(const ObjectKey &objectKey, JsonDbObject object, quint32 stateNumber)
{
    if (jsondbSettings->debug())
        qDebug() << "ObjectTable::deindexObject" << objectKey << object.value(JsonDbString::kVersionStr).toString() << endl << mIndexes.keys();

    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        Q_ASSERT(mWriteTxn);
        const IndexSpec &indexSpec = it.value();
        if (jsondbSettings->debug())
            qDebug() << "ObjectTable::deindexObject" << indexSpec.propertyName;
        if (indexSpec.propertyName == JsonDbString::kUuidStr)
            continue;
        if (indexSpec.lazy)
            continue;
        indexSpec.index->deindexObject(objectKey, object, stateNumber);
    }
}

void JsonDbObjectTable::updateIndex(JsonDbIndex *index)
{
    quint32 indexStateNumber = qMax(1u, index->bdb()->tag());
    if (indexStateNumber == stateNumber())
        return;
    QJsonObject changes = changesSince(indexStateNumber).value("result").toObject();
    quint32 count = changes.value("count").toDouble();
    QJsonArray changeList = changes.value("changes").toArray();
    bool inTransaction = mBdb->isWriteTxnActive();
    if (!inTransaction)
        index->begin();
    else if (!index->bdb()->isWriteTxnActive()) {
        index->begin();
        mBdbTransactions.append(index->begin());
    }
    for (quint32 i = 0; i < count; i++) {
        QJsonObject change = changeList.at(i).toObject();
        JsonDbObject before = change.value("before").toObject();
        JsonDbObject after = change.value("after").toObject();
        ObjectKey objectKey(after.value(JsonDbString::kUuidStr).toString());
        if (!before.isEmpty())
            index->deindexObject(objectKey, before, stateNumber());
        if (!after.isEmpty())
            index->indexObject(objectKey, after, stateNumber());
    }
    if (!inTransaction)
        index->commit(stateNumber());
}


bool JsonDbObjectTable::get(const ObjectKey &objectKey, QJsonObject *object, bool includeDeleted)
{
    QByteArray baObjectKey(objectKey.toByteArray());
    QByteArray baObject;
    bool ok = mBdb->getOne(baObjectKey, &baObject);
    if (!ok)
        return false;
    QJsonObject o(QJsonDocument::fromBinaryData(baObject).object());
    if (!includeDeleted && o.value(JsonDbString::kDeletedStr).toBool())
        return false;
    *object = o;
    return true;
}

bool JsonDbObjectTable::put(const ObjectKey &objectKey, const JsonDbObject &object)
{
    QByteArray baObjectKey(objectKey.toByteArray());
    return mBdb->putOne(baObjectKey, object.toBinaryData());
}

bool JsonDbObjectTable::remove(const ObjectKey &objectKey)
{
    QByteArray baObjectKey(objectKey.toByteArray());
    return mBdb->removeOne(baObjectKey);
}

QString JsonDbObjectTable::errorMessage() const
{
    return mBdb->errorMessage();
}

GetObjectsResult JsonDbObjectTable::getObjects(const QString &keyName, const QJsonValue &keyValue, const QString &objectType)
{
    GetObjectsResult result;
    JsonDbObjectList objectList;
    bool typeSpecified = !objectType.isEmpty();

    if (keyName == JsonDbString::kUuidStr) {
        ObjectKey objectKey(keyValue.toString());
        JsonDbObjectList objectList;
        JsonDbObject object;
        bool ok = get(objectKey, &object);
        if (ok
            && (!typeSpecified
                || (object.value(JsonDbString::kTypeStr).toString() == objectType)))
            objectList.append(object);
        result.data = objectList;
        return result;
    }

    if (!mIndexes.contains(keyName) && (keyName != JsonDbString::kTypeStr)) {
        qDebug() << "ObjectTable::getObject" << "no index for" << keyName << mFilename;
        return result;
    }

    if (!mIndexes.contains(keyName) && (keyName == JsonDbString::kTypeStr)) {
        QBtreeTxn *txn = mBdb->btree()->beginRead();
        QBtreeCursor cursor(txn);
        for (bool ok = cursor.first(); ok; ok = cursor.next()) {
            QByteArray baKey, baObject;
            ok = cursor.current(&baKey, &baObject);
            if (!ok)
                break;
            if (baKey.size() != 16) // state key is 5 bytes, or history key is 5 + 16 bytes
                continue;
            JsonDbObject object = QJsonDocument::fromBinaryData(baObject).object();
            if (object.value(JsonDbString::kDeletedStr).toBool())
                continue;
            objectList.append(object);
        }
        txn->abort();
        result.data = objectList;
        return result;
    }

    const IndexSpec *indexSpec = &mIndexes[keyName];
    QByteArray forwardKey(makeForwardKey(makeFieldValue(keyValue, indexSpec->propertyType), ObjectKey()));
    //fprintf(stderr, "getObject bdb=%p\n", indexSpec->index->bdb());
    if (indexSpec->lazy)
        updateIndex(indexSpec->index);
    QBtreeTxn *txn;
    bool activeWriteTransaction = indexSpec->index->bdb()->isWriteTxnActive();
    if (activeWriteTransaction)
        txn = const_cast<QBtreeTxn *>(indexSpec->index->bdb()->existingWriteTxn().txn());
    else
        txn = indexSpec->index->bdb()->btree()->beginRead();
    QBtreeCursor cursor(txn);
    if (cursor.seekRange(forwardKey)) {
        do {
            QByteArray checkKey;
            QByteArray forwardValue;
            bool ok = cursor.current(&checkKey, &forwardValue);
            QJsonValue checkValue;
            forwardKeySplit(checkKey, checkValue);
            if (checkValue != keyValue)
                break;

            ObjectKey objectKey;
            forwardValueSplit(forwardValue, objectKey);
            if (jsondbSettings->debug() && jsondbSettings->verbose())
                qDebug() << "ok" << ok << "forwardValue" << forwardValue << "objectKey" << objectKey;

            JsonDbObject map;
            if (get(objectKey, &map)) {
                //qDebug() << "ObjectTable::getObject" << "deleted" << map.value(JsonDbString::kDeletedStr, false).toBool();
                if (map.contains(JsonDbString::kDeletedStr) && map.value(JsonDbString::kDeletedStr).toBool())
                    continue;
                if (typeSpecified && (map.value(JsonDbString::kTypeStr).toString() != objectType))
                    continue;

                objectList.append(map);
            } else {
                if (jsondbSettings->debug())
                    qDebug() << "Failed to get object" << objectKey << errorMessage();
            }
        } while (cursor.next());
    }
    if (!activeWriteTransaction)
        txn->abort();

    result.data = objectList;
    return result;
}

quint32 JsonDbObjectTable::storeStateChange(const ObjectKey &key, ObjectChange::Action action, const JsonDbObject &oldObject)
{
    quint32 stateNumber = mStateNumber + 1;

    int oldSize = mStateChanges.size();
    mStateChanges.resize(oldSize + 20);
    uchar *data = (uchar *)mStateChanges.data() + oldSize;

    qToBigEndian(key, data);
    qToBigEndian<quint32>(action, data+16);
    if (!oldObject.isEmpty())
        mStateObjectChanges.append(oldObject);
    return stateNumber;
}

quint32 JsonDbObjectTable::storeStateChange(const QList<ObjectChange> &changes)
{
    quint32 stateNumber = mStateNumber + 1;
    int oldSize = mStateChanges.size();
    mStateChanges.resize(oldSize + changes.size() * 20);
    uchar *data = (uchar *)mStateChanges.data() + oldSize;
    foreach (const ObjectChange &change, changes) {
        qToBigEndian(change.objectKey, data);
        qToBigEndian<quint32>(change.action, data+16);
        data += 20;
        if (!change.oldObject.isEmpty())
            mStateObjectChanges.append(change.oldObject);
    }
    return stateNumber;
}

void JsonDbObjectTable::changesSince(quint32 stateNumber, QMap<ObjectKey,ObjectChange> *changes)
{
    if (!changes)
        return;
    stateNumber = qMax(quint32(1), stateNumber+1);

    QElapsedTimer timer;
    if (jsondbSettings->performanceLog())
        timer.start();
    QBtreeTxn *txn = mBdb->btree()->beginRead();
    QBtreeCursor cursor(txn);
    QByteArray baStateKey(5, 0);
    makeStateKey(baStateKey, stateNumber);

    QMap<ObjectKey,ObjectChange> changeMap; // collect one change per uuid

    if (cursor.seekRange(baStateKey)) {
        do {
            QByteArray baObject;
            bool ok = cursor.current(&baStateKey, &baObject);
            if (!ok)
                break;

            if (!isStateKey(baStateKey))
                continue;
            stateNumber = qFromBigEndian<quint32>((const uchar *)baStateKey.constData());

            if (baObject.size() % 20 != 0) {
                qWarning() << __FUNCTION__ << __LINE__ << "state size must be a multiplier 20"
                           << baObject.size() << baObject.toHex();
                continue;
            }

            for (int i = 0; i < baObject.size() / 20; ++i) {
                const uchar *data = (const uchar *)baObject.constData() + i*20;
                ObjectKey objectKey = qFromBigEndian<ObjectKey>(data);
                quint32 action = qFromBigEndian<quint32>(data + 16);
                Q_ASSERT(action <= ObjectChange::LastAction);
                QByteArray baValue;
                QJsonObject oldObject;
                if ((action != ObjectChange::Created)
                    && mBdb->getOne(baStateKey + objectKey.key.toRfc4122(), &baValue)) {
                    oldObject = QJsonDocument::fromBinaryData(baValue).object();
                    Q_ASSERT(objectKey == ObjectKey(oldObject.value(JsonDbString::kUuidStr).toString()));
                }
                ObjectChange change(objectKey, ObjectChange::Action(action), oldObject);
                if (changeMap.contains(objectKey)) {
                    ObjectChange oldChange = changeMap.value(objectKey);
                    // create followed by delete cancels out
                    ObjectChange::Action newAction = ObjectChange::Action(action);
                    ObjectChange::Action oldAction = oldChange.action;
                    if ((oldAction == ObjectChange::Created)
                        && (newAction == ObjectChange::Deleted)) {
                        changeMap.remove(objectKey);
                    } else {
                        if ((oldAction == ObjectChange::Deleted)
                            && (newAction == ObjectChange::Created))
                            oldChange.action = ObjectChange::Updated;
                        changeMap.insert(objectKey, oldChange);
                    }
                } else {
                    changeMap.insert(objectKey, change);
                }
           }
        } while (cursor.next());
    }
    *changes = changeMap;
    txn->abort();

    if (jsondbSettings->performanceLog())
        qDebug() << "changesSince" << mFilename << timer.elapsed() << "ms";
}

QJsonObject JsonDbObjectTable::changesSince(quint32 stateNumber, const QSet<QString> &limitTypes)
{
    if (jsondbSettings->verbose())
        qDebug() << "changesSince" << stateNumber << "current state" << this->stateNumber();

    QJsonArray result;
    int count = 0;

    QMap<ObjectKey,ObjectChange> changes;
    changesSince(stateNumber, &changes);

    for (QMap<ObjectKey,ObjectChange>::const_iterator it = changes.begin();
         it != changes.end();
         ++it) {
        const ObjectChange &change = it.value();
        QJsonObject before;
        QJsonObject after;
        switch (change.action) {
        case ObjectChange::Created:
            get(change.objectKey, &after);
            break;
        case ObjectChange::Updated:
            before = change.oldObject;
            get(change.objectKey, &after);
            break;
        case ObjectChange::Deleted:
            before = change.oldObject;
            break;
        }
        if (!limitTypes.isEmpty()) {
            QString type = (after.isEmpty() ? before : after).value(JsonDbString::kTypeStr).toString();
            if (!limitTypes.contains(type))
                continue;
        }
        QJsonObject res;
        res.insert("before", before);
        res.insert("after", after);
        result.append(res);
        ++count;
    }
    QJsonObject resultmap, errormap;
    resultmap.insert("count", count);
    resultmap.insert("startingStateNumber", (qint32)stateNumber);
    resultmap.insert("currentStateNumber", (qint32)this->stateNumber());
    resultmap.insert("changes", result);
    QJsonObject changesSince(JsonDbPartition::makeResponse(resultmap, errormap));
    return changesSince;
}

QT_END_NAMESPACE_JSONDB
