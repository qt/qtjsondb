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

#include <QObject>
#include <QByteArray>
#include <QVariant>
#include <QFileInfo>
#include <QDir>

#include "aodb.h"
#include "jsondb-strings.h"
#include "jsondb.h"
#include "jsondb-proxy.h"
#include "jsondbindex.h"
#include "qsonconversion.h"

QT_BEGIN_NAMESPACE_JSONDB

static bool debugIndexObject = false;

JsonDbIndex::JsonDbIndex(const QString &fileName, const QString &propertyName, QObject *parent)
    : QObject(parent)
    , mPropertyName(propertyName)
    , mPath(propertyName.split('.'))
    , mStateNumber(0)
    , mBdb(0)
    , mScriptEngine(0)
{
    QFileInfo fi(fileName);
    QDir dir(fi.absolutePath());
    QString dirName = fi.dir().path();
    QString baseName = fi.fileName();
    if (baseName.endsWith(".db"))
        baseName.chop(3);
    mFileName = QString("%1/%2-%3-Index.db").arg(dirName).arg(baseName).arg(propertyName);
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
    mPropertyFunction = mScriptEngine->evaluate(QString("var %1 = %2; %1;").arg("index").arg(propertyFunction));    if (mPropertyFunction.isError() || !mPropertyFunction.isFunction()) {
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

    mBdb.reset(new AoDb());

    if (!mBdb->open(mFileName, AoDb::NoSync | AoDb::UseSyncMarker)) {
        qCritical() << "mBdb->open" << mBdb->errorMessage();
        return false;
    }

    if (!mBdb->setCmpFunc(forwardKeyCmp)) {
        qCritical() << "mBdb->setCmpFunc" << mBdb->errorMessage();
        return false;
    }

    mStateNumber = mBdb->tag();
    if (gDebugRecovery) qDebug() << "JsonDbIndex::open" << mStateNumber << mFileName;
    return true;
}

void JsonDbIndex::close()
{
    if (mBdb)
        mBdb->close();
}

AoDb *JsonDbIndex::bdb()
{
    if (!mBdb)
        open();
    return mBdb.data();
}

QVariantList JsonDbIndex::indexValues(QsonObject &object)
{
    mFieldValues.clear();
    if (!mScriptEngine) {
        int size = mPath.size();
        if (mPath[size-1] == QString("*")) {
            QVariant v = JsonDb::propertyLookup(object, mPath.mid(0, size-1));
            mFieldValues = v.toList();
        } else {
            QVariant v = JsonDb::propertyLookup(object, mPath);
            mFieldValues.append(v);
        }
    } else {
        QJSValue globalObject = mScriptEngine->globalObject();
        QJSValueList args;
        args << qsonToJSValue(object, mScriptEngine);
        mPropertyFunction.call(globalObject, args);
    }
    return mFieldValues;
}

void JsonDbIndex::propertyValueEmitted(QJSValue value)
{
    if (!value.isUndefined())
        mFieldValues.append(value.toVariant());
}

void JsonDbIndex::indexObject(const ObjectKey &objectKey, QsonObject &object, quint32 stateNumber, bool inTransaction)
{
    if (mPropertyName == JsonDbString::kUuidStr)
        return;

    bool ok;
    if (!mBdb)
        open();
    QVariantList fieldValues = indexValues(object);
    //qDebug() << "JsonDbIndex::indexObject" << mPath << fieldValues;
    if (!inTransaction)
        mBdb->begin();
    foreach (const QVariant fieldValue, fieldValues) {
        if (!fieldValue.isValid())
            continue;
        QByteArray forwardKey(makeForwardKey(fieldValue, objectKey));
        QByteArray forwardValue(makeForwardValue(objectKey));

        if (debugIndexObject)
            qDebug() << "indexing" << objectKey << mPropertyName << fieldValue
                     << "forwardIndex" << "key" << forwardKey.toHex()
                     << "forwardIndex" << "value" << forwardValue.toHex()
                     << object;
        ok = mBdb->put(forwardKey, forwardValue);
        if (!ok) qCritical() << __FUNCTION__ << "putting fowardIndex" << mBdb->errorMessage();
    }
    if (!inTransaction)
        mBdb->commit(stateNumber);
    if (gDebugRecovery && (stateNumber < mStateNumber))
        qDebug() << "JsonDbIndex::indexObject" << "stale update" << stateNumber << mStateNumber << mFileName;
    mStateNumber = qMax(stateNumber, mStateNumber);

#ifdef CHECK_INDEX_ORDERING
    checkIndex()
#endif
}

void JsonDbIndex::deindexObject(const ObjectKey &objectKey, QsonObject &object, quint32 stateNumber, bool inTransaction)
{
    if (mPropertyName == JsonDbString::kUuidStr)
        return;
    if (!mBdb)
        open();
    QVariantList fieldValues = indexValues(object);
    if (!inTransaction)
        mBdb->begin();
    foreach (const QVariant fieldValue, fieldValues) {
        if (!fieldValue.isValid())
            continue;
        if (debugIndexObject)
            qDebug() << "deindexing" << objectKey << mPropertyName << fieldValue;
        QByteArray forwardKey(makeForwardKey(fieldValue, objectKey));
        if (!mBdb->remove(forwardKey)) {
            qDebug() << "deindexing failed" << objectKey << mPropertyName << fieldValue << object << forwardKey.toHex();
        }
    }
    if (gDebugRecovery && (stateNumber < mStateNumber))
        qDebug() << "JsonDbIndex::deindexObject" << "stale update" << stateNumber << mStateNumber << mFileName;
    if (!inTransaction)
        mBdb->commit(stateNumber);
#ifdef CHECK_INDEX_ORDERING
    checkIndex();
#endif
}

quint32 JsonDbIndex::stateNumber() const
{
    return mStateNumber;
}

bool JsonDbIndex::begin()
{
    if (!mBdb)
        open();
    return mBdb->begin();
}
bool JsonDbIndex::commit(quint32 stateNumber)
{
    return mBdb->commit(stateNumber);
}
bool JsonDbIndex::abort()
{
    return mBdb->abort();
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
    AoDbCursor cursorf(mBdb.data());
    bool ok = cursorf.first();
    if (ok) {
        countf++;
        QByteArray outkey1;
        ok = cursorf.currentKey(outkey1);
        while (cursorf.next()) {
            countf++;
            QByteArray outkey2;
            cursorf.currentKey(outkey2);
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
    AoDbCursor cursorr(mBdb.data());
    ok = cursorr.last();
    if (ok) {
        countr++;
        QByteArray outkey1;
        ok = cursorr.currentKey(outkey1);
        while (cursorr.prev()) {
            countr++;
            QByteArray outkey2;
            cursorr.currentKey(outkey2);
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

JsonDbIndexCursor::JsonDbIndexCursor(JsonDbIndex *index)
    : mCursor(index->bdb())
{
}

bool JsonDbIndexCursor::seek(const QVariant &value)
{
    QByteArray forwardKey(makeForwardKey(value, ObjectKey()));
    return mCursor.seek(forwardKey);
}

bool JsonDbIndexCursor::seekRange(const QVariant &value)
{
    QByteArray forwardKey(makeForwardKey(value, ObjectKey()));
    return mCursor.seekRange(forwardKey);
}

bool JsonDbIndexCursor::current(QVariant &key, ObjectKey &value)
{
    QByteArray baKey, baValue;
    if (!mCursor.current(baKey, baValue))
        return false;
    forwardKeySplit(baKey, key);
    forwardValueSplit(baValue, value);
    return true;
}

bool JsonDbIndexCursor::currentKey(QVariant &key)
{
    QByteArray baKey;
    if (!mCursor.currentKey(baKey))
        return false;
    forwardKeySplit(baKey, key);
    return true;
}

bool JsonDbIndexCursor::currentValue(ObjectKey &value)
{
    QByteArray baValue;
    if (!mCursor.currentValue(baValue))
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
