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
#include "jsondbpartition_p.h"
#include "jsondbindex.h"
#include "jsondbindex_p.h"
#include "jsondbstrings.h"
#include "jsondbbtree.h"
#include "jsondbobject.h"
#include "jsondbsettings.h"
#include "qbtree.h"
#include "qbtreecursor.h"
#include "qbtreetxn.h"
#include "jsondbutils_p.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

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
    mBdb = new JsonDbBtree();
}

JsonDbObjectTable::~JsonDbObjectTable()
{
    delete mBdb;
    mBdb = 0;
}

bool JsonDbObjectTable::open(const QString &fileName)
{
    mFilename = fileName;
    mBdb->setCacheSize(jsondbSettings->cacheSize());
    mBdb->setFileName(mFilename);
    if (!mBdb->open()) {
        qCritical() << JSONDB_ERROR << "failed to open db" << mFilename << "with error" << mBdb->errorMessage();
        return false;
    }
    mStateNumber = mBdb->tag();
    if (jsondbSettings->verbose())
        qDebug() << JSONDB_INFO << "opened db" << mFilename << "with state number" << mStateNumber;
    return true;
}

void JsonDbObjectTable::close()
{
    mBdb->close();
}

bool JsonDbObjectTable::begin()
{
    Q_ASSERT(!mBdb->isWriting());
    Q_ASSERT(mBdbTransactions.isEmpty());
    return mBdb->beginWrite() != NULL;
}

void JsonDbObjectTable::begin(JsonDbIndex *index)
{
    if (!index->bdb()->isWriting())
        mBdbTransactions.append(index->begin());
}

bool JsonDbObjectTable::commit(quint32 stateNumber)
{
    Q_ASSERT(mBdb->isWriting());

    QByteArray baStateKey(5, 0);
    makeStateKey(baStateKey, stateNumber);
    bool ok = mBdb->writeTransaction()->put(baStateKey, mStateChanges);
    if (!ok)
        qDebug() << "putting statekey ok" << ok << "baStateKey" << baStateKey.toHex();
    for (int i = 0; i < mStateObjectChanges.size(); ++i) {
        const JsonDbUpdate &change = mStateObjectChanges.at(i);
        const JsonDbObject &oldObject = change.oldObject;
        if (!oldObject.isEmpty()) {
            bool ok = mBdb->writeTransaction()->put(baStateKey + oldObject.uuid().toRfc4122(), oldObject.toBinaryData());
            if (!ok)
                qDebug() << "putting state object ok" << ok << "baStateKey" << baStateKey.toHex()
                         << "object" << oldObject;
        }
        if (mChangeCache.size())
            mChangeCache.insert(stateNumber, change);
    }
    mStateChanges.clear();
    mStateObjectChanges.clear();
    mStateNumber = stateNumber;

    for (int i = 0; i < mBdbTransactions.size(); i++) {
        JsonDbBtree::Transaction *txn = mBdbTransactions.at(i);
        if (!txn->commit(stateNumber)) {
            qCritical() << __FILE__ << __LINE__ << txn->btree()->errorMessage();
        }
    }
    mBdbTransactions.clear();
    return mBdb->writeTransaction()->commit(stateNumber);
}

bool JsonDbObjectTable::abort()
{
    Q_ASSERT(mBdb->isWriting());
    mStateChanges.clear();
    mStateObjectChanges.clear();
    for (int i = 0; i < mBdbTransactions.size(); i++) {
        JsonDbBtree::Transaction *txn = mBdbTransactions.at(i);
        txn->abort();
    }
    mBdbTransactions.clear();
    mBdb->writeTransaction()->abort();
    return true;
}

bool JsonDbObjectTable::compact()
{
    foreach (JsonDbIndex *index, mIndexes) {
        // _uuid index does not have bdb() because it is actually the object table itself
        if (!index->bdb())
            continue;
        if (!index->bdb()->compact())
            return false;
    }
    return mBdb->compact();
}

bool JsonDbObjectTable::sync(JsonDbObjectTable::SyncFlags flags)
{
    if (flags & SyncObjectTable) {
        if (!mBdb->sync())
            return false;
    }

    if (flags & SyncIndexes) {
        foreach (JsonDbIndex *index, mIndexes) {
            bool wasOpen = index->isOpen();
            if (index->bdb()) {
                quint32 stateNumber = index->stateNumber();
                if (flags & SyncStateNumbers && stateNumber != mStateNumber) {
                    index->begin();
                    index->commit(mStateNumber);
                }

                if (!index->bdb()->sync())
                    return false;
            }
            if (!wasOpen && index->bdb())
                index->close();
        }
    }

    return true;
}

