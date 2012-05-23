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

#ifndef JSONDB_PARTITION_P_H
#define JSONDB_PARTITION_P_H

#include <QMultiHash>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QPointer>

#include "jsondberrors.h"
#include "jsondbnotification.h"
#include "jsondbobjectkey.h"
#include "jsondbowner.h"
#include "jsondbpartition.h"
#include "jsondbpartitionglobal.h"
#include "jsondbschemamanager_p.h"
#include "jsondbstat.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbBtree;
class JsonDbOwner;
class JsonDbObjectTable;
class JsonDbIndexSpec;
class JsonDbIndex;
class JsonDbIndexQuery;
class JsonDbView;

class Q_JSONDB_PARTITION_EXPORT JsonDbPartitionPrivate {
    Q_DECLARE_PUBLIC(JsonDbPartition)
public:
    JsonDbPartitionPrivate(JsonDbPartition *q);
    ~JsonDbPartitionPrivate();

    bool beginTransaction();
    JsonDbPartition::TxnCommitResult commitTransaction(quint32 stateNumber = 0);
    bool abortTransaction();

    void initIndexes();
    void initSchemas();

    JsonDbObjectTable *findObjectTable(const QString &objectType) const;

    bool addIndex(const JsonDbIndexSpec &indexSpec);
    bool removeIndex(const QString &indexName, const QString &objectType = QString());

    JsonDbView *addView(const QString &viewType);
    void removeView(const QString &viewType);

    bool getObject(const QString &uuid, JsonDbObject &object, const QString &objectType = QString(), bool includeDeleted = false) const;
    bool getObject(const ObjectKey & objectKey, JsonDbObject &object, const QString &objectType = QString(), bool includeDeleted = false) const;

    GetObjectsResult getObjects(const QString &keyName, const QJsonValue &key, const QString &type = QString(),
                                bool updateViews = true);

    JsonDbIndexQuery *compileIndexQuery(const JsonDbOwner *owner, const JsonDbQuery *query);

    void doIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                      JsonDbIndexQuery *indexQuery);

    static void sortValues(const JsonDbQuery *query, JsonDbObjectList &results, JsonDbObjectList &joinedResults);

    bool checkCanAddSchema(const JsonDbObject &schema, const JsonDbObject &oldSchema, QString &errorMsg);
    bool checkCanRemoveSchema(const JsonDbObject &schema, QString &errorMsg);
    bool validateSchema(const QString &schemaName, const JsonDbObject &object, QString &errorMsg);
    bool checkNaturalObjectType(const JsonDbObject &object, QString &errorMsg);

    JsonDbError::ErrorCode checkBuiltInTypeValidity(const JsonDbObject &object, const JsonDbObject &oldObject, QString &errorMsg);
    JsonDbError::ErrorCode checkBuiltInTypeAccessControl(bool forCreation, const JsonDbOwner *owner, const JsonDbObject &object,
                                                         const JsonDbObject &oldObject, QString &errorMsg);
    void updateBuiltInTypes(const JsonDbObject &object, const JsonDbObject &oldObject);
    void setSchema(const QString &schemaName, const QJsonObject &schema);
    void removeSchema(const QString &schemaName);
    void updateSchemaIndexes(const QString &schemaName, QJsonObject object, const QStringList &path=QStringList());
    void updateSpaceStatus();
    bool hasSpace();

    void updateEagerViews(const QSet<QString> &eagerViewTypes, const JsonDbUpdateList &changes);
    void updateEagerViewTypes(const QString &viewType, quint32 stateNumber, int increment = 1);
    void updateEagerViewStateNumbers();
    void notifyHistoricalChanges(JsonDbNotification *n);

    void _q_mainSyncTimer();
    void _q_indexSyncTimer();
    void _q_objectsUpdated(bool viewUpdated, const JsonDbUpdateList &changes);

    class EdgeCount {
    public:
        EdgeCount() : count(0){};
        int count;
        bool operator >(int val) const { return count > val; }
        bool operator ==(int val) const { return count == val; }
        EdgeCount &operator +=(int delta) { count += delta; if (count < 0) count = 0; return *this; }
    };
    typedef QHash<QString, EdgeCount>        ViewEdgeWeights;
    typedef QHash<QString, ViewEdgeWeights>  WeightedSourceViewGraph;

    JsonDbPartition *q_ptr;
    JsonDbObjectTable     *mObjectTable;
    QVector<JsonDbObjectTable *> mTableTransactions;

    QString      mPartitionName;
    QString      mFilename;
    int          mTransactionDepth;
    bool         mTransactionOk;
    QHash<QString,QPointer<JsonDbView> > mViews;
    QSet<QString> mViewTypes;
    QMultiHash<QString, QPointer<JsonDbNotification> > mKeyedNotifications;
    WeightedSourceViewGraph mEagerViewSourceGraph;
    JsonDbSchemaManager   mSchemas;
    QTimer      *mMainSyncTimer;
    QTimer      *mIndexSyncTimer;
    JsonDbOwner *mDefaultOwner;
    bool         mIsOpen;
    JsonDbPartition::DiskSpaceStatus mDiskSpaceStatus;

};

class WithTransaction {
public:
    WithTransaction(JsonDbPartitionPrivate *partition = 0, QString name=QString())
        : mPartition(0)
    {
        Q_UNUSED(name)
        if (partition && partition->mIsOpen)
            setPartition(partition);
    }

    ~WithTransaction()
    {
        if (mPartition)
            mPartition->commitTransaction();
    }

    void setPartition(JsonDbPartitionPrivate *partition)
    {
        Q_ASSERT(!mPartition);
        mPartition = partition;
        if (!mPartition->beginTransaction())
            mPartition = 0;
    }

    bool addObjectTable(JsonDbObjectTable *table);

    void abort()
    {
        if (mPartition)
            mPartition->abortTransaction();
        mPartition = 0;
    }

    JsonDbPartition::TxnCommitResult commit(quint32 stateNumber = 0)
    {
        JsonDbPartition::TxnCommitResult result;
        if (mPartition)
            result = mPartition->commitTransaction(stateNumber);
        mPartition = 0;
        return result;
    }

private:
    JsonDbPartitionPrivate *mPartition;
};

QDebug &operator<<(QDebug &, const ObjectKey &);

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_PARTITION_P_H
