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

#include <QFileInfo>
#include <QDir>

#include <QtJsonDbQson/private/qson_p.h>
#include <QtJsonDbQson/private/qsonparser_p.h>

#include "jsondb-trace.h"
#include "objecttable.h"
#include "jsondb.h"
#include "jsondbindex.h"
#include "jsondb-strings.h"

namespace QtAddOn { namespace JsonDb {

#ifndef QT_NO_DEBUG_OUTPUT
extern bool gDebug;
#define DBG() if (gDebug) qDebug() << Q_FUNC_INFO << __LINE__
#else
#define DBG() if (0) qDebug() << Q_FUNC_INFO
#endif

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

ObjectTable::ObjectTable(JsonDbBtreeStorage *storage)
    : QObject(storage), mStorage(storage), mBdb(0),  mInTransaction(false)
{
    mBdb = new AoDb();
}

ObjectTable::~ObjectTable()
{
    delete mBdb;
    mBdb = 0;
}

bool ObjectTable::open(const QString&fileName, QFlags<AoDb::DbFlag> flags)
{
    mFilename = fileName;
#if 0
    if (!mBdb->setCmpFunc(objectKeyCmp, 0)) {
        qCritical() << "mBdb->setCmpFunc" << mBdb->errorMessage();
        return false;
    }
#endif
    if (!mBdb->open(mFilename, flags)) {
        qCritical() << "mBdb->open" << mBdb->errorMessage();
        return false;
    }
    mStateNumber = mBdb->tag();
    if (gDebugRecovery) qDebug() << "ObjectTable::open" << mStateNumber << mFilename;
    return true;
}

void ObjectTable::close()
{
    mBdb->close();
}

bool ObjectTable::begin()
{
    Q_ASSERT(!mInTransaction);
    mInTransaction = true;
    bool ok = mBdb->begin();
    Q_ASSERT(mBdbTransactions.isEmpty());
    return ok;
}

bool ObjectTable::commit(quint32 tag)
{
    Q_ASSERT(mInTransaction);
    //qDebug() << "ObjectTable::commit" << tag << mFilename;

    QByteArray baStateKey(5, 0);
    makeStateKey(baStateKey, tag);
    bool ok = mBdb->put(baStateKey, mStateChanges);
    if (!ok)
        qDebug() << "putting statekey ok" << ok << "baStateKey" << baStateKey.toHex();
    for (int i = 0; i < mStateObjectChanges.size(); ++i) {
        QsonMap object = mStateObjectChanges.at(i);
        bool ok = mBdb->put(baStateKey + object.uuid().toRfc4122(), object.data());
        if (!ok) {
            qDebug() << "putting state object ok" << ok << "baStateKey" << baStateKey.toHex()
                     << "object" << object;
        }
    }
    mStateChanges.clear();
    mStateObjectChanges.clear();
    mStateNumber = tag;

    for (int i = 0; i < mBdbTransactions.size(); i++) {
      AoDb *bdb = mBdbTransactions.at(i);
      if (!bdb->commit(tag)) {
        qCritical() << __FILE__ << __LINE__ << bdb->errorMessage();
      }
    }
    mBdbTransactions.clear();
    mInTransaction = false;
    return mBdb->commit(tag);
}

bool ObjectTable::abort()
{
    Q_ASSERT(mInTransaction);
    mStateChanges.clear();
    mStateObjectChanges.clear();
    for (int i = 0; i < mBdbTransactions.size(); i++) {
      AoDb *bdb = mBdbTransactions.at(i);
      if (!bdb->abort()) {
        qCritical() << __FILE__ << __LINE__ << bdb->errorMessage();
      }
    }
    mBdbTransactions.clear();
    mInTransaction = false;
    return mBdb->abort();
}

bool ObjectTable::compact()
{
    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        const IndexSpec &indexSpec = it.value();
        if (!indexSpec.index->bdb()->compact())
            return false;
    }
    return mBdb->compact();
}

IndexSpec *ObjectTable::indexSpec(const QString &fieldName)
{
    //qDebug() << "ObjectTable::indexSpec" << fieldName << mFilename << (mIndexes.contains(fieldName) ? "exists" : "missing") << (long)this << mIndexes.keys();
    if (mIndexes.contains(fieldName))
        return &mIndexes[fieldName];
    else
        return 0;
}

QsonObject ObjectTable::addIndex(const QString &fieldname, const QString &fieldType, const QString &objectType, bool lazy)
{
    //qDebug() << "ObjectTable::addIndex" << fieldname << mFilename << (mIndexes.contains(fieldname) ? "exists" : "to be created");
    if (mIndexes.contains(fieldname))
        return QsonObject();

    //if (gVerbose) qDebug() << "ObjectTable::addIndex" << fieldname << mFilename;

    QStringList path = fieldname.split('.');

    IndexSpec &indexSpec = mIndexes[fieldname];
    indexSpec.fieldName = fieldname;
    indexSpec.path = path;
    indexSpec.fieldType = fieldType;
    indexSpec.objectType = objectType;
    indexSpec.lazy = false; //lazy;
    indexSpec.index = new JsonDbIndex(mFilename, fieldname, this);
    indexSpec.index->open();

    QsonMap indexObject;
    indexObject.insert(JsonDbString::kTypeStr, kIndexTypeStr);
    indexObject.insert(kFieldStr, fieldname);
    indexObject.insert(kFieldTypeStr, fieldType);
    indexObject.insert(kObjectTypeStr, objectType);
    indexObject.insert("lazy", lazy);
    Q_ASSERT(mIndexes.contains(fieldname));

    QByteArray baIndexObject;
    bool needsReindexing = false;
    if (!mStorage->mBdbIndexes->get(fieldname.toLatin1(), baIndexObject)) {
        baIndexObject = indexObject.data();
        bool ok = mStorage->mBdbIndexes->put(fieldname.toLatin1(), baIndexObject);
        if (gDebugRecovery) qDebug() << "Index" << fieldname << "is new" << "reindexing";
        needsReindexing = true;
        Q_ASSERT(ok);
    } else if (fieldname == JsonDbString::kUuidStr) {
        // nothing more to do
        return QsonObject();
    } else if (indexSpec.index->stateNumber() != mStateNumber) {
        needsReindexing = true;
        if (gDebugRecovery) qDebug() << "Index" << fieldname << "stateNumber" << indexSpec.index->stateNumber() << "objectTable.stateNumber" << mStateNumber << "reindexing" << "clearing";
        indexSpec.index->clear();
    }
    if (needsReindexing)
        reindexObjects(fieldname, path, stateNumber());

    return QsonObject();
}

QsonObject ObjectTable::removeIndex(const QString &fieldname, const QString &fieldType, const QString &objectType)
{
    if (!mIndexes.contains(fieldname) || fieldname == JsonDbString::kUuidStr || fieldname == JsonDbString::kTypeStr)
        return QsonObject();

    IndexSpec &indexSpec = mIndexes[fieldname];
    if (indexSpec.fieldType != fieldType || indexSpec.objectType != objectType)
        return QsonMap();

    if (mStorage->mBdbIndexes->remove(fieldname.toLatin1())) {
        if (indexSpec.index->bdb()->isTransaction()) { // Incase index is removed via Jdb::remove( _type=Index )
            indexSpec.index->abort();
            mBdbTransactions.remove(mBdbTransactions.indexOf(indexSpec.index->bdb()));
        }
        indexSpec.index->close();
        QFile::remove(indexSpec.index->bdb()->fileName());
        delete indexSpec.index;
        mIndexes.remove(fieldname);
    }

    return QsonObject();
}

void ObjectTable::reindexObjects(const QString &fieldName, const QStringList &path, quint32 stateNumber, bool inTransaction)
{
    if (gDebugRecovery) qDebug() << "reindexObjects" << fieldName << "{";
    if (fieldName == JsonDbString::kUuidStr) {
        qCritical() << "} ObjectTable::reindexObject" << "no need to reindex _uuid";
        return;
    }

    IndexSpec &indexSpec = mIndexes[fieldName];
    JsonDbIndex *index = indexSpec.index;

    AoDbCursor cursor(mBdb);
    if (!inTransaction)
        index->begin();
    for (bool ok = cursor.first(); ok; ok = cursor.next()) {
        QByteArray baKey, baObject;
        bool ok = cursor.current(baKey, baObject);
        Q_ASSERT(ok);
        if (baKey.size() != 16) // state key is 5 bytes, or history key is 5 + 16 bytes
            continue;
        ObjectKey objectKey(baKey);
        QsonMap object = QsonParser::fromRawData(baObject);
        if (object.valueBool(JsonDbString::kDeletedStr, false))
            continue;
        QVariant fieldValue = JsonDb::propertyLookup(object, path);
        if (fieldValue.isValid()) {
            index->indexObject(objectKey, object, stateNumber, true);
        }
    }
    if (!inTransaction)
        index->commit(stateNumber);
    if (gDebugRecovery) qDebug() << "} reindexObjects";
}

void ObjectTable::indexObject(const ObjectKey &objectKey, QsonMap object, quint32 stateNumber)
{
    if (gDebug) qDebug() << "ObjectTable::indexObject" << objectKey << object.valueString(JsonDbString::kVersionStr) << endl << mIndexes.keys();
    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        Q_ASSERT(mInTransaction);
        const IndexSpec &indexSpec = it.value();
        if (indexSpec.fieldName == JsonDbString::kUuidStr)
            continue;
        if (indexSpec.lazy)
            continue;
        if (!mBdbTransactions.contains(indexSpec.index->bdb())) {
            indexSpec.index->begin();
            mBdbTransactions.append(indexSpec.index->bdb());
        }
        indexSpec.index->indexObject(objectKey, object, stateNumber, true);
    }
}