JsonDbStat JsonDbObjectTable::stat() const
{
    JsonDbStat result;
    foreach (JsonDbIndex *index, mIndexes) {
        if (index->bdb()) {
            JsonDbBtree::Stat stat = index->bdb()->btree() ?
                        index->bdb()->stats() :
                        JsonDbBtree::Stat();
            result += JsonDbStat(stat.reads, stat.hits, stat.writes);
        }
        // _uuid index does not have bdb() because it is actually the object table itself
    }
    JsonDbBtree::Stat stat = mBdb->btree() ? mBdb->btree()->stats() : JsonDbBtree::Stat();
    result += JsonDbStat(stat.reads, stat.hits, stat.writes);
    return result;
}


void JsonDbObjectTable::flushCaches()
{
    foreach (JsonDbIndex *index, mIndexes) {
        // _uuid index does not have bdb() because it is actually the object table itself
        if (!index->bdb())
            continue;
        index->bdb()->setCacheSize(1);
        index->bdb()->setCacheSize(jsondbSettings->cacheSize());
    }
    mChangeCache.clear();
}

void JsonDbObjectTable::closeIndexes()
{
    foreach (JsonDbIndex *index, mIndexes)
        index->close();
}

JsonDbIndex *JsonDbObjectTable::index(const QString &indexName)
{
    return mIndexes.value(indexName);
}

QList<JsonDbIndex *> JsonDbObjectTable::indexes() const
{
    return mIndexes.values();
}

bool JsonDbObjectTable::addIndex(const JsonDbIndexSpec &indexSpec)
{
    Q_ASSERT(indexSpec.propertyName.isEmpty() ^ indexSpec.propertyFunction.isEmpty());
    Q_ASSERT(!indexSpec.name.isEmpty());

    if (indexSpec.name.isEmpty())
        return false;

    if (mIndexes.contains(indexSpec.name))
        return true;

    JsonDbIndex *index = new JsonDbIndex(mFilename, this);
    index->setIndexSpec(indexSpec);
    index->setCacheSize(jsondbSettings->cacheSize());
    index->open(); // open it to read the state number
    mIndexes.insert(indexSpec.name, index);

    if (mStateNumber && (index->stateNumber() == 0 || index->stateNumber() != mStateNumber)) {
        if (jsondbSettings->verbose())
            qDebug() << JSONDB_INFO << "reindexing index" << indexSpec.name << "at stateNumber" << index->stateNumber() << ", objectTable.stateNumber at stateNumber" << mStateNumber;
        index->clearData();
        reindexObjects(indexSpec.name, stateNumber());
    }
    index->close(); // close it until it's actually needed

    return true;
}

bool JsonDbObjectTable::addIndexOnProperty(const QString &propertyName,
                                           const QString &propertyType,
                                           const QString &objectType)
{
    JsonDbIndexSpec indexSpec;
    indexSpec.name = propertyName;
    indexSpec.propertyName = propertyName;
    indexSpec.propertyType = propertyType;
    if (!objectType.isEmpty())
        indexSpec.objectTypes.append(objectType);
    return addIndex(indexSpec);
}

bool JsonDbObjectTable::removeIndex(const QString &indexName)
{
    if (indexName == JsonDbString::kUuidStr || indexName == JsonDbString::kTypeStr)
        return true;

    JsonDbIndex *index = mIndexes.take(indexName);
    if (!index)
        return false;

    if (index->bdb()
            && index->bdb()->isWriting()) { // Incase index is removed via Jdb::remove( _type=Index )
        mBdbTransactions.remove(mBdbTransactions.indexOf(index->bdb()->writeTransaction()));
        index->abort();
    }
    index->close();
    if (index->bdb())
        QFile::remove(index->bdb()->fileName());
    delete index;

    return true;
}

