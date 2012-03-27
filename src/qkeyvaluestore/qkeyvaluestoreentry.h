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

#ifndef QKEYVALUESTOREENTRY_H
#define QKEYVALUESTOREENTRY_H

#include <QByteArray>

class QKeyValueStoreEntry
{
    QByteArray m_key;
    QByteArray m_value;
    qint64 m_offset;
    quint32 m_hash;
public:
    QKeyValueStoreEntry();
    enum QKeyValueStoreEntryOperation { Add = 0x00, Remove = 0x01};
    QByteArray key() const { return m_key; }
    void setKey(const QByteArray &key) { m_key = key; }
    QByteArray value() const { return m_value; }
    void setValue(const QByteArray &value) { m_value = value; }
    qint64 offset() const { return m_offset; }
    void setOffset(qint64 offset) { m_offset = offset; }
    quint32 hash();
private:
    QKeyValueStoreEntryOperation m_operation;
public:
    quint8 operation() const { return (quint8)m_operation; }
    void setOperation(QKeyValueStoreEntryOperation operation) { m_operation = operation; }
};

#endif // QDATASTORAGEENTRY_H