void ObjectTable::deindexObject(const ObjectKey &objectKey, QsonMap object, quint32 stateNumber)
{
    if (gDebug) qDebug() << "ObjectTable::deindexObject" << objectKey << object.valueString(JsonDbString::kVersionStr) << endl << mIndexes.keys();

    for (QHash<QString,IndexSpec>::const_iterator it = mIndexes.begin();
         it != mIndexes.end();
         ++it) {
        const IndexSpec &indexSpec = it.value();
        if (gDebug) qDebug() << "ObjectTable::deindexObject" << indexSpec.fieldName;
        if (indexSpec.fieldName == JsonDbString::kUuidStr)
            continue;
        if (indexSpec.lazy)
            continue;
        Q_ASSERT(mInTransaction);
        if (!mBdbTransactions.contains(indexSpec.index->bdb())) {
            indexSpec.index->begin();
            mBdbTransactions.append(indexSpec.index->bdb());
        }
        indexSpec.index->deindexObject(objectKey, object, stateNumber, true);
    }
}

void ObjectTable::updateIndex(JsonDbIndex *index)
{
    quint32 indexStateNumber = qMax(1u, index->bdb()->tag());
    if (indexStateNumber == stateNumber())
        return;
    QsonMap changes = changesSince(indexStateNumber).subObject("result");
    quint32 count = changes.valueInt("count", 0);
    QsonList changeList = changes.subList("changes");
    if (!mInTransaction)
        index->begin();
    else if (!mBdbTransactions.contains(index->bdb())) {
        index->begin();
        mBdbTransactions.append(index->bdb());
    }
    for (quint32 i = 0; i < count; i++) {
        QsonMap change = changeList.objectAt(i).toMap();
        QsonMap before = change.subObject("before").toMap();
        QsonMap after = change.subObject("after").toMap();
        ObjectKey objectKey(after.uuid());
        if (!before.isEmpty())
            index->deindexObject(objectKey, before, stateNumber(), true);
        if (!after.isEmpty())
            index->indexObject(objectKey, after, stateNumber(), true);
    }
    if (!mInTransaction)
        index->commit(stateNumber());
}