void JsonDbObjectTable::reindexObjects(const QString &indexName, quint32 stateNumber)
{
    Q_ASSERT(mIndexes.contains(indexName));

    if (indexName == JsonDbString::kUuidStr) {
        return;
    }

    if (jsondbSettings->verbose())
        qDebug() << JSONDB_INFO << "index" << indexName << "{";

    JsonDbIndex *index = mIndexes.value(indexName);
    Q_ASSERT(index != 0);
    bool isInIndexTransaction = index->bdb()->writeTransaction();
    bool isInObjectTableTransaction = mBdb->writeTransaction();
    JsonDbBtree::Transaction *bdbTxn = mBdb->writeTransaction() ? mBdb->writeTransaction() : mBdb->beginWrite();
    JsonDbBtree::Cursor cursor(bdbTxn);
    if (!isInIndexTransaction)
        index->begin();
    for (bool ok = cursor.first(); ok; ok = cursor.next()) {
        QByteArray baKey, baObject;
        if (!cursor.current(&baKey, &baObject))
            Q_ASSERT(false);
        if (baKey.size() != 16) // state key is 5 bytes, or history key is 5 + 16 bytes
            continue;
        ObjectKey objectKey(baKey);
        JsonDbObject object = QJsonDocument::fromBinaryData(baObject).object();
        if (object.value(JsonDbString::kDeletedStr).toBool())
            continue;
        QJsonValue fieldValue = object.valueByPath(index->indexSpec().propertyName);
        if (!fieldValue.isNull())
            index->indexObject(objectKey, object, stateNumber);
    }
    if (!isInIndexTransaction)
        index->commit(stateNumber);
    if (!isInObjectTableTransaction)
        bdbTxn->abort();
    if (jsondbSettings->verbose())
        qDebug() << "}";
}

void JsonDbObjectTable::indexObject(const ObjectKey &objectKey, JsonDbObject object, quint32 stateNumber)
{
    if (jsondbSettings->debug())
        qDebug() << "ObjectTable::indexObject" << objectKey << object.value(JsonDbString::kVersionStr).toString() << endl << mIndexes.keys();
    foreach (JsonDbIndex *index, mIndexes) {
        Q_ASSERT(mBdb->isWriting());
        const JsonDbIndexSpec &indexSpec = index->indexSpec();
        if (indexSpec.propertyName == JsonDbString::kUuidStr)
            continue;
        index->indexObject(objectKey, object, stateNumber);
    }
}

void JsonDbObjectTable::deindexObject(const ObjectKey &objectKey, JsonDbObject object, quint32 stateNumber)
{
    if (jsondbSettings->debug())
        qDebug() << "ObjectTable::deindexObject" << objectKey << object.value(JsonDbString::kVersionStr).toString() << endl << mIndexes.keys();

    foreach (JsonDbIndex *index, mIndexes) {
        Q_ASSERT(mBdb->isWriting());
        const JsonDbIndexSpec &indexSpec = index->indexSpec();
        if (jsondbSettings->debug())
            qDebug() << "ObjectTable::deindexObject" << indexSpec.propertyName;
        if (indexSpec.propertyName == JsonDbString::kUuidStr)
            continue;
        index->deindexObject(objectKey, object, stateNumber);
    }
}

void JsonDbObjectTable::updateIndex(JsonDbIndex *index)
{
    quint32 indexStateNumber = qMax(1u, index->bdb()->tag());
    if (indexStateNumber == stateNumber())
        return;
    JsonDbUpdateList changeList;
    changesSince(indexStateNumber, QSet<QString>(), &changeList);
    bool inTransaction = mBdb->isWriting();
    if (!inTransaction)
        index->begin();
    else if (!index->bdb()->isWriting()) {
        index->begin();
        mBdbTransactions.append(index->begin());
    }
    foreach (const JsonDbUpdate &change, changeList) {
        JsonDbObject before = change.oldObject;
        JsonDbObject after = change.newObject;
        ObjectKey objectKey(after.value(JsonDbString::kUuidStr).toString());
        if (!before.isEmpty())
            index->deindexObject(objectKey, before, stateNumber());
        if (!after.isDeleted())
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

    if (!mIndexes.contains(keyName) && keyName != JsonDbString::kTypeStr) {
        qDebug() << "ObjectTable::getObject" << "no index for" << keyName << mFilename;
        return result;
    }

    if (!mIndexes.contains(keyName) && keyName == JsonDbString::kTypeStr) {
        bool isInTransaction = mBdb->writeTransaction();
        JsonDbBtree::Transaction *txn = mBdb->writeTransaction() ? mBdb->writeTransaction() : mBdb->beginWrite();
        JsonDbBtree::Cursor cursor(txn);
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
        if (!isInTransaction)
            txn->abort();
        result.data = objectList;
        return result;
    }

    JsonDbIndex *index = mIndexes.value(keyName);
    Q_ASSERT(index != 0);
    QJsonValue fieldValue = JsonDbIndexPrivate::makeFieldValue(keyValue, index->indexSpec().propertyType);
    JsonDbIndexPrivate::truncateFieldValue(&fieldValue, index->indexSpec().propertyType);
    QByteArray forwardKey = JsonDbIndexPrivate::makeForwardKey(fieldValue, ObjectKey());
    bool isInTransaction = index->bdb()->writeTransaction();
    JsonDbBtree::Transaction *txn = index->bdb()->writeTransaction() ? index->bdb()->writeTransaction() : index->bdb()->beginWrite();
    JsonDbBtree::Cursor cursor(txn);
    if (cursor.seekRange(forwardKey)) {
        do {
            QByteArray checkKey;
            QByteArray forwardValue;
            bool ok = cursor.current(&checkKey, &forwardValue);
            QJsonValue checkValue;
            JsonDbIndexPrivate::forwardKeySplit(checkKey, checkValue);
            if (checkValue != fieldValue)
                break;

            ObjectKey objectKey;
            JsonDbIndexPrivate::forwardValueSplit(forwardValue, objectKey);
            if (jsondbSettings->debug() && jsondbSettings->verbose())
                qDebug() << JSONDB_INFO << "ok =" << ok << "forwardValue =" << forwardValue << "objectKey =" << objectKey;

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
                    qDebug() << JSONDB_WARN << "failed to get object" << objectKey << "with error " << errorMessage();
            }
        } while (cursor.next());
    }
    if (!isInTransaction)
        txn->abort();

    result.data = objectList;
    return result;
}

