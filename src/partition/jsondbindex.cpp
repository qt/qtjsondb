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

#include "jsondbindex.h"
#include "jsondbindex_p.h"

#include "jsondbpartition_p.h"
#include "jsondbstrings.h"
#include "jsondbproxy.h"
#include "jsondbbtree.h"
#include "jsondbsettings.h"
#include "jsondbobjecttable.h"
#include "jsondbscriptengine.h"

#include "jsondbutils_p.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

JsonDbIndexPrivate::JsonDbIndexPrivate(JsonDbIndex *q)
    : q_ptr(q)
    , mObjectTable(0)
    , mCacheSize(0)
{
}

JsonDbIndexPrivate::~JsonDbIndexPrivate()
{
}

QString JsonDbIndexPrivate::fileName() const
{
    return QString::fromLatin1("%1/%2-%3-Index.db").arg(mPath, mBaseName, mSpec.name);
}

bool JsonDbIndexPrivate::initScriptEngine()
{
    if (mScriptEngine)
        return true;

    Q_Q(JsonDbIndex);

    mScriptEngine = JsonDbScriptEngine::scriptEngine();

    JsonDbJoinProxy *mapProxy = new JsonDbJoinProxy(0, 0, mScriptEngine);
    QObject::connect(mapProxy, SIGNAL(viewObjectEmitted(QJSValue)),
                     q, SLOT(_q_propertyValueEmitted(QJSValue)));

    QString script(QStringLiteral("(function(proxy) { %2 var jsondb={emit: proxy.create, lookup: proxy.lookup }; var fcn = (%1); return fcn})")
                   .arg(mSpec.propertyFunction, jsondbSettings->useStrictMode() ? QStringLiteral("\"use strict\"; ") : QStringLiteral("/* use nonstrict mode */")));
    mPropertyFunction = mScriptEngine->evaluate(script);
    if (mPropertyFunction.isError() || !mPropertyFunction.isCallable()) {
        qDebug() << "Unable to parse index value function: " << mPropertyFunction.toString();
        return false;
    }
    QJSValueList args;
    args << mScriptEngine->newQObject(mapProxy);
    mPropertyFunction = mPropertyFunction.call(args);
    if (mPropertyFunction.isError() || !mPropertyFunction.isCallable()) {
        qDebug() << "Unable to evaluate index value function: " << mPropertyFunction.toString();
        return false;
    }
    return true;
}

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

JsonDbIndex::JsonDbIndex(const QString &fileName, JsonDbObjectTable *objectTable)
    : QObject(objectTable), d_ptr(new JsonDbIndexPrivate(this))
{
    Q_D(JsonDbIndex);
    d->mObjectTable = objectTable;

    QFileInfo fi(fileName);
    QString dirName = fi.dir().path();
    d->mPath = dirName;
    d->mBaseName = fi.fileName();
    if (d->mBaseName.endsWith(QLatin1String(".db")))
        d->mBaseName.chop(3);
}

JsonDbIndex::~JsonDbIndex()
{
    close();
}

void JsonDbIndex::setIndexSpec(const JsonDbIndexSpec &spec)
{
    Q_D(JsonDbIndex);
    d->mSpec = spec;

    d->mPropertyNamePath = spec.propertyName.split(QLatin1Char('.'));
#ifndef NO_COLLATION_SUPPORT
    d->mCollator = JsonDbCollator(QLocale(spec.locale), _q_correctCollationString(spec.collation));
    d->mCollator.setCasePreference(_q_correctCasePreferenceString(d->mSpec.casePreference));
#endif

    if (!d->mSpec.propertyName.isEmpty() && !d->mSpec.propertyFunction.isEmpty())
        d->mSpec.propertyFunction = QString();

    setObjectName(d->mSpec.name);
}

const JsonDbIndexSpec &JsonDbIndex::indexSpec() const
{
    Q_D(const JsonDbIndex);
    return d->mSpec;
}

QString JsonDbIndex::fileName() const
{
    Q_D(const JsonDbIndex);
    return d->fileName();
}

