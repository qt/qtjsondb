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

#include <QByteArray>
#include <QMap>
#include <QDebug>

#include "qkeyvaluestorecursor.h"

QKeyValueStoreCursor::QKeyValueStoreCursor(QKeyValueStoreTxn *txn)
{
    p = new QKeyValueStoreCursorPrivate(txn);
}

QKeyValueStoreCursor::~QKeyValueStoreCursor()
{
    delete p;
}

bool QKeyValueStoreCursor::first()
{
    return p->first();
}

bool QKeyValueStoreCursor::last()
{
    return p->last();
}

bool QKeyValueStoreCursor::next()
{
    return p->next();
}

bool QKeyValueStoreCursor::previous()
{
    return p->prev();
}

bool QKeyValueStoreCursor::current(QByteArray *baKey, QByteArray *baValue) const
{
    return p->current(baKey, baValue);
}

bool QKeyValueStoreCursor::seek(const QByteArray &baKey)
{
    return p->seek(baKey);
}

bool QKeyValueStoreCursor::seekRange(const QByteArray &baKey)
{
    return p->seekRange(baKey);
}

/*
 * PRIVATE API, DO NOT TOUCH THIS UNLESS YOU KNOW WHAT YOU ARE DOING!
 */

/*
 * The cursor uses lazy fetching of values, that means that the cursor only
 * gets the key when moving, and only if there is a call to current it
 * retrieves the value.
 */
#if defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
#define TXN_OFFSETS m_txn->p->m_offsets
#else
#define TXN_OFFSETS m_txn->p->m_store->m_offsets
#endif

// Joao's hack to get a const element
template <class T> const T &const_(const T &t) { return t; }
QKeyValueStoreCursorPrivate::QKeyValueStoreCursorPrivate(QKeyValueStoreTxn *txn) :
    m_txn(txn)
{
    if (TXN_OFFSETS.isEmpty())
        return;
    m_cursor = TXN_OFFSETS.constBegin();
    // Load the corresponding key
    m_key = m_cursor.key();
}

QKeyValueStoreCursorPrivate::~QKeyValueStoreCursorPrivate()
{
}

bool QKeyValueStoreCursorPrivate::first()
{
    if (TXN_OFFSETS.isEmpty())
        return false;
    // Move to the right position
    m_cursor = TXN_OFFSETS.constBegin();
    // Load the corresponding key
    m_key = m_cursor.key();
    return true;
}

bool QKeyValueStoreCursorPrivate::last()
{
    if (TXN_OFFSETS.isEmpty())
        return false;
    // Move to the right position
    m_cursor = TXN_OFFSETS.constEnd();
    m_cursor--;
    // Load the corresponding key
    m_key = m_cursor.key();
    return true;
}

bool QKeyValueStoreCursorPrivate::next()
{
    if (TXN_OFFSETS.isEmpty())
        return false;
    m_cursor++;
    if (m_cursor == TXN_OFFSETS.constEnd()) {
        m_cursor--;
        return false;
    }
    // Load the corresponding key
    m_key = m_cursor.key();
    return true;
}

bool QKeyValueStoreCursorPrivate::prev()
{
    if (TXN_OFFSETS.isEmpty())
        return false;
    if (m_cursor == TXN_OFFSETS.constBegin())
        return false;
    // Move to the right position
    m_cursor--;
    // Load the corresponding key
    m_key = m_cursor.key();
    return true;
}

/*
 * Unless the transaction and the db are empty, current cannot return false.
 */
bool QKeyValueStoreCursorPrivate::current(QByteArray *baKey, QByteArray *baValue) const
{
    if (TXN_OFFSETS.isEmpty())
        return false;
    if (baKey)
        *baKey = m_key;
    // We use the transaction to find the item
    if (baValue) {
        if (!m_txn->get(m_key, baValue))
            return false;
        m_value = *baValue;
    }
    return true;
}

bool QKeyValueStoreCursorPrivate::seek(const QByteArray &baKey)
{
    if (TXN_OFFSETS.contains(baKey)) {
        m_cursor = const_(TXN_OFFSETS).find(baKey);
    } else {
        return false;
    }
    m_key = m_cursor.key();
    return true;
}

bool QKeyValueStoreCursorPrivate::seekRange(const QByteArray &baKey)
{
    m_cursor = const_(TXN_OFFSETS).lowerBound(baKey);
    if (m_cursor == TXN_OFFSETS.constEnd())
        return false;
    m_key = m_cursor.key();
    return true;
}
