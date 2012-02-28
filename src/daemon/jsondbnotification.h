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

#ifndef JSONDB_NOTIFICATION_H
#define JSONDB_NOTIFICATION_H

#include <QObject>
#include <QJSValue>

#include "jsondb-global.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbOwner;
class JsonDbQuery;
class JsonDbNotification {
public:
    enum Action { None = 0x0000, Create = 0x0001, Update = 0x0002, Delete = 0x0004 };
    Q_DECLARE_FLAGS(Actions, Action)

    JsonDbNotification(const JsonDbOwner *owner, const QString &uuid, const QString &query, QStringList actions, const QString &partition);
    ~JsonDbNotification();

    const JsonDbOwner *owner() const { return mOwner; }
    const QString&  uuid() const { return mUuid; }
    const QString&  query() const { return mQuery; }
    Actions         actions() const { return mActions; }
    JsonDbQuery *parsedQuery() { return mCompiledQuery; }
    void            setCompiledQuery(JsonDbQuery *parsedQuery) { mCompiledQuery = parsedQuery; }
    const QString & partition() const { return mPartition; }
    quint32 initialStateNumber() const { return mInitialStateNumber; }
    void setInitialStateNumber(quint32 stateNumber) { mInitialStateNumber = stateNumber; }
private:
    const JsonDbOwner *mOwner;
    QString       mUuid;
    QString       mQuery;
    JsonDbQuery *mCompiledQuery;
    Actions       mActions;
    QString       mPartition;
    quint32       mInitialStateNumber;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(JsonDbNotification::Actions)

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_NOTIFICATION_H
