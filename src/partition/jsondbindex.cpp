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
#include <QLocale>

#include "jsondbstrings.h"
#include "jsondbproxy.h"
#include "jsondbindex.h"
#include "jsondbbtree.h"
#include "jsondbsettings.h"
#include "jsondbobjecttable.h"
#include "jsondbscriptengine.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

static const int collationStringsCount = 13;
static const char * const collationStrings[collationStringsCount] = {
    "default",
    "big5han",
    "dict",
    "direct",
    "gb2312",
    "phonebk",
    "pinyin",
    "phonetic",
    "reformed",
    "standard",
    "stroke",
    "trad",
    "unihan"
};

static const int casePreferenceStringsCount = 3;
static const char * const casePreferenceStrings[casePreferenceStringsCount] = {
    "IgnoreCase",
    "PreferUpperCase",
    "PreferLowerCase"
};

#ifndef NO_COLLATION_SUPPORT
JsonDbCollator::Collation _q_correctCollationString(const QString &s)
{
    for (int i = 0; i < collationStringsCount; ++i) {
        if (s == QLatin1String(collationStrings[i]))
            return JsonDbCollator::Collation(i);
    }
    return JsonDbCollator::Default;
}
JsonDbCollator::CasePreference _q_correctCasePreferenceString(const QString &s)
{
    for (int i = 0; i < casePreferenceStringsCount; ++i) {
        if (s == QLatin1String(casePreferenceStrings[i]))
            return JsonDbCollator::CasePreference(i);
    }
    return JsonDbCollator::IgnoreCase;
}
#else
int _q_correctCollationString(const QString &s)
{
    Q_UNUSED(s);
    return 0;
}
int _q_correctCasePreferenceString(const QString &s)
{
    Q_UNUSED(s);
    return 0;
}
#endif //NO_COLLATION_SUPPORT

QString _q_bytesToHexString(const QByteArray &ba)
{
    static const ushort digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

    uint len = ba.size();
    const char *data = ba.constData();
    uint idx = 0;

    QString result(len*2, Qt::Uninitialized);
    QChar *resultData = result.data();

    for (uint i = 0; i < len; ++i, idx += 2) {
        uint j = (data[i] >> 4) & 0xf;
        resultData[idx] = QChar(digits[j]);
        j = data[i] & 0xf;
        resultData[idx+1] = QChar(digits[j]);
    }

    return result;
}

JsonDbIndex::JsonDbIndex(const QString &fileName, const QString &indexName, const QString &propertyName,
                         const QString &propertyType, const QStringList &objectType, const QString &locale, const QString &collation,
                         const QString &casePreference, Qt::CaseSensitivity caseSensitivity, JsonDbObjectTable *objectTable)
    : QObject(objectTable)
    , mObjectTable(objectTable)
    , mIndexName(indexName)
    , mPropertyName(propertyName)
    , mPath(propertyName.split('.'))
    , mPropertyType(propertyType)
    , mObjectType(objectType)
    , mLocale(locale)
    , mCollation(collation)
    , mCasePreference(casePreference)
    , mCaseSensitivity(caseSensitivity)
#ifndef NO_COLLATION_SUPPORT
    , mCollator(JsonDbCollator(QLocale(locale), _q_correctCollationString(collation)))
#endif
    , mStateNumber(0)
    , mBdb(0)
    , mScriptEngine(0)
    , mCacheSize(0)
{
    QFileInfo fi(fileName);
    QString dirName = fi.dir().path();
    QString baseName = fi.fileName();
    if (baseName.endsWith(QStringLiteral(".db")))
        baseName.chop(3);
    mFileName = QString::fromLatin1("%1/%2-%3-Index.db").arg(dirName).arg(baseName).arg(indexName);
#ifndef NO_COLLATION_SUPPORT
    mCollator.setCasePreference(_q_correctCasePreferenceString(mCasePreference));
#endif
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
        mScriptEngine = JsonDbScriptEngine::scriptEngine();

    // for "emit"
    JsonDbJoinProxy *mapProxy = new JsonDbJoinProxy(0, 0, this);
    connect(mapProxy, SIGNAL(viewObjectEmitted(QJSValue)),
            this, SLOT(propertyValueEmitted(QJSValue)));
    QString proxyName(QString::fromLatin1("_jsondbIndexProxy%1").arg(mIndexName));
    proxyName.replace(QLatin1Char('.'), QLatin1Char('$'));
    mScriptEngine->globalObject().setProperty(proxyName, mScriptEngine->newQObject(mapProxy));

    QString script(QString::fromLatin1("(function() { var jsondb={emit: %2.create, lookup: %2.lookup }; var fcn = (%1); return fcn})()")
                   .arg(propertyFunction).arg(proxyName));
    mPropertyFunction = mScriptEngine->evaluate(script);
    if (mPropertyFunction.isError() || !mPropertyFunction.isCallable()) {
        qDebug() << "Unable to parse index value function: " << mPropertyFunction.toString();
        return false;
    }

   return true;
}

