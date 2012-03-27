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

#ifndef QKEYVALUESTORE_P_H
#define QKEYVALUESTORE_P_H

#include <QString>
#include <QByteArray>
#include <QHash>
#include <QFile>
#include <QMap>
#include <QQueue>
#include <QDebug>

#include "qkeyvaluestoreentry.h"
#include "qkeyvaluestoretxn.h"
#include "qkeyvaluestorefile.h"

class QKeyValueStore;
class QKeyValueStoreTxn;
class QKeyValueStorePrivate {
public:
    QKeyValueStorePrivate(QKeyValueStore *parent, const QString &name);
    ~QKeyValueStorePrivate();

    // Public API Mirror
    int open();
    int close();
    bool sync();
    bool compact();
    QKeyValueStoreTxn *beginRead();
    QKeyValueStoreTxn *beginWrite();
    bool isWriting() const;
    QKeyValueStoreTxn *writeTransaction();
    // This is for write transactions to report back.
    void addEntry(QKeyValueStoreEntry *entry);
    void reportReadyWrite();
    void reportReadyRead();

    // Internals
    QKeyValueStore *m_parent;
    QString m_name;
    QKeyValueStoreTxn *m_writeTransaction;
    QKeyValueStoreFile *m_file;
    quint32 m_currentTag;
    bool m_autoSync;
    bool m_runCompaction;
    bool m_needCompaction;
    quint32 m_compactThreshold;
    quint32 m_compactableOperations;
    enum QKeyValueStoreState { Closed, Ready, DataToSync, RebuildBTree, Error = 0xFFFF };
    QKeyValueStoreState m_state;
    quint32 m_pendingBytes;
    quint32 m_syncThreshold;
    quint32 m_marker;
    quint32 m_ongoingReadTransactions;
    quint32 m_ongoingWriteTransactions;
    QMap<quint64, qint64> m_dataMarks;
    QMap<QByteArray, qint64> m_offsets;

    // Internal methods, should not be called from the public class!!!
    void syncToDisk();
    void syncTree(quint64 timestamp);
    int rebuildBTree();
    quint64 checkFileContents();
    quint64 checkTreeContents();
};

#endif // QKEYVALUESTORE_P_H
