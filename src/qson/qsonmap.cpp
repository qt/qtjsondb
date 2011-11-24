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

#include "qsonmap_p.h"
#include "qsonlist_p.h"
#include "qsonelement_p.h"
#include "qsonstrings_p.h"
#include "qsonuuid_p.h"
#include "qsonversion_p.h"

#include <QUuid>
#include <QCryptographicHash>

#include <QDebug>

namespace QtAddOn { namespace JsonDb {

/*!
  \class QtAddOn::JsonDb::QsonMap
  \brief The QsonMap class provides methods for accessing the elements of a QsonObject.
  \sa QsonObject, QsonList
*/

QsonMap::QsonMap(const QsonObject &object)
{
    switch (object.type()) {
    case MapType:
        mHeader = object.mHeader;
        mBody = object.mBody;
        mFooter = object.mFooter;
        break;
    default:
        mHeader = new QsonPage(QsonPage::OBJECT_HEADER_PAGE);
        mFooter = new QsonPage(QsonPage::OBJECT_FOOTER_PAGE);
        break;
    }
}

bool QsonMap::isDocument() const
{
    return mHeader->type() == QsonPage::DOCUMENT_HEADER_PAGE;
}

bool QsonMap::isMeta() const
{
    return mHeader->type() == QsonPage::META_HEADER_PAGE;
}

int QsonMap::size() const
{
    return index()->size();
}

QStringList QsonMap::keys() const
{
    return index()->keys();
}

bool QsonMap::contains(const QString &key) const
{
    return index()->contains(key);
}

QsonObject::Type QsonMap::valueType(const QString &key) const
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber == -1) {
        return QsonObject::UnknownType;
    } else if (entry.pageNumber < -1) {
        return QsonObject::StringType;
    } else if (entry.valueOffset == 0) {
        switch (mBody.at(entry.pageNumber)->type()) {
        case QsonPage::OBJECT_HEADER_PAGE:
        case QsonPage::DOCUMENT_HEADER_PAGE:
        case QsonPage::META_HEADER_PAGE:
            return QsonObject::MapType;
        case QsonPage::LIST_HEADER_PAGE:
            return QsonObject::ListType;
        default:
            Q_ASSERT(false);
            return QsonObject::UnknownType; // should never happen (TM)
        }
    } else {
        switch (mBody.at(entry.pageNumber)->constData()[entry.valueOffset]) {
        case QsonPage::NULL_TYPE:
            return QsonObject::NullType;
        case QsonPage::TRUE_TYPE:
        case QsonPage::FALSE_TYPE:
            return QsonObject::BoolType;
        case QsonPage::INT_TYPE:
            return QsonObject::IntType;
        case QsonPage::UINT_TYPE:
            return QsonObject::UIntType;
        case QsonPage::DOUBLE_TYPE:
            return QsonObject::DoubleType;
        case QsonPage::STRING_TYPE:
        case QsonPage::KEY_TYPE:
        case QsonPage::UUID_TYPE:
        case QsonPage::VERSION_TYPE:
            return QsonObject::StringType;
        default:
            Q_ASSERT(false);
            return QsonObject::UnknownType; // should never happen (TM)
        }
    }
}

bool QsonMap::isNull(const QString &key) const
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber == -1) {
        return true;
    } else if (entry.valueOffset == 0) {
        return false;
    } else {
        return mBody.at(entry.pageNumber)->readNull(entry.valueOffset);
    }
}

bool QsonMap::valueBool(const QString &key, bool fallback) const
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber == -1) {
        return fallback;
    } else if (entry.valueOffset == 0) {
        return true;
    } else {
        return mBody.at(entry.pageNumber)->readBool(entry.valueOffset);
    }
}

quint64 QsonMap::valueUInt(const QString &key, quint64 fallback) const
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber == -3) { // _version
        return mFooter->readUInt(4, 0);
    } else if (entry.pageNumber == -4) { // _lastVersion
        return mHeader->readUInt(4, 0);
    } else if (entry.pageNumber == -1 || entry.valueOffset == 0) {
        return fallback;
    } else {
        return mBody.at(entry.pageNumber)->readUInt(entry.valueOffset, fallback);
    }
}

