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
#include <QByteArray>
#include <QVariant>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#include "jsondb-strings.h"
#include "jsondb.h"
#include "jsondb-proxy.h"
#include "jsondbindex.h"
#include "qmanagedbtree.h"

QT_BEGIN_NAMESPACE_JSONDB

static bool debugIndexObject = false;

JsonDbIndex::JsonDbIndex(const QString &fileName, const QString &indexName, const QString &propertyName,
                         const QString &propertyType, QObject *parent)
    : QObject(parent)
    , mPropertyName(propertyName)
    , mPath(propertyName.split('.'))
    , mPropertyType(propertyType)
    , mStateNumber(0)
    , mBdb(0)
    , mScriptEngine(0)
    , mCacheSize(0)
{
    QFileInfo fi(fileName);
    QString dirName = fi.dir().path();
    QString baseName = fi.fileName();
    if (baseName.endsWith(".db"))
        baseName.chop(3);
    mFileName = QString("%1/%2-%3-Index.db").arg(dirName).arg(baseName).arg(indexName);
}

JsonDbIndex::~JsonDbIndex()
{
    if (mBdb) {
        close();
        mBdb.reset();
    }
}

bool JsonDbIndex::setPropertyFunction(const QString &propertyFunction)
{
    if (!mScriptEngine)
        mScriptEngine = new QJSEngine(this);
    mPropertyFunction = mScriptEngine->evaluate(QString("var %1 = %2; %1;").arg("index").arg(propertyFunction));
    if (mPropertyFunction.isError() || !mPropertyFunction.isCallable()) {
        qDebug() << "Unable to parse index value function: " << mPropertyFunction.toString();
        return false;
    }

    // for "create"
    JsonDbJoinProxy *mapProxy = new JsonDbJoinProxy(0, 0, this);
    connect(mapProxy, SIGNAL(viewObjectEmitted(QJSValue)),
            this, SLOT(propertyValueEmitted(QJSValue)));
    mScriptEngine->globalObject().setProperty("_jsondb", mScriptEngine->newQObject(mapProxy));
    mScriptEngine->evaluate("var jsondb = {emit: _jsondb.create, lookup: _jsondb.lookup };");

   return true;
}

bool JsonDbIndex::open()
{
    if (mPropertyName == JsonDbString::kUuidStr)
        return true;

    mBdb.reset(new QManagedBtree());

    if (mCacheSize)
        mBdb->setCacheSize(mCacheSize);

    if (!mBdb->open(mFileName, QBtree::NoSync | QBtree::UseSyncMarker)) {
        qCritical() << "mBdb->open" << mBdb->errorMessage();
        return false;
    }

    mBdb->setCmpFunc(forwardKeyCmp);

    mStateNumber = mBdb->tag();
    if (gDebugRecovery) qDebug() << "JsonDbIndex::open" << mStateNumber << mFileName;
    return true;
}

void JsonDbIndex::close()
{
    if (mBdb)
        mBdb->close();
}

/*!
  Returns true if the index's btree file exists.
*/
bool JsonDbIndex::exists() const
{
    QFile file(mFileName);
    return file.exists();
}

QManagedBtree *JsonDbIndex::bdb()
{
    if (!mBdb)
        open();
    return mBdb.data();
}

QList<QJsonValue> JsonDbIndex::indexValues(JsonDbObject &object)
{
    mFieldValues.clear();
    if (!mScriptEngine) {
        int size = mPath.size();
        if (mPath[size-1] == QString("*")) {
            QJsonValue v = JsonDb::propertyLookup(object, mPath.mid(0, size-1));
            QJsonArray array = v.toArray();
            mFieldValues.reserve(array.size());
            for (int i = 0; i < array.size(); ++i)
                mFieldValues.append(array.at(i));
        } else {
            QJsonValue v = JsonDb::propertyLookup(object, mPath);
            if (!v.isUndefined())
                mFieldValues.append(v);
        }
    } else {
        QJSValueList args;
        args << mScriptEngine->toScriptValue(object.toVariantMap());
        mPropertyFunction.call(args);
    }
    return mFieldValues;
}