bool JsonDbIndex::open()
{
    Q_D(JsonDbIndex);
    if (d->mSpec.propertyName == JsonDbString::kUuidStr)
        return true;

    if (d->mCacheSize)
        d->mBdb.setCacheSize(d->mCacheSize);

    d->mBdb.setFileName(d->fileName());
    if (!d->mBdb.open(JsonDbBtree::Default)) {
        qCritical() << "mBdb.open" << d->mBdb.errorMessage();
        return false;
    }

    d->mBdb.setCompareFunction(JsonDbIndexPrivate::indexCompareFunction);

    if (jsondbSettings->verbose())
        qDebug() << JSONDB_INFO << "opened index" << d->mBdb.fileName() << "with tag" << d->mBdb.tag();
    return true;
}

void JsonDbIndex::close()
{
    Q_D(JsonDbIndex);
    if (!d->mBdb.isOpen())
        return;
    if (jsondbSettings->verbose())
        qDebug() << JSONDB_INFO << "closed index" << d->mBdb.fileName() << "with tag" << d->mBdb.tag();
    d->mBdb.close();
}

bool JsonDbIndex::isOpen() const
{
    Q_D(const JsonDbIndex);
    if (d->mSpec.propertyName == JsonDbString::kUuidStr)
        return true;
    return d->mBdb.isOpen();
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
    Q_D(JsonDbIndex);
    if (d->mSpec.propertyName == JsonDbString::kUuidStr)
        return 0;
    if (!d->mBdb.isOpen())
        open();
    return &d->mBdb;
}

QJsonValue JsonDbIndexPrivate::indexValue(const QJsonValue &v)
{
    if (!v.isString())
        return v;

    QJsonValue result;
    if (mSpec.caseSensitivity == Qt::CaseInsensitive)
        result = v.toString().toLower();
    else
        result = v;

#ifndef NO_COLLATION_SUPPORT
    if (!mSpec.collation.isEmpty() && !mSpec.locale.isEmpty())
        result = _q_bytesToHexString(mCollator.sortKey(v.toString()));
#endif

    return result;
}

QList<QJsonValue> JsonDbIndex::indexValues(JsonDbObject &object)
{
    Q_D(JsonDbIndex);

    d->mFieldValues.clear();

    if (!d->mSpec.hasPropertyFunction()) {
        int size = d->mPropertyNamePath.size();
        if (d->mPropertyNamePath.at(size-1) == QLatin1Char('*')) {
            QJsonValue v = object.valueByPath(d->mPropertyNamePath.mid(0, size-1));
            QJsonArray array = v.toArray();
            d->mFieldValues.reserve(array.size());
            for (int i = 0; i < array.size(); ++i)
                d->mFieldValues.append(d->indexValue(array.at(i)));
        } else {
            QJsonValue v = object.valueByPath(d->mPropertyNamePath);
            if (!v.isUndefined())
                d->mFieldValues.append(d->indexValue(v));
        }
    } else {
        if (!d->initScriptEngine())
            return d->mFieldValues;

        QJSValueList args;
        args << d->mScriptEngine->toScriptValue(static_cast<QJsonObject>(object));
        QJSValue result = d->mPropertyFunction.call(args);
        if (result.isError())
            qDebug() << "Error calling index propertyFunction" << d->mSpec.name << result.toString();
    }
    return d->mFieldValues;
}

void JsonDbIndexPrivate::_q_propertyValueEmitted(QJSValue value)
{
    if (!value.isUndefined())
        mFieldValues.append(mScriptEngine->fromScriptValue<QJsonValue>(value));
}

