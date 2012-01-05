/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "qsonlist_p.h"
#include "qsonmap_p.h"
#include "qsonelement_p.h"
#include "qsonstrings_p.h"

#include <QDebug>

QT_BEGIN_NAMESPACE_JSONDB

/*!
  \class QtAddOn::JsonDb::QsonList
  \brief The QsonList class provides methods for accessing the elements of a QsonObject array.
  \sa QsonMap, QsonList
*/

QsonList::QsonList(const QsonObject &object)
{
    switch (object.type()) {
    case ListType:
        mHeader = object.mHeader;
        mBody = object.mBody;
        mFooter = object.mFooter;
        break;
    default:
        mHeader = new QsonPage(QsonPage::LIST_HEADER_PAGE);
        mFooter = new QsonPage(QsonPage::LIST_FOOTER_PAGE);
        break;
    }
}

int QsonList::size() const
{
    return index()->size();
}

int QsonList::count() const
{
    return index()->size();
}

QsonObject::Type QsonList::typeAt(int pos) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1) {
        return QsonObject::UnknownType; // should never happen (TM)
    } else if (entry.valueOffset == 0) {
        switch (mBody.at(entry.pageNumber)->type()) {
        case QsonPage::OBJECT_HEADER_PAGE:
        case QsonPage::DOCUMENT_HEADER_PAGE:
        case QsonPage::META_HEADER_PAGE:
            return QsonObject::MapType;
        case QsonPage::LIST_HEADER_PAGE:
            return QsonObject::ListType;
        default:
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
            return QsonObject::UnknownType; // should never happen (TM)
        }
    }
}

bool QsonList::isNull(int pos) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1) {
        return true;
    } else if (entry.valueOffset == 0) {
        return false;
    } else {
        return mBody.at(entry.pageNumber)->readNull(entry.valueOffset);
    }
}

bool QsonList::boolAt(int pos) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1) {
        return false;
    } else if (entry.valueOffset == 0) {
        return true;
    } else {
        return mBody.at(entry.pageNumber)->readBool(entry.valueOffset);
    }
}

quint64 QsonList::uintAt(int pos, quint64 fallback) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1 || entry.valueOffset == 0) {
        return fallback;
    } else {
        return mBody.at(entry.pageNumber)->readUInt(entry.valueOffset, fallback);
    }
}

qint64 QsonList::intAt(int pos, qint64 fallback) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1 || entry.valueOffset == 0) {
        return fallback;
    } else {
        return mBody.at(entry.pageNumber)->readInt(entry.valueOffset, fallback);
    }
}

double QsonList::doubleAt(int pos, double fallback) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1 || entry.valueOffset == 0) {
        return fallback;
    } else {
        return mBody.at(entry.pageNumber)->readDouble(entry.valueOffset, fallback);
    }
}

QString QsonList::stringAt(int pos) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1 || entry.valueOffset == 0) {
        return QString();
    } else {
        return mBody.at(entry.pageNumber)->readString(entry.valueOffset);
    }
}

QStringList QsonList::toStringList() const
{
    QStringList result;
    QsonObject::CachedIndex *idx = index();
    for (CachedIndex::const_iterator i = idx->constBegin(); i != idx->constEnd(); ++i) {
        const QsonEntry &entry = *i;
        if (entry.pageNumber == -1 || entry.valueOffset == 0)
            continue;

        switch (mBody.at(entry.pageNumber)->constData()[entry.valueOffset]) {
        case QsonPage::STRING_TYPE:
        case QsonPage::KEY_TYPE:
        case QsonPage::UUID_TYPE:
        case QsonPage::VERSION_TYPE:
            result.append(mBody.at(entry.pageNumber)->readString(entry.valueOffset));
            break;
        default:
            break;
        }
    }
    return result;
}

QsonMap QsonList::objectAt(int pos) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1 || entry.valueOffset > 0) {
        return QsonMap();
    }

    QsonPagePtr header = mBody.at(entry.pageNumber);
    switch (header->type()) {
    case QsonPage::OBJECT_HEADER_PAGE:
    case QsonPage::DOCUMENT_HEADER_PAGE:
    case QsonPage::META_HEADER_PAGE:
        return QsonMap(mBody.mid(entry.pageNumber, entry.valueLength));
    default:
        return QsonMap();
    }
}