bool JsonDbIndex::open()
{
    if (mPropertyName == JsonDbString::kUuidStr)
        return true;

    mBdb.reset(new JsonDbBtree());

    if (mCacheSize)
        mBdb->setCacheSize(mCacheSize);

    if (!mBdb->open(mFileName, JsonDbBtree::Default)) {
        qCritical() << "mBdb->open" << mBdb->errorMessage();
        return false;
    }

    mBdb->setCompareFunction(forwardKeyCmp);

    mStateNumber = mBdb->tag();
    if (jsondbSettings->debug() && jsondbSettings->verbose())
        qDebug() << "JsonDbIndex::open" << mStateNumber << mFileName;
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

bool JsonDbIndex::validateIndex(const JsonDbObject &newIndex, const JsonDbObject &oldIndex, QString &message)
{
    message.clear();

    if (!newIndex.isEmpty() && !oldIndex.isEmpty() && oldIndex.type() == JsonDbString::kIndexTypeStr) {
        if (oldIndex.value(JsonDbString::kPropertyNameStr).toString() != newIndex.value(JsonDbString::kPropertyNameStr).toString())
            message = QString::fromLatin1("Changing old index propertyName '%1' to '%2' not supported")
                             .arg(oldIndex.value(JsonDbString::kPropertyNameStr).toString())
                             .arg(newIndex.value(JsonDbString::kPropertyNameStr).toString());
        else if (oldIndex.value(JsonDbString::kPropertyTypeStr).toString() != newIndex.value(JsonDbString::kPropertyTypeStr).toString())
            message = QString::fromLatin1("Changing old index propertyType from '%1' to '%2' not supported")
                             .arg(oldIndex.value(JsonDbString::kPropertyTypeStr).toString())
                             .arg(newIndex.value(JsonDbString::kPropertyTypeStr).toString());
        else if (oldIndex.value(JsonDbString::kObjectTypeStr) != newIndex.value(JsonDbString::kObjectTypeStr))
            message = QString::fromLatin1("Changing old index objectType from '%1' to '%2' not supported")
                             .arg(oldIndex.value(JsonDbString::kObjectTypeStr).toString())
                             .arg(newIndex.value(JsonDbString::kObjectTypeStr).toString());
        else if (oldIndex.value(JsonDbString::kPropertyFunctionStr).toString() != newIndex.value(JsonDbString::kPropertyFunctionStr).toString())
            message = QString::fromLatin1("Changing old index propertyFunction from '%1' to '%2' not supported")
                             .arg(oldIndex.value(JsonDbString::kPropertyFunctionStr).toString())
                             .arg(newIndex.value(JsonDbString::kPropertyFunctionStr).toString());
    }

    if (!(newIndex.contains(JsonDbString::kPropertyFunctionStr) ^ newIndex.contains(JsonDbString::kPropertyNameStr)))
        message = QStringLiteral("Index object must have one of propertyName or propertyFunction set");
    else if (newIndex.contains(JsonDbString::kPropertyFunctionStr) && !newIndex.contains(JsonDbString::kNameStr))
        message = QStringLiteral("Index object with propertyFunction must have name");

    return message.isEmpty();
}

QString JsonDbIndex::determineName(const JsonDbObject &index)
{
    QString indexName = index.value(JsonDbString::kNameStr).toString();
    QString propertyName = index.value(JsonDbString::kPropertyNameStr).toString();

    if (indexName.isEmpty())
        return propertyName;
    return indexName;
}

JsonDbBtree *JsonDbIndex::bdb()
{
    if (!mBdb)
        open();
    return mBdb.data();
}

QJsonValue JsonDbIndex::indexValue(const QJsonValue &v)
{
    if (!v.isString())
        return v;

    QJsonValue result;
    if (mCaseSensitivity == Qt::CaseInsensitive)
        result = v.toString().toLower();
    else
        result = v;

#ifndef NO_COLLATION_SUPPORT
    if (!mCollation.isEmpty() && !mLocale.isEmpty())
        result = _q_bytesToHexString(mCollator.sortKey(v.toString()));
#endif

    return result;
}

QList<QJsonValue> JsonDbIndex::indexValues(JsonDbObject &object)
{
    mFieldValues.clear();
    if (!mScriptEngine) {
        int size = mPath.size();
        if (mPath[size-1] == QLatin1String("*")) {
            QJsonValue v = object.propertyLookup(mPath.mid(0, size-1));
            QJsonArray array = v.toArray();
            mFieldValues.reserve(array.size());
            for (int i = 0; i < array.size(); ++i) {
                mFieldValues.append(indexValue(array.at(i)));
            }
        } else {
            QJsonValue v = object.propertyLookup(mPath);
            if (!v.isUndefined()) {
                mFieldValues.append(indexValue(v));
            }
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
        mFieldValues.append(JsonDbScriptEngine::fromJSValue(value));
}

void JsonDbIndex::indexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 stateNumber)
{
    if (mPropertyName == JsonDbString::kUuidStr)
        return;

    if (!mObjectType.isEmpty() && !mObjectType.contains(object.value(JsonDbString::kTypeStr).toString()))
        return;

    Q_ASSERT(!object.contains(JsonDbString::kDeletedStr)
             && !object.value(JsonDbString::kDeletedStr).toBool());
    QList<QJsonValue> fieldValues = indexValues(object);
    if (!fieldValues.size())
        return;
    bool ok;
    if (!mBdb)
        open();
    if (!mBdb->isWriting())
        mObjectTable->begin(this);
    JsonDbBtree::Transaction *txn = mBdb->writeTransaction();
    for (int i = 0; i < fieldValues.size(); i++) {
        QJsonValue fieldValue = fieldValues.at(i);
        fieldValue = makeFieldValue(fieldValue, mPropertyType);
        if (fieldValue.isUndefined())
            continue;
        QByteArray forwardKey(makeForwardKey(fieldValue, objectKey));
        QByteArray forwardValue(makeForwardValue(objectKey));

        if (jsondbSettings->debug())
            qDebug() << "indexing" << objectKey << mPropertyName << fieldValue
                     << "forwardIndex" << "key" << forwardKey.toHex()
                     << "forwardIndex" << "value" << forwardValue.toHex()
                     << object;
        ok = txn->put(forwardKey, forwardValue);
        if (!ok) qCritical() << __FUNCTION__ << "putting fowardIndex" << mBdb->errorMessage();
    }
    if (jsondbSettings->debug() && (stateNumber < mStateNumber))
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
    if (!mObjectType.isEmpty() && !mObjectType.contains(object.value(JsonDbString::kTypeStr).toString()))
        return;
    if (!mBdb)
        open();
    QList<QJsonValue> fieldValues = indexValues(object);
    if (!fieldValues.size())
        return;
    if (!mBdb->isWriting())
        mObjectTable->begin(this);
    JsonDbBtree::Transaction *txn = mBdb->writeTransaction();
    for (int i = 0; i < fieldValues.size(); i++) {
        QJsonValue fieldValue = fieldValues.at(i);
        fieldValue = makeFieldValue(fieldValue, mPropertyType);
        if (fieldValue.isUndefined())
            continue;
        if (jsondbSettings->debug())
            qDebug() << "deindexing" << objectKey << mPropertyName << fieldValue;
        QByteArray forwardKey(makeForwardKey(fieldValue, objectKey));
        if (!txn->remove(forwardKey)) {
            qDebug() << "deindexing failed" << objectKey << mPropertyName << fieldValue << object << forwardKey.toHex();
        }
    }
    if (jsondbSettings->verbose() && (stateNumber < mStateNumber))
        qDebug() << "JsonDbIndex::deindexObject" << "stale update" << stateNumber << mStateNumber << mFileName;
#ifdef CHECK_INDEX_ORDERING
    checkIndex();
#endif
}

quint32 JsonDbIndex::stateNumber() const
{
    return mStateNumber;
}

JsonDbBtree::Transaction *JsonDbIndex::begin()
{
    if (!mBdb)
        open();
    return mBdb->beginWrite();
}
bool JsonDbIndex::commit(quint32 stateNumber)
{
    if (mBdb->isWriting())
        return mBdb->writeTransaction()->commit(stateNumber);
    return false;
}
bool JsonDbIndex::abort()
{
    if (mBdb->isWriting())
        mBdb->writeTransaction()->abort();
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
    bool isInTransaction = mBdb.data()->btree()->writeTransaction();
    JsonDbBtree::Transaction *txnf = mBdb.data()->btree()->writeTransaction() ? mBdb.data()->btree()->writeTransaction() : mBdb.data()->btree()->beginWrite();
    JsonDbBtree::Cursor cursorf(txnf);
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
    if (!isInTransaction)
        txnf->abort();

    qDebug() << "checkIndex" << mPropertyName << "reversed";
    // now check other direction
    int countr = 0;
    isInTransaction = mBdb.data()->btree()->writeTransaction();
    JsonDbBtree::Transaction *txnr = mBdb.data()->btree()->writeTransaction() ? mBdb.data()->btree()->writeTransaction() : mBdb.data()->btree()->beginWrite();
    JsonDbBtree::Cursor cursorr(txnr);
    ok = cursorr.last();
    if (ok) {
        countr++;
        QByteArray outkey1;
        ok = cursorr.current(&outkey1, 0);
        while (cursorr.previous()) {
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
    if (!isInTransaction)
        txnr->abort();
    qDebug() << "checkIndex" << mPropertyName << "done" << countf << countr << "entries checked";

}

void JsonDbIndex::setCacheSize(quint32 cacheSize)
{
    mCacheSize = cacheSize;
    if (mBdb)
        mBdb->setCacheSize(cacheSize);
}

#include "moc_jsondbindex.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