quint32 JsonDbObjectTable::storeStateChange(const ObjectKey &key, const JsonDbUpdate &change)
{
    quint32 stateNumber = mStateNumber + 1;

    int oldSize = mStateChanges.size();
    mStateChanges.resize(oldSize + 20);
    uchar *data = (uchar *)mStateChanges.data() + oldSize;

    qToBigEndian(key, data);
    qToBigEndian<quint32>(change.action, data+16);
    mStateObjectChanges.append(change);
    return stateNumber;
}

quint32 JsonDbObjectTable::changesSince(quint32 startingStateNumber, QMap<ObjectKey,JsonDbUpdate> *changes)
{
    if (!changes)
        return -1;
    startingStateNumber = qMax(quint32(1), startingStateNumber+1);
    quint32 currentStateNumber = stateNumber();

    QElapsedTimer timer;
    if (jsondbSettings->performanceLog())
        timer.start();

    // prune older changes
    // TODO: should do this more systematically
    if (!mChangeCache.isEmpty()) {
        quint32 firstChangeInCache = mChangeCache.begin().key();
        quint32 extraVersions = jsondbSettings->changeLogCacheVersions();
        if (firstChangeInCache + extraVersions < startingStateNumber) {
            if (jsondbSettings->verbose())
                qDebug() << "ChangeCache" << "dropping" << firstChangeInCache
                         << "to" << startingStateNumber - extraVersions << mFilename;
            for (quint32 i = firstChangeInCache; i < startingStateNumber - extraVersions; i++)
                while (mChangeCache.contains(i))
                    mChangeCache.remove(i);
        }
    }
    if (jsondbSettings->verbose() && !mChangeCache.contains(startingStateNumber) && startingStateNumber <= currentStateNumber) {
        if (!mChangeCache.isEmpty())
            qDebug() << "ChangesSince" << "fetching" << startingStateNumber << "to" << mChangeCache.begin().key() << "/" << currentStateNumber << mFilename;
        else
            qDebug() << "ChangesSince" << "fetching" << startingStateNumber << "to" << currentStateNumber << mFilename;
    }
    {
        bool inTransaction = mBdb->writeTransaction();
        JsonDbBtree::Transaction *txn = mBdb->writeTransaction() ? mBdb->writeTransaction() : mBdb->beginWrite();

        // read in the changes we need
        for (quint32 stateNumber = startingStateNumber; stateNumber <= currentStateNumber; stateNumber++) {

            // skip this one if we've already fetched this state number
            if (mChangeCache.contains(stateNumber))
                continue;

            QByteArray baStateKey(5, 0);
            makeStateKey(baStateKey, stateNumber);
            QByteArray baObject;
            bool ok = txn->get(baStateKey, &baObject);
            // if this table did not have a transaction on this state number, then continue
            if (!ok)
                continue;

            if (baObject.size() % 20 != 0) {
                qWarning() << __FUNCTION__ << __LINE__ << "state size must be a multiplier 20"
                           << baObject.size() << baObject.toHex();
                continue;
            }

            for (int i = 0; i < baObject.size() / 20; ++i) {
                const uchar *data = (const uchar *)baObject.constData() + i*20;
                ObjectKey objectKey = qFromBigEndian<ObjectKey>(data);
                QByteArray baObjectKey(objectKey.key.toRfc4122());
                quint32 action = qFromBigEndian<quint32>(data + 16);
                QByteArray baValue;
                QJsonObject oldObject;
                if ((action != JsonDbNotification::Create)
                        && mBdb->getOne(baStateKey + baObjectKey, &baValue)) {
                    oldObject = QJsonDocument::fromBinaryData(baValue).object();
                    Q_ASSERT(objectKey == ObjectKey(oldObject.value(JsonDbString::kUuidStr).toString()));
                }
                QJsonObject newObject;
                mBdb->getOne(baObjectKey, &baValue);
                newObject = QJsonDocument::fromBinaryData(baValue).object();
                if (jsondbSettings->debug())
                    qDebug() << "change" << action << endl << oldObject << endl << newObject;

                JsonDbUpdate change(oldObject, newObject, JsonDbNotification::Action(action));
                mChangeCache.insert(stateNumber, change);
            }

        }
        if (!inTransaction)
            txn->abort();
    }

    QMap<ObjectKey,JsonDbUpdate> changeMap; // collect one change per uuid
    for (QMultiMap<quint32,JsonDbUpdate>::const_iterator it = mChangeCache.lowerBound(startingStateNumber);
         it != mChangeCache.end(); ++it) {
        const JsonDbUpdate &change = it.value();
        const JsonDbObject newObject = change.newObject;
        ObjectKey objectKey(newObject.uuid());

        if (changeMap.contains(objectKey)) {
            const JsonDbUpdate &oldChange = changeMap.value(objectKey);
            // create followed by delete cancels out
            JsonDbNotification::Action newAction = change.action;
            JsonDbNotification::Action oldAction = oldChange.action;

            if ((oldAction == JsonDbNotification::Create)
                && (newAction == JsonDbNotification::Remove)) {
                changeMap.remove(objectKey);
            } else {
                JsonDbUpdate combinedChange;
                if (newAction == JsonDbNotification::Remove)
                    combinedChange.action = JsonDbNotification::Remove;
                else
                    combinedChange.action = JsonDbNotification::Update;
                combinedChange.oldObject = oldChange.oldObject;
                combinedChange.newObject = change.newObject;
                changeMap.insert(objectKey, combinedChange);
            }
        } else {
            changeMap.insert(objectKey, change);
        }
    }
    *changes = changeMap;

    if (jsondbSettings->performanceLog())
        qDebug() << "changesSince" << mFilename << timer.elapsed() << "ms";
    return mStateNumber;
}

