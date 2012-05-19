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

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QString>
#include <QElapsedTimer>
#include <QUuid>
#include <QtAlgorithms>
#include <QtEndian>
#include <QStringBuilder>
#include <QTimerEvent>
#include <QMap>
#include <QByteArray>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <errno.h>

#include "jsondbstrings.h"
#include "jsondberrors.h"
#include "jsondbpartition.h"
#include "jsondbpartition_p.h"
#include "jsondbindex.h"
#include "jsondbindex_p.h"
#include "jsondbindexquery.h"
#include "jsondbobjecttable.h"
#include "jsondbbtree.h"
#include "jsondbsettings.h"
#include "jsondbscriptengine.h"
#include "jsondbview.h"
#include "jsondbschemamanager_impl_p.h"
#include "jsondbobjecttypes_impl_p.h"
#include "jsondbutils_p.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

const QString gDatabaseSchemaVersion = QStringLiteral("0.2");

JsonDbPartitionPrivate::JsonDbPartitionPrivate(JsonDbPartition *q)
    : q_ptr(q)
    , mObjectTable(0)
    , mTransactionDepth(0)
    , mDefaultOwner(0)
    , mIsOpen(false)
    , mDiskSpaceStatus(JsonDbPartition::UnknownStatus)
{
    mMainSyncTimer = new QTimer(q);
    mMainSyncTimer->setInterval(jsondbSettings->syncInterval() < 1000 ? 5000 : jsondbSettings->syncInterval());
    mMainSyncTimer->setTimerType(Qt::VeryCoarseTimer);
    QObject::connect(mMainSyncTimer, SIGNAL(timeout()), q, SLOT(_q_mainSyncTimer()));

    mIndexSyncTimer = new QTimer(q);
    mIndexSyncTimer->setInterval(jsondbSettings->indexSyncInterval() < 1000 ? 12000 : jsondbSettings->indexSyncInterval());
    mIndexSyncTimer->setTimerType(Qt::VeryCoarseTimer);
    QObject::connect(mIndexSyncTimer, SIGNAL(timeout()), q, SLOT(_q_indexSyncTimer()));
}

JsonDbPartitionPrivate::~JsonDbPartitionPrivate() {

}

JsonDbObjectTable *JsonDbPartitionPrivate::findObjectTable(const QString &objectType) const
{
    if (JsonDbView *view = mViews.value(objectType))
        return view->objectTable();
    else if (mIsOpen)
        return mObjectTable;
    return 0;
}

void JsonDbPartitionPrivate::updateEagerViews(const QSet<QString> &eagerViewTypes, const JsonDbUpdateList &changes)
{
    QSet<QString> viewTypes(eagerViewTypes);
    JsonDbUpdateList changeList(changes);

    QSet<QString> viewsUpdated;

    if (jsondbSettings->verbose())
        qDebug() << "updateEagerViews {" << mObjectTable->stateNumber() << viewTypes;

    while (!viewTypes.isEmpty()) {
        bool madeProgress = false;
        foreach (const QString &targetType, viewTypes) {
            JsonDbView *view = mViews.value(targetType);
            if (!view) {
                if (jsondbSettings->verbose())
                    qWarning() << "non-view viewType?" << targetType << "eager views to update" << viewTypes;
                viewTypes.remove(targetType);
                madeProgress = true;
                continue;
            }
            QSet<QString> typesNeeded(view->sourceTypeSet());
            typesNeeded.intersect(viewTypes);
            if (!typesNeeded.isEmpty())
                continue;
            viewTypes.remove(targetType);
            QList<JsonDbUpdate> additionalChanges;
            view->updateEagerView(changeList, &additionalChanges);
            viewsUpdated.insert(targetType);
            if (jsondbSettings->verbose())
                qDebug() << "updated eager view" << targetType << additionalChanges.size() << additionalChanges;
            changeList.append(additionalChanges);
            // if this triggers other eager types, we need to update that also
            if (mEagerViewSourceGraph.contains(targetType)) {
                const ViewEdgeWeights &edgeWeights = mEagerViewSourceGraph[targetType];
                for (ViewEdgeWeights::const_iterator it = edgeWeights.begin(); it != edgeWeights.end(); ++it) {
                    if (it.value() == 0)
                        continue;
                    const QString &viewType = it.key();
                    if (viewsUpdated.contains(viewType))
                        qWarning() << "View update cycle detected" << targetType << viewType << viewsUpdated;
                    else
                        viewTypes.insert(viewType);
                }
            }

            madeProgress = true;
        }
        if (!madeProgress) {
            qCritical() << "Failed to update any views" << viewTypes;
            break;
        }
    }

    updateEagerViewStateNumbers();
    foreach (JsonDbNotification *n, mKeyedNotifications.values()) {
        if (n && n->lastStateNumber() == mObjectTable->stateNumber())
            n->notifyStateChange();
    }

    if (jsondbSettings->verbose())
        qDebug() << "updateEagerViews }" << mObjectTable->stateNumber() << viewsUpdated;
}

// Updates the in-memory state numbers on each view so that we know it
// has seen all relevant updates from this transaction
void JsonDbPartitionPrivate::updateEagerViewStateNumbers()
{
    if (jsondbSettings->verbose())
        qDebug() << "updateEagerViewStateNumbers" << mPartitionName << mObjectTable->stateNumber() << "{";

    QStringList visitedViews;
    for (WeightedSourceViewGraph::ConstIterator it = mEagerViewSourceGraph.begin(); it != mEagerViewSourceGraph.end(); ++it) {
        const ViewEdgeWeights &edgeWeights = it.value();
        for (ViewEdgeWeights::const_iterator jt = edgeWeights.begin(); jt != edgeWeights.end(); ++jt) {
            const QString &viewType = jt.key();
            if (jt.value() == 0)
                continue;
            if (visitedViews.contains(viewType))
                continue;
            JsonDbView *view = mViews.value(viewType);
            if (view)
                view->updateViewStateNumber(mObjectTable->stateNumber());
            else
                if (jsondbSettings->debug())
                    qCritical() << "no view for" << viewType << mPartitionName;
            visitedViews.append(viewType);
        }
    }
    if (jsondbSettings->verbose())
        qDebug() << "updateEagerViewStateNumbers" << mPartitionName << mObjectTable->stateNumber() << "}";
}

void JsonDbPartitionPrivate::notifyHistoricalChanges(JsonDbNotification *n)
{
    Q_Q(JsonDbPartition);
    quint32 stateNumber = n->initialStateNumber();;
    quint32 lastStateNumber = mObjectTable->stateNumber();
    JsonDbQuery *parsedQuery = n->parsedQuery();
    QSet<QString> matchedTypes = parsedQuery->matchedTypes();
    QList<JsonDbObjectTable*> objectTables;

    if (stateNumber == 0) {
        JsonDbObject oldObject;
        JsonDbQueryResult queryRes = q->queryObjects(n->owner(), parsedQuery);
        foreach (const JsonDbObject &o, queryRes.data) {
            JsonDbNotification::Action action = JsonDbNotification::Create;
            n->notifyIfMatches(queryRes.objectTable, oldObject, o, action, lastStateNumber);
        }
    } else {
        foreach (const QString matchedType, matchedTypes) {
            JsonDbObjectTable *objectTable = findObjectTable(matchedType);
            if (objectTables.contains(objectTable))
                continue;
            objectTables.append(objectTable);
            QList<JsonDbUpdate> updateList;
            quint32 objectTableStateNumber = objectTable->changesSince(stateNumber, matchedTypes, &updateList);
            if (jsondbSettings->verbose() && lastStateNumber != objectTableStateNumber)
                qDebug() << "old object table for type" << matchedType << objectTableStateNumber << lastStateNumber;
            foreach (const JsonDbUpdate &update, updateList) {
                JsonDbObject before = update.oldObject;
                JsonDbObject after = update.newObject;
                bool beforeMatch = before.isEmpty() ? false : parsedQuery->match(before, 0, 0);
                bool afterMatch = after.isDeleted() ? false : parsedQuery->match(after, 0, 0);
                JsonDbNotification::Action action = JsonDbNotification::Update;

                if (!beforeMatch && !afterMatch)
                    continue;
                if (!beforeMatch)
                    action = JsonDbNotification::Create;
                else if (!afterMatch)
                    action = JsonDbNotification::Remove;

                n->notifyIfMatches(objectTable, before, after, action, lastStateNumber);
            }
        }
    }
    n->notifyStateChange();
}

/*!
  Updates the per-partition information on eager views.

  For each partition, we maintain a graph with weighted edges from
  source types to \a viewType.

  Adds \a increment weight to the edge from each of the source types
  of the view to the target type.  Call with \a increment of 1 when
  adding an eager view type, and -1 when removing an eager view type.

  It recursively updates the graph for each source type that is also a
  view.

  If \a stateNumber is non-zero, do a full update of each the views so
  that they will be ready for eager updates.
 */
void JsonDbPartitionPrivate::updateEagerViewTypes(const QString &viewType, quint32 stateNumber, int increment)
{
    Q_Q(JsonDbPartition);

    JsonDbView *view = mViews.value(viewType);

    if (!view)
        return;

    // An update of the Map/Reduce definition also causes the view to
    // need to be updated, so we count it as a view source.

    // this is a bit conservative, since we add both Map and Reduce as
    // sources, but it shortens the code
    mEagerViewSourceGraph[JsonDbString::kMapTypeStr][viewType] += increment;
    mEagerViewSourceGraph[JsonDbString::kReduceTypeStr][viewType] += increment;
    foreach (const QString sourceType, view->sourceTypes()) {
        mEagerViewSourceGraph[sourceType][viewType] += increment;
        if (jsondbSettings->verbose())
            qDebug() << "SourceView" << sourceType << viewType << mEagerViewSourceGraph[sourceType][viewType].count;
        // now recurse until we get to a non-view sourceType
        updateEagerViewTypes(sourceType, stateNumber, increment);
    }

    if (stateNumber)
        q->updateView(viewType, stateNumber);
}