qint64 QsonMap::valueInt(const QString &key, qint64 fallback) const
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber == -3) { // _version
        return (qint64) mFooter->readUInt(4, 0);
    } else if (entry.pageNumber == -4) { // _lastVersion
        return (qint64) mHeader->readUInt(4, 0);
    } else if (entry.pageNumber == -1 || entry.valueOffset == 0) {
        return fallback;
    } else {
        return mBody.at(entry.pageNumber)->readInt(entry.valueOffset, fallback);
    }
}

double QsonMap::valueDouble(const QString &key, double fallback) const
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber == -1 || entry.valueOffset == 0) {
        return fallback;
    } else {
        return mBody.at(entry.pageNumber)->readDouble(entry.valueOffset, fallback);
    }
}

QString QsonMap::valueString(const QString &key, const QString &fallback) const
{
    // do not require the index for fixed location stuff
    if (isDocument() && !key.isEmpty() && key.at(0) == QChar::fromLatin1('_')) {
        if (key == QsonStrings::kUuidStr)
            return mHeader->readString(26);
        else if (key == QsonStrings::kVersionStr)
            return mFooter->readString(4);
        else if (key == QsonStrings::kLastVersionStr)
            return mHeader->readString(4);
    }

    QsonEntry entry = index()->value(key);
    if (entry.pageNumber == -1 || entry.valueOffset == 0)
        return fallback;
    else
        return mBody.at(entry.pageNumber)->readString(entry.valueOffset);
}

QUuid QsonMap::uuid() const
{
    if (!isDocument())
        return QUuid();
    return mHeader->readUuid(26);
}

QsonMap QsonMap::subObject(const QString &key) const
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber < 0 || entry.valueOffset > 0) {
        return QsonMap();
    }

    switch (mBody.at(entry.pageNumber)->type()) {
    case QsonPage::OBJECT_HEADER_PAGE:
    case QsonPage::DOCUMENT_HEADER_PAGE:
    case QsonPage::META_HEADER_PAGE:
        return QsonMap(mBody.mid(entry.pageNumber, entry.valueLength));
    default:
        return QsonMap();
    }
}

QsonList QsonMap::subList(const QString &key) const
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber < 0 || entry.valueOffset > 0)
        return QsonList();

    switch (mBody.at(entry.pageNumber)->type()) {
    case QsonPage::LIST_HEADER_PAGE:
        return QsonList(mBody.mid(entry.pageNumber, entry.valueLength));
    default:
        return QsonList();
    }
}

QsonMap& QsonMap::insert(const QString &key, QsonObject::Special value)
{
    if (value != QsonObject::NullValue)
        return *this;

    CachedIndex::Cleaner cleaner(mIndex);

    if (!specialHandling(key)) {
        remove(key);
        ensurePage(QsonPage::KEY_VALUE_PAGE);
        if (!mBody.last()->writeKey(key)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeKey(key);
        }
        if (!mBody.last()->writeValue()) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeValue();
        }
    }
    return *this;
}

QsonMap& QsonMap::insert(const QString &key, bool value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    if (!specialHandling(key)) {
        remove(key);
        ensurePage(QsonPage::KEY_VALUE_PAGE);
        if (!mBody.last()->writeKey(key)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeKey(key);
        }
        if (!mBody.last()->writeValue(value)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeValue(value);
        }
    }
    return *this;
}

QsonMap& QsonMap::insert(const QString &key, quint64 value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    if (!specialHandling(key)) {
        remove(key);
        ensurePage(QsonPage::KEY_VALUE_PAGE);
        if (!mBody.last()->writeKey(key)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeKey(key);
        }
        if (!mBody.last()->writeValue(value)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeValue(value);
        }
    }
    return *this;
}

QsonMap& QsonMap::insert(const QString &key, qint64 value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    if (!specialHandling(key)) {
        remove(key);
        ensurePage(QsonPage::KEY_VALUE_PAGE);
        if (!mBody.last()->writeKey(key)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeKey(key);
        }
        if (!mBody.last()->writeValue(value)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeValue(value);
        }
    }

    return *this;
}

QsonMap& QsonMap::insert(const QString &key, double value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    if (!specialHandling(key)) {
        remove(key);
        ensurePage(QsonPage::KEY_VALUE_PAGE);
        if (!mBody.last()->writeKey(key)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeKey(key);
        }
        if (!mBody.last()->writeValue(value)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeValue(value);
        }
    }
    return *this;
}

QsonMap& QsonMap::insert(const QString &key, const QString &value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    if (!specialHandling(key, &value)) {
        remove(key);
        ensurePage(QsonPage::KEY_VALUE_PAGE);
        if (!mBody.last()->writeKey(key)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeKey(key);
        }
        if (!mBody.last()->writeValue(value)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeValue(value);
        }
    }
    return *this;
}

