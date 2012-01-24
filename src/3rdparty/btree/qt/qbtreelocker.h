/****************************************************************************
**
** Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef QBTREELOCKER_H
#define QBTREELOCKER_H

#include <QByteArray>
#include "qbtreedata.h"

class QBtree;
class QBtreeTxn;

class QBtreeReadLocker
{
public:
    explicit QBtreeReadLocker(QBtree *db);
    ~QBtreeReadLocker();

    inline bool isValid() const { return mTxn != 0; }

    bool get(const QByteArray &baKey, QByteArray *baValue) const;
    bool get(const char *key, int keySize, QBtreeData *value) const;
    bool get(const QBtreeData &key, QBtreeData *value) const;

    quint32 tag() const;

    void abort();

private:
    QBtreeTxn *mTxn;

    // forbid copy constructor
    QBtreeReadLocker(const QBtreeReadLocker &);
    void operator=(const QBtreeReadLocker &);
};

class QBtreeWriteLockerPrivate;
class QBtreeWriteLocker
{
public:
    explicit QBtreeWriteLocker(QBtree *db);
    ~QBtreeWriteLocker();

    bool isValid() const;

    bool get(const QByteArray &baKey, QByteArray *baValue) const;
    bool get(const char *key, int keySize, QBtreeData *value) const;
    bool get(const QBtreeData &key, QBtreeData *value) const;
    bool put(const QByteArray &baKey, const QByteArray &baValue);
    bool put(const char *key, int keySize, const char *value, int valueSize);
    bool put(const QBtreeData &baKey, const QBtreeData &baValue);
    bool remove(const QByteArray &baKey);
    bool remove(const QBtreeData &baKey);
    bool remove(const char *key, int keySize);

    quint32 tag() const;

    void abort();
    bool commit(quint32 tag);

    void setAutoCommitTag(quint32 tag);
    quint32 autoCommitTag() const;
    void unsetAutoCommitTag();

private:
    QBtreeWriteLockerPrivate *d_ptr;

    // forbid copy constructor
    QBtreeWriteLocker(const QBtreeWriteLocker &);
    void operator=(const QBtreeWriteLocker &);
};

#endif // QBTREELOCKER_H