void JsonDbPartitionPrivate::_q_mainSyncTimer()
{
    if (mTransactionDepth)
        return;

    if (jsondbSettings->debug())
        qDebug() << "Syncing main object table";

    mObjectTable->sync(JsonDbObjectTable::SyncObjectTable);
    mMainSyncTimer->stop();
}

void JsonDbPartitionPrivate::_q_indexSyncTimer()
{
    if (mTransactionDepth)
        return;

    if (jsondbSettings->debug())
        qDebug() << "Syncing indexes and views";

    // sync the main object table's indexes
    mObjectTable->sync(JsonDbObjectTable::SyncIndexes);

    // sync the views
    foreach (JsonDbView *view, mViews)
        view->objectTable()->sync(JsonDbObjectTable::SyncObjectTable | JsonDbObjectTable::SyncIndexes);

    mIndexSyncTimer->stop();
}

void JsonDbPartitionPrivate::_q_objectsUpdated(bool viewUpdated, const JsonDbUpdateList &changes)
{
    QList<JsonDbUpdate> updatesToEagerViews;
    QSet<QString> eagerViewTypes;
    bool foundViewChange = false;

    quint32 partitionStateNumber = mObjectTable->stateNumber();

    if (jsondbSettings->debug())
        qDebug() << "objectsUpdated" << mPartitionName << partitionStateNumber;

    foreach (const JsonDbUpdate &updated, changes) {

        JsonDbObject oldObject = updated.oldObject;
        JsonDbObject object = updated.newObject;
        JsonDbNotification::Action action = updated.action;

        QString oldObjectType = oldObject.type();
        QString objectType = object.type();
        quint32 stateNumber = 0;

        JsonDbObjectTable *objectTable = findObjectTable(objectType);
        stateNumber = objectTable->stateNumber();

        QStringList notificationKeys;
        if (!oldObjectType.isEmpty() || !objectType.isEmpty()) {
            notificationKeys << objectType;
            if (!oldObjectType.isEmpty() && objectType.compare(oldObjectType))
                notificationKeys << oldObjectType;

            // eagerly update views if this object that was created isn't a view type itself
            if (jsondbSettings->verbose()) qDebug() << "objectType" << oldObjectType << mEagerViewSourceGraph.contains(oldObjectType) << objectType << mEagerViewSourceGraph.contains(objectType);
            if (mViews.contains(objectType)) {
                if (jsondbSettings->verbose()) qDebug() << "foundViewChange" << objectType;
                foundViewChange = true;
            } else if ((mEagerViewSourceGraph.contains(oldObjectType))
                       || (mEagerViewSourceGraph.contains(objectType))) {
                JsonDbUpdateList updateList;
                if (oldObjectType == objectType) {
                    updateList.append(updated);
                } else {
                    JsonDbObject tombstone(oldObject);
                    tombstone.insert(QLatin1String("_deleted"), true);
                    updateList.append(JsonDbUpdate(oldObject, tombstone, JsonDbNotification::Remove));
                    updateList.append(JsonDbUpdate(JsonDbObject(), object, JsonDbNotification::Create));
                }
                foreach (const JsonDbUpdate &splitUpdate, updateList) {
                    const QString updatedObjectType = splitUpdate.newObject.type();
                    const ViewEdgeWeights &edgeWeights = mEagerViewSourceGraph[updatedObjectType];
                    bool neededUpdate = false;
                    for (ViewEdgeWeights::const_iterator it = edgeWeights.begin(); it != edgeWeights.end(); ++it) {
                        if (jsondbSettings->verbose()) qDebug() << "edge weight" << updatedObjectType << it.key() << it.value().count;
                        if (it.value() > 0) {
                            eagerViewTypes.insert(it.key());
                            if (mEagerViewSourceGraph.contains(updatedObjectType)) {
                                neededUpdate = true;
                                break;
                            }
                        }
                    }
                    if (neededUpdate) {
                        updatesToEagerViews.append(splitUpdate);
                        break;
                    }
                }
            }

        }

        if (object.contains(JsonDbString::kUuidStr))
            notificationKeys << object.value(JsonDbString::kUuidStr).toString();
        notificationKeys << QStringLiteral("__generic_notification__");

        QHash<QString, JsonDbObject> objectCache;
        for (int i = 0; i < notificationKeys.size(); i++) {
            QString key = notificationKeys[i];
            for (QMultiHash<QString, QPointer<JsonDbNotification> >::const_iterator it = mKeyedNotifications.find(key);
                 (it != mKeyedNotifications.end()) && (it.key() == key);
                 ++it) {
                JsonDbNotification *n = it.value();
                if (n)
                    n->notifyIfMatches(objectTable, oldObject, object, action, stateNumber);
            }
        }
    }

    if (foundViewChange)
        Q_ASSERT(viewUpdated);
    // if there are no non-view object changes, do not update the eager view state numbers
    if (changes.isEmpty() || viewUpdated) {
        return;
    } if (updatesToEagerViews.isEmpty()) {
        updateEagerViewStateNumbers();

        foreach (JsonDbNotification *n, mKeyedNotifications.values()) {
            if (n && n->lastStateNumber() == mObjectTable->stateNumber())
                n->notifyStateChange();
        }
    } else {
        updateEagerViews(eagerViewTypes, updatesToEagerViews);
    }
}

JsonDbPartition::JsonDbPartition(QObject *parent)
    : QObject(parent), d_ptr(new JsonDbPartitionPrivate(this))
{
    qRegisterMetaType<QSet<QString> >("QSet<QString>");
    qRegisterMetaType<JsonDbUpdateList>("JsonDbUpdateList");
}

JsonDbPartition::~JsonDbPartition()
{
    Q_D(JsonDbPartition);
    if (d->mTransactionDepth) {
        qCritical() << "JsonDbBtreePartition::~JsonDbBtreePartition"
                    << "closing while transaction open" << "mTransactionDepth" << d->mTransactionDepth;
    }

    close();
}

void JsonDbPartition::setPartitionSpec(const JsonDbPartitionSpec &spec)
{
    Q_D(JsonDbPartition);
    d->mSpec = spec;
    d->mFilename = QDir(d->mSpec.path).absoluteFilePath(d->mSpec.name);
    if (!d->mFilename.endsWith(QLatin1String(".db")))
        d->mFilename += QLatin1String(".db");
}

const JsonDbPartitionSpec &JsonDbPartition::partitionSpec() const
{
    Q_D(const JsonDbPartition);
    return d->mSpec;
}

void JsonDbPartition::setDefaultOwner(JsonDbOwner *owner)
{
    Q_D(JsonDbPartition);
    d->mDefaultOwner = owner;
}

JsonDbOwner *JsonDbPartition::defaultOwner() const
{
    Q_D(const JsonDbPartition);
    return d->mDefaultOwner;
}

QString JsonDbPartition::filename() const
{
    Q_D(const JsonDbPartition);
    return d->mFilename;
}

bool JsonDbPartition::isOpen() const
{
    Q_D(const JsonDbPartition);
    return d->mIsOpen;
}

JsonDbObjectTable *JsonDbPartition::mainObjectTable() const
{
    Q_D(const JsonDbPartition);
    return d->mObjectTable;
}

bool JsonDbPartition::close()
{
    Q_D(JsonDbPartition);

    if (!d->mIsOpen)
        return true;
    else if (d->mTransactionDepth || !d->mTableTransactions.isEmpty())
        return false;

    if (d->mMainSyncTimer->isActive())
        d->mMainSyncTimer->stop();
    if (d->mIndexSyncTimer->isActive())
        d->mIndexSyncTimer->stop();

    d->mSchemas.clear();
    d->mViewTypes.clear();
    d->mKeyedNotifications.clear();

    foreach (JsonDbView *view, d->mViews.values()) {
        // sync the view object table, its indexes, and their state numbers to prevent reindexing on restart
        view->objectTable()->sync(JsonDbObjectTable::SyncObjectTable | JsonDbObjectTable::SyncIndexes | JsonDbObjectTable::SyncStateNumbers);
        view->close();
        delete view;
    }
    d->mViews.clear();

    if (d->mObjectTable) {
        // sync the main object table, its indexes, and their state numbers to prevent reindexing on restart
        d->mObjectTable->sync(JsonDbObjectTable::SyncObjectTable | JsonDbObjectTable::SyncIndexes | JsonDbObjectTable::SyncStateNumbers);

        delete d->mObjectTable;
        d->mObjectTable = 0;
    }

    d->mIsOpen = false;
    return true;
}

bool JsonDbPartition::open()
{
    Q_D(JsonDbPartition);

    if (jsondbSettings->debug())
        qDebug() << "JsonDbBtree::open" << d->mSpec.name << d->mFilename;

    if (d->mIsOpen)
        return true;

    if (!QFileInfo(d->mFilename).absoluteDir().exists()) {
        qWarning() << "Partition directory does not exist" << QFileInfo(d->mFilename).absolutePath();
        return false;
    }

    if (!d->mObjectTable)
        d->mObjectTable = new JsonDbObjectTable(this);

    if (!d->mObjectTable->open(d->mFilename)) {
        qWarning() << "JsonDbPartition::open() failed to open object table" << d->mFilename;
        return false;
    }

    d->updateSpaceStatus();
    d->mIsOpen = true;

    d->initSchemas();
    d->initIndexes();
    JsonDbView::initViews(this);

    return true;
}

