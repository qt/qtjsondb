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
    , mScriptEngine(0)
    , mCacheSize(0)
{
}

JsonDbIndexPrivate::~JsonDbIndexPrivate()
{
}

QString JsonDbIndexPrivate::fileName() const
{
    return QString::fromLatin1("%1/%2-%3-Index.db").arg(mPath).arg(mBaseName).arg(mSpec.name);
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

    if (spec.propertyName.isEmpty() && !spec.propertyFunction.isEmpty()) {
        if (!d->mScriptEngine)
            d->mScriptEngine = JsonDbScriptEngine::scriptEngine();

        // for "emit"
        JsonDbJoinProxy *mapProxy = new JsonDbJoinProxy(0, 0, this);
        connect(mapProxy, SIGNAL(viewObjectEmitted(QJSValue)),
                this, SLOT(propertyValueEmitted(QJSValue)));
        QString proxyName(QString::fromLatin1("_jsondbIndexProxy%1").arg(d->mSpec.name));
        proxyName.replace(QLatin1Char('.'), QLatin1Char('$'));
        d->mScriptEngine->globalObject().setProperty(proxyName, d->mScriptEngine->newQObject(mapProxy));

        QString script(QString::fromLatin1("(function() { var jsondb={emit: %2.create, lookup: %2.lookup }; var fcn = (%1); return fcn})()")
                       .arg(spec.propertyFunction).arg(proxyName));
        d->mPropertyFunction = d->mScriptEngine->evaluate(script);
        if (d->mPropertyFunction.isError() || !d->mPropertyFunction.isCallable())
            qDebug() << "Unable to parse index value function: " << d->mPropertyFunction.toString();
    }

    setObjectName(d->mSpec.name);
}

const JsonDbIndexSpec &JsonDbIndex::indexSpec() const
{
    Q_D(const JsonDbIndex);
    return d->mSpec;
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

    d->mBdb.setCompareFunction(forwardKeyCmp);

    if (jsondbSettings->verbose())
        qDebug() << "JsonDbIndex::open" << d->mBdb.tag() << d->mBdb.fileName();
    return true;
}