QsonMap& QsonMap::insert(const QString &key, const QsonObject &value)
{
    if (value.isNull()) {
        return insert(key, NullValue);
    } else if (value.type() == QsonObject::StringType) {
        return insert(key, QsonElement(value).value<QString>());
    }

    CachedIndex::Cleaner cleaner(mIndex);

    if (!specialHandling(key)) {
        remove(key);
        ensurePage(QsonPage::KEY_VALUE_PAGE);
        if (!mBody.last()->writeKey(key)) {
            ensurePage(QsonPage::KEY_VALUE_PAGE, true);
            mBody.last()->writeKey(key);
        }

        const QsonPage::PageType headerType = value.mHeader->type();
        bool isMeta = headerType == QsonPage::META_HEADER_PAGE;

        if (isMeta)
            mBody.append(QsonPagePtr(new QsonPage(QsonPage::OBJECT_HEADER_PAGE)));
        else if (headerType != QsonPage::EMPTY_PAGE && headerType != QsonPage::UNKNOWN_PAGE)
            mBody.append(value.mHeader);

        switch (value.type()) {
        case QsonObject::ListType:
        case QsonObject::MapType:
            mBody.append(value.mBody);
            break;
        case QsonObject::BoolType:
            writeValue(QsonElement(value).value<bool>());
            break;
        case QsonObject::IntType:
            writeValue(QsonElement(value).value<qint64>());
            break;
        case QsonObject::UIntType:
            writeValue(QsonElement(value).value<quint64>());
            break;
        case QsonObject::DoubleType:
            writeValue(QsonElement(value).value<double>());
            break;
        default:
            break;
        }

        if (isMeta)
            mBody.append(QsonPagePtr(new QsonPage(QsonPage::OBJECT_FOOTER_PAGE)));
        else if (value.mFooter->type() != QsonPage::EMPTY_PAGE && value.mFooter->type() != QsonPage::UNKNOWN_PAGE)
            mBody.append(value.mFooter);
    } else if (key == QsonStrings::kMetaStr && value.type() == QsonObject::MapType) {
        ensureDocument();
        QsonEntry entry = index()->value(QsonStrings::kMetaStr);
        QsonContent newBody;
        if (value.mBody.size() > 0) { // only insert, if there is actually content
            newBody.append(QsonPagePtr(new QsonPage(QsonPage::META_HEADER_PAGE)));
            newBody.append(value.mBody);
            newBody.append(QsonPagePtr(new QsonPage(QsonPage::META_FOOTER_PAGE)));
        }
        if (entry.pageNumber == -1) {
            newBody.append(mBody);
        } else {
            newBody.append(mBody.mid(entry.valueLength));
        }
        mBody = newBody;
    }
    return *this;
}

QsonMap& QsonMap::remove(const QString &key)
{
    QsonEntry entry = index()->value(key);
    if (entry.pageNumber < 0)
        return *this;

    CachedIndex::Cleaner cleaner(mIndex);

    bool keyInSamePage;
    int keySize = 4 + (2 * qMin(30000, key.size()));

    if (entry.valueOffset == 0) {
        mBody.erase(mBody.begin()+entry.pageNumber, mBody.begin()+entry.pageNumber+entry.valueLength);
        keyInSamePage = false;
    } else {
        if (entry.valueOffset-keySize >= 4) {
            entry.valueOffset -= keySize;
            entry.valueLength += keySize;
            keyInSamePage = true;
        } else {
            keyInSamePage = false;
        }

        QsonPagePtr valuePage = mBody[entry.pageNumber];

        if ((valuePage->mOffset - entry.valueLength) > 4) {
            int cutFromPos = entry.valueOffset;
            int cutToPos = cutFromPos + entry.valueLength;

            if (cutToPos < valuePage->mOffset) {
                QByteArray newPage;
                newPage.reserve(valuePage->mOffset);
                newPage.append(valuePage->constData(), cutFromPos);
                newPage.append(valuePage->constData() + cutToPos, valuePage->mOffset - cutToPos);
                valuePage->mPage = newPage;
                valuePage->mPageOffset = 0;
            }
            valuePage->mOffset -= entry.valueLength;
            valuePage->updateOffset();
            mBody[entry.pageNumber] = valuePage;
        } else {
            mBody.removeAt(entry.pageNumber);
        }
    }

    if (!keyInSamePage && (entry.pageNumber > 0)) { // _meta is page 0 w/o key
        int keyPageNr = entry.pageNumber - 1;
        QsonPagePtr keyPage = mBody[keyPageNr];
        int newSize = keyPage->mOffset - keySize;

        if (newSize == 4) {
            mBody.removeAt(keyPageNr);
        } else {
            keyPage->resize(newSize);
            keyPage->mOffset = newSize;
            keyPage->updateOffset();
            mBody[keyPageNr] = keyPage;
        }
    }

    return *this;
}