bool JsonDbPartition::clear()
{
    Q_D(JsonDbPartition);

    if (d->mObjectTable->bdb()) {
        qCritical() << "Cannot clear database while it is open.";
        return false;
    }
    QStringList filters;
    QFileInfo fi(d->mFilename);
    filters << QString::fromLatin1("%1*.db").arg(fi.baseName());
    QDir dir(fi.absolutePath());
    QStringList lst = dir.entryList(filters);
    foreach (const QString &fileName, lst) {
        if (jsondbSettings->verbose())
            qDebug() << "removing" << fileName;
        if (!dir.remove(fileName)) {
            qCritical() << "Failed to remove" << fileName;
            return false;
        }
    }
    return true;
}

JsonDbView *JsonDbPartitionPrivate::addView(const QString &viewType)
{
    Q_Q(JsonDbPartition);

    JsonDbView *view = mViews.value(viewType);
    if (view)
        return view;

    view = new JsonDbView(q, viewType, q);
    view->open();
    mViews.insert(viewType, view);
    return view;
}

void JsonDbPartitionPrivate::removeView(const QString &viewType)
{
    JsonDbView *view = mViews.take(viewType);
    Q_ASSERT(view);
    view->close();
    view->deleteLater();
}

void JsonDbPartition::updateView(const QString &objectType, quint32 stateNumber)
{
    // No point in updating the view if we are out of space.
    Q_D(JsonDbPartition);
    if (!d->hasSpace())
        return;
    if (JsonDbView *view = d->mViews.value(objectType))
        view->updateView(stateNumber);
}

bool JsonDbPartitionPrivate::checkCanAddSchema(const JsonDbObject &schema, const JsonDbObject &oldSchema, QString &errorMsg)
{
    if (!schema.contains(QStringLiteral("name")) || !schema.contains(QStringLiteral("schema"))) {
        errorMsg = QStringLiteral("_schemaType objects must specify both name and schema properties");
        return false;
    }

    QString schemaName = schema.value(QStringLiteral("name")).toString();

    if (schemaName.isEmpty()) {
        errorMsg = QStringLiteral("name property of _schemaType object must be specified");
        return false;
    } else if (mSchemas.contains(schemaName) && oldSchema.value(QStringLiteral("name")).toString() != schemaName) {
        errorMsg = QString(QStringLiteral("A schema with name %1 already exists")).arg(schemaName);
        return false;
    }

    return true;
}

JsonDbView *JsonDbPartition::findView(const QString &objectType) const
{
    Q_D(const JsonDbPartition);
    return d->mViews.value(objectType);
}

JsonDbObjectTable *JsonDbPartition::findObjectTable(const QString &objectType) const
{
    Q_D(const JsonDbPartition);
    return d->findObjectTable(objectType);
}

bool JsonDbPartitionPrivate::beginTransaction()
{
    if (mTransactionDepth++ == 0) {
        Q_ASSERT(mTableTransactions.isEmpty());
    }
    return true;
}

JsonDbPartition::TxnCommitResult JsonDbPartitionPrivate::commitTransaction(quint32 stateNumber)
{
    if (--mTransactionDepth == 0) {
        JsonDbPartition::TxnCommitResult ret;
        switch (mDiskSpaceStatus) {
        case JsonDbPartition::UnknownStatus:
            ret = JsonDbPartition::TxnStorageError;
            break;
        case JsonDbPartition::HasSpace:
            ret = JsonDbPartition::TxnSucceeded;
            break;
        case JsonDbPartition::OutOfSpace:
        default:
            ret = JsonDbPartition::TxnOutOfSpace;
            break;
        }

        quint32 nextStateNumber = stateNumber ? stateNumber : (mObjectTable->stateNumber() + 1);

        if (jsondbSettings->debug())
            qDebug() << "commitTransaction" << mSpec.name << nextStateNumber;

        if (!stateNumber && (mTableTransactions.size() == 1))
            nextStateNumber = mTableTransactions.at(0)->stateNumber() + 1;

        for (int i = 0; i < mTableTransactions.size(); i++) {
            JsonDbObjectTable *table = mTableTransactions.at(i);
            if (hasSpace()) {
                if (!table->commit(nextStateNumber)) {
                    qCritical() << __FILE__ << __LINE__ << "Failed to commit transaction on object table";
                    table->abort();
                    /*
                     * At this point we check the error from the pwrite call.
                     * To do that we instantiate the btree and ask what was the
                     * error. Any error that is not ENOSPC will be considered
                     * a storage error.
                     */
                    JsonDbBtree *btree = table->bdb();
                    if (btree->lastWriteError() == ENOSPC)
                        ret = JsonDbPartition::TxnOutOfSpace;
                    else
                        ret = JsonDbPartition::TxnStorageError;
                }
            } else {
                table->abort();
            }
        }
        mTableTransactions.clear();

        if (ret == JsonDbPartition::TxnSucceeded) {
            if (!mMainSyncTimer->isActive())
                mMainSyncTimer->start();
            if (!mIndexSyncTimer->isActive())
                mIndexSyncTimer->start();
        }

        return ret;
    }
    return JsonDbPartition::TxnSucceeded;
}

bool JsonDbPartitionPrivate::abortTransaction()
{
    if (--mTransactionDepth == 0) {
        if (jsondbSettings->verbose())
            qDebug() << "JsonDbBtreePartition::abortTransaction()";
        bool ret = true;

        for (int i = 0; i < mTableTransactions.size(); i++) {
            JsonDbObjectTable *table = mTableTransactions.at(i);
            if (!table->abort()) {
                qCritical() << __FILE__ << __LINE__ << "Failed to abort transaction";
                ret = false;
            }
        }
        mTableTransactions.clear();
        return ret;
    }
    return true;
}

int JsonDbPartition::flush(bool *ok)
{
    Q_D(JsonDbPartition);
    if (d->mIsOpen) {
        *ok = d->mObjectTable->sync(JsonDbObjectTable::SyncObjectTable);

        if (*ok)
            return static_cast<int>(d->mObjectTable->stateNumber());
    }
    return -1;
}

void JsonDbPartition::addNotification(JsonDbNotification *notification)
{
    Q_D(JsonDbPartition);
    if (!d->mIsOpen)
        return;

    notification->setPartition(this);

    const QList<JsonDbOrQueryTerm> &orQueryTerms = notification->parsedQuery()->queryTerms;
    const QSet<QString> matchedTypes = notification->parsedQuery()->matchedTypes();

    if (!matchedTypes.isEmpty()) {
        JsonDbObjectTable *objectTable = 0;
        foreach (const QString &matchedType, matchedTypes) {
            JsonDbObjectTable *typeTable = findObjectTable(matchedType);
            if (!objectTable)
                objectTable = typeTable;
            else if (objectTable != typeTable)
                qWarning() << "Notifications cannot span multiple object tables";
        }

        notification->setObjectTable(objectTable);
    }

    bool generic = true;
    for (int i = 0; i < orQueryTerms.size(); i++) {
        const JsonDbOrQueryTerm &orQueryTerm = orQueryTerms[i];
        const QList<JsonDbQueryTerm> &terms = orQueryTerm.terms();
        if (terms.size() == 1) {
            const JsonDbQueryTerm &term = terms[0];
            if (term.op() == QLatin1Char('=')) {
                if (term.propertyName() == JsonDbString::kUuidStr) {
                    d->mKeyedNotifications.insert(term.value().toString(), notification);
                    generic = false;
                    break;
                } else if (term.propertyName() == JsonDbString::kTypeStr) {
                    QString objectType = term.value().toString();
                    d->mKeyedNotifications.insert(objectType, notification);
                    generic = false;
                    break;
                }
            }
        }
    }

    if (generic)
        d->mKeyedNotifications.insert(QStringLiteral("__generic_notification__"), notification);

    quint32 stateNumber = notification->initialStateNumber() > -1 ? notification->initialStateNumber() : d->mObjectTable->stateNumber();
    notification->setInitialStateNumber(stateNumber);

    foreach (const QString &objectType, matchedTypes)
        d->updateEagerViewTypes(objectType, stateNumber, 1);

    d->notifyHistoricalChanges(notification);
}

void JsonDbPartition::removeNotification(JsonDbNotification *notification)
{
    Q_D(JsonDbPartition);
    if (!d->mIsOpen)
        return;

    const JsonDbQuery *parsedQuery = notification->parsedQuery();
    const QList<JsonDbOrQueryTerm> &orQueryTerms = parsedQuery->queryTerms;
    for (int i = 0; i < orQueryTerms.size(); i++) {
        const JsonDbOrQueryTerm &orQueryTerm = orQueryTerms[i];
        const QList<JsonDbQueryTerm> &terms = orQueryTerm.terms();
        if (terms.size() == 1) {
            const JsonDbQueryTerm &term = terms[0];
            if (term.op() == QLatin1Char('=')) {
                if (term.propertyName() == JsonDbString::kTypeStr) {
                    d->mKeyedNotifications.remove(term.value().toString(), notification);
                } else if (term.propertyName() == JsonDbString::kUuidStr) {
                    QString objectType = term.value().toString();
                    d->mKeyedNotifications.remove(objectType, notification);
                }
            }
        }
    }

    d->mKeyedNotifications.remove(QStringLiteral("__generic_notification__"), notification);

    foreach (const QString &objectType, parsedQuery->matchedTypes())
        d->updateEagerViewTypes(objectType, 0, -1);
}