void JsonDbIndex::close()
{
    Q_D(JsonDbIndex);
    if (!d->mBdb.isOpen())
        return;
    if (jsondbSettings->verbose())
        qDebug() << "JsonDbIndex::close" << d->mBdb.tag() << d->mBdb.fileName();
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

QJsonValue JsonDbIndex::indexValue(const QJsonValue &v)
{
    Q_D(JsonDbIndex);
    if (!v.isString())
        return v;

    QJsonValue result;
    if (d->mSpec.caseSensitivity == Qt::CaseInsensitive)
        result = v.toString().toLower();
    else
        result = v;

#ifndef NO_COLLATION_SUPPORT
    if (!d->mSpec.collation.isEmpty() && !d->mSpec.locale.isEmpty())
        result = _q_bytesToHexString(d->mCollator.sortKey(v.toString()));
#endif

    return result;
}

QList<QJsonValue> JsonDbIndex::indexValues(JsonDbObject &object)
{
    Q_D(JsonDbIndex);
    d->mFieldValues.clear();
    if (!d->mScriptEngine) {
        int size = d->mPropertyNamePath.size();
        if (d->mPropertyNamePath.at(size-1) == QLatin1Char('*')) {
            QJsonValue v = object.valueByPath(d->mPropertyNamePath.mid(0, size-1));
            QJsonArray array = v.toArray();
            d->mFieldValues.reserve(array.size());
            for (int i = 0; i < array.size(); ++i) {
                d->mFieldValues.append(indexValue(array.at(i)));
            }
        } else {
            QJsonValue v = object.valueByPath(d->mPropertyNamePath);
            if (!v.isUndefined()) {
                d->mFieldValues.append(indexValue(v));
            }
        }
    } else {
        QJSValueList args;
        args << d->mScriptEngine->toScriptValue(object.toVariantMap());
        d->mPropertyFunction.call(args);
    }
    return d->mFieldValues;
}

void JsonDbIndex::propertyValueEmitted(QJSValue value)
{
    Q_D(JsonDbIndex);
    if (!value.isUndefined())
        d->mFieldValues.append(d->mScriptEngine->fromScriptValue<QJsonValue>(value));
}

void JsonDbIndex::indexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 objectStateNumber)
{
    Q_D(JsonDbIndex);
    if (d->mSpec.propertyName == JsonDbString::kUuidStr)
        return;

    if (!d->mSpec.objectTypes.isEmpty() && !d->mSpec.objectTypes.contains(object.value(JsonDbString::kTypeStr).toString()))
        return;

    Q_ASSERT(!object.contains(JsonDbString::kDeletedStr)
             && !object.value(JsonDbString::kDeletedStr).toBool());
    QList<QJsonValue> fieldValues = indexValues(object);
    if (!fieldValues.size())
        return;
    bool ok;
    if (!d->mBdb.isOpen())
        open();
    if (!d->mBdb.isWriting())
        d->mObjectTable->begin(this);
    JsonDbBtree::Transaction *txn = d->mBdb.writeTransaction();
    for (int i = 0; i < fieldValues.size(); i++) {
        QJsonValue fieldValue = fieldValues.at(i);
        fieldValue = makeFieldValue(fieldValue, d->mSpec.propertyType);
        if (fieldValue.isUndefined())
            continue;
        truncateFieldValue(&fieldValue, d->mSpec.propertyType);
        QByteArray forwardKey(makeForwardKey(fieldValue, objectKey));
        QByteArray forwardValue(makeForwardValue(objectKey));

        if (jsondbSettings->debug())
            qDebug() << "indexing" << objectKey << d->mSpec.propertyName << fieldValue
                     << "forwardIndex" << "key" << forwardKey.toHex()
                     << "forwardIndex" << "value" << forwardValue.toHex()
                     << object;
        ok = txn->put(forwardKey, forwardValue);
        if (!ok) qCritical() << __FUNCTION__ << "putting fowardIndex" << d->mBdb.errorMessage();
    }
    if (jsondbSettings->debug() && (objectStateNumber < stateNumber()))
        qDebug() << "JsonDbIndex::indexObject" << "stale update" << objectStateNumber << stateNumber() << d->mBdb.fileName();

#ifdef CHECK_INDEX_ORDERING
    checkIndex()
#endif
}

void JsonDbIndex::deindexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 objectStateNumber)
{
    Q_D(JsonDbIndex);
    if (d->mSpec.propertyName == JsonDbString::kUuidStr)
        return;
    if (!d->mSpec.objectTypes.isEmpty() && !d->mSpec.objectTypes.contains(object.value(JsonDbString::kTypeStr).toString()))
        return;
    if (!d->mBdb.isOpen())
        open();
    QList<QJsonValue> fieldValues = indexValues(object);
    if (!fieldValues.size())
        return;
    if (!d->mBdb.isWriting())
        d->mObjectTable->begin(this);
    JsonDbBtree::Transaction *txn = d->mBdb.writeTransaction();
    for (int i = 0; i < fieldValues.size(); i++) {
        QJsonValue fieldValue = fieldValues.at(i);
        fieldValue = makeFieldValue(fieldValue, d->mSpec.propertyType);
        if (fieldValue.isUndefined())
            continue;
        truncateFieldValue(&fieldValue, d->mSpec.propertyType);
        if (jsondbSettings->debug())
            qDebug() << "deindexing" << objectKey << d->mSpec.propertyName << fieldValue;
        QByteArray forwardKey(makeForwardKey(fieldValue, objectKey));
        if (!txn->remove(forwardKey)) {
            qDebug() << "deindexing failed" << objectKey << d->mSpec.propertyName << fieldValue << object << forwardKey.toHex();
        }
    }
    if (jsondbSettings->verbose() && (objectStateNumber < stateNumber()))
        qDebug() << "JsonDbIndex::deindexObject" << "stale update" << objectStateNumber << stateNumber() << d->mBdb.fileName();
#ifdef CHECK_INDEX_ORDERING
    checkIndex();
#endif
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

#include "moc_jsondbindex.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