bool ObjectTable::get(const ObjectKey &objectKey, QsonMap &object, bool includeDeleted)
{
    QByteArray baObjectKey(objectKey.toByteArray());
    QByteArray baObject;
    bool ok = mBdb->get(baObjectKey, baObject);
    if (!ok)
        return false;
    QsonMap o = QsonParser::fromRawData(baObject).toMap();
    if (!includeDeleted && o.valueBool(JsonDbString::kDeletedStr, false))
        return false;
    object = o;
    return true;
}

bool ObjectTable::put(const ObjectKey &objectKey, QsonObject &object)
{
    QByteArray baObjectKey(objectKey.toByteArray());
    return mBdb->put(baObjectKey, object.data());
}

bool ObjectTable::remove(const ObjectKey &objectKey)
{
    QByteArray baObjectKey(objectKey.toByteArray());
    return mBdb->remove(baObjectKey);
}

QString ObjectTable::errorMessage() const
{
    return mBdb->errorMessage();
}

QsonMap ObjectTable::getObjects(const QString &keyName, const QVariant &keyValue, const QString &objectType)
{
    QsonMap resultmap;
    QsonList objectList;
    bool typeSpecified = !objectType.isEmpty();

    if (keyName == JsonDbString::kUuidStr) {
        ObjectKey objectKey(keyValue.toString());
        QsonList objectList;
        QsonMap object;
        bool ok = get(objectKey, object);
        if (ok)
            objectList.append(object);
        resultmap.insert(QByteArray("result"), objectList);
        resultmap.insert(JsonDbString::kCountStr, objectList.size());
        return resultmap;
    }

    if (!mIndexes.contains(keyName)) {
        qDebug() << "ObjectTable::getObject" << "no index for" << keyName << mFilename;
        resultmap.insert(JsonDbString::kCountStr, 0);
        return resultmap;
    }
    const IndexSpec *indexSpec = &mIndexes[keyName];
    QByteArray forwardKey(makeForwardKey(keyValue, ObjectKey()));
    //fprintf(stderr, "getObject bdb=%p\n", indexSpec->index->bdb());
    if (indexSpec->lazy)
        updateIndex(indexSpec->index);
    AoDbCursor cursor(indexSpec->index->bdb());
    if (cursor.seekRange(forwardKey)) {
        do {
            QByteArray serializedObject;
            QByteArray checkKey;
            QByteArray forwardValue;
            bool ok = cursor.current(checkKey, forwardValue);
            QVariant checkValue;
            forwardKeySplit(checkKey, checkValue);
            if (checkValue != keyValue)
                break;

            ObjectKey objectKey;
            forwardValueSplit(forwardValue, objectKey);
            DBG() << "ok" << ok << "forwardValue" << forwardValue << "objectKey" << objectKey;

            QsonMap map;
            if (get(objectKey, map)) {
                //qDebug() << "ObjectTable::getObject" << "deleted" << map.valueBool(JsonDbString::kDeletedStr, false);
                if (map.contains(JsonDbString::kDeletedStr) && map.valueBool(JsonDbString::kDeletedStr, false))
                    continue;
                if (typeSpecified && (map.valueString(JsonDbString::kTypeStr) != objectType))
                    continue;

                objectList.append(map);
            } else {
              DBG() << "Failed to get object" << objectKey << errorMessage();
            }
        } while (cursor.next());
    }

    resultmap.insert(QByteArray("result"), objectList);
    resultmap.insert(JsonDbString::kCountStr, objectList.size());
    return resultmap;
}