bool JsonDbPartitionPrivate::getObject(const QString &uuid, JsonDbObject &object, const QString &objectType, bool includeDeleted) const
{
    ObjectKey objectKey(uuid);
    return getObject(objectKey, object, objectType, includeDeleted);
}

bool JsonDbPartitionPrivate::getObject(const ObjectKey &objectKey, JsonDbObject &object, const QString &objectType, bool includeDeleted) const
{
    JsonDbObjectTable *table = findObjectTable(objectType);

    bool ok = table->get(objectKey, &object, includeDeleted);
    if (ok)
        return ok;
    QHash<QString,QPointer<JsonDbView> >::const_iterator it = mViews.begin();
    for (; it != mViews.end(); ++it) {
        JsonDbView *view = it.value();
        if (!view) {
            qDebug() << "no view";
        } else {
            if (!view->objectTable())
                qDebug() << "no object table for view";
            bool ok = view->objectTable()->get(objectKey, &object);
            if (ok)
                return ok;
        }
    }
    return false;
}

GetObjectsResult JsonDbPartitionPrivate::getObjects(const QString &keyName, const QJsonValue &keyValue, const QString &_objectType, bool updateViews)
{
    Q_Q(JsonDbPartition);
    const QString &objectType = (keyName == JsonDbString::kTypeStr) ? keyValue.toString() : _objectType;
    JsonDbObjectTable *table = findObjectTable(objectType);

    if (updateViews && (table != mObjectTable))
        q->updateView(objectType);
    return table->getObjects(keyName, keyValue, objectType);
}

JsonDbChangesSinceResult JsonDbPartition::changesSince(quint32 stateNumber, const QSet<QString> &limitTypes)
{
    Q_D(JsonDbPartition);

    JsonDbChangesSinceResult result;
    if (!d->mIsOpen) {
        result.code = JsonDbError::PartitionUnavailable;
        result.message = QStringLiteral("Partition unvailable");
        return result;
    }

    JsonDbObjectTable *objectTable = 0;
    if (!limitTypes.size()) {
        objectTable = d->mObjectTable;
    } else {
        foreach (const QString &limitType, limitTypes) {
            JsonDbObjectTable *ot = findObjectTable(limitType);
            if (!objectTable) {
                objectTable = ot;
            } else if (ot == objectTable) {
                continue;
            } else {
                result.code = JsonDbError::InvalidRequest;
                result.message = QStringLiteral("limit types must be from the same object table");
                return result;
            }
        }
    }

    Q_ASSERT(objectTable);
    JsonDbUpdateList changeList;
    quint32 currentStateNumber = objectTable->changesSince(stateNumber, limitTypes, &changeList);

    result.startingStateNumber = stateNumber;
    result.currentStateNumber = currentStateNumber;
    result.changes = changeList;
    return result;
}

void JsonDbPartition::flushCaches()
{
    Q_D(JsonDbPartition);
    if (!d->mIsOpen)
        return;

    d->mObjectTable->flushCaches();
    foreach (QPointer<JsonDbView> view, d->mViews) {
        Q_ASSERT(view.data());
        view->reduceMemoryUsage();
    }
    JsonDbScriptEngine::releaseScriptEngine();
}

void JsonDbPartition::closeIndexes()
{
    Q_D(JsonDbPartition);

    if (!d->mIsOpen)
        return;

    d->mObjectTable->closeIndexes();
    QHash<QString,QPointer<JsonDbView> >::const_iterator it = d->mViews.begin(), e = d->mViews.end();
    for (; it != e; ++it) {
        it.value()->closeIndexes();
    }
}

void JsonDbPartitionPrivate::initIndexes()
{
    mObjectTable->addIndexOnProperty(JsonDbString::kUuidStr, QStringLiteral("string"));
    mObjectTable->addIndexOnProperty(JsonDbString::kTypeStr, QStringLiteral("string"));

    GetObjectsResult getObjectsResult = mObjectTable->getObjects(JsonDbString::kTypeStr, JsonDbString::kIndexTypeStr, JsonDbString::kIndexTypeStr);
    foreach (const QJsonObject indexObject, getObjectsResult.data) {
        if (jsondbSettings->verbose())
            qDebug() << "initIndexes" << "index" << indexObject;
        QString indexObjectType = indexObject.value(JsonDbString::kTypeStr).toString();
        if (indexObjectType == JsonDbString::kIndexTypeStr)
            addIndex(JsonDbIndexSpec::fromIndexObject(indexObject));
    }
}

bool JsonDbPartitionPrivate::addIndex(const JsonDbIndexSpec &indexSpec)
{
    Q_ASSERT(!indexSpec.name.isEmpty());
    JsonDbObjectTable *table = 0;
    if (indexSpec.objectTypes.isEmpty()) {
        table = mObjectTable;
    } else {
        foreach (const QString &objectType, indexSpec.objectTypes) {
            JsonDbObjectTable *t = findObjectTable(objectType);
            if (table && t != table) {
                qDebug() << "addIndex" << "index on multiple tables" << indexSpec.objectTypes;
                return false;
            }
            table = t;
        }
    }
    if (!table)
        return false;
    if (table->index(indexSpec.name))
        return true;
    return table->addIndex(indexSpec);
}

bool JsonDbPartitionPrivate::removeIndex(const QString &indexName, const QString &objectType)
{
    JsonDbObjectTable *table = findObjectTable(objectType);
    if (!table->index(indexName))
        return false;
    return table->removeIndex(indexName);
}

QHash<QString, qint64> JsonDbPartition::fileSizes() const
{
    Q_D(const JsonDbPartition);

    QHash<QString, qint64> result;
    if (!d->mIsOpen)
        return result;

    QList<QFileInfo> fileInfo;
    fileInfo << d->mObjectTable->bdb()->fileName();

    foreach (JsonDbIndex *index, d->mObjectTable->indexes()) {
        if (index->bdb())
            fileInfo << index->bdb()->fileName();
    }

    foreach (JsonDbView *view, d->mViews) {
        JsonDbObjectTable *objectTable = view->objectTable();
        fileInfo << objectTable->bdb()->fileName();
        foreach (JsonDbIndex *index, objectTable->indexes()) {
            if (index->bdb())
                fileInfo << index->bdb()->fileName();
        }
    }

    foreach (const QFileInfo &info, fileInfo)
        result.insert(info.fileName(), info.size());
    return result;
}