void JsonDbIndex::propertyValueEmitted(QJSValue value)
{
    if (!value.isUndefined())
        mFieldValues.append(JsonDb::fromJSValue(value));
}

void JsonDbIndex::indexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 stateNumber)
{
    if (mPropertyName == JsonDbString::kUuidStr)
        return;

    Q_ASSERT(!object.contains(JsonDbString::kDeletedStr)
             && !object.value(JsonDbString::kDeletedStr).toBool());
    bool ok;
    if (!mBdb)
        open();
    QList<QJsonValue> fieldValues = indexValues(object);
    bool inTransaction = mBdb->isWriteTxnActive();
    QManagedBtreeTxn txn = inTransaction ? mBdb->existingWriteTxn() : mBdb->beginWrite();
    for (int i = 0; i < fieldValues.size(); i++) {
        QJsonValue fieldValue = fieldValues.at(i);
        fieldValue = makeFieldValue(fieldValue, mPropertyType);
        if (fieldValue.isUndefined())
            continue;
        QByteArray forwardKey(makeForwardKey(fieldValue, objectKey));
        QByteArray forwardValue(makeForwardValue(objectKey));

        if (debugIndexObject)
            qDebug() << "indexing" << objectKey << mPropertyName << fieldValue
                     << "forwardIndex" << "key" << forwardKey.toHex()
                     << "forwardIndex" << "value" << forwardValue.toHex()
                     << object;
        ok = txn.put(forwardKey, forwardValue);
        if (!ok) qCritical() << __FUNCTION__ << "putting fowardIndex" << mBdb->errorMessage();
    }
    if (!inTransaction)
        txn.commit(stateNumber);
    if (gDebugRecovery && (stateNumber < mStateNumber))
        qDebug() << "JsonDbIndex::indexObject" << "stale update" << stateNumber << mStateNumber << mFileName;
    mStateNumber = qMax(stateNumber, mStateNumber);

#ifdef CHECK_INDEX_ORDERING
    checkIndex()
#endif
}

void JsonDbIndex::deindexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 stateNumber)
{
    if (mPropertyName == JsonDbString::kUuidStr)
        return;
    if (!mBdb)
        open();
    QList<QJsonValue> fieldValues = indexValues(object);
    bool inTransaction = mBdb->isWriteTxnActive();
    QManagedBtreeTxn txn = inTransaction ? mBdb->existingWriteTxn() : mBdb->beginWrite();
    for (int i = 0; i < fieldValues.size(); i++) {
        QJsonValue fieldValue = fieldValues.at(i);
        fieldValue = makeFieldValue(fieldValue, mPropertyType);
        if (fieldValue.isUndefined())
            continue;
        if (debugIndexObject)
            qDebug() << "deindexing" << objectKey << mPropertyName << fieldValue;
        QByteArray forwardKey(makeForwardKey(fieldValue, objectKey));
        if (!txn.remove(forwardKey)) {
            qDebug() << "deindexing failed" << objectKey << mPropertyName << fieldValue << object << forwardKey.toHex();
        }
    }
    if (gDebugRecovery && (stateNumber < mStateNumber))
        qDebug() << "JsonDbIndex::deindexObject" << "stale update" << stateNumber << mStateNumber << mFileName;
    if (!inTransaction)
        txn.commit(stateNumber);
#ifdef CHECK_INDEX_ORDERING
    checkIndex();
#endif
}

quint32 JsonDbIndex::stateNumber() const
{
    return mStateNumber;
}