QsonList QsonList::listAt(int pos) const
{
    QsonEntry entry = index()->at(pos);
    if (entry.pageNumber == -1 || entry.valueOffset > 0) {
        return QsonMap();
    }

    QsonPagePtr header = mBody.at(entry.pageNumber);
    switch (header->type()) {
    case QsonPage::LIST_HEADER_PAGE:
        return QsonList(mBody.mid(entry.pageNumber, entry.valueLength));
    default:
        return QsonMap();
    }
}

QsonList& QsonList::append(QsonObject::Special value)
{
    if (value != QsonObject::NullValue)
        return *this;

    CachedIndex::Cleaner cleaner(mIndex);

    ensurePage(QsonPage::ARRAY_VALUE_PAGE);
    if (!mBody.last()->writeValue()) {
        ensurePage(QsonPage::ARRAY_VALUE_PAGE, true);
        mBody.last()->writeValue();
    }
    return *this;
}

QsonList& QsonList::append(bool value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    ensurePage(QsonPage::ARRAY_VALUE_PAGE);
    if (!mBody.last()->writeValue(value)) {
        ensurePage(QsonPage::ARRAY_VALUE_PAGE, true);
        mBody.last()->writeValue(value);
    }
    return *this;
}

QsonList& QsonList::append(quint64 value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    ensurePage(QsonPage::ARRAY_VALUE_PAGE);
    if (!mBody.last()->writeValue(value)) {
        ensurePage(QsonPage::ARRAY_VALUE_PAGE, true);
        mBody.last()->writeValue(value);
    }
    return *this;
}

QsonList& QsonList::append(qint64 value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    ensurePage(QsonPage::ARRAY_VALUE_PAGE);
    if (!mBody.last()->writeValue(value)) {
        ensurePage(QsonPage::ARRAY_VALUE_PAGE, true);
        mBody.last()->writeValue(value);
    }
    return *this;
}

QsonList& QsonList::append(double value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    ensurePage(QsonPage::ARRAY_VALUE_PAGE);
    if (!mBody.last()->writeValue(value)) {
        ensurePage(QsonPage::ARRAY_VALUE_PAGE, true);
        mBody.last()->writeValue(value);
    }
    return *this;
}

QsonList& QsonList::append(const QString &value)
{
    CachedIndex::Cleaner cleaner(mIndex);

    ensurePage(QsonPage::ARRAY_VALUE_PAGE);
    if (!mBody.last()->writeValue(value)) {
        ensurePage(QsonPage::ARRAY_VALUE_PAGE, true);
        mBody.last()->writeValue(value);
    }
    return *this;
}

QsonList& QsonList::append(const QsonObject &value)
{
    const QsonPage::PageType headerType = value.mHeader->type();

    bool isMeta = (headerType == QsonPage::META_HEADER_PAGE);
    bool isFullObject = (headerType != QsonPage::EMPTY_PAGE && headerType != QsonPage::UNKNOWN_PAGE);

    CachedIndex::Cleaner cleaner(mIndex);
    QsonObject::CachedIndex::Cleaner(value.mIndex);

    if (isMeta)
        mBody.append(QsonPagePtr(new QsonPage(QsonPage::OBJECT_HEADER_PAGE)));
    else if (isFullObject)
        mBody.append(value.mHeader);

    switch (value.type()) {
    case QsonObject::ListType:
    case QsonObject::MapType:
        mBody.append(value.mBody);
        break;
    case QsonObject::NullType:
        append(QsonObject::NullValue);
        break;
    case QsonObject::BoolType:
        append(QsonElement(value).value<bool>());
        break;
    case QsonObject::IntType:
        append(QsonElement(value).value<qint64>());
        break;
    case QsonObject::UIntType:
        append(QsonElement(value).value<quint64>());
        break;
    case QsonObject::DoubleType:
        append(QsonElement(value).value<double>());
        break;
    case QsonObject::StringType:
        append(QsonElement(value).value<QString>());
        break;
    default:
        if (value.isNull()) {
            append(QsonObject::NullValue);
        }
        break;
    }

    if (isMeta)
        mBody.append(QsonPagePtr(new QsonPage(QsonPage::OBJECT_FOOTER_PAGE)));
    else if (isFullObject)
        mBody.append(value.mFooter);

    return *this;
}