JsonDbIndexQuery *JsonDbPartitionPrivate::compileIndexQuery(const JsonDbOwner *owner, const JsonDbQuery *query)
{
    Q_Q(JsonDbPartition);

    JsonDbIndexQuery *indexQuery = 0;
    JsonDbQuery *residualQuery = new JsonDbQuery();
    QString orderField;
    QSet<QString> typeNames;
    const QList<JsonDbOrderTerm> &orderTerms = query->orderTerms;
    const QList<JsonDbOrQueryTerm> &orQueryTerms = query->queryTerms;
    QString indexCandidate;
    int indexedQueryTermCount = 0;
    JsonDbObjectTable *table = mObjectTable; //TODO fix me
    JsonDbView *view = 0;
    QList<QString> unindexablePropertyNames; // fields for which we cannot use an index
    if (orQueryTerms.size()) {
        // first pass to find unindexable property names
        for (int i = 0; i < orQueryTerms.size(); i++)
            unindexablePropertyNames.append(orQueryTerms[i].findUnindexablePropertyNames());
        for (int i = 0; i < orQueryTerms.size(); i++) {
            const JsonDbOrQueryTerm orQueryTerm = orQueryTerms[i];
            const QList<QString> &querypropertyNames = orQueryTerm.propertyNames();
            if (querypropertyNames.size() == 1) {
                //QString fieldValue = queryTerm.value().toString();
                QString propertyName = querypropertyNames[0];

                const QList<JsonDbQueryTerm> &queryTerms = orQueryTerm.terms();
                const JsonDbQueryTerm &queryTerm = queryTerms[0];

                if ((typeNames.size() == 1)
                    && mViews.contains(typeNames.toList()[0])) {
                    view = mViews[typeNames.toList()[0]];
                    table = view->objectTable();
                }

                if (table->index(propertyName))
                    indexedQueryTermCount++;
                else if (indexCandidate.isEmpty()
                         && (propertyName != JsonDbString::kTypeStr)
                         && !unindexablePropertyNames.contains(propertyName)) {
                    indexCandidate = propertyName;
                    if (!queryTerm.joinField().isEmpty())
                        indexCandidate = queryTerm.joinPaths()[0].join(QStringLiteral("->"));
                }

                propertyName = queryTerm.propertyName();
                QString fieldValue = queryTerm.value().toString();
                QString op = queryTerm.op();

                if (propertyName == JsonDbString::kTypeStr) {
                    if (op == QLatin1Char('=') || op == QLatin1String("in")) {
                        QSet<QString> types;
                        if (op == QLatin1Char('=')) {
                            types << fieldValue;
                            for (int i = 1; i < queryTerms.size(); ++i) {
                                if (queryTerms[i].propertyName() == JsonDbString::kTypeStr && queryTerms[i].op() == QStringLiteral("="))
                                    types << queryTerms[i].value().toString();
                            }
                        } else {
                            QJsonArray array = queryTerm.value().toArray();
                            types.clear();
                            for (int t = 0; t < array.size(); t++)
                                types << array.at(t).toString();
                        }

                        if (typeNames.count()) {
                            typeNames.intersect(types);
                            if (!typeNames.count()) {
                                // make this a null query -- I really need a domain (partial order) here and not a set
                                typeNames = types;
                            }
                        } else {
                            typeNames = types;
                        }
                    } else if ((op == QLatin1String("!=")) || (op == QLatin1String("notIn"))) {
                        QSet<QString> types;
                        if (op == QLatin1String("!="))
                            types << fieldValue;
                        else {
                            QJsonArray array = queryTerm.value().toArray();
                            for (int t = 0; t < array.size(); t++)
                                types << array.at(t).toString();
                        }
                    }
                }
            }
        }
    }
    if ((typeNames.size() == 1)
        && mViews.contains(typeNames.toList()[0])) {
        view = mViews[typeNames.toList()[0]];
        table = view->objectTable();
    }

    for (int i = 0; i < orderTerms.size(); i++) {
        const JsonDbOrderTerm &orderTerm = orderTerms[i];
        QString propertyName = orderTerm.propertyName;
        if (!table->index(propertyName)) {
            if (jsondbSettings->verbose() || jsondbSettings->performanceLog())
                qDebug() << "Unindexed sort term" << propertyName << orderTerm.ascending;
            residualQuery->orderTerms.append(orderTerm);
            continue;
        }
        if (unindexablePropertyNames.contains(propertyName)) {
            if (jsondbSettings->verbose() || jsondbSettings->performanceLog())
                qDebug() << "Unindexable sort term uses notExists" << propertyName << orderTerm.ascending;
            residualQuery->orderTerms.append(orderTerm);
            continue;
        }
        if (!indexQuery) {
            orderField = propertyName;
            if (view)
                view->updateView();

            JsonDbIndex *index = table->index(propertyName);
            Q_ASSERT(index != 0);
            JsonDbIndexSpec indexSpec = index->indexSpec();
            indexQuery = JsonDbIndexQuery::indexQuery(q, table, propertyName, indexSpec.propertyType,
                                                      owner, orderTerm.ascending);
        } else if (orderField != propertyName) {
            qCritical() << QString::fromLatin1("unimplemented: multiple order terms. Sorting on '%1'").arg(orderField);
            residualQuery->orderTerms.append(orderTerm);
        }
    }

    for (int i = 0; i < orQueryTerms.size(); i++) {
        const JsonDbOrQueryTerm &orQueryTerm = orQueryTerms[i];
        const QList<JsonDbQueryTerm> &queryTerms = orQueryTerm.terms();
        if (queryTerms.size() == 1) {
            JsonDbQueryTerm queryTerm = queryTerms[0];
            QString propertyName = queryTerm.propertyName();
            QString op = queryTerm.op();

            if (!queryTerm.joinField().isEmpty()) {
                residualQuery->queryTerms.append(queryTerm);
                propertyName = queryTerm.joinField();
                op = QStringLiteral("exists");
                queryTerm.setPropertyName(propertyName);
                queryTerm.setOp(op);
                queryTerm.setJoinField(QString());
            }
            if (!table->index(propertyName)
                || (indexQuery
                    && (propertyName != orderField))) {
                if (jsondbSettings->verbose() || jsondbSettings->debug())
                    qDebug() << "residual query term" << propertyName << "orderField" << orderField;
                residualQuery->queryTerms.append(queryTerm);
                continue;
            }

            if (!indexQuery
                && (propertyName != JsonDbString::kTypeStr)
                && table->index(propertyName)
                && !unindexablePropertyNames.contains(propertyName)) {
                orderField = propertyName;
                JsonDbIndexSpec indexSpec = table->index(propertyName)->indexSpec();
                if (view)
                    view->updateView();
                indexQuery = JsonDbIndexQuery::indexQuery(q, table, propertyName, indexSpec.propertyType, owner);
            }

            if (propertyName == orderField) {
                indexQuery->compileOrQueryTerm(queryTerm);
            } else {
                residualQuery->queryTerms.append(orQueryTerm);
            }
        } else {
            residualQuery->queryTerms.append(orQueryTerm);
        }
    }

    if (!indexQuery) {
        QString defaultIndex = JsonDbString::kUuidStr;
        if (typeNames.size()) {
            if ((typeNames.size() == 1)
                && mViews.contains(typeNames.toList()[0])) {
                view = mViews[typeNames.toList()[0]];
                table = view->objectTable();
            } else
                defaultIndex = JsonDbString::kTypeStr;
        }
        if (view)
            view->updateView();
        JsonDbIndex *index = table->index(defaultIndex);
        indexQuery = JsonDbIndexQuery::indexQuery(q, table, defaultIndex, index ? index->indexSpec().propertyType : QString(), owner);
        if (typeNames.size() == 0)
            qCritical() << "searching all objects" << query->query;

        if (defaultIndex == JsonDbString::kTypeStr) {
            foreach (const JsonDbOrQueryTerm &term, orQueryTerms) {
                QList<JsonDbQueryTerm> terms = term.terms();
                if (terms.size() == 1 && terms[0].propertyName() == JsonDbString::kTypeStr) {
                    indexQuery->compileOrQueryTerm(terms[0]);
                    break;
                }
            }
        }
    }
    if (typeNames.count() > 0)
        indexQuery->setTypeNames(typeNames);
    if (residualQuery->queryTerms.size() || residualQuery->orderTerms.size())
        indexQuery->setResidualQuery(residualQuery);
    else
        delete residualQuery;
    indexQuery->setAggregateOperation(query->mAggregateOperation);
    indexQuery->setResultExpressionList(query->mapExpressionList);
    indexQuery->setResultKeyList(query->mapKeyList);
    return indexQuery;
}

void JsonDbPartitionPrivate::doIndexQuery(const JsonDbOwner *owner, JsonDbObjectList &results, int &limit, int &offset,
                                      JsonDbIndexQuery *indexQuery)
{
    if (jsondbSettings->debugQuery())
        qDebug() << "doIndexQuery" << "limit" << limit << "offset" << offset;

    bool countOnly = (indexQuery->aggregateOperation() == QLatin1String("count"));
    int count = 0;
    for (JsonDbObject object = indexQuery->first();
         !object.isEmpty();
         object = indexQuery->next()) {
        if (!owner->isAllowed(object, indexQuery->partition(), QStringLiteral("read")))
            continue;
        if (limit && (offset <= 0)) {
            if (!countOnly) {
                if (jsondbSettings->debugQuery())
                    qDebug() << "appending result" << object << endl;
                JsonDbObject result = indexQuery->resultObject(object);
                results.append(result);
            }
            limit--;
            count++;
        }
        offset--;
        if (limit == 0)
            break;
    }
    if (countOnly) {
        QJsonObject countObject;
        countObject.insert(QLatin1String("count"), count);
        results.append(countObject);
    }
}

JsonDbQueryResult JsonDbPartition::queryObjects(const JsonDbOwner *owner, const JsonDbQuery *query, int limit, int offset)
{
    Q_D(JsonDbPartition);

    JsonDbQueryResult result;

    if (!d->mIsOpen) {
        result.code = JsonDbError::PartitionUnavailable;
        result.message = QStringLiteral("Partition unavailable");
        return result;
    }

    JsonDbObjectList results;
    JsonDbObjectList joinedResults;

    if (!(query->queryTerms.size() || query->orderTerms.size())) {
        result.code = JsonDbError::MissingQuery;
        result.message = QString::fromLatin1("Missing query: %1")
                .arg(query->queryExplanation.join(QStringLiteral("\n")));
        return result;
    }

    QElapsedTimer time;
    time.start();
    JsonDbIndexQuery *indexQuery = d->compileIndexQuery(owner, query);

    int elapsedToCompile = time.elapsed();
    d->doIndexQuery(owner, results, limit, offset, indexQuery);
    int elapsedToQuery = time.elapsed();
    quint32 stateNumber = indexQuery->stateNumber();
    JsonDbQuery *residualQuery = indexQuery->residualQuery();
    if (residualQuery && residualQuery->orderTerms.size()) {
        if (jsondbSettings->verbose())
            qDebug() << "queryPersistentObjects" << "sorting";
        d->sortValues(residualQuery, results, joinedResults);
    }

    QStringList sortKeys;
    sortKeys.append(indexQuery->propertyName());
    sortKeys.append(indexQuery->objectTable()->filename());


    result.data = results;
    result.offset = offset;
    result.state = stateNumber;
    result.sortKeys = sortKeys;
    result.objectTable = indexQuery->objectTable();

    delete indexQuery;

    int elapsedToDone = time.elapsed();
    if (jsondbSettings->verbose())
        qDebug() << "elapsed" << elapsedToCompile << elapsedToQuery << elapsedToDone << query->query;
    return result;
}

