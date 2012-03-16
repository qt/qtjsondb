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

#include "jsondbobject.h"

#include <QJSValue>
#include <QJSValueIterator>
#include <QStringBuilder>
#include <QStringList>
#include <QCryptographicHash>

#include <qjsondocument.h>

#include "jsondb-strings.h"

QT_ADDON_JSONDB_BEGIN_NAMESPACE

JsonDbObject::JsonDbObject()
{
}

JsonDbObject::JsonDbObject(const QJsonObject &object)
    : QJsonObject(object)
{
}

JsonDbObject::~JsonDbObject()
{
}

QByteArray JsonDbObject::toBinaryData() const
{
    return QJsonDocument(*this).toBinaryData();
}

QUuid JsonDbObject::uuid() const
{
    return QUuid(value(JsonDbString::kUuidStr).toString());
}

QString JsonDbObject::version() const
{
    return value(JsonDbString::kVersionStr).toString();
}

QString JsonDbObject::type() const
{
    return value(JsonDbString::kTypeStr).toString();
}

bool JsonDbObject::isDeleted() const
{
    QJsonValue deleted = value(JsonDbString::kDeletedStr);

    if (deleted.isUndefined() || (deleted.isBool() && deleted.toBool() == false))
        return false;

    return true;
}

void JsonDbObject::markDeleted()
{
    insert(JsonDbString::kDeletedStr, true);
}

struct Uuid
{
    uint    data1;
    ushort  data2;
    ushort  data3;
    uchar   data4[8];
};

