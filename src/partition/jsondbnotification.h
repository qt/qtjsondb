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

#include "jsondbobject.h"
#include "jsondbpartitionglobal.h"
#include "jsondbquery.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbObjectTable;
class JsonDbOwner;
class JsonDbPartition;
class JsonDbQuery;
class Q_JSONDB_PARTITION_EXPORT JsonDbNotification : public QObject {
    Q_OBJECT
public:
    enum Action { None = 0x0000, Create = 0x0001, Update = 0x0002, Remove = 0x0004, StateChange = 0x0008 };
    Q_DECLARE_FLAGS(Actions, Action)

    JsonDbNotification(const JsonDbOwner *owner, const JsonDbQuery &query, QStringList actions, qint32 initialStateNumber = -1);
    ~JsonDbNotification();

    void notifyIfMatches(JsonDbObjectTable *objectTable, const JsonDbObject &oldObject, const JsonDbObject &newObject,
                         Action action, quint32 stateNumber);
    void notifyStateChange();

    inline const JsonDbOwner *owner() const { return mOwner; }
    inline Actions actions() const { return mActions; }

    inline const JsonDbQuery &parsedQuery() const { return mCompiledQuery; }

    inline JsonDbPartition *partition() const { return mPartition; }
    inline void setPartition(JsonDbPartition *partition){ mPartition = partition; }
    inline JsonDbObjectTable *objectTable() const { return mObjectTable; }
    inline void setObjectTable(JsonDbObjectTable *objectTable) { mObjectTable = objectTable; }

    inline qint32 initialStateNumber() const { return mInitialStateNumber; }
    inline void setInitialStateNumber(qint32 stateNumber) { mInitialStateNumber = stateNumber; }
    inline quint32 lastStateNumber() const { return mLastStateNumber; }
    inline void setLastStateNumber(quint32 stateNumber) { mLastStateNumber = stateNumber; }

Q_SIGNALS:
    void notified(const QJsonObject &object, quint32 stateNumber, JsonDbNotification::Action action);

private:
    const JsonDbOwner *mOwner;
    JsonDbQuery   mCompiledQuery;
    Actions mActions;
    JsonDbPartition *mPartition;
    JsonDbObjectTable *mObjectTable;
    qint32  mInitialStateNumber;
    quint32 mLastStateNumber;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(JsonDbNotification::Actions)

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_NOTIFICATION_H