quint32 ObjectTable::storeStateChange(const ObjectKey &key, ObjectChange::Action action, const QsonMap &oldObject)
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

quint32 ObjectTable::storeStateChange(const QList<ObjectChange> &changes)
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

void ObjectTable::changesSince(quint32 stateNumber, QMap<quint32, QList<ObjectChange> > *changes)
{
    if (!changes)
        return;
    stateNumber = qMax(quint32(1), stateNumber+1);

    AoDbCursor cursor(mBdb);
    QByteArray baStateKey(5, 0);
    makeStateKey(baStateKey, stateNumber);

    if (cursor.seekRange(baStateKey)) {
        do {
            QByteArray baObject;
            bool ok = cursor.current(baStateKey, baObject);
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

            QList<ObjectChange> ch;
            for (int i = 0; i < baObject.size() / 20; ++i) {
                const uchar *data = (const uchar *)baObject.constData() + i*20;
                ObjectKey objectKey = qFromBigEndian<ObjectKey>(data);
                quint32 action = qFromBigEndian<quint32>(data + 16);
                Q_ASSERT(action <= ObjectChange::LastAction);
                QByteArray baValue;
                QsonMap oldObject;
                if (mBdb->get(baStateKey + objectKey.key.toRfc4122(), baValue)) {
                    oldObject = QsonParser::fromRawData(baValue);
                    Q_ASSERT(objectKey == ObjectKey(oldObject.uuid()));
                }
                ch.append(ObjectChange(objectKey, ObjectChange::Action(action), oldObject));
            }
            if (changes)
                changes->insert(stateNumber, ch);
        } while (cursor.next());
    }
}

QsonMap ObjectTable::changesSince(quint32 stateNumber, const QSet<QString> &limitTypes)
{
    if (gVerbose)
        qDebug() << "changesSince" << stateNumber << "current state" << this->stateNumber();

    QsonList result;
    int count = 0;

    QMap<quint32, QList<ObjectChange> > changes;
    changesSince(stateNumber, &changes);

    QMap<quint32, QList<ObjectChange> >::const_iterator it, e;
    for (it = changes.begin(), e = changes.end(); it != e; ++it) {
        const QList<ObjectChange> &changes = it.value();
        for (int i = 0; i < changes.size(); ++i) {
            const ObjectChange &change = changes.at(i);
            QsonMap before;
            QsonMap after;
            switch (change.action) {
            case ObjectChange::Created:
                get(change.objectKey, after);
                break;
            case ObjectChange::Updated:
                before = change.oldObject;
                get(change.objectKey, after);
                break;
            case ObjectChange::Deleted:
                before = change.oldObject;
                break;
            }
            if (!limitTypes.isEmpty()) {
                QString type = (after.isEmpty() ? before : after).valueString(JsonDbString::kTypeStr);
                if (!limitTypes.contains(type))
                    continue;
            }
            QsonMap res;
            res.insert("before", before);
            res.insert("after", after);
            result.append(res);
            ++count;
        }
    }
    QsonMap resultmap, errormap;
    resultmap.insert("count", count);
    resultmap.insert("startingStateNumber", stateNumber);
    resultmap.insert("currentStateNumber", this->stateNumber());
    resultmap.insert("changes", result);
    return JsonDb::makeResponse(resultmap, errormap);
}

} } // end namespace QtAddOn::JsonDb
