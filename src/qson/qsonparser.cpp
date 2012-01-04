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

#include "qsonparser_p.h"

#include <QDebug>

QT_BEGIN_NAMESPACE_JSONDB

QsonParser::QsonParser(bool streamMode)
    : mObjectReady(false)
    , mHasError(false)
    , mStreamMode(streamMode)
    , mStreamReady(false)
    , mStreamDone(false)
    , mParseOffset(0)
{
}

void QsonParser::append(const QByteArray &buffer)
{
    mBuffer.append(buffer);
    if (!mHasError && !mObjectReady && !mStreamDone)
        scanBuffer();
}

void QsonParser::append(const char *data, int size)
{
    mBuffer.append(data, size);
    if (!mHasError && !mObjectReady && !mStreamDone)
        scanBuffer();
}

void QsonParser::append(QIODevice& device)
{
    if (mHasError || mObjectReady || mStreamDone)
        return;

    int available = device.bytesAvailable();
    if (available < 4)
        return;

    char peek[4];
    if (device.peek(peek, 4) != 4) {
        qWarning() << __FUNCTION__ << "peek() failed";
        return;
    }

    int shouldRead = pageSize(peek, available);
    if (shouldRead > 0) {
        int currentSize = mBuffer.size();
        mBuffer.resize(currentSize + shouldRead);
        int didRead = device.read(mBuffer.data() + currentSize, shouldRead);
        if (didRead != shouldRead) {
            mBuffer.resize(currentSize + didRead);
        }
        if (!mHasError && !mObjectReady && !mStreamDone)
            scanBuffer();

        append(device);
    } else if (shouldRead < 0) {
        qWarning() << __FUNCTION__ << "was unable to discover a page";
    }
}

bool QsonParser::hasError() const
{
    return mHasError;
}

bool QsonParser::isObjectReady() const
{
    return mObjectReady && !mHasError;
}

QsonObject QsonParser::getObject()
{
    if (mObjectReady) {
        QsonObject result(mContent);
        mContent.clear();
        mObjectReady = false;
        if (!mBuffer.isEmpty())
            scanBuffer();
        return result;
    } else {
        return QsonObject();
    }
}

bool QsonParser::isStream() const
{
    return mStreamReady;
}

bool QsonParser::streamDone()
{
    bool result = mStreamDone;
    mStreamDone = false;
    if (result && !mBuffer.isEmpty())
        scanBuffer();
    return result;
}

int QsonParser::pageSize(const char *data, int maxSize)
{
    if (maxSize < 4)
        return 0;
    if (data[0] != 'Q') {
        qWarning() << "magic fail at 0" << (quint8) data[0];
        return -1;
    }

    switch (data[1]) {
    case 'S':
        break;
    case QsonPage::KEY_VALUE_PAGE:
    case QsonPage::ARRAY_VALUE_PAGE: {
        qson_size *result = (qson_size*) (data + 2);
        if (*result <= maxSize)
            return *result;
        else
            return 0;
    }
    default:
        qWarning() << "magic fail at 1" << (quint8) data[1];
        return -1;
    }

    if (data[2] != 'N') {
        qWarning() << "magic fail at 2" << (quint8) data[2];
        return -1;
    }

    switch (data[3]) {
    case QsonPage::OBJECT_HEADER_PAGE:
    case QsonPage::OBJECT_FOOTER_PAGE:
    case QsonPage::LIST_HEADER_PAGE:
    case QsonPage::LIST_FOOTER_PAGE:
    case QsonPage::META_HEADER_PAGE:
    case QsonPage::META_FOOTER_PAGE:
        return 4;
    case QsonPage::DOCUMENT_HEADER_PAGE:
        return (maxSize >= 44) ? 44 : 0;
    case QsonPage::DOCUMENT_FOOTER_PAGE:
        return (maxSize >= 26) ? 26 : 0;
    default:
        qWarning() << "magic fail at 3" << (quint8) data[3];
        return -1;
    }
}

void QsonParser::scanBuffer()
{
    int nextPageSize = pageSize(mBuffer.constData() + mParseOffset, mBuffer.size() - mParseOffset);

    if (nextPageSize == -1) {
        qWarning() << __FUNCTION__ << "unknown qsonpage";
        mHasError = true;
    } else if (!mHasError && nextPageSize > 0) {
        QsonPagePtr page = QsonPagePtr(new QsonPage(mBuffer, mParseOffset, nextPageSize));

        QsonPage::PageType type = page->type();
        mParseOffset += nextPageSize;

        switch (type) {
        case QsonPage::KEY_VALUE_PAGE:
            if (mStack.isEmpty() || mStack.last() == QsonPage::LIST_HEADER_PAGE) {
                qWarning() << __FUNCTION__ << "key/value page in list";
                mHasError = true;
            } else {
                mContent.append(page);
            }
            break;
        case QsonPage::ARRAY_VALUE_PAGE:
            if (mStack.isEmpty() || mStack.last() != QsonPage::LIST_HEADER_PAGE) {
                qWarning() << __FUNCTION__ << "array page in object";
                mHasError = true;
            } else
                mContent.append(page);
            break;
        case QsonPage::LIST_HEADER_PAGE:
            if (mStreamMode && !mStreamReady && mStack.isEmpty()) {
                mStreamReady = true;
                break;
            }
        case QsonPage::OBJECT_HEADER_PAGE:
        case QsonPage::DOCUMENT_HEADER_PAGE:
            mContent.append(page);
            mStack.append(type);
            break;
        case QsonPage::META_HEADER_PAGE:
            if (mStack.isEmpty() || mContent.last()->type() != QsonPage::DOCUMENT_HEADER_PAGE) {
                qWarning() << __FUNCTION__ << "unexpected meta header" << mStack.isEmpty() << "hello" << (char) mContent.last()->type();
                mHasError = true;
            } else {
                mContent.append(page);
                mStack.append(type);
            }
            break;
        case QsonPage::LIST_FOOTER_PAGE:
            if (mStreamMode && mStreamReady && mStack.isEmpty()) {
                mStreamReady = false;
                mStreamDone = true;
                break;
            }
        case QsonPage::OBJECT_FOOTER_PAGE:
        case QsonPage::DOCUMENT_FOOTER_PAGE:
        case QsonPage::META_FOOTER_PAGE:
            if (mStack.isEmpty() || mStack.last() != headerFor(type)) {
                qWarning() << __FUNCTION__ << "unexpected footer page";
                mHasError = true;
            } else {
                mContent.append(page);
                mStack.removeLast();
            }
            if (mHasError == false && mStack.isEmpty())
                mObjectReady = true;
            break;
        default:
            qWarning() << __FUNCTION__ << "unknown page type";
            mHasError = true;
        }

        if (mParseOffset == mBuffer.size()) {
            mBuffer.clear();
            mParseOffset = 0;
        } else if (mStack.isEmpty()) {
            mBuffer = mBuffer.mid(mParseOffset);
            mParseOffset = 0;
        }

        if (!mHasError && !mObjectReady && !mStreamDone)
            return scanBuffer();
    }

    if (mHasError) {
        mStack.clear();
        mContent.clear();
    }
}

QsonObject QsonParser::fromRawData(const QByteArray &buffer)
{
    // ### TODO: improve me
    QsonParser parser;
    parser.append(buffer);
    return parser.isObjectReady() ? parser.getObject() : QsonObject();
}

QT_END_NAMESPACE_JSONDB