QManagedBtreeTxn JsonDbIndex::begin()
{
    if (!mBdb)
        open();
    return mWriteTxn = mBdb->beginWrite();
}
bool JsonDbIndex::commit(quint32 stateNumber)
{
    if (mBdb->isWriteTxnActive())
        return mWriteTxn.commit(stateNumber);
    return false;
}
bool JsonDbIndex::abort()
{
    if (mBdb->isWriteTxnActive())
        mWriteTxn.abort();
    return true;
}
bool JsonDbIndex::clearData()
{
    return mBdb->clearData();
}

void JsonDbIndex::checkIndex()
{
    if (mPropertyName == JsonDbString::kUuidStr)
        return;

    qDebug() << "checkIndex" << mPropertyName;
    int countf = 0;
    QBtreeCursor cursorf(mBdb.data()->btree());
    bool ok = cursorf.first();
    if (ok) {
        countf++;
        QByteArray outkey1;
        ok = cursorf.current(&outkey1, 0);
        while (cursorf.next()) {
            countf++;
            QByteArray outkey2;
            cursorf.current(&outkey2, 0);
            //qDebug() << outkey1.toHex() << outkey2.toHex();
            if (memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) >= 0) {
                qDebug() << "out of order index" << mPropertyName << endl
                         << outkey1.toHex() << endl
                         << outkey2.toHex() << endl;
            }
            outkey1 = outkey2;
        }
    }

    qDebug() << "checkIndex" << mPropertyName << "reversed";
    // now check other direction
    int countr = 0;
    QBtreeCursor cursorr(mBdb.data()->btree());
    ok = cursorr.last();
    if (ok) {
        countr++;
        QByteArray outkey1;
        ok = cursorr.current(&outkey1, 0);
        while (cursorr.prev()) {
            countr++;
            QByteArray outkey2;
            cursorr.current(&outkey2, 0);
            //qDebug() << outkey1.toHex() << outkey2.toHex();
            if (memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) <= 0) {
                qDebug() << "reverse walk: out of order index" << mPropertyName << endl
                         << outkey1.toHex() << endl
                         << outkey2.toHex() << endl;
            }
            outkey1 = outkey2;
        }
    }
    qDebug() << "checkIndex" << mPropertyName << "done" << countf << countr << "entries checked";

}

void JsonDbIndex::setCacheSize(quint32 cacheSize)
{
    mCacheSize = cacheSize;
    if (mBdb)
        mBdb->setCacheSize(cacheSize);
}

JsonDbIndexCursor::JsonDbIndexCursor(JsonDbIndex *index)
    : mCursor(index->bdb()->btree())
{
}

bool JsonDbIndexCursor::seek(const QJsonValue &value)
{
    QByteArray forwardKey(makeForwardKey(makeFieldValue(value, mIndex->propertyType()), ObjectKey()));
    return mCursor.seek(forwardKey);
}

bool JsonDbIndexCursor::seekRange(const QJsonValue &value)
{
    QByteArray forwardKey(makeForwardKey(makeFieldValue(value, mIndex->propertyType()), ObjectKey()));
    return mCursor.seekRange(forwardKey);
}

bool JsonDbIndexCursor::current(QJsonValue &key, ObjectKey &value)
{
    QByteArray baKey, baValue;
    if (!mCursor.current(&baKey, &baValue))
        return false;
    forwardKeySplit(baKey, key);
    forwardValueSplit(baValue, value);
    return true;
}

bool JsonDbIndexCursor::currentKey(QJsonValue &key)
{
    QByteArray baKey;
    if (!mCursor.current(&baKey, 0))
        return false;
    forwardKeySplit(baKey, key);
    return true;
}

bool JsonDbIndexCursor::currentValue(ObjectKey &value)
{
    QByteArray baValue;
    if (!mCursor.current(0, &baValue))
        return false;
    forwardValueSplit(baValue, value);
    return true;
}

bool JsonDbIndexCursor::first()
{
    return mCursor.first();
}

bool JsonDbIndexCursor::next()
{
    return mCursor.next();
}

bool JsonDbIndexCursor::prev()
{
    return mCursor.prev();
}

QT_END_NAMESPACE_JSONDB
