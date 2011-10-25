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

#include "qsonpage_p.h"

#include <string.h>

#include <QStringBuilder>
#include <QDebug>
#include <QUuid>
#include <qendian.h>

namespace QtAddOn { namespace JsonDb {

QsonPage::QsonPage()
 : mOwnsData(false)
 , mMaxSize(0)
 , mOffset(0)
 , mPage(0)
{}

QsonPage::QsonPage(PageType type)
    : mOwnsData(true)
{
    switch(type) {
    case EMPTY_PAGE:
        mMaxSize = 0;
        mOffset = 0;
        mPage = 0;
        return;
    case OBJECT_HEADER_PAGE:
    case OBJECT_FOOTER_PAGE:
    case LIST_HEADER_PAGE:
    case LIST_FOOTER_PAGE:
        mMaxSize = 4;
        mOffset = mMaxSize;
        break;
    case DOCUMENT_FOOTER_PAGE:
        mMaxSize = 26;
        mOffset = mMaxSize;
        break;
    case DOCUMENT_HEADER_PAGE:
        mMaxSize = 44;
        mOffset = mMaxSize;
        break;
    default:
        mMaxSize = 65535; // qson_size::max or something?
        mOffset = 4;
    }

    // reserve memory
    mPage = (char*) malloc(mMaxSize);

    // initialize the page
    switch (type) {
    case UNKNOWN_PAGE:
        mOffset = 0;
        break;
    case KEY_VALUE_PAGE:
    case ARRAY_VALUE_PAGE:
        mPage[0] = 'Q';
        mPage[1] = type;
        memcpy(mPage+2, &mOffset, 2);
        break;
    default:
        memcpy(mPage, "QSN", 3);
        mPage[3] = (char) type;
        memset(mPage + 4, 0, mMaxSize - 4);
    }
    switch (type) {
        case DOCUMENT_HEADER_PAGE:
            mPage[26] = UUID_TYPE;
            mPage[27] = 0;
            // no break
        case DOCUMENT_FOOTER_PAGE:
            mPage[4] = VERSION_TYPE;
            mPage[5] = 0;
        default:
            break;
    }
}

QsonPage::QsonPage(const char* data, qson_size size)
    : mOwnsData(true)
    , mMaxSize(size)
    , mPage((char*) malloc(size))
{
    memcpy(mPage, data, size);

    switch (type()) {
    case QsonPage::UNKNOWN_PAGE:
        break;
    case KEY_VALUE_PAGE:
    case ARRAY_VALUE_PAGE:
        memcpy(&mMaxSize, mPage + 2, 2);
        if (mMaxSize > size) {
            mMaxSize = size;
        }
        break;
    case DOCUMENT_HEADER_PAGE:
        mMaxSize = 44;
        break;
    case DOCUMENT_FOOTER_PAGE:
        mMaxSize = 26;
        break;
    default:
        mMaxSize = 4;
        break;
    }
    mOffset = mMaxSize;
}

QsonPage::QsonPage(const QsonPage &copy)
    : QSharedData(copy)
    , mOwnsData(true)
    , mMaxSize(copy.mMaxSize)
    , mOffset(copy.mOffset)
{
    if (mMaxSize > 0) {
        mPage = (char*) malloc(mMaxSize);
        memcpy(mPage, copy.mPage, mMaxSize);
    } else {
        mPage = 0;
    }
}

QsonPage::~QsonPage()
{
    if (mOwnsData) {
        free(mPage);
    }
}

QsonPage::PageType QsonPage::type() const
{
    if (mPage == 0) {
        return QsonPage::EMPTY_PAGE;
    }

    if (mMaxSize < 4) {
        return QsonPage::UNKNOWN_PAGE;
    }

    if (mPage[0] != 'Q') {
        return QsonPage::UNKNOWN_PAGE;
    }

    switch (mPage[1]) {
    case QsonPage::KEY_VALUE_PAGE:
        return QsonPage::KEY_VALUE_PAGE;
    case QsonPage::ARRAY_VALUE_PAGE:
        return QsonPage::ARRAY_VALUE_PAGE;
    case 'S':
        break;
    default:
        return QsonPage::UNKNOWN_PAGE;
    }

    if (mPage[2] != 'N') {
        return QsonPage::UNKNOWN_PAGE;
    }

    switch (mPage[3]) {
    case QsonPage::OBJECT_HEADER_PAGE:
        return QsonPage::OBJECT_HEADER_PAGE;
    case QsonPage::OBJECT_FOOTER_PAGE:
        return QsonPage::OBJECT_FOOTER_PAGE;
    case QsonPage::LIST_HEADER_PAGE:
        return QsonPage::LIST_HEADER_PAGE;
    case QsonPage::LIST_FOOTER_PAGE:
        return QsonPage::LIST_FOOTER_PAGE;
    case QsonPage::DOCUMENT_HEADER_PAGE:
        return QsonPage::DOCUMENT_HEADER_PAGE;
    case QsonPage::DOCUMENT_FOOTER_PAGE:
        return QsonPage::DOCUMENT_FOOTER_PAGE;
    case QsonPage::META_HEADER_PAGE:
        return QsonPage::META_HEADER_PAGE;
    case QsonPage::META_FOOTER_PAGE:
        return QsonPage::META_FOOTER_PAGE;
    default:
        return QsonPage::UNKNOWN_PAGE;
    }
}

int QsonPage::dataSize() const
{
    return mOffset;
}

const char *QsonPage::constData() const
{
    return mPage;
}

char *QsonPage::data()
{
    return mPage;
}

bool QsonPage::writeKey(const QString& key)
{
    switch (type()) {
    case KEY_VALUE_PAGE: {
        if (mMaxSize - mOffset - 2 < stringSize(key)) {
            return false;
        }
        mPage[mOffset] = QsonPage::KEY_TYPE;
        mOffset += 1;
        mPage[mOffset] = 0;
        mOffset += 1;

        bool ret = writeString(key);
        updateOffset();
        return ret;
    }
    default:
        return false;
    }
}

bool QsonPage::writeVersion(const QsonVersion &version)
{
    int size = version.content().size();
    if (version.isValid()) {
        if (mMaxSize - mOffset < size) {
            return false;
        }
        memcpy(mPage + mOffset, version.content().constData(), size);
        mOffset += size;
        updateOffset();
    }
    return true;
}

bool QsonPage::writeValue() // null
{
    if (mMaxSize - mOffset < 2) {
        return false;
    }
    mPage[mOffset] = QsonPage::NULL_TYPE;
    mOffset += 1;
    mPage[mOffset] = 0;
    mOffset += 1;
    updateOffset();
    return true;
}

bool QsonPage::writeValue(const bool value)
{
    if (mMaxSize - mOffset < 2) {
        return false;
    }
    mPage[mOffset] = value ? QsonPage::TRUE_TYPE : QsonPage::FALSE_TYPE;
    mOffset += 1;
    mPage[mOffset] = 0;
    mOffset += 1;
    updateOffset();
    return true;
}

bool QsonPage::writeValue(const qint64 value)
{
    if (mMaxSize - mOffset < 10) { // 10 bytes to store an int
        return false;
    }
    mPage[mOffset] = QsonPage::INT_TYPE;
    mOffset += 1;
    mPage[mOffset] = 0;
    mOffset += 1;
    uchar *target = (uchar *) (mPage + mOffset);
    qToLittleEndian(value, target);
    mOffset += sizeof(qint64);
    updateOffset();
    return true;
}

bool QsonPage::writeValue(const quint64 value)
{
    if (mMaxSize - mOffset < 10) { // 10 bytes to store an int
        return false;
    }
    mPage[mOffset] = QsonPage::UINT_TYPE;
    mOffset += 1;
    mPage[mOffset] = 0;
    mOffset += 1;
    uchar *target = (uchar *) (mPage + mOffset);
    qToLittleEndian(value, target);
    mOffset += sizeof(quint64);
    updateOffset();
    return true;
}

bool QsonPage::writeValue(const double value)
{
    if (mMaxSize - mOffset < 10) { // 10 bytes to store a double
        return false;
    }

    mPage[mOffset] = QsonPage::DOUBLE_TYPE;
    mOffset += 1;
    mPage[mOffset] = 0;
    mOffset += 1;
    uchar *target = (uchar *) (mPage + mOffset);
    memcpy(target, &value, sizeof(value));
    mOffset += sizeof(double);
    updateOffset();
    return true;
}

bool QsonPage::writeValue(const QString& value)
{
    qson_size size = stringSize(value);
    if (mMaxSize - mOffset - 2 < size) {
        return false;
    }
    mPage[mOffset] = QsonPage::STRING_TYPE;
    mOffset += 1;
    mPage[mOffset] = 0;
    mOffset += 1;
    bool ret = writeString(value);
    updateOffset();
    return ret;
}

int QsonPage::readSize(int offset, bool expectKey) const
{
    int sanityCheck = offset + 1;
    if (mOffset > sanityCheck) {
        if (mPage[sanityCheck] != 0) {
            qWarning() << "sanity check failed" << sanityCheck;
            return 0;
        }
        switch (mPage[offset]) {
        case QsonPage::KEY_TYPE:
            if (!expectKey) {
                return 0;
            }
        case QsonPage::STRING_TYPE:
            if (mOffset > (offset + 3)) {
                qson_size *stringSize = (qson_size*) (mPage + offset + 2);
                if (mOffset > (offset + 3 + *stringSize)) {
                    return 4 + *stringSize;
                }
            }
            return 0;
        case QsonPage::INT_TYPE:
        case QsonPage::UINT_TYPE:
            if (!expectKey && mOffset > (offset + 9)) {
                return sizeof(quint64) + 2;
            }
            return 0;
        case QsonPage::DOUBLE_TYPE:
            if (!expectKey && mOffset > (offset + 9)) {
                return 10;
            }
            return 0;
        case QsonPage::NULL_TYPE:
        case QsonPage::TRUE_TYPE:
        case QsonPage::FALSE_TYPE:
            return expectKey ? 0 : 2;
        case QsonPage::UUID_TYPE:
            if (!expectKey && mOffset > (offset + 17)) {
                return 18;
            }
            return 0;
        case QsonPage::VERSION_TYPE:
            if (!expectKey && mOffset > (offset + 21)) {
                return 22;
            }
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}

bool QsonPage::readNull(int offset) const
{
    if (mOffset > offset) {
        char type = mPage[offset];
        if (type == NULL_TYPE) {
            return true;
        } else if (type == NULL_TYPE || type == STRING_TYPE) {
            return readString(offset).isEmpty();
        }
    }
    return false;
}

bool QsonPage::readBool(int offset) const
{
    int sanityCheck = offset + 1;
    if (mOffset > sanityCheck && mPage[sanityCheck] == 0) {
        char type = mPage[offset];
        switch (type) {
        case FALSE_TYPE:
        case NULL_TYPE:
            return false;
        case TRUE_TYPE:
            return true;
        case INT_TYPE:
        case UINT_TYPE:
        case DOUBLE_TYPE:
            return readInt(offset, 0) != 0;
        case STRING_TYPE:
        case KEY_TYPE:
            return !readString(offset).isEmpty();
        default:
            return true;
        }
    }
    return false;
}

quint64 QsonPage::readUInt(int offset, quint64 fallback) const
{
    int sanityCheck = offset + 1;
    if (mOffset > sanityCheck && mPage[sanityCheck] == 0) {
        char type = mPage[offset];
        if (type == QsonPage::UINT_TYPE && mOffset > (sanityCheck + sizeof(quint64))) {
            const uchar *valuep = (const uchar *) (mPage + sanityCheck + 1);
            return qFromLittleEndian<quint64>(valuep);
        } else if (type == QsonPage::VERSION_TYPE && mOffset > (sanityCheck + sizeof(quint32))) {
            const uchar *valuep = (const uchar *)(mPage + sanityCheck + 1);
            quint32 value = qFromLittleEndian<quint32>(valuep);
            return value;
        } else if (type == QsonPage::INT_TYPE) {
            return (quint64) readInt(offset, fallback);
        } else if (type == QsonPage::DOUBLE_TYPE) {
            return (quint64) readDouble(offset, fallback);
        } else if (type == QsonPage::STRING_TYPE) {
            bool ok;
            quint64 result = readString(offset).toULongLong(&ok);
            return ok ? result : fallback;
        }
    }
    return fallback;
}

qint64 QsonPage::readInt(int offset, qint64 fallback) const
{
    int sanityCheck = offset + 1;
    if (mOffset > sanityCheck && mPage[sanityCheck] == 0) {
        char type = mPage[offset];
        if (type == QsonPage::INT_TYPE && mOffset > (sanityCheck + sizeof(qint64))) {
            const uchar *valuep =  (const uchar *)(mPage + sanityCheck + 1);
            return qFromLittleEndian<qint64>((const uchar *)valuep);
        } else if (type == QsonPage::UINT_TYPE  || type == QsonPage::VERSION_TYPE) {
            return (qint64) readUInt(offset, fallback);
        } else if (type == QsonPage::DOUBLE_TYPE) {
            return (qint64) readDouble(offset, fallback);
        } else if (type == QsonPage::STRING_TYPE) {
            bool ok;
            qint64 result = readString(offset).toLongLong(&ok);
            return ok ? result : fallback;
        }
    }
    return fallback;
}

double QsonPage::readDouble(int offset, double fallback) const
{
    int sanityCheck = offset + 1;
    if (mOffset > sanityCheck && mPage[sanityCheck] == 0) {
        char type = mPage[offset];
        if (type == QsonPage::DOUBLE_TYPE && mOffset > (sanityCheck + 8)) {
            const uchar *valuep = (const uchar *) (mPage + sanityCheck + 1);
            double value;
            // TODO: endian safe
            memcpy((char *)&value, valuep, sizeof(value));
            return value;
        } else if (type == QsonPage::INT_TYPE) {
            return (double) readInt(offset, fallback);
        } else if (type == QsonPage::UINT_TYPE || type == QsonPage::VERSION_TYPE) {
            return (double) readDouble(offset, fallback);
        } else if (type == QsonPage::STRING_TYPE) {
            bool ok;
            double result = readString(offset).toDouble(&ok);
            return ok ? result : fallback;
        }
    }
    return fallback;
}

QString QsonPage::readString(int offset) const
{
    int sanityCheck = offset + 1;
    if (mOffset < (offset + 4) || mPage[sanityCheck] != 0) {
        return QString();
    }

    switch (mPage[offset]) {
    case QsonPage::KEY_TYPE:
    case QsonPage::STRING_TYPE: {
        qson_size* stringSize = (qson_size*) (mPage + offset + 2);
        if ((sanityCheck + 2 + *stringSize) < mOffset) {
            QChar *value = (QChar*) (mPage + offset + 4);
            return QString(value, *stringSize / 2);
        }
        break;
    }
    case QsonPage::VERSION_TYPE:
        if (mOffset >= (offset + 22)) {
            QsonVersion version(mPage + offset);
            return version.toString();
        }
        break;
    case QsonPage::UUID_TYPE:
        if (mOffset >= (offset + 18)) {
            QByteArray uuid(mPage + offset + 2, 16);
            QByteArray hex = uuid.toHex();
            return QLatin1Char('{')
                    % QString::fromLatin1(hex.left(8))
                    % QLatin1Char('-')
                    % QString::fromLatin1(hex.mid(8, 4))
                    % QLatin1Char('-')
                    % QString::fromLatin1(hex.mid(12, 4))
                    % QLatin1Char('-')
                    % QString::fromLatin1(hex.mid(16, 4))
                    % QLatin1Char('-')
                    % QString::fromLatin1(hex.mid(20, 12))
                    % QLatin1Char('}');
        }
        break;
    default:
        break;
    }
    return QString();
}

QUuid QsonPage::readUuid(int offset) const
{
    int sanityCheck = offset + 1;
    if (mOffset < (offset + 4) || mPage[sanityCheck] != 0)
        return QUuid();

    switch (mPage[offset]) {
    case QsonPage::UUID_TYPE:
        if (mOffset >= (offset + 18))
            return QUuid::fromRfc4122(QByteArray::fromRawData(mPage + offset + 2, 16));
        break;
    case QsonPage::STRING_TYPE: {
        qson_size* stringSize = (qson_size*) (mPage + offset + 2);
        if ((sanityCheck + 2 + *stringSize) < mOffset) {
            QChar *value = (QChar*) (mPage + offset + 4);
            return QUuid(QString::fromRawData(value, *stringSize / 2));
        }
    }
    default:
        break;
    }
    return QUuid();
}

inline qson_size QsonPage::stringSize(const QString &string) const
{
    return (2 * qMin(30000, string.size()));
}

bool QsonPage::writeString(const QString& string)
{
    qson_size size = stringSize(string);
    if (mMaxSize - mOffset - 2 < size) {
        return false;
    }

    memcpy(mPage + mOffset, &size, 2);
    mOffset += 2;

    memcpy(mPage + mOffset, string.constData(), size);
    mOffset += size;

    return true;
}

} } // end namespace QtAddOn::JsonDb