bool JsonDbIndex::indexObject(JsonDbObject &object, quint32 objectStateNumber)
{
    Q_D(JsonDbIndex);
    if (d->mSpec.propertyName == JsonDbString::kUuidStr)
        return true;

    if (!d->mSpec.objectTypes.isEmpty() && !d->mSpec.objectTypes.contains(object.value(JsonDbString::kTypeStr).toString()))
        return true;

    Q_ASSERT(!object.contains(JsonDbString::kDeletedStr)
             && !object.value(JsonDbString::kDeletedStr).toBool());
    QList<QJsonValue> fieldValues = indexValues(object);
    if (!fieldValues.size())
        return true;

    QUuid objectKey = object.uuid();

    if (!d->mBdb.isOpen())
        open();
    if (!d->mBdb.isWriting())
        d->mObjectTable->begin(this);
    JsonDbBtree::Transaction *txn = d->mBdb.writeTransaction();
    for (int i = 0; i < fieldValues.size(); i++) {
        QJsonValue fieldValue = fieldValues.at(i);
        fieldValue = d->makeFieldValue(fieldValue, d->mSpec.propertyType);
        if (fieldValue.isUndefined())
            continue;
        d->truncateFieldValue(&fieldValue, d->mSpec.propertyType);
        QByteArray forwardKey = JsonDbIndexPrivate::makeForwardKey(fieldValue, objectKey);
        QByteArray forwardValue = JsonDbIndexPrivate::makeForwardValue(objectKey);

        if (jsondbSettings->debug())
            qDebug() << "indexing" << objectKey << d->mSpec.propertyName << fieldValue
                     << "forwardIndex" << "key" << forwardKey.toHex()
                     << "forwardIndex" << "value" << forwardValue.toHex()
                     << object;
        if (!txn->put(forwardKey, forwardValue)) {
            qCritical() << d->mSpec.name << "indexing failed" << d->mBdb.errorMessage();
            return false;
        }
    }
    if (jsondbSettings->debug() && (objectStateNumber < stateNumber()))
        qDebug() << "JsonDbIndex::indexObject" << "stale update" << objectStateNumber << stateNumber() << d->mBdb.fileName();
    return true;
}

bool JsonDbIndex::deindexObject(JsonDbObject &object, quint32 objectStateNumber)
{
    Q_D(JsonDbIndex);

    if (d->mSpec.propertyName == JsonDbString::kUuidStr)
        return true;

    if (!d->mSpec.objectTypes.isEmpty() && !d->mSpec.objectTypes.contains(object.value(JsonDbString::kTypeStr).toString()))
        return true;

    QList<QJsonValue> fieldValues = indexValues(object);
    if (!fieldValues.size())
        return true;

    QUuid objectKey = object.uuid();

    if (!d->mBdb.isOpen())
        open();
    if (!d->mBdb.isWriting())
        d->mObjectTable->begin(this);
    JsonDbBtree::Transaction *txn = d->mBdb.writeTransaction();
    for (int i = 0; i < fieldValues.size(); i++) {
        QJsonValue fieldValue = fieldValues.at(i);
        fieldValue = d->makeFieldValue(fieldValue, d->mSpec.propertyType);
        if (fieldValue.isUndefined())
            continue;
        d->truncateFieldValue(&fieldValue, d->mSpec.propertyType);
        if (jsondbSettings->debug())
            qDebug() << "deindexing" << objectKey << d->mSpec.propertyName << fieldValue;
        QByteArray forwardKey = JsonDbIndexPrivate::makeForwardKey(fieldValue, objectKey);
        if (!txn->remove(forwardKey)) {
            qCritical() << d->mSpec.name << "deindexing failed" << d->mBdb.errorMessage();
            return false;
        }
    }
    if (jsondbSettings->verbose() && (objectStateNumber < stateNumber()))
        qDebug() << "JsonDbIndex::deindexObject" << "stale update" << objectStateNumber << stateNumber() << d->mBdb.fileName();
    return true;
}

quint32 JsonDbIndex::stateNumber() const
{
    Q_D(const JsonDbIndex);
    if (!d->mBdb.isOpen()) {
        JsonDbIndex *that = const_cast<JsonDbIndex *>(this);
        that->open();
    }
    return d->mBdb.tag();
}