void QsonMap::generateUuid()
{
    CachedIndex::Cleaner cleaner(mIndex);

    if (ensureDocument()) {
        QByteArray uuid;
        QsonEntry entry = index()->value(QsonStrings::kIdStr);
        if (entry.pageNumber != -1 && entry.valueOffset > 0) {
            uuid = QsonUUIDv3(mBody.at(entry.pageNumber)->readString(entry.valueOffset));
            Q_ASSERT(uuid.size() == 16);
        } else {
            uuid = QUuid::createUuid().toRfc4122();
            Q_ASSERT(uuid.size() == 16);
        }
        memcpy(mHeader->data() + 28, uuid.constData(), 16);
    }
}

void QsonMap::computeVersion(bool increaseCount)
{
    CachedIndex::Cleaner cleaner(mIndex);

    ensureDocument();
    QCryptographicHash md5(QCryptographicHash::Md5);

    int metaCount = 0;
    foreach (const QsonPagePtr &page, mBody) {
        switch (page->type()) {
        case QsonPage::KEY_VALUE_PAGE:
        case QsonPage::ARRAY_VALUE_PAGE:
            if (metaCount == 0) {
                // do not take paging into account, i.e. cut page header off
                md5.addData(page->constData() + 4, page->dataSize() - 4);
            }
            break;
        case QsonPage::META_HEADER_PAGE:
            metaCount++;
            break;
        case QsonPage::META_FOOTER_PAGE:
            metaCount--;
            break;
        default:
            if (metaCount == 0) {
                // every other page needs to go into the hash
                md5.addData(page->constData(), page->dataSize());
            }
        }
    }

    const char *header = mHeader->constData();
    char *footer = mFooter->data();

    QByteArray hash = md5.result();
    QByteArray lastHash(header + 10, 16);

    // write the hash
    memcpy(footer + 10, hash.constData(), 16);

    quint32 *lastCount = (quint32*) (header + 6);
    quint32 *count = (quint32*) (footer + 6);

    // increase the count if changed
    *count = (*lastCount) + ((increaseCount && (hash != lastHash)) ? 1 : 0);

    // make sure, we never generate a 0 version
    if (*count == 0) {
        *count = 1;
    }

    // if first version, set the _lastVersion to _version
    if (*count == 1) {
        // only detach now
        memcpy(mHeader->data() + 6, footer + 6, 20);
    }
}