QsonObject::CachedIndex *QsonList::index() const
{
    if (!mIndex.isEmpty())
        return const_cast<QsonObject::CachedIndex*>(&mIndex);

    // FIXME probably we do not need to generate full index, in most cases we need only one QsonEntry
    QList<QsonPage::PageType> stack;
    QsonEntry currentEntry;

    for (int ii = 0; ii < mBody.length(); ii++) {
        const QsonPage &currentPage = *mBody.at(ii);
        const QsonPage::PageType type = currentPage.type();
        if (type == QsonPage::ARRAY_VALUE_PAGE) {
            if (stack.isEmpty()) {
                qson_size pageSize = currentPage.dataSize();
                qson_size pageOffset = 4;
                while (pageOffset < pageSize) {
                    int valueSize = currentPage.readSize(pageOffset, false);
                    if (valueSize == 0) {
                        qWarning() << "expected value not found in list" << ii << pageOffset << pageSize;
                        mIndex.clear();
                        return &mIndex;
                    } else {
                        currentEntry.pageNumber = ii;
                        currentEntry.valueOffset = pageOffset;
                        currentEntry.valueLength = valueSize;
                        mIndex.append(currentEntry);
                        pageOffset += valueSize;
                    }
                }
            } else if (stack.last() != QsonPage::LIST_HEADER_PAGE) {
                qWarning() << "unexpected array page in object";
                mIndex.clear();
                return &mIndex;
            }
        } else if (type == QsonPage::OBJECT_HEADER_PAGE) {
            if (stack.isEmpty()) {
                currentEntry.pageNumber = ii;
                currentEntry.valueOffset = 0;
            }
            stack.push_back(type);
        } else if (type == QsonPage::OBJECT_FOOTER_PAGE) {
            if (stack.isEmpty() || stack.takeLast() != QsonPage::OBJECT_HEADER_PAGE) {
                qWarning() << "unexpected object footer";
                mIndex.clear();
                return &mIndex;
            } else if (stack.isEmpty()) {
                currentEntry.valueLength = 1 + ii - currentEntry.pageNumber;
                mIndex.append(currentEntry);
            }
        } else if (type == QsonPage::LIST_HEADER_PAGE || type == QsonPage::DOCUMENT_HEADER_PAGE) {
            if (stack.isEmpty()) {
                currentEntry.pageNumber = ii;
                currentEntry.valueOffset = 0;
            }
            stack.push_back(type);
        } else if (type == QsonPage::LIST_FOOTER_PAGE) {
            if (stack.isEmpty() || stack.takeLast() != QsonPage::LIST_HEADER_PAGE) {
                qWarning() << "unexpected list footer";
                mIndex.clear();
                return &mIndex;
            } else if (stack.isEmpty()) {
                currentEntry.valueLength = 1 + ii - currentEntry.pageNumber;
                mIndex.append(currentEntry);
            }
        } else if (type == QsonPage::DOCUMENT_FOOTER_PAGE) {
            if (stack.isEmpty() || stack.takeLast() != QsonPage::DOCUMENT_HEADER_PAGE) {
                qWarning() << "unexpected document footer";
                mIndex.clear();
                return &mIndex;
            } else if (stack.isEmpty()) {
                currentEntry.valueLength = 1 + ii - currentEntry.pageNumber;
                mIndex.append(currentEntry);
            }
        } else if (type == QsonPage::META_HEADER_PAGE) {
            if (stack.isEmpty()) {
                qWarning() << "unexpected meta header in list";
                mIndex.clear();
                return &mIndex;
            }
            stack.push_back(type);
        } else if (type == QsonPage::META_FOOTER_PAGE) {
            if (stack.isEmpty() || stack.takeLast() != QsonPage::META_HEADER_PAGE) {
                qWarning() << "unexpected meta footer in list";
            }
        } else if (type == QsonPage::KEY_VALUE_PAGE) {
            if (stack.isEmpty() || stack.last() == QsonPage::LIST_HEADER_PAGE) {
                qWarning() << "unexpected key value page in list";
                mIndex.clear();
                return &mIndex;
            }
        } else {
            qWarning() << "unexpected page";
            mIndex.clear();
            return &mIndex;
        }
    }

    return &mIndex;
}

QT_END_NAMESPACE_JSONDB