JsonDbBtree::Transaction *JsonDbIndex::begin()
{
    Q_D(JsonDbIndex);
    if (!d->mBdb.isOpen())
        open();
    return d->mBdb.beginWrite();
}
bool JsonDbIndex::commit(quint32 stateNumber)
{
    Q_D(JsonDbIndex);
    if (d->mBdb.isWriting())
        return d->mBdb.writeTransaction()->commit(stateNumber);
    return false;
}
bool JsonDbIndex::abort()
{
    Q_D(JsonDbIndex);
    if (d->mBdb.isWriting())
        d->mBdb.writeTransaction()->abort();
    return true;
}
bool JsonDbIndex::clearData()
{
    Q_D(JsonDbIndex);
    d->mBdb.setFileName(d->fileName());
    return d->mBdb.clearData();
}

void JsonDbIndex::setCacheSize(quint32 cacheSize)
{
    Q_D(JsonDbIndex);
    d->mCacheSize = cacheSize;
    d->mBdb.setCacheSize(cacheSize);
}

JsonDbIndexSpec JsonDbIndexSpec::fromIndexObject(const QJsonObject &indexObject)
{
    JsonDbIndexSpec indexSpec;

    Q_ASSERT(indexObject.value(JsonDbString::kTypeStr).toString() == JsonDbString::kIndexTypeStr);
    if (indexObject.value(JsonDbString::kTypeStr).toString() != JsonDbString::kIndexTypeStr)
        return indexSpec;

    indexSpec.name = indexObject.value(JsonDbString::kNameStr).toString();
    indexSpec.propertyName = indexObject.value(JsonDbString::kPropertyNameStr).toString();
    indexSpec.propertyType = indexObject.value(JsonDbString::kPropertyTypeStr).toString();
    indexSpec.propertyFunction = indexObject.value(JsonDbString::kPropertyFunctionStr).toString();
    indexSpec.locale = indexObject.value(JsonDbString::kLocaleStr).toString();
    indexSpec.collation = indexObject.value(JsonDbString::kCollationStr).toString();
    indexSpec.casePreference = indexObject.value(JsonDbString::kCasePreferenceStr).toString();
    QJsonValue objectTypeValue = indexObject.value(JsonDbString::kObjectTypeStr);
    if (objectTypeValue.isString()) {
        indexSpec.objectTypes.append(objectTypeValue.toString());
    } else if (objectTypeValue.isArray()) {
        foreach (const QJsonValue &objectType, objectTypeValue.toArray())
            indexSpec.objectTypes.append(objectType.toString());
    }
    indexSpec.caseSensitivity = Qt::CaseSensitive;
    if (indexObject.contains(JsonDbString::kCaseSensitiveStr))
        indexSpec.caseSensitivity = indexObject.value(JsonDbString::kCaseSensitiveStr).toBool() ? Qt::CaseSensitive : Qt::CaseInsensitive;

    if (indexSpec.name.isEmpty())
        indexSpec.name = indexSpec.propertyName;

    Q_ASSERT(!indexSpec.propertyName.isEmpty() || !indexSpec.propertyFunction.isEmpty());
    Q_ASSERT(!indexSpec.name.isEmpty());

    return indexSpec;
}

static int intcmp(const uchar *aptr, const uchar *bptr)
{
    qint32 a = qFromBigEndian<qint32>((const uchar *)aptr);
    qint32 b = qFromBigEndian<qint32>((const uchar *)bptr);
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static int doublecmp(const uchar *aptr, const uchar *bptr)
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

static int qstringcmp(const quint16 *achar, quint32 acount, const quint16 *bchar, quint32 bcount)
{
    int rv = 0;
    quint32 minCount = qMin(acount, bcount);
    for (quint32 i = 0; i < minCount; i++) {
        if ((rv = (achar[i] - bchar[i])) != 0)
            return rv;
    }
    return acount-bcount;
}

int JsonDbIndexPrivate::indexCompareFunction(const QByteArray &ab, const QByteArray &bb)
{
    const char *aptr = ab.constData();
    size_t asiz = ab.size();
    const char *bptr = bb.constData();
    size_t bsiz = bb.size();

    if (!bsiz && !asiz)
        return 0;
    if (!bsiz)
        return 1;
    if (!asiz)
        return -1;

    int rv = 0;
    QJsonValue::Type avt = (QJsonValue::Type)qFromBigEndian<quint32>((const uchar *)&aptr[0]);
    QJsonValue::Type bvt = (QJsonValue::Type)qFromBigEndian<quint32>((const uchar *)&bptr[0]);
    Q_ASSERT(avt <= QJsonValue::Undefined);
    Q_ASSERT(bvt <= QJsonValue::Undefined);
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

inline static quint16 fieldValueSize(QJsonValue::Type vt, const QJsonValue &fieldValue)
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
    case QJsonValue::String: {
        quint16 size = 2 * fieldValue.toString().count();
        Q_ASSERT(size <= JsonDbSettings::instance()->indexFieldValueSize());
        return (quint16)size;
        }
    }
    return 0;
}