bool QsonMap::mergeVersions(const QsonMap &other, bool isReplication)
{
    CachedIndex::Cleaner cleaner(mIndex);

    if (!isDocument() || !other.isDocument()) {
        qWarning() << "merging only supported for documents";
        return false;
    }
    if (valueString(QsonStrings::kUuidStr) != other.valueString(QsonStrings::kUuidStr)) {
        qWarning() << "merging only supported for matching uuids";
        return false;
    }

    QsonVersion thisVersion = QsonVersion::version(*this);
    QsonVersion otherVersion = QsonVersion::version(other);

    if (!thisVersion.isValid() || !otherVersion.isValid()) {
        qWarning() << "always make sure to call computeVersion() before mergeVersions()";
        return false;
    }

    if (thisVersion == otherVersion) {
        return false;
    }

    // make sure recreating a dead object results in a live one
    if (!isReplication
            && valueBool(QsonStrings::kDeleted)
            && !other.valueBool(QsonStrings::kDeleted)
            && other.valueInt(QsonStrings::kVersionStr) == 1)
        otherVersion.setUpdateCount(1 + thisVersion.updateCount());

    QsonVersionMap liveVersions;
    QsonVersionSet ancestorVersions;

    populateMerge(*this, thisVersion, liveVersions, ancestorVersions);

    // let's check for replay
    if (ancestorVersions.contains(otherVersion)) {
        return false;
    }
    if (liveVersions.keys().contains(otherVersion)) {
        return false;
    }

    populateMerge(other, otherVersion, liveVersions, ancestorVersions);

    // remove everything we assume an ancestor
    foreach (const QsonVersion ancestor, ancestorVersions) {
        liveVersions.remove(ancestor);
    }

    // sanity check
    if (liveVersions.size() == 0) {
        qWarning() << "no live version left";
        return false;
    }

    // select winner
    QsonMap winner = liveVersions.take(liveVersions.keys().last());

    // now remove all tombstones from conflicts
    for (QsonVersionMap::iterator ii = liveVersions.begin(); ii != liveVersions.end(); ii++) {
        if (ii->valueBool(QsonStrings::kDeleted)) {
            // keep the tombstone only as an ancestor
            ancestorVersions.insert(QsonVersion::version(*ii));
            ii = liveVersions.erase(ii);
        }
    }

    QsonMap meta;
    // do we have conflicts?
    if (liveVersions.size() > 0) {
        QsonList conflicts;
        for (QsonVersionMap::iterator ii = liveVersions.begin(); ii != liveVersions.end(); ii++) {
            // make sure meta is empty in conflicts, TODO: removeMeta()
            ii->insert(QsonStrings::kMetaStr, meta);
            // add it to the resulting conflicts object
            conflicts.append(*ii);
        }
        // stick conflicts into _meta
        meta.insert(QsonStrings::kConflictsStr, conflicts);
    }

    // do we have ancestors?
    if (ancestorVersions.size() > 0) {
        QsonList ancestors;
        foreach (const QsonVersion ancestor, ancestorVersions) {
            // add them to the list
            ancestors.ensurePage(QsonPage::ARRAY_VALUE_PAGE);
            if (!ancestors.mBody.last()->writeVersion(ancestor)) {
                ancestors.ensurePage(QsonPage::ARRAY_VALUE_PAGE, true);
                ancestors.mBody.last()->writeVersion(ancestor);
            }
        }
        // stick the ancestor list into _meta
        meta.insert(QsonStrings::kAncestorsStr, ancestors);
    }

    // now stick _meta into the winner
    winner.insert(QsonStrings::kMetaStr, meta);

    // now we replace ourself by the winner
    // returning winner is not an option, because return false is "nothing to do"
    mHeader = winner.mHeader;
    mBody = winner.mBody;
    mFooter = winner.mFooter;

    // increase last version, such that computeVersion() actually increases
    memcpy(mHeader->data() + 6, mFooter->constData() + 6, 20);

    return true;
}

void QsonMap::populateMerge(const QsonMap &document, const QsonVersion &version, QsonVersionMap &live, QsonVersionSet &ancestors)
{
    CachedIndex::Cleaner cleaner(mIndex);
    QsonVersion lastVersion = QsonVersion::lastVersion(document);

    if (version == QsonVersion::version(document)) {
        live[version] = document;
        if (version != lastVersion)
            ancestors.insert(lastVersion);
    } else {
        QsonMap updatedDocument(document);
        updatedDocument.insert(QsonStrings::kVersionStr, version.toString()); // TODO
        live[version] = updatedDocument;
        lastVersion = version;
    }

    QsonMap meta = document.subObject(QsonStrings::kMetaStr);

    QsonList oldVersions = meta.subList(QsonStrings::kAncestorsStr);
    QsonObject::CachedIndex *oldIdx = oldVersions.index();
    CachedIndex::Cleaner cleanerOldIdx(*oldIdx);
    foreach (const QsonEntry &oldEntry, *oldIdx) {
        if (oldEntry.valueOffset > 0 && oldEntry.valueLength == 22) {
            QsonVersion oldVersion(oldVersions.mBody[oldEntry.pageNumber]->constData() + oldEntry.valueOffset);
            if (oldVersion.isValid())
                ancestors.insert(oldVersion);
        }
    }

    QsonList conflicts = meta.subList(QsonStrings::kConflictsStr);
    QsonObject::CachedIndex *conflictIdx = conflicts.index();
    CachedIndex::Cleaner cleanerConflictIdx(*conflictIdx);
    foreach (const QsonEntry conflictEntry, *conflictIdx) {
        if (conflictEntry.valueOffset == 0 && conflicts.mBody.at(conflictEntry.pageNumber)->type() == QsonPage::DOCUMENT_HEADER_PAGE) {
            QsonMap conflictObj(conflicts.mBody.mid(conflictEntry.pageNumber, conflictEntry.valueLength));
            QsonVersion conflictVersion = QsonVersion::version(conflictObj);
            if (conflictVersion.isValid()) {
                QsonVersion cLastVersion = QsonVersion::lastVersion(conflictObj);
                QsonVersion cVersion = QsonVersion::lastVersion(conflictObj);
                if (cVersion != cLastVersion) {
                    ancestors.insert(cLastVersion);
                } else {
                    // make sure to increase the _lastVersion on conflicts
                    memcpy(conflictObj.mHeader->data() + 6, conflictObj.mFooter->constData() + 6, 20);
                }
                live[conflictVersion] = conflictObj;
            }
        }
    }
}

