/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef HBtreeTRANSACTION_H
#define HBtreeTRANSACTION_H

#include <QByteArray>
#include <QScopedPointer>

class HBtree;
class HBtreeTransactionPrivate;
class HBtreeTransaction
{
public:
    enum Type {
        ReadOnly,
        ReadWrite
    };

    ~HBtreeTransaction();

    bool put(const QByteArray &key, const QByteArray &value);
    bool get(const QByteArray &key, QByteArray *pValue);
    QByteArray get(const QByteArray &key);
    bool del(const QByteArray &key);
    bool remove(const QByteArray &key)
    { return del(key); }

    bool commit(quint64 tag);
    void abort();

    bool isReadOnly() const { return type_ == ReadOnly; }
    bool isReadWrite() const { return type_ == ReadWrite; }

    quint32 tag() const { return (quint32)tag_; }
    HBtree *btree() const { return btree_; }

private:
    Q_DISABLE_COPY(HBtreeTransaction)

    friend class HBtree;
    friend class HBtreePrivate;
    HBtreeTransaction(HBtree *btree, Type type);

    friend class HBtreeCursor;
    HBtree *btree_;
    Type type_;
    quint32 rootPage_;
    quint64 tag_;
    quint32 revision_;
};

#endif // HBtreeTRANSACTION_H