static void serializeFieldValue(char *data, QJsonValue::Type vt, const QJsonValue &fieldValue)
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
        quint16 size = 2 * str.count();
        Q_ASSERT(size <= JsonDbSettings::instance()->indexFieldValueSize());
        memcpy(data, (const char *)str.constData(), size);
    }
    }
}

static void deserializeFieldValue(QJsonValue::Type vt, QJsonValue &fieldValue, const char *data, quint16 size)
{
    Q_ASSERT(size <= JsonDbSettings::instance()->indexFieldValueSize());
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

void JsonDbIndexPrivate::truncateFieldValue(QJsonValue *value, const QString &type)
{
    Q_ASSERT(value);
    if ((type.isEmpty() || type == QLatin1String("string")) && value->type() == QJsonValue::String) {
        QString str = value->toString();
        int maxSize = JsonDbSettings::instance()->indexFieldValueSize() / 2;
        if (str.size() > maxSize)
            *value = str.left(maxSize);
    }
}

QJsonValue JsonDbIndexPrivate::makeFieldValue(const QJsonValue &value, const QString &type)
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

QByteArray JsonDbIndexPrivate::makeForwardKey(const QJsonValue &fieldValue, const ObjectKey &objectKey)
{
    QJsonValue::Type vt = fieldValue.type();
    Q_ASSERT(vt <= QJsonValue::Undefined);
    quint32 size = fieldValueSize(vt, fieldValue);

    QByteArray forwardKey(4+size+16, 0);
    char *data = forwardKey.data();
    qToBigEndian<quint32>(vt, (uchar *)&data[0]);
    serializeFieldValue(data+4, vt, fieldValue);
    qToBigEndian(objectKey, (uchar *)&data[4+size]);

    return forwardKey;
}

void JsonDbIndexPrivate::forwardKeySplit(const QByteArray &forwardKey, QJsonValue &fieldValue)
{
    const char *data = forwardKey.constData();
    QJsonValue::Type vt = (QJsonValue::Type)qFromBigEndian<quint32>((const uchar *)&data[0]);
    Q_ASSERT(vt <= QJsonValue::Undefined);
    quint32 fvSize = forwardKey.size()-4-16;
    deserializeFieldValue(vt, fieldValue, data+4, fvSize);
}

void JsonDbIndexPrivate::forwardKeySplit(const QByteArray &forwardKey, QJsonValue &fieldValue, ObjectKey &objectKey)
{
    const char *data = forwardKey.constData();
    QJsonValue::Type vt = (QJsonValue::Type)qFromBigEndian<quint32>((const uchar *)&data[0]);
    Q_ASSERT(vt <= QJsonValue::Undefined);
    quint32 fvSize = forwardKey.size()-4-16;
    deserializeFieldValue(vt, fieldValue, data+4, fvSize);
    objectKey = qFromBigEndian<ObjectKey>((const uchar *)&data[4+fvSize]);
}

QByteArray JsonDbIndexPrivate::makeForwardValue(const ObjectKey &objectKey)
{
    QByteArray forwardValue(16, 0);
    char *data = forwardValue.data();
    qToBigEndian(objectKey,  (uchar *)&data[0]);
    return forwardValue;
}

void JsonDbIndexPrivate::forwardValueSplit(const QByteArray &forwardValue, ObjectKey &objectKey)
{
    const uchar *data = (const uchar *)forwardValue.constData();
    objectKey = qFromBigEndian<ObjectKey>(&data[0]);
}

#include "moc_jsondbindex.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