// copied from src/client/qjsondbobject.cpp:
static const Uuid JsonDbNamespace = {0x6ba7b810, 0x9dad, 0x11d1, { 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };

/*!
    Returns deterministic uuid that can be used to identify given \a identifier.

    The uuid is generated using QtJsonDb UUID namespace on a value of the
    given \a identifier.

    \sa QJsonDbObject::createUuidFromString(), QJsonDbObject::createUuid()
*/
QUuid JsonDbObject::createUuidFromString(const QString &identifier)
{
    const QUuid ns(JsonDbNamespace.data1, JsonDbNamespace.data2, JsonDbNamespace.data3,
                   JsonDbNamespace.data4[0], JsonDbNamespace.data4[1], JsonDbNamespace.data4[2],
                   JsonDbNamespace.data4[3], JsonDbNamespace.data4[4], JsonDbNamespace.data4[5],
                   JsonDbNamespace.data4[6], JsonDbNamespace.data4[7]);
    return QUuid::createUuidV3(ns, identifier);
}

void JsonDbObject::generateUuid()
{
    QLatin1String idStr("_id");
    if (contains(idStr)) {
        QUuid uuid(createUuidFromString(value(idStr).toString()));
        insert(JsonDbString::kUuidStr, uuid.toString());
    } else {
        QUuid uuid(QUuid::createUuid());
        insert(JsonDbString::kUuidStr, uuid.toString());
    }
}

/*!
 * \brief JsonDbObject::computeVersion mostly for legacy reasons.
 *        Same as calling updateVersionOptimistic(*this, &versionWritten)
 *
 * \sa updateVersionOptimistic(), version()
 * \return the new version, which is also written to the object
 */
QString JsonDbObject::computeVersion()
{
    QString versionWritten;
    updateVersionOptimistic(*this, &versionWritten);
    return versionWritten;
}

/*!
 * \brief JsonDbObject::updateVersionOptimistic implement an optimisticWrite
 * \param other the object containing the update to be written. Do NOT call computeVersion()
 *        on the other object before passing it in! other._meta.history is assumed untrusted.
 * \param versionWritten contains the version string of the write upon return
 * \return true if the passed object is a valid write. As this version can operate
 *         on conflicts too, version() and versionWritten can differ.
 */
bool JsonDbObject::updateVersionOptimistic(const JsonDbObject &other, QString *versionWrittenOut)
{
    QString versionWritten;
    // this is trusted and expected to contain a _meta object with book keeping info
    QJsonObject meta = value(JsonDbString::kMetaStr).toObject();

    // an array of all versions this object has replaced
    QJsonArray history = meta.value(QStringLiteral("history")).toArray();

    // all known conflicts
    QJsonArray conflicts = meta.value(JsonDbString::kConflictsStr).toArray();

    QString replacedVersion = other.version();

    int replacedCount;
    QString replacedHash = tokenizeVersion(replacedVersion, &replacedCount);

    int updateCount = replacedCount;
    QString hash = replacedHash;

    // we don't trust other._meta.history, so other._version must be replacedVersion
    // if other.computeVersion() was called before updateVersionOptimistic(), other can at max be a replay
    // as we lost which version other is replacing.
    bool isReplay = !other.computeVersion(replacedCount, replacedHash, &updateCount, &hash);

    bool isValidWrite = false;

    // first we check if this version can eliminate a conflict
    for (QJsonArray::const_iterator ii = conflicts.begin(); ii < conflicts.end(); ii++) {

        JsonDbObject conflict((*ii).toObject());
        if (conflict.version() == replacedVersion) {
            if (!isReplay)
                conflicts.removeAt(ii.i);
            if (!isValidWrite) {
                addAncestor(&history, updateCount, hash);
                versionWritten = versionAsString(updateCount, hash);
            }
            isValidWrite = true;
        }
    }

    // now we check if this version can progress the head
    if (version() == replacedVersion) {
        if (!isReplay)
            *this = other;
        if (!isValidWrite)
            versionWritten = versionAsString(updateCount, hash);
        insert(JsonDbString::kVersionStr, versionWritten);
        isValidWrite = true;
    }

    // make sure we can resurrect a tombstone
    // Issue: Recreating a _uuid must have a updateCount higher than the tombstone
    //        otherwise it is considered a conflict.
    if (!isValidWrite && isDeleted()) {
        if (!isReplay) {
            addAncestor(&history, replacedCount, replacedHash);
            isReplay = false;
        }

        replacedHash = tokenizeVersion(version(), &replacedCount);
        updateCount = replacedCount + 1;
        versionWritten = versionAsString(updateCount, hash);

        *this = other;
        insert(JsonDbString::kVersionStr, versionWritten);
        isValidWrite = true;
    }

    // update the book keeping of what versions we have replaced in this version branch
    if (isValidWrite && !isReplay) {
        addAncestor(&history, replacedCount, replacedHash);

        meta = QJsonObject();

        if (history.size())
            meta.insert(QStringLiteral("history"), history);
        if (conflicts.size())
            meta.insert(JsonDbString::kConflictsStr, history);

        if (!meta.isEmpty())
            insert(JsonDbString::kMetaStr, meta);
    }

    // last chance for a valid write: other is a replay from history
    if (!isValidWrite && isAncestorOf(history, updateCount, hash)) {
        isValidWrite = true;
        versionWritten = versionAsString(updateCount, hash);
    }

    if (versionWrittenOut)
        *versionWrittenOut = versionWritten;

    return isValidWrite;
}

/*!
 * \brief JsonDbObject::updateVersionReplicating implements a replicatedWrite
 * \param other the (remote) object to include into this one.
 * \return if the passed object was a valid replication
 */
bool JsonDbObject::updateVersionReplicating(const JsonDbObject &other)
{
    // these two will be the final _meta content
    QJsonArray history;
    QJsonArray conflicts;

    // let's go thru all version, i.e. this, this._conflicts, other, and other._conflicts
    {
        // thanks to the operator <, documents will sort and remove duplicates
        // the value is just for show, QSet is based on QHash, which does not sort
        QMap<JsonDbObject,bool> documents;

        QUuid id = uuid();
        populateMerge(&documents, id, *this);
        if (!populateMerge(&documents, id, other, true))
            return false;

        // now we have all versions sorted and duplicates removed
        // let's figure out what to keep, what to toss
        // this is O(n^2) but should be fine in real world situations
        for (QMap<JsonDbObject,bool>::const_iterator ii = documents.begin(); ii != documents.end(); ii++) {
            bool alive = !ii.key().isDeleted();
            for (QMap<JsonDbObject,bool>::const_iterator jj = ii + 1; alive && jj != documents.end(); jj++)
                if (ii.key().isAncestorOf(jj.key()))
                    alive = false;

            if (ii+1 == documents.end()) {
                // last element, so found the winner,
                // assigning to *this, which is head
                *this = ii.key();
                populateHistory(&history, *this, false);
            } else if (alive) {
                // this is a conflict, strip _meta and keep it
                JsonDbObject conflict(ii.key());
                conflict.remove(JsonDbString::kMetaStr);
                conflicts.append(conflict);
            } else {
                // this version was replaced, just keep history
                populateHistory(&history, ii.key(), true);
            }
        }
    }

    // let's write a new _meta into head
    if (history.size() || conflicts.size()) {
        QJsonObject meta;
        if (history.size())
            meta.insert(QStringLiteral("history"), history);
        if (conflicts.size())
            meta.insert(JsonDbString::kConflictsStr, conflicts);
        insert(JsonDbString::kMetaStr, meta);
    } else {
        // this is really just for sanity reason, but it feels better to have it
        // aka: this branch should never be reached in real world situations
        remove(JsonDbString::kMetaStr);
    }

    return true;
}

bool JsonDbObject::populateMerge(QMap<JsonDbObject,bool> *documents, const QUuid &id, const JsonDbObject &source, bool validateSource, bool recurse) const
{
    // is this the same uuid?
    bool valid = source.uuid() == id;

    if (valid && validateSource) {
        // validate that the version is actually correct
        int count;
        QString hash = tokenizeVersion(source.version(), &count);
        if (count == 0 || source.computeVersion(count, hash, 0, 0))
            valid = false;
    }

    // there is source._meta.conflicts to explore
    if (recurse && source.contains(JsonDbString::kMetaStr)) {
        QJsonArray conflicts = source.value(JsonDbString::kMetaStr).toObject().value(JsonDbString::kConflictsStr).toArray();
        for (int ii = 0; ii < conflicts.size(); ii++)
            if (!populateMerge(documents, id, conflicts.at(ii).toObject(), validateSource, false))
                valid = false;
    }

    if (valid && documents)
        documents->insert(source,true);

    return valid;
}

void JsonDbObject::populateHistory(QJsonArray *history, const JsonDbObject &doc, bool includeCurrent) const
{
    QJsonArray versions = doc.value(JsonDbString::kMetaStr).toObject().value(QStringLiteral("history")).toArray();

    for (int ii = 0; ii < versions.size(); ii++) {
        QJsonValue hash = versions.at(ii);
        if (hash.isString()) {
            addAncestor(history, ii + 1, hash.toString());
        } else if (hash.isArray()) {
            QJsonArray hashArray = hash.toArray();
            for (QJsonArray::const_iterator jj = hashArray.begin(); jj != hashArray.end(); jj++) {
                if ((*jj).isString())
                    addAncestor(history, ii + 1, (*jj).toString());
            }
        }
    }

    if (includeCurrent) {
        int updateCount;
        QString hash = tokenizeVersion(doc.version(), &updateCount);
        addAncestor(history, updateCount, hash);
    }
}

QString JsonDbObject::tokenizeVersion(const QString &versionIn, int *updateCountOut) const
{
    int updateCount;
    QString hash;

    if (versionIn.isEmpty()) {
        updateCount = 0;
    } else {
        QStringList splitUp = versionIn.split(QChar('-'));
        if (splitUp.size() == 2) {
            updateCount = qMax(1, splitUp.at(0).toInt());
            hash = splitUp.at(1);
        } else {
            updateCount = 1;
            hash = versionIn;
        }
    }

    if (updateCountOut)
        *updateCountOut = updateCount;

    return hash;
}

QString JsonDbObject::versionAsString(const int updateCount, const QString &hash) const
{
    return QString::number(updateCount) % QStringLiteral("-") % hash;
}


bool JsonDbObject::computeVersion(const int oldUpdateCount, const QString& oldHash, int *newUpdateCount, QString *newHash) const
{
    QCryptographicHash md5(QCryptographicHash::Md5);

    for (const_iterator ii = begin(); ii != end(); ii++) {
        QString key = ii.key();
        if (key == JsonDbString::kUuidStr || key == JsonDbString::kVersionStr || key == JsonDbString::kMetaStr)
            continue;

        md5.addData((char *) key.constData(), key.size() * 2);

        char kar = ii.value().type();
        md5.addData((char *) &kar, 1);

        switch (ii.value().type()) {
        case QJsonValue::Bool:
            kar = ii.value().toBool() ? '1' : '0';
            md5.addData((char *) &kar, 1);
            break;
        case QJsonValue::Double: {
            double value = ii.value().toDouble();
            md5.addData((char *) &value, sizeof(double));
            break;
        }
        case QJsonValue::String: {
            QString value = ii.value().toString();
            md5.addData((char *) value.constData(), value.size() * 2);
            break;
        }
        case QJsonValue::Array: {
            QJsonDocument doc(ii.value().toArray());
            int size;
            const char *data = doc.rawData(&size);
            md5.addData(data, size);
            break;
        }
        case QJsonValue::Object: {
            QJsonDocument doc(ii.value().toObject());
            int size;
            const char *data = doc.rawData(&size);
            md5.addData(data, size);
            break;
        }
        default:;
            // do nothing
        }
    }

    QString computedHash = QString::fromLatin1(md5.result().toHex().constData(), 10);

    if (computedHash != oldHash) {
        if (newUpdateCount)
            *newUpdateCount = oldUpdateCount + 1;
        if (newHash)
            *newHash = computedHash;
        return true;
    }

    return false;
}

/*!
 * \brief JsonDbObject::isAncestorOf tests if this JsonDbObject contains an ancestor version
 *        of the passed JsonDbObject. It does NOT take _uuid into account, it works on version()
 *        only.
 *
 *        For this method to return a valid answer, the passed object needs to have an intact
 *        _meta object.
 *
 * \param other the object to check ancestorship
 * \return true if this object is an ancestor version of the passed object
 */
bool JsonDbObject::isAncestorOf(const JsonDbObject &other) const
{
    QJsonArray history = other.value(JsonDbString::kMetaStr).toObject().value(QStringLiteral("history")).toArray();

    int updateCount;
    QString hash = tokenizeVersion(version(), &updateCount);

    return isAncestorOf(history, updateCount, hash);
}

bool JsonDbObject::isAncestorOf(const QJsonArray &history, const int updateCount, const QString &hash) const
{
    if (updateCount < 1 || history.size() < updateCount)
        return false;

    QJsonValue knownHashes = history.at(updateCount - 1);
    if (knownHashes.isString())
        return knownHashes.toString() == hash;
    else if (knownHashes.isArray())
        return knownHashes.toArray().contains(hash);
    else
        return false;
}

void JsonDbObject::addAncestor(QJsonArray *history, const int updateCount, const QString &hash) const
{
    if (updateCount < 1 || !history)
        return;

    int pos = updateCount - 1;
    for (int ii = history->size(); ii < updateCount; ii++)
        history->append(QJsonValue::Null);

    QJsonValue old = history->at(pos);
    if (old.isArray()) {
        QJsonArray multi = old.toArray();
        for (int ii = multi.size(); ii-- > 0;) {
            QString oldHash = multi.at(ii).toString();
            if (oldHash == hash) {
                return;
            } else if (oldHash < hash) {
                multi.insert(ii + 1, hash);
                history->replace(pos, multi);
                return;
            }
        }
        multi.prepend(hash);
        history->replace(pos, multi);
    } else if (!old.isString()) {
        history->replace(pos, hash);
    } else {
        QString oldHash = old.toString();
        if (oldHash == hash)
            return;

        QJsonArray multi;
        if (oldHash < hash) {
            multi.append(oldHash);
            multi.append(hash);
        } else if (oldHash > hash) {
            multi.append(hash);
            multi.append(oldHash);
        }
        history->replace(pos, multi);
    }
}

/*!
 * \brief JsonDbObject::operator < only operates based on version number.
 *        Versions are sorted by update count first, then by string comparing
 *        the hash. This operator does NOT sort by _uuid.
 *
 * \sa computeVersion(), version()
 * \param other the JsonDbObject to compare it to.
 * \return bool when left side is considered to be an early version
 */
bool JsonDbObject::operator <(const JsonDbObject &other) const
{
    int myCount;
    QString myHash = tokenizeVersion(version(), &myCount);
    int otherCount;
    QString otherHash = tokenizeVersion(other.version(), &otherCount);

    if (myCount != otherCount)
        return myCount < otherCount;

    return myHash < otherHash;
}


QJsonValue JsonDbObject::propertyLookup(const QString &path) const
{
    return propertyLookup(path.split('.'));
}

QJsonValue JsonDbObject::propertyLookup(const QStringList &path) const
{
    if (!path.size()) {
        qCritical() << "JsonDb::propertyLookup empty path";
        abort();
        return QJsonValue(QJsonValue::Undefined);
    }
    // TODO: one malloc here
    QJsonValue value(*this);
    for (int i = 0; i < path.size(); i++) {
        const QString &key = path.at(i);
        // this part of the property is a list
        if (value.isArray()) {
            QJsonArray objectList = value.toArray();
            bool ok = false;
            int index = key.toInt(&ok);
            if (ok && (index >= 0) && (objectList.size() > index))
                value = objectList.at(index);
            else
                value = QJsonValue(QJsonValue::Undefined);
        } else if (value.isObject()) {
            QJsonObject o = value.toObject();
            if (o.contains(key))
                value = o.value(key);
            else
                value = QJsonValue(QJsonValue::Undefined);
        } else {
            value = QJsonValue(QJsonValue::Undefined);
        }
    }
    return value;
}

QJsonValue JsonDbObject::fromJSValue(const QJSValue &v)
{
    if (v.isNull())
        return QJsonValue(QJsonValue::Null);
    if (v.isNumber())
        return QJsonValue(v.toNumber());
    if (v.isString())
        return QJsonValue(v.toString());
    if (v.isBool())
        return QJsonValue(v.toBool());
    if (v.isArray()) {
        QJsonArray a;
        int size = v.property("length").toInt();
        for (int i = 0; i < size; i++) {
            a.append(fromJSValue(v.property(i)));
        }
        return a;
    }
    if (v.isObject()) {
        QJSValueIterator it(v);
        QJsonObject o;
        while (it.hasNext()) {
            it.next();
            QString name = it.name();
            QJSValue value = it.value();
            o.insert(name, fromJSValue(value));
        }
        return o;
    }
    return QJsonValue(QJsonValue::Undefined);
}

QJSValue JsonDbObject::toJSValue(const QJsonValue &v, QJSEngine *scriptEngine)
{
    switch (v.type()) {
    case QJsonValue::Null:
        return QJSValue(QJSValue::NullValue);
    case QJsonValue::Undefined:
        return QJSValue(QJSValue::UndefinedValue);
    case QJsonValue::Double:
        return QJSValue(v.toDouble());
    case QJsonValue::String:
        return QJSValue(v.toString());
    case QJsonValue::Bool:
        return QJSValue(v.toBool());
    case QJsonValue::Array: {
        QJSValue jsArray = scriptEngine->newArray();
        QJsonArray array = v.toArray();
        for (int i = 0; i < array.size(); i++)
            jsArray.setProperty(i, toJSValue(array.at(i), scriptEngine));
        return jsArray;
    }
    case QJsonValue::Object:
        return toJSValue(v.toObject(), scriptEngine);
    }
    return QJSValue(QJSValue::UndefinedValue);
}

QJSValue JsonDbObject::toJSValue(const QJsonObject &object, QJSEngine *scriptEngine)
{
    QJSValue jsObject = scriptEngine->newObject();
    for (QJsonObject::const_iterator it = object.begin(); it != object.end(); ++it)
        jsObject.setProperty(it.key(), toJSValue(it.value(), scriptEngine));
    return jsObject;
}

QT_ADDON_JSONDB_END_NAMESPACE