bool QsonMap::ensureDocument()
{
    if (mHeader->type() != QsonPage::DOCUMENT_HEADER_PAGE) {
        mHeader = new QsonPage(QsonPage::DOCUMENT_HEADER_PAGE);
        mFooter = new QsonPage(QsonPage::DOCUMENT_FOOTER_PAGE);
        generateUuid();
        return false;
    } else {
        return true;
    }
}

bool QsonMap::specialHandling(const QString &key, const QString *value)
{
    if (!key.isEmpty() && key.at(0) == QChar::fromLatin1('_')) {
        if (key == QsonStrings::kUuidStr) {
            if (value) {
                ensureDocument();
                QByteArray uuid = QByteArray::fromHex(value->toLatin1()).leftJustified(16, 0, true);
                memcpy(mHeader->data() + 28, uuid.constData(), 16);
            } else {
                qWarning() << QsonStrings::kUuidStr << "takes a string value, ignoring";
            }
            return true;
        } else if (key == QsonStrings::kLastVersionStr) {
            if (value) {
                QsonVersion version = QsonVersion::fromLiteral(*value);
                if (version.isValid()) {
                    ensureDocument();
                    memcpy(mHeader->data() + 4, version.content().constData(), version.content().size());
                }
            }
            return true;
        } else if (key == QsonStrings::kVersionStr) {
            if (value) {
                QsonVersion version = QsonVersion::fromLiteral(*value);
                if (version.isValid()) {
                    ensureDocument();
                    // set both _version and _lastVersion
                    memcpy(mHeader->data() + 4, version.content().constData(), version.content().size());
                    memcpy(mFooter->data() + 4, version.content().constData(), version.content().size());
                }
            }
            return true;
        } else if (key == QsonStrings::kMetaStr) {
            return true;
        }
    }
    return false;
}

