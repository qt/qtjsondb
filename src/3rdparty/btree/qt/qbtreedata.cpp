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

#include <QDebug>

#include "qbtreedata.h"
#include "btree.h"

#include <errno.h>
#include <string.h>

QBtreeData::QBtreeData(const QBtreeData &other)
    : mByteArray(other.mByteArray), mData(other.mData), mPage(other.mPage), mSize(other.mSize)
{
    if (mPage)
        ref();
}

QBtreeData::QBtreeData(const QByteArray &array)
    : mByteArray(array), mData(array.constData()), mPage(0), mSize(array.size())
{
}

QBtreeData::QBtreeData(struct btval *value)
    : mData(0)
{
    Q_ASSERT(value);
    mSize = value->size;
    mPage = (void *)value->mp;
    if (!mSize)
        return;
    if (mPage) {
        Q_ASSERT(!value->free_data);
        // copy the mpage. Do not increase the ref count of the mpage.
        mData = (const char *)value->data;
    } else {
        Q_ASSERT(value->free_data);
        mByteArray = QByteArray((const char *)value->data, value->size);
        mData = mByteArray.constData();
        mSize = mByteArray.size();
        btval_reset(value);
    }
}

QBtreeData::~QBtreeData()
{
    if (mPage)
        deref();
}

int QBtreeData::ref()
{
    Q_ASSERT(mPage);
    struct btval value;
    value.data = 0;
    value.size = 0;
    value.free_data = 0;
    value.mp = (struct mpage *)mPage;
    return btval_ref(&value);
}

int QBtreeData::deref()
{
    Q_ASSERT(mPage);
    struct btval value;
    value.data = 0;
    value.size = 0;
    value.free_data = 0;
    value.mp = (struct mpage *)mPage;
    int r = btval_deref(&value);
    if (r <= 0) {
        mPage = 0;
        mData = 0;
        mSize = 0;
    }
    return r;
}

QBtreeData &QBtreeData::operator=(const QBtreeData &other)
{
    if (this != &other) {
        if (mPage)
            deref();
        mByteArray = other.mByteArray;
        mData = other.mData;
        mSize = other.mSize;
        mPage = other.mPage;
        if (mPage)
            ref();
    }
    return *this;
}

QByteArray QBtreeData::toByteArray() const
{
    if (mPage) {
        Q_ASSERT(mByteArray.isNull());
        // deref the mpage, if we are asked for the qbytearray, no reason to keep that around
        QBtreeData *that = const_cast<QBtreeData *>(this);
        that->mByteArray = QByteArray(mData, mSize);
        that->mData = that->mByteArray.constData();
        that->mSize = that->mByteArray.size();
        that->deref();
        that->mPage = 0;
    } else if (mData && mByteArray.isNull()) { // created from raw data
        QBtreeData *that = const_cast<QBtreeData *>(this);
        that->mByteArray = QByteArray::fromRawData(mData, mSize);
        that->mData = that->mByteArray.constData();
        that->mSize = that->mByteArray.size();
    }
    return mByteArray;
}
