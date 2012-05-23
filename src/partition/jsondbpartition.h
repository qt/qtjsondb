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

#ifndef JSONDB_PARTITION_H
#define JSONDB_PARTITION_H

#include <QStringList>
#include <QSet>
#include <QPointer>

#include "jsondbpartitionglobal.h"
#include "jsondberrors.h"
#include "jsondbobjectkey.h"
#include "jsondbnotification.h"
#include "jsondbowner.h"
#include "jsondbpartitionspec.h"
#include "jsondbstat.h"
#include "jsondbschemamanager_p.h"

QT_BEGIN_HEADER

class TestJsonDb;

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbOwner;
class JsonDbObjectTable;
class JsonDbIndex;
class JsonDbView;

struct Q_JSONDB_PARTITION_EXPORT JsonDbUpdate {
    JsonDbUpdate(const JsonDbObject &oldObj, const JsonDbObject &newObj, JsonDbNotification::Action act) :
        oldObject(oldObj), newObject(newObj), action(act) { }
    JsonDbUpdate() : action(JsonDbNotification::None) {}
    JsonDbObject oldObject;
    JsonDbObject newObject;
    JsonDbNotification::Action action;
};


typedef QList<JsonDbUpdate> JsonDbUpdateList;
struct Q_JSONDB_PARTITION_EXPORT JsonDbWriteResult {
    JsonDbWriteResult() : state(0), code(JsonDbError::NoError) { }
    JsonDbObjectList objectsWritten;
    quint32 state;
    JsonDbError::ErrorCode code;
    QString message;
};

struct Q_JSONDB_PARTITION_EXPORT JsonDbChangesSinceResult {
    JsonDbChangesSinceResult() : currentStateNumber(0), startingStateNumber(0), code(JsonDbError::NoError) { }
    quint32 currentStateNumber;
    quint32 startingStateNumber;
    JsonDbUpdateList changes;
    JsonDbError::ErrorCode code;
    QString message;
};

typedef QList<JsonDbObject> JsonDbObjectList;
struct Q_JSONDB_PARTITION_EXPORT JsonDbQueryResult {
    JsonDbQueryResult() : offset(0), state(0), code(JsonDbError::NoError), objectTable(0) { }
    JsonDbObjectList data;
    int offset;
    quint32 state;
    QStringList sortKeys;
    JsonDbError::ErrorCode code;
    QString message;
    JsonDbObjectTable *objectTable;
};

class JsonDbPartitionPrivate;
class Q_JSONDB_PARTITION_EXPORT JsonDbPartition : public QObject
{
    Q_OBJECT
public:
    enum ConflictResolutionMode {
        RejectStale,    // write must not introduce a conflict
        Replace,        // accept write as is (almost no matter what)
        ReplicatedWrite,    // master/master replication, may create obj._meta.conflicts
        ViewObject          // internal for view object
    };

    enum DiskSpaceStatus {
        UnknownStatus,  // Unknown, this happens between construction and open
        HasSpace,         // Available space >= minimum required
        OutOfSpace      // Available space < minimum required
    };

    enum TxnCommitResult {
        TxnSucceeded,      // Everything ok
        TxnOutOfSpace,     // Not enough space to write to disk
        TxnStorageError    // Problems with the storage system
    };

    JsonDbPartition(QObject *parent = 0);
    ~JsonDbPartition();

    void setPartitionSpec(const JsonDbPartitionSpec &spec);
    const JsonDbPartitionSpec &partitionSpec() const;

    void setDefaultOwner(JsonDbOwner *owner);
    JsonDbOwner *defaultOwner() const;

    QString filename() const;
    bool open();
    bool close();
    bool isOpen() const;

    bool clear();
    void closeIndexes();
    void flushCaches();
    bool compact();

    JsonDbQueryResult queryObjects(const JsonDbOwner *owner, const JsonDbQuery &query, int limit = -1, int offset = 0);
    JsonDbWriteResult updateObjects(const JsonDbOwner *owner, const JsonDbObjectList &objects, ConflictResolutionMode mode = RejectStale, JsonDbUpdateList *changeList = 0);
    JsonDbWriteResult updateObject(const JsonDbOwner *owner, const JsonDbObject &object, ConflictResolutionMode mode = RejectStale, JsonDbUpdateList *changeList = 0);
    JsonDbChangesSinceResult changesSince(quint32 stateNumber, const QSet<QString> &limitTypes = QSet<QString>());
    int flush(bool *ok);

    void addNotification(JsonDbNotification *notification);
    void removeNotification(JsonDbNotification *notification);

    JsonDbObjectTable *mainObjectTable() const;
    JsonDbObjectTable *findObjectTable(const QString &objectType) const;
    JsonDbView *findView(const QString &objectType) const;

    JsonDbStat stat() const;
    QHash<QString, qint64> fileSizes() const;

public Q_SLOTS:
    void updateView(const QString &objectType, quint32 stateNumber=0);

private:
    Q_DECLARE_PRIVATE(JsonDbPartition)
    Q_DISABLE_COPY(JsonDbPartition)
    Q_PRIVATE_SLOT(d_func(), void _q_mainSyncTimer())
    Q_PRIVATE_SLOT(d_func(), void _q_indexSyncTimer())
    Q_PRIVATE_SLOT(d_func(), void _q_objectsUpdated(bool,JsonDbUpdateList))
    QScopedPointer<JsonDbPartitionPrivate> d_ptr;

    friend class JsonDbIndexQuery;
    friend class JsonDbObjectTable;
    friend class JsonDbOwner;
    friend class JsonDbQuery;
    friend class JsonDbMapDefinition;
    friend class JsonDbReduceDefinition;
    friend class JsonDbView;

    friend class ::TestPartition;
    friend class ::TestJsonDb;
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_PARTITION_H
