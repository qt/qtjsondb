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

#ifndef QKEYVALUESTORETXN_P_H
#define QKEYVALUESTORETXN_P_H

#include "qkeyvaluestore.h"
#include "qkeyvaluestore_p.h"

#include <QMap>
#include <QHash>
#include <QByteArray>
#include <QFile>

class QKeyValueStore;
class QKeyValueStoreTxnPrivate {
public:
    QKeyValueStoreTxnPrivate(bool readonly = true);
    ~QKeyValueStoreTxnPrivate();
    bool put(const QByteArray &key, const QByteArray &value);
    bool remove(const QByteArray &key);
    bool get(const QByteArray &key, QByteArray *value);
    bool commit(quint32 tag);
    bool abort();

    bool m_readonly;
    quint32 m_incommingBytes;
    quint32 m_tag;
    QKeyValueStore *m_btree;
    // This is all the data we need, these are all the entries we care about when writing.
    QMap<QByteArray, QKeyValueStoreEntry *> m_incomming;
#if defined(JSONDB_USE_KVS_MULTIPLE_TRANSACTIONS)
    /*
     * The reason this is protected is because for this version we assume
     * only one transaction at all times, therefore there is no need to
     * keep track of the changes.
     */
    // We need this in order to preserve the state.
    QMap<QByteArray, QKeyValueStorePrivate::fileOffsets> m_offsets;
#else
    // Since we modified the QMap directly we need to mark it.
    bool m_localModifications;
#endif
    QKeyValueStorePrivate *m_store;
};

#endif // QKEYVALUESTORETXN_P_H