JsonDbWriteResult JsonDbPartition::updateObjects(const JsonDbOwner *owner, const JsonDbObjectList &objects, JsonDbPartition::ConflictResolutionMode mode,
                                                 JsonDbUpdateList *changeList)
{
    Q_D(JsonDbPartition);
    JsonDbWriteResult result;

    if (!d->mIsOpen) {
        result.code = JsonDbError::PartitionUnavailable;
        result.message  = QStringLiteral("Partition unavailable");
        return result;
    }
    if (!d->hasSpace()) {
        result.code = JsonDbError::OutOfSpace;
        result.message  = QStringLiteral("Out of space!");
        return result;
    }

    WithTransaction transaction(d);
    QList<JsonDbUpdate> updated;
    QString errorMsg;

    foreach (const JsonDbObject &toUpdate, objects) {
        JsonDbObject object = toUpdate;

        bool forRemoval = object.value(JsonDbString::kDeletedStr).toBool();

        if (!object.contains(JsonDbString::kUuidStr) || object.value(JsonDbString::kUuidStr).isNull()) {
            if (forRemoval) {
                result.code = JsonDbError::MissingUUID;
                result.message = QStringLiteral("Missing '_uuid' field in object");
                return result;
            }

            if (!object.contains(JsonDbString::kOwnerStr)
                || ((object.value(JsonDbString::kOwnerStr).toString() != owner->ownerId())
                    && !owner->isAllowed(object, d->mSpec.name, QStringLiteral("setOwner"))))
                object.insert(JsonDbString::kOwnerStr, owner->ownerId());
            object.generateUuid();
        }

        if (!forRemoval && object.value(JsonDbString::kTypeStr).toString().isEmpty()) {
            result.code = JsonDbError::MissingType;
            result.message = QStringLiteral("Missing '_type' field in object");
            return result;
        }

        if (!(mode == ViewObject || d->checkNaturalObjectType(object, errorMsg))) {
            result.code = JsonDbError::MissingType;
            result.message = errorMsg;
        }
        if (!(forRemoval || d->validateSchema(object.type(), object, errorMsg))) {
            result.code = JsonDbError::FailedSchemaValidation;
            result.message = errorMsg;
            return result;
        }

        JsonDbObjectTable *objectTable = findObjectTable(object.type());

        if (mode != ViewObject) {
            if (d->mViewTypes.contains(object.type())) {
                result.code = JsonDbError::InvalidType;
                result.message = QString::fromLatin1("Cannot write object of view type '%1'").arg(object.value(JsonDbString::kTypeStr).toString());
                return result;
            }

            transaction.addObjectTable(objectTable);
        }

        JsonDbObject master;
        bool knownObject = d->getObject(object.uuid(), master, object.type(), true);
        bool forCreation = !knownObject || master.isDeleted();

        if (mode != ReplicatedWrite && forCreation && forRemoval) {
            result.code =  JsonDbError::MissingObject;
            result.message = QStringLiteral("Cannot remove non-existing object");
            if (object.contains(JsonDbString::kUuidStr))
                result.message.append(QString::fromLatin1(" _uuid %1").arg(object.uuid().toString()));
            if (object.contains(JsonDbString::kTypeStr))
                result.message.append(QString::fromLatin1(" _type %1").arg(object.value(JsonDbString::kTypeStr).toString()));
            return result;
        }

        if (!(forCreation || owner->isAllowed(master, d->mSpec.name, QStringLiteral("write")))) {
            result.code = JsonDbError::OperationNotPermitted;
            result.message = QStringLiteral("Access denied");
            return result;
        }

        if (knownObject && !master.isDeleted() && !master.value(JsonDbString::kOwnerStr).toString().isEmpty())
            object.insert(JsonDbString::kOwnerStr, master.value(JsonDbString::kOwnerStr));
        else if (object.value(JsonDbString::kOwnerStr).toString().isEmpty())
            object.insert(JsonDbString::kOwnerStr, owner->ownerId());

        if (!(forRemoval || owner->isAllowed(object, d->mSpec.name, QStringLiteral("write")))) {
            result.code = JsonDbError::OperationNotPermitted;
            result.message = QStringLiteral("Access denied");
            return result;
        }

        bool validWrite;
        QString versionWritten;
        JsonDbObject oldMaster;
        if (!forCreation)
            oldMaster = master;

        switch (mode) {
        case RejectStale:
            validWrite = master.updateVersionOptimistic(object, &versionWritten);
            break;
        case ViewObject:
            master = object;
            versionWritten = master.computeVersion(false);
            validWrite = true;
            break;
        case Replace:
            master = object;
            versionWritten = master.computeVersion();
            validWrite = true;
            break;
        case ReplicatedWrite:
            validWrite = master.updateVersionReplicating(object);
            versionWritten = master.version();
            break;
        default:
            result.code = JsonDbError::InvalidRequest;
            result.message = QStringLiteral("Missing writeMode implementation.");
            return result;
        }

        if (!validWrite) {
            if (mode == ReplicatedWrite) {
                result.code = JsonDbError::InvalidRequest;
                result.message = QStringLiteral("Replication has reject your update for sanity reasons");
            } else {
                if (jsondbSettings->debug())
                    qDebug() << "Stale update detected - expected version:" << oldMaster.version() << object;
                result.code = JsonDbError::UpdatingStaleVersion;
                result.message = QString(QStringLiteral("Updating stale version of %1 object. Expected version %2, received %3"))
                        .arg(oldMaster.type()).arg(oldMaster.version()).arg(object.version());
            }
            return result;
        }

        // recheck, it might just be a conflict removal
        forRemoval = master.isDeleted();

        if (master.type().isNull())
            master.insert(JsonDbString::kTypeStr, oldMaster.type());

        bool isVisibleWrite = oldMaster.version() != master.version();

        if (isVisibleWrite) {
            JsonDbError::ErrorCode errorCode;
            if (!(forRemoval || (errorCode = d->checkBuiltInTypeValidity(master, oldMaster, errorMsg)) == JsonDbError::NoError)) {
                result.code = errorCode;
                result.message = errorMsg;
                return result;
            } else if (oldMaster.type() == JsonDbString::kSchemaTypeStr &&
                       !d->checkCanRemoveSchema(oldMaster, errorMsg)) {
                result.code = JsonDbError::InvalidSchemaOperation;
                result.message = errorMsg;
                return result;
            } else if ((errorCode = d->checkBuiltInTypeAccessControl(forCreation, owner, master, oldMaster, errorMsg)) != JsonDbError::NoError) {
                result.code = errorCode;
                result.message = errorMsg;
                return result;
            }
        }

        ObjectKey objectKey(master.uuid());

        if (!forCreation)
            objectTable->deindexObject(objectKey, oldMaster, objectTable->stateNumber());

        if (!objectTable->put(objectKey, master)) {
            result.code = JsonDbError::DatabaseError;
            result.message = objectTable->errorMessage();
        }

        if (jsondbSettings->debug())
            qDebug() << "Wrote object" << objectKey << endl << master << endl << oldMaster;

        JsonDbNotification::Action action = JsonDbNotification::Update;
        if (forRemoval)
            action = JsonDbNotification::Remove;
        else if (forCreation)
            action = JsonDbNotification::Create;

        JsonDbUpdate change(oldMaster, master, action);
        quint32 stateNumber = objectTable->storeStateChange(objectKey, change);
        if (changeList)
            changeList->append(change);

        if (!forRemoval)
            objectTable->indexObject(objectKey, master, objectTable->stateNumber());

        d->updateBuiltInTypes(master, oldMaster);

        result.state = stateNumber;


        master.insert(JsonDbString::kVersionStr, versionWritten);
        result.objectsWritten.append(master);
        updated.append(change);
    }

    transaction.commit();

    QMetaObject::invokeMethod(this, "_q_objectsUpdated", Qt::QueuedConnection,
                              Q_ARG(bool, mode == ViewObject), Q_ARG(JsonDbUpdateList, updated));
    return result;
}

JsonDbWriteResult JsonDbPartition::updateObject(const JsonDbOwner *owner, const JsonDbObject &object, JsonDbPartition::ConflictResolutionMode mode, JsonDbUpdateList *changeList)
{
    return updateObjects(owner, JsonDbObjectList() << object, mode, changeList);
}

bool JsonDbPartition::compact()
{
    Q_D(JsonDbPartition);

    if (!d->mIsOpen)
        return false;

    for (QHash<QString,QPointer<JsonDbView> >::const_iterator it = d->mViews.begin();
         it != d->mViews.end();
         ++it) {
        it.value()->objectTable()->compact();
    }
    bool result = true;
    result &= d->mObjectTable->compact();
    return result;
}

JsonDbStat JsonDbPartition::stat() const
{
    Q_D(const JsonDbPartition);

    JsonDbStat result;
    if (d->mIsOpen) {
        for (QHash<QString,QPointer<JsonDbView> >::const_iterator it = d->mViews.begin();
             it != d->mViews.end();
             ++it) {
            result += it.value()->objectTable()->stat();
        }
        result += d->mObjectTable->stat();
    }
    return result;
}

struct QJsonSortable {
    QJsonValue key;
    QJsonObject result;
    QJsonObject joinedResult;
};

bool sortableLessThan(const QJsonSortable &a, const QJsonSortable &b)
{
    return JsonDbIndexQuery::lessThan(a.key, b.key);
}
bool sortableGreaterThan(const QJsonSortable &a, const QJsonSortable &b)
{
    return JsonDbIndexQuery::greaterThan(a.key, b.key);
}

void JsonDbPartitionPrivate::sortValues(const JsonDbQuery *parsedQuery, JsonDbObjectList &results, JsonDbObjectList &joinedResults)
{
    const QList<JsonDbOrderTerm> &orderTerms = parsedQuery->orderTerms;
    if (!orderTerms.size() || (results.size() < 2))
        return;
    const JsonDbOrderTerm &orderTerm0 = orderTerms[0];
    QString field0 = orderTerm0.propertyName;
    bool ascending = orderTerm0.ascending;
    QStringList path0 = field0.split('.');

    if (orderTerms.size() == 1) {
        QVector<QJsonSortable> valuesToSort(results.size());
        int resultsSize = results.size();
        int joinedResultsSize = joinedResults.size();

        for (int i = 0; i < resultsSize; i++) {
            QJsonSortable *p = &valuesToSort[i];
            JsonDbObject r = results.at(i);
            p->key = r.valueByPath(path0);
            p->result = r;
            if (joinedResultsSize > i)
                p->joinedResult = joinedResults.at(i);
        }

        if (ascending)
            qStableSort(valuesToSort.begin(), valuesToSort.end(), sortableLessThan);
        else
            qStableSort(valuesToSort.begin(), valuesToSort.end(), sortableGreaterThan);

        results = JsonDbObjectList();
        joinedResults = JsonDbObjectList();
        for (int i = 0; i < resultsSize; i++) {
            QJsonSortable *p = &valuesToSort[i];
            results.append(p->result);
            if (joinedResultsSize > i)
                joinedResults.append(p->joinedResult);
        }
    } else {
        qCritical() << "Unimplemented: sorting on multiple keys or non-string keys";
    }
}