quint32 JsonDbObjectTable::changesSince(quint32 stateNumber, const QSet<QString> &limitTypes, QList<JsonDbUpdate> *updateList, JsonDbObjectTable::TypeChangeMode splitTypeChanges)
{
    if (jsondbSettings->verbose())
        qDebug() << "changesSince" << stateNumber << limitTypes << "{";
    QMap<ObjectKey,JsonDbUpdate> changeSet;
    changesSince(stateNumber, &changeSet);
    QList<JsonDbUpdate> changeList;
    bool allTypes = limitTypes.isEmpty();
    foreach (const JsonDbUpdate &update, changeSet) {
        const JsonDbObject &oldObject = update.oldObject;
        const JsonDbObject &newObject = update.newObject;
        QString oldType = oldObject.value(JsonDbString::kTypeStr).toString();
        QString newType = newObject.value(JsonDbString::kTypeStr).toString();
        // if the types don't match, split into two updates
        if (!oldObject.isEmpty() && oldType != newType && splitTypeChanges == JsonDbObjectTable::SplitTypeChanges) {
            if (allTypes || limitTypes.contains(oldType)) {
                JsonDbObject tombstone(oldObject);
                tombstone.insert(JsonDbString::kDeletedStr, true);
                changeList.append(JsonDbUpdate(oldObject, tombstone, JsonDbNotification::Remove));
            }
            if (allTypes || limitTypes.contains(newType)) {
                changeList.append(JsonDbUpdate(JsonDbObject(), newObject, JsonDbNotification::Create));
            }
        } else if (allTypes || limitTypes.contains(oldType) || limitTypes.contains(newType)) {
            changeList.append(update);
        }
    }
    if (updateList)
        *updateList = changeList;
    if (jsondbSettings->verbose())
        qDebug() << "changesSince" << changeList.size() << "changes" << "}";
    return mStateNumber;
}

#include "moc_jsondbobjecttable.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