QsonObject::CachedIndex *QsonMap::index() const
{
    if (!mIndex.isEmpty())
        return const_cast<QsonObject::CachedIndex*>(&mIndex);

    // FIXME probably we do not need to generate full index, in most cases we need only one QsonEntry
    QList<QsonPage::PageType> stack;
    bool keyNext = true;
    QString currentKey;
    QsonEntry currentEntry;

    for (int ii = 0; ii < mBody.length(); ii++) {
        const QsonPage &currentPage = *mBody.at(ii);
        const QsonPage::PageType type = currentPage.type();

        if (type == QsonPage::KEY_VALUE_PAGE) {
            if (stack.isEmpty()) {
                qson_size pageSize = currentPage.dataSize();
                qson_size pageOffset = 4;
                while (pageOffset < pageSize) {
                    if (keyNext) {
                        int keySize = currentPage.readSize(pageOffset, true);
                        if (keySize == 0) {
                            qWarning() << "expected key not found";
                            mIndex.clear();
                            return &mIndex;
                        } else {
                            currentKey = currentPage.readString(pageOffset);
                            pageOffset += keySize;
                            keyNext = false;
                        }
                    } else {
                        int valueSize = currentPage.readSize(pageOffset, false);
                        if (valueSize == 0) {
                            qWarning() << "expected value not found in map" << currentKey << ii << pageOffset << pageSize;
                            mIndex.clear();
                            return &mIndex;
                        } else {
                            currentEntry.pageNumber = ii;
                            currentEntry.valueOffset = pageOffset;
                            currentEntry.valueLength = valueSize;
                            mIndex[currentKey] = currentEntry;
                            pageOffset += valueSize;
                            keyNext = true;
                        }
                    }
                }
            } else if (stack.last() == QsonPage::LIST_HEADER_PAGE) {
                qWarning() << "unexpected keyvalue page in list";
                mIndex.clear();
                return &mIndex;
            }
        } else if (type == QsonPage::OBJECT_HEADER_PAGE) {
            if (stack.isEmpty()) {
                if (keyNext) {
                    qWarning() << "unexpected object header";
                    mIndex.clear();
                    return &mIndex;
                } else {
                    currentEntry.pageNumber = ii;
                    currentEntry.valueOffset = 0;
                }
            }
            stack.push_back(type);
        } else if (type == QsonPage::OBJECT_FOOTER_PAGE) {
            if (stack.isEmpty() || stack.takeLast() != QsonPage::OBJECT_HEADER_PAGE) {
                qWarning() << "unexpected object footer";
                mIndex.clear();
                return &mIndex;
            } else if (stack.isEmpty()) {
                currentEntry.valueLength = 1 + ii - currentEntry.pageNumber;
                mIndex[currentKey] = currentEntry;
                keyNext = true;
            }
        } else if (type == QsonPage::LIST_HEADER_PAGE || type == QsonPage::DOCUMENT_HEADER_PAGE) {
            if (stack.isEmpty()) {
                if (keyNext) {
                    qWarning() << "unexpected footer";
                    mIndex.clear();
                    return &mIndex;
                } else {
                    currentEntry.pageNumber = ii;
                    currentEntry.valueOffset = 0;
                }
            }
            stack.push_back(type);
        } else if (type == QsonPage::LIST_FOOTER_PAGE) {
            if (stack.isEmpty() || stack.takeLast() != QsonPage::LIST_HEADER_PAGE) {
                qWarning() << "unexpected list footer";
                mIndex.clear();
                return &mIndex;
            } else if (stack.isEmpty()) {
                currentEntry.valueLength = 1 + ii - currentEntry.pageNumber;
                mIndex[currentKey] = currentEntry;
                keyNext = true;
            }
        } else if (type == QsonPage::DOCUMENT_FOOTER_PAGE) {
            if (stack.isEmpty() || stack.takeLast() != QsonPage::DOCUMENT_HEADER_PAGE) {
                qWarning() << "unexpected document footer";
                mIndex.clear();
                return &mIndex;
            } else if (stack.isEmpty()) {
                currentEntry.valueLength = 1 + ii - currentEntry.pageNumber;
                mIndex[currentKey] = currentEntry;
                keyNext = true;
            }
        } else if (type == QsonPage::META_HEADER_PAGE) {
            if (stack.isEmpty()) {
                if (ii == 0 && isDocument()) {
                    currentKey = QsonStrings::kMetaStr;
                    currentEntry.pageNumber = 0;
                    currentEntry.valueOffset = 0;
                } else {
                    qWarning() << "unexpected meta header" << ii << isDocument();
                    mIndex.clear();
                    return &mIndex;
                }
            } else if (stack.last() != QsonPage::DOCUMENT_HEADER_PAGE) {
                qWarning() << "unexpected meta header";
                mIndex.clear();
                return &mIndex;
            }
            stack.push_back(type);
        } else if (type == QsonPage::META_FOOTER_PAGE) {
            if (stack.isEmpty() || stack.takeLast() != QsonPage::META_HEADER_PAGE) {
                qWarning() << "unexpected meta footer";
                mIndex.clear();
                return &mIndex;
            } else if (stack.isEmpty()) {
                currentEntry.valueLength = 1 + ii - currentEntry.pageNumber;
                mIndex[QsonStrings::kMetaStr] = currentEntry;
                keyNext = true;
            }
        } else if (type == QsonPage::ARRAY_VALUE_PAGE) {
            if (stack.isEmpty() || stack.last() != QsonPage::LIST_HEADER_PAGE) {
                qWarning() << "unexpected array page in object";
                mIndex.clear();
                return &mIndex;
            }
        } else {
            qWarning() << "unexpected page";
            mIndex.clear();
            return &mIndex;
        }
    }

    if (isDocument()) {
        mIndex[QsonStrings::kUuidStr] = QsonEntry(-2);
        mIndex[QsonStrings::kVersionStr] = QsonEntry(-3);
        quint32 *lastCount = (quint32*) (mHeader->constData() + 6);
        if ((*lastCount > 0) && memcmp(mHeader->constData() + 10, mFooter->constData() + 10, 16))
            mIndex[QsonStrings::kLastVersionStr] = QsonEntry(-4);
    }

    return &mIndex;
}

} } // end namespace QtAddOn::JsonDb