bool JsonDbPartitionPrivate::checkCanRemoveSchema(const JsonDbObject &schema, QString &message)
{
    Q_Q(JsonDbPartition);

    QString schemaName = schema.value(QStringLiteral("name")).toString();

    // for View types, make sure no Maps or Reduces point at this type
    // the call to getObject will have updated the view, so pending Map or Reduce object removes will be forced
    JsonDbView *view = q->findView(schemaName);
    if (view && view->isActive()) {
        message = QString::fromLatin1("An active view with targetType of %2 exists. You cannot remove the schema").arg(schemaName);
        return false;
    }

    // check if any objects exist
    GetObjectsResult getObjectResponse = getObjects(JsonDbString::kTypeStr, schemaName);

    // for non-View types, if objects exist the schema cannot be removed
    if (!mViewTypes.contains(schemaName)) {
        if (getObjectResponse.data.size() != 0) {
            message = QString::fromLatin1("%1 object(s) of type %2 exist. You cannot remove the schema")
                    .arg(getObjectResponse.data.size())
                    .arg(schemaName);
            return false;
        }
    }

    return true;
}

bool JsonDbPartitionPrivate::validateSchema(const QString &schemaName, const JsonDbObject &object, QString &errorMsg)
{
    errorMsg.clear();

    if (!jsondbSettings->validateSchemas()) {
        if (jsondbSettings->debug())
            qDebug() << "Not validating schemas";
        return true;
    }

    QJsonObject result = mSchemas.validate(schemaName, object);
    if (!result.value(JsonDbString::kCodeStr).isNull()) {
        errorMsg = result.value(JsonDbString::kMessageStr).toString();
        if (jsondbSettings->debug() || jsondbSettings->softValidation())
            qDebug() << "Schema validation error: " << errorMsg << object;
        if (jsondbSettings->softValidation())
            return true;
        return false;
    }

    return true;
}

bool JsonDbPartitionPrivate::checkNaturalObjectType(const JsonDbObject &object, QString &errorMsg)
{
    QString type = object.value(JsonDbString::kTypeStr).toString();
    if (mViewTypes.contains(type)) {
        QByteArray str = QJsonDocument(object).toJson();
        errorMsg = QString::fromLatin1("Cannot create/remove object of view type '%1': '%2'").arg(type).arg(QString::fromUtf8(str));
        return false;
    }

    return true;
}

JsonDbError::ErrorCode JsonDbPartitionPrivate::checkBuiltInTypeAccessControl(bool forCreation, const JsonDbOwner *owner, const JsonDbObject &object, const JsonDbObject &oldObject, QString &errorMsg)
{
    Q_Q(JsonDbPartition);

    if (!jsondbSettings->enforceAccessControl())
        return JsonDbError::NoError;

    QString objectType = object.value(JsonDbString::kTypeStr).toString();
    errorMsg.clear();

    // Access control checks
    if (objectType == JsonDbString::kMapTypeStr ||
            objectType == JsonDbString::kReduceTypeStr) {
        // Check that owner can write targetType
        QJsonValue targetType = object.value(QLatin1String("targetType"));
        JsonDbObject fake; // Just for access control
        fake.insert (JsonDbString::kOwnerStr, object.value(JsonDbString::kOwnerStr));
        fake.insert (JsonDbString::kTypeStr, targetType);
        if (!owner->isAllowed(fake, mSpec.name, QStringLiteral("write"))) {
            errorMsg = QString::fromLatin1("Access denied %1").arg(targetType.toString());
            return JsonDbError::OperationNotPermitted;
        }
        bool forRemoval = object.isDeleted();

        // For removal it is enough to be able to write to targetType
        if (!forRemoval) {
            if (!forCreation) {
                // In update we want to check also the old targetType
                QJsonValue oldTargetType = oldObject.value(QLatin1String("targetType"));
                fake.insert (JsonDbString::kTypeStr, oldTargetType);
                if (!owner->isAllowed(fake, mSpec.name, QStringLiteral("write"))) {
                    errorMsg = QString::fromLatin1("Access denied %1").arg(oldTargetType.toString());
                    return JsonDbError::OperationNotPermitted;
                }
            }
            // For create/update we need to check the read acces to sourceType(s) also
            if (objectType == JsonDbString::kMapTypeStr) {
                QScopedPointer<JsonDbMapDefinition> def(new JsonDbMapDefinition(owner, q, object));
                QStringList sourceTypes = def->sourceTypes();
                for (int i = 0; i < sourceTypes.size(); i++) {
                    fake.insert (JsonDbString::kTypeStr, sourceTypes[i]);
                    if (!owner->isAllowed(fake, mSpec.name, QStringLiteral("read"))) {
                        errorMsg = QString::fromLatin1("Access denied %1").arg(sourceTypes[i]);
                        return JsonDbError::OperationNotPermitted;
                    }
                }
            } else if (objectType == JsonDbString::kReduceTypeStr) {
                QJsonValue sourceType = object.value(QLatin1String("sourceType"));
                fake.insert (JsonDbString::kTypeStr, sourceType);
                if (!owner->isAllowed(fake, mSpec.name, QStringLiteral("read"))) {
                    errorMsg = QString::fromLatin1("Access denied %1").arg(sourceType.toString());
                    return JsonDbError::OperationNotPermitted;
                }
            }
        }
    } else if (objectType == JsonDbString::kSchemaTypeStr) {
        // Check that owner can write name
        QJsonValue name = object.value(JsonDbString::kNameStr);
        JsonDbObject fake; // Just for access control
        fake.insert (JsonDbString::kOwnerStr, object.value(JsonDbString::kOwnerStr));
        fake.insert (JsonDbString::kTypeStr, name);
        if (!owner->isAllowed(fake, mSpec.name, QStringLiteral("write"))) {
            errorMsg = QString::fromLatin1("Access denied %1").arg(name.toString());
            return JsonDbError::OperationNotPermitted;
        }
    } else if (objectType == JsonDbString::kIndexTypeStr) {

        // Only the owner or admin can update Index
        if (!forCreation) {
            QJsonValue oldOwner = oldObject.value(JsonDbString::kOwnerStr);
            if (owner->ownerId() != oldOwner.toString() && !owner->allowAll()) {
                // Only admin (allowAll = true) can update Index:s owned by somebody else
                errorMsg = QString::fromLatin1("Only admin can update Index:s not owned by itself");
                return JsonDbError::OperationNotPermitted;
            }
        }

        // Check that owner can read all objectTypes
        QJsonValue objectTypeProperty = object.value(JsonDbString::kObjectTypeStr);
        JsonDbObject fake; // Just for access control
        if (objectTypeProperty.isUndefined() || objectTypeProperty.isNull()) {
            if (!owner->allowAll()) {
                // Only admin (allowAll = true) can do Index:s without objectType
                errorMsg = QString::fromLatin1("Only admin can do Index:s without objectType");
                return JsonDbError::OperationNotPermitted;
            }
        } else if (objectTypeProperty.isArray()) {
            QJsonArray arr = objectTypeProperty.toArray();
            foreach (QJsonValue val, arr) {
                fake.insert (JsonDbString::kOwnerStr, object.value(JsonDbString::kOwnerStr));
                fake.insert (JsonDbString::kTypeStr, val.toString());
                if (!owner->isAllowed(fake, mSpec.name, QStringLiteral("read"))) {
                    errorMsg = QString::fromLatin1("Access denied %1 in Index %2").arg(val.toString()).
                            arg(JsonDbIndex::determineName(object));
                    return JsonDbError::OperationNotPermitted;
                }
            }
        } else if (objectTypeProperty.isString()) {
            fake.insert (JsonDbString::kOwnerStr, object.value(JsonDbString::kOwnerStr));
            fake.insert (JsonDbString::kTypeStr, objectTypeProperty.toString());
            if (!owner->isAllowed(fake, mSpec.name, QStringLiteral("read"))) {
                errorMsg = QString::fromLatin1("Access denied %1 in Index %2").arg(objectTypeProperty.toString()).
                        arg(JsonDbIndex::determineName(object));
                return JsonDbError::OperationNotPermitted;
            }
        } else {
            errorMsg = QString::fromLatin1("Invalid objectType in Index %1").arg(JsonDbIndex::determineName(object));
            return JsonDbError::InvalidIndexOperation;
        }
    }
    return JsonDbError::NoError;
}

JsonDbError::ErrorCode JsonDbPartitionPrivate::checkBuiltInTypeValidity(const JsonDbObject &object, const JsonDbObject &oldObject, QString &errorMsg)
{
    Q_Q(JsonDbPartition);

    QString objectType = object.value(JsonDbString::kTypeStr).toString();
    errorMsg.clear();

    if (objectType == JsonDbString::kSchemaTypeStr &&
            !checkCanAddSchema(object, oldObject, errorMsg))
        return JsonDbError::InvalidSchemaOperation;
    else if (objectType == JsonDbString::kMapTypeStr &&
             !JsonDbMapDefinition::validateDefinition(object, q, errorMsg))
        return JsonDbError::InvalidMap;
    else if (objectType == JsonDbString::kReduceTypeStr &&
             !JsonDbReduceDefinition::validateDefinition(object, q, errorMsg))
        return JsonDbError::InvalidReduce;
    else if (objectType == JsonDbString::kIndexTypeStr && !JsonDbIndex::validateIndex(object, oldObject, errorMsg))
        return JsonDbError::InvalidIndexOperation;

    return JsonDbError::NoError;
}

void JsonDbPartitionPrivate::updateBuiltInTypes(const JsonDbObject &object, const JsonDbObject &oldObject)
{
    Q_Q(JsonDbPartition);

    if (oldObject.type() == JsonDbString::kIndexTypeStr) {
        QString indexName = JsonDbIndex::determineName(oldObject);
        removeIndex(indexName, oldObject.value(JsonDbString::kObjectTypeStr).toString());
    }

    if (object.type() == JsonDbString::kIndexTypeStr && !object.isDeleted())
        addIndex(JsonDbIndexSpec::fromIndexObject(object));

    if (oldObject.type() == JsonDbString::kSchemaTypeStr)
        removeSchema(oldObject.value(JsonDbString::kNameStr).toString());

    if (object.type() == JsonDbString::kSchemaTypeStr &&
        object.value(JsonDbString::kSchemaStr).type() == QJsonValue::Object
        && !object.isDeleted())
        setSchema(object.value(JsonDbString::kNameStr).toString(), object.value(JsonDbString::kSchemaStr).toObject());

    if (!oldObject.isEmpty()
        && (oldObject.type() == JsonDbString::kMapTypeStr || oldObject.type() == JsonDbString::kReduceTypeStr))
        JsonDbView::removeDefinition(q, oldObject);

    if (!object.isDeleted()
        && (object.type() == JsonDbString::kMapTypeStr || object.type() == JsonDbString::kReduceTypeStr)
        && !(object.contains(JsonDbString::kActiveStr) && !object.value(JsonDbString::kActiveStr).toBool()))
        JsonDbView::createDefinition(q, object);
}

void JsonDbPartitionPrivate::setSchema(const QString &schemaName, const QJsonObject &schema)
{
    if (jsondbSettings->verbose())
        qDebug() << "setSchema" << schemaName << schema;

    QJsonObject errors = mSchemas.insert(schemaName, schema);

    if (!errors.isEmpty()) {
        qWarning() << "setSchema failed because of errors" << schemaName << schema;
        qWarning() << errors;
        // FIXME should we accept broken schemas?
    }

    if (schema.contains(QStringLiteral("extends"))) {
        QJsonValue extendsValue = schema.value(QStringLiteral("extends"));
        QString extendedSchemaName;
        if (extendsValue.type() == QJsonValue::String)
            extendedSchemaName = extendsValue.toString();
        else if ((extendsValue.type() == QJsonValue::Object)
                 && extendsValue.toObject().contains(QStringLiteral("$ref")))
            extendedSchemaName = extendsValue.toObject().value(QStringLiteral("$ref")).toString();
        if (extendedSchemaName == JsonDbString::kViewTypeStr) {
            mViewTypes.insert(schemaName);
            addView(schemaName);
            if (jsondbSettings->verbose())
                qDebug() << "viewTypes" << mViewTypes;
        }
    }
    if (schema.contains(QStringLiteral("properties")))
        updateSchemaIndexes(schemaName, schema);
}

void JsonDbPartitionPrivate::removeSchema(const QString &schemaName)
{
    if (jsondbSettings->verbose())
        qDebug() << "removeSchema" << schemaName;

    if (mSchemas.contains(schemaName)) {
        QJsonObject schema = mSchemas.take(schemaName);

        if (schema.contains(QStringLiteral("extends"))) {
            QJsonValue extendsValue = schema.value(QStringLiteral("extends"));
            QString extendedSchemaName;
            if (extendsValue.type() == QJsonValue::String)
                extendedSchemaName = extendsValue.toString();
            else if ((extendsValue.type() == QJsonValue::Object)
                     && extendsValue.toObject().contains(QStringLiteral("$ref")))
                extendedSchemaName = extendsValue.toObject().value(QStringLiteral("$ref")).toString();

            if (extendedSchemaName == JsonDbString::kViewTypeStr) {
                mViewTypes.remove(schemaName);
                removeView(schemaName);
            }
        }
    }
}

void JsonDbPartitionPrivate::updateSchemaIndexes(const QString &schemaName, QJsonObject object, const QStringList &path)
{
    QJsonObject properties = object.value(QStringLiteral("properties")).toObject();
    const QList<QString> keys = properties.keys();
    for (int i = 0; i < keys.size(); i++) {
        const QString &k = keys[i];
        QJsonObject propertyInfo = properties.value(k).toObject();
        if (propertyInfo.contains(QStringLiteral("properties")))
            updateSchemaIndexes(schemaName, propertyInfo, path + (QStringList() << k));
    }
}

void JsonDbPartitionPrivate::updateSpaceStatus()
{
    mDiskSpaceStatus = JsonDbPartition::UnknownStatus;
    struct statvfs status;

    QByteArray local8BitPath = mFilename.toLocal8Bit();
    const char *path = local8BitPath.constData();
    if (statvfs(path, &status) < 0) {
        qCritical() << "statvfs failed" << mFilename;
        return;
    }
    qint64 available = status.f_bsize * status.f_bavail;
    if (available < jsondbSettings->minimumRequiredSpace()) {
        qWarning() << "out of space";
        mDiskSpaceStatus = JsonDbPartition::OutOfSpace;
        return;
    }
    mDiskSpaceStatus = JsonDbPartition::HasSpace;
}

bool JsonDbPartitionPrivate::hasSpace()
{
    if (mDiskSpaceStatus == JsonDbPartition::HasSpace)
        return true;
    updateSpaceStatus();
    return (mDiskSpaceStatus == JsonDbPartition::HasSpace);
}

bool WithTransaction::addObjectTable(JsonDbObjectTable *table)
{
    if (!mPartition)
        return false;
    if (!mPartition->mTableTransactions.contains(table)) {
        bool ok = table->begin();
        mPartition->mTableTransactions.append(table);
        return ok;
    }
    return true;
}


void JsonDbPartitionPrivate::initSchemas()
{
    Q_Q(JsonDbPartition);

    if (jsondbSettings->verbose())
        qDebug() << "initSchemas";
    {
        JsonDbObjectList schemas = getObjects(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr,
                                                QString()).data;
        for (int i = 0; i < schemas.size(); ++i) {
            JsonDbObject schemaObject = schemas.at(i);
            QString schemaName = schemaObject.value(QStringLiteral("name")).toString();
            QJsonObject schema = schemaObject.value(QStringLiteral("schema")).toObject();
            setSchema(schemaName, schema);
        }
    }

    foreach (const QString &schemaName, (QStringList() << JsonDbString::kNotificationTypeStr << JsonDbString::kViewTypeStr
                                         << JsonDbString::kCapabilityTypeStr << JsonDbString::kIndexTypeStr)) {
        if (!mSchemas.contains(schemaName)) {
            QFile schemaFile(QString::fromLatin1(":schema/%1.json").arg(schemaName));
            schemaFile.open(QIODevice::ReadOnly);
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(schemaFile.readAll(), &error);
            schemaFile.close();
            if (doc.isNull()) {
                qWarning() << "Parsing " << schemaName << " schema" << error.error;
                return;
            }
            QJsonObject schema = doc.object();
            JsonDbObject schemaObject;
            schemaObject.insert(JsonDbString::kTypeStr, JsonDbString::kSchemaTypeStr);
            schemaObject.insert(QStringLiteral("name"), schemaName);
            schemaObject.insert(QStringLiteral("schema"), schema);
            q->updateObject(mDefaultOwner, schemaObject, JsonDbPartition::Replace);
        }
    }
    const QString capabilityNameIndexUuid = QStringLiteral("{7853c8fb-cf23-453f-b9c4-804c6a0a38c6}");
    JsonDbObject capabilityNameIndex;
    if (!getObject(capabilityNameIndexUuid, capabilityNameIndex)) {
        JsonDbObject nameIndex;
        nameIndex.insert(JsonDbString::kUuidStr, capabilityNameIndexUuid);
        nameIndex.insert(JsonDbString::kTypeStr, JsonDbString::kIndexTypeStr);
        nameIndex.insert(JsonDbString::kNameStr, QLatin1String("capabilityName"));
        nameIndex.insert(JsonDbString::kPropertyNameStr, QLatin1String("name"));
        nameIndex.insert(JsonDbString::kPropertyTypeStr, QLatin1String("string"));
        nameIndex.insert(JsonDbString::kObjectTypeStr, JsonDbString::kCapabilityTypeStr);
        q->updateObject(mDefaultOwner, nameIndex, JsonDbPartition::Replace);

        QFile capabilityFile(QStringLiteral(":schema/RootCapability.json"));
        capabilityFile.open(QIODevice::ReadOnly);
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(capabilityFile.readAll(), &error);
        capabilityFile.close();
        if (doc.isNull()) {
            qWarning() << "Parsing root capability" << error.error;
            return;
        }
        JsonDbObject capability = doc.object();
        QString name = capability.value(QStringLiteral("name")).toString();
        GetObjectsResult getObjectResponse = getObjects(QStringLiteral("capabilityName"), name, JsonDbString::kCapabilityTypeStr);
        int count = getObjectResponse.data.size();
        if (!count) {
            if (jsondbSettings->verbose())
                qDebug() << "Creating capability" << capability;
            q->updateObject(mDefaultOwner, capability);
        } else {
            JsonDbObject currentCapability = getObjectResponse.data.at(0);
            if (currentCapability.value(QStringLiteral("accessRules")) != capability.value(QStringLiteral("accessRules")))
                q->updateObject(mDefaultOwner, capability);
        }
    }
}

#include "moc_jsondbpartition.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
