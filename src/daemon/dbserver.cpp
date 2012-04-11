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

#include <QtNetwork>
#include <QDir>
#include <QElapsedTimer>

#include "jsondbstrings.h"
#include "jsondberrors.h"

#include "jsondbephemeralpartition.h"
#include "jsondbindexquery.h"
#include "jsondbobjecttable.h"
#include "jsondbsettings.h"
#include "jsondbview.h"
#include "dbserver.h"

#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <pwd.h>
#include <errno.h>
#endif

QT_USE_NAMESPACE_JSONDB_PARTITION

static const int gReadBufferSize = 65536;

static QJsonObject createError( JsonDbError::ErrorCode code, const QString& message )
{
    QJsonObject map;
    map.insert(JsonDbString::kResultStr, QJsonValue());
    QJsonObject errormap;
    errormap.insert(JsonDbString::kCodeStr, (int)code);
    errormap.insert(JsonDbString::kMessageStr, message);
    map.insert( JsonDbString::kErrorStr, errormap );
    return map;
}

static void sendError( JsonStream *stream, JsonDbError::ErrorCode code,
                       const QString& message, int id )
{
    QJsonObject map;
    map.insert( JsonDbString::kResultStr, QJsonValue());
    QJsonObject errormap;
    errormap.insert(JsonDbString::kCodeStr, code);
    errormap.insert(JsonDbString::kMessageStr, message);
    map.insert( JsonDbString::kErrorStr, errormap );
    map.insert( JsonDbString::kIdStr, id );

    if (jsondbSettings->debug())
        qDebug() << "Sending error" << map;
    stream->send(map);
}

DBServer::DBServer(const QString &searchPath, QObject *parent) :
    QObject(parent)
  , mDefaultPartition(0)
  , mEphemeralPartition(0)
  , mTcpServerPort(0)
  , mServer(0)
  , mTcpServer(0)
  , mOwner(new JsonDbOwner(this))
{
    // for queued connection handling
    qRegisterMetaType<JsonDbPartition*>("JsonDbPartition*");
    qRegisterMetaType<QSet<QString> >("QSet<QString>");
    qRegisterMetaType<JsonDbUpdateList>("JsonDbUpdateList");

    // make the user-specified path (or PWD) the first in the search path, then and
    // the /etc one the last
    QStringList searchPaths = jsondbSettings->configSearchPath();

    if (searchPath.isEmpty())
        searchPaths.prepend(QDir::currentPath());
    else
        searchPaths.prepend(searchPath);

    if (!searchPaths.contains(QLatin1String("/etc/jsondb")))
        searchPaths.append(QLatin1String("/etc/jsondb"));

    // get the unique set of paths and use QDir to ensure
    // that we have the canonical form of the path
    QStringList uniquePaths;
    foreach (const QString &path, searchPaths) {
        QString canonicalPath = QDir(path).canonicalPath();
        if (!(canonicalPath.isEmpty() || uniquePaths.contains(canonicalPath)))
            uniquePaths.append(canonicalPath);
    }

    jsondbSettings->setConfigSearchPath(uniquePaths);

    mOwner->setAllowAll(true);
}

DBServer::~DBServer()
{
    close();
}

void DBServer::clearNotifications()
{
    QMapIterator<QString,JsonDbNotification*> mi(mNotificationMap);
    while (mi.hasNext())
        delete mi.next().value();

    mNotificationMap.clear();
    mNotifications.clear();
    mKeyedNotifications.clear();
}

void DBServer::sigHUP()
{
    if (jsondbSettings->debug())
        qDebug() << "SIGHUP received";
    loadPartitions();
    reduceMemoryUsage();
}

void DBServer::sigTerm()
{
    if (jsondbSettings->debug())
        qDebug() << "SIGTERM received";
    close();
}

void DBServer::sigINT()
{
    if (jsondbSettings->debug())
        qDebug() << "SIGINT received";
    close();
}

bool DBServer::socket()
{
    QString socketName = ::getenv("JSONDB_SOCKET");
    if (socketName.isEmpty())
        socketName = "qt5jsondb";
    if (jsondbSettings->debug())
        qDebug() << "Listening on socket" << socketName;
    QLocalServer::removeServer(socketName);
    mServer = new QLocalServer(this);
    connect(mServer, SIGNAL(newConnection()), this, SLOT(handleConnection()));
    if (mServer->listen(socketName))
        QFile::setPermissions(mServer->fullServerName(), QFile::ReadUser | QFile::WriteUser | QFile::ExeUser |
                                                        QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
                                                        QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
    else
        qCritical() << "DBServer::start - Unable to open" << socketName << "socket";

    if (mTcpServerPort) {
        mTcpServer = new QTcpServer(this);
        connect(mTcpServer, SIGNAL(newConnection()), this, SLOT(handleTcpConnection()));
        if (!mTcpServer->listen(QHostAddress::Any, mTcpServerPort))
            qCritical() << QString("Unable to open jsondb remote socket on port %1").arg(mTcpServerPort);
    }
    return true;
}

bool DBServer::start()
{
    QElapsedTimer timer;
    if (jsondbSettings->performanceLog())
        timer.start();

    QString username;
#if defined(Q_OS_UNIX)
    struct passwd *pwdent = ::getpwent();
    username = QString::fromLocal8Bit(pwdent->pw_name);
#else
    username = QStringLiteral("com.example.JsonDb");
#endif

    mOwner->setOwnerId(username);

    if (!loadPartitions()) {
        qCritical() << "DBServer::start - Unable to open database";
        return false;
    }

    if (jsondbSettings->performanceLog()) {
        JsonDbStat stats = stat();
        qDebug().nospace() << "+ JsonDB Perf: " << "[action]" << "start" << "[action]:[ms]" << timer.elapsed()
                           << "[ms]:[reads]" << stats.reads << "[reads]:[hits]" << stats.hits
                           << "[hits]:[writes]" << stats.writes << "[writes]";
    }

    return true;
}

bool DBServer::clear()
{
    bool fail = false;
    foreach (JsonDbPartition *partition, mPartitions.values())
        fail = !partition->clear() || fail;
    return !fail;
}

void DBServer::close()
{
    foreach (JsonDbPartition *partition, mPartitions.values()) {
        if (mCompactOnClose)
            partition->compact();
        partition->close();
    }
    clearNotifications();
    QCoreApplication::exit();
}

bool DBServer::loadPartitions()
{
    if (!mEphemeralPartition) {
        mEphemeralPartition = new JsonDbEphemeralPartition("Ephemeral", this);
        connect(mEphemeralPartition, SIGNAL(objectsUpdated(JsonDbUpdateList)),
                this, SLOT(objectsUpdated(JsonDbUpdateList)));
    }

    QHash<QString, JsonDbPartition*> partitions;
    QList<QJsonObject> definitions = findPartitionDefinitions();
    QString defaultPartitionName;

    foreach (const QJsonObject &definition, definitions) {
        QString name = definition.value(JsonDbString::kNameStr).toString();

        if (definition.value(JsonDbString::kDefaultStr).toBool() && defaultPartitionName.isEmpty())
            defaultPartitionName = name;

        if (mPartitions.contains(name)) {
            partitions[name] = mPartitions.take(name);
        } else {

            if (partitions.contains(name)) {
                qWarning() << "Duplicate partition name:" << name;
                continue;
            }

            QString path = definition.value(JsonDbString::kPathStr).toString();
            QDir pathDir(path);
            pathDir.mkpath(QLatin1String("."));

            JsonDbPartition *partition = new JsonDbPartition(pathDir.absoluteFilePath(name), name, mOwner, this);
            partitions[name] = partition;
            connect(partition, SIGNAL(objectsUpdated(JsonDbUpdateList)), this, SLOT(objectsUpdated(JsonDbUpdateList)));

            // TODO: for removable partitions, this shouldn't cause a total failure
            if (!partition->open()) {
                close();
                return false;
            }

            // create an object in the Ephemeral partition to reflect this partition
            JsonDbObject partitionRecord(definition);
            partitionRecord.insert(JsonDbString::kUuidStr, JsonDbObject::createUuidFromString(name).toString());
            partitionRecord.insert(JsonDbString::kTypeStr, JsonDbString::kPartitionTypeStr);
            mEphemeralPartition->updateObjects(mOwner, JsonDbObjectList() << partitionRecord, JsonDbPartition::ForcedWrite);
        }
    }

    // close any partitions that were declared previously but are no longer present
    foreach (JsonDbPartition *partition, mPartitions.values()) {

        if (mDefaultPartition == partition)
            mDefaultPartition = 0;

        QList<JsonDbObject> toRemove;

        // remove the ephemeral object representing this partition
        JsonDbObject partitionRecord;
        partitionRecord.insert(JsonDbString::kUuidStr, JsonDbObject::createUuidFromString(partition->name()).toString());
        partitionRecord.insert(JsonDbString::kTypeStr, JsonDbString::kPartitionTypeStr);
        partitionRecord.markDeleted();
        toRemove.append(partitionRecord);

        // remove any notifications for the partition being closed
        QJsonObject bindings;
        bindings.insert(QLatin1String("notification"), JsonDbString::kNotificationTypeStr);
        bindings.insert(QLatin1String("partition"), partition->name());

        QScopedPointer<JsonDbQuery> query(JsonDbQuery::parse(QLatin1String("[?_type=%notification][?partition=%partition]"),
                                                             bindings));
        JsonDbQueryResult results = mEphemeralPartition->queryObjects(mOwner, query.data());
        foreach (const JsonDbObject &result, results.data) {
            JsonDbObject notification = result;
            notification.markDeleted();
            toRemove.append(notification);
        }

        mEphemeralPartition->updateObjects(mOwner, toRemove, JsonDbPartition::ForcedWrite);

        disconnect(partition, SIGNAL(objectsUpdated(JsonDbUpdateList)), this, SLOT(objectsUpdated(JsonDbUpdateList)));
        partition->close();
        delete partition;
    }

    mPartitions = partitions;

    if (!mDefaultPartition)
        mDefaultPartition = mPartitions[defaultPartitionName];

    return true;
}

void DBServer::reduceMemoryUsage()
{
    foreach (JsonDbPartition *partition, mPartitions.values())
        partition->flushCaches();
}

JsonDbStat DBServer::stat() const
{
    JsonDbStat result;
    foreach (JsonDbPartition *partition, mPartitions.values())
        result += partition->stat();
    return result;
}

void DBServer::handleConnection()
{
    if (QIODevice *connection = mServer->nextPendingConnection()) {
        qobject_cast<QLocalSocket *>(connection)->setReadBufferSize(gReadBufferSize);
        if (jsondbSettings->debug())
            qDebug() << "client connected to jsondb server" << connection;
        connect(connection, SIGNAL(disconnected()), this, SLOT(removeConnection()));
        JsonStream *stream = new JsonStream(this);
        stream->setDevice(connection);
        connect(stream, SIGNAL(receive(QJsonObject)), this,
                SLOT(receiveMessage(QJsonObject)));
        mConnections.insert(connection, stream);
    }
}

void DBServer::handleTcpConnection()
{
    if (QTcpSocket *connection = mTcpServer->nextPendingConnection()) {
        if (jsondbSettings->debug())
            qDebug() << "remote client connected to jsondb server" << connection;
        connect(connection, SIGNAL(disconnected()), this, SLOT(removeConnection()));
        JsonStream *stream = new JsonStream(this);
        stream->setDevice(connection);
        connect(stream, SIGNAL(receive(QJsonObject)), this,
                SLOT(receiveMessage(QJsonObject)));
        mConnections.insert(connection, stream);
    }
}

JsonDbOwner *DBServer::getOwner(JsonStream *stream)
{
    QIODevice *device = stream->device();

    if (!(jsondbSettings->enforceAccessControl() || mOwners.contains(stream->device()))) {
        // We are not enforcing policies here, allow requests
        // from all applications.
        mOwners[device] = createDummyOwner(stream);
    }

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    if (!mOwners.contains(device)) {
        ucred peercred;
        socklen_t peercredlen = sizeof peercred;
        int s = 0;
        QLocalSocket *sock = qobject_cast<QLocalSocket *>(stream->device());
        if (sock) {
            s = sock->socketDescriptor();
        } else {
            QAbstractSocket *sock = qobject_cast<QAbstractSocket *>(stream->device());
            if (sock)
                s = sock->socketDescriptor();
        }
        if (s <= 0) {
            qWarning() << Q_FUNC_INFO << "socketDescriptor () does not return a valid descriptor.";
            return 0;
        }
        if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &peercred, &peercredlen)) {
            qWarning() << Q_FUNC_INFO << "getsockopt(...SO_PEERCRED...) failed" << ::strerror(errno) << errno;
            return 0;
        }

        QScopedPointer<JsonDbOwner> owner(new JsonDbOwner(this));
        if (owner->setOwnerCapabilities (peercred.uid, mDefaultPartition))
            mOwners[stream->device()] = owner.take();
        else return 0;
    }
#else
    // security hole
    // TODO: Mac socket authentication
    if (!mOwners.contains(device)) {
        JsonDbOwner *owner = new JsonDbOwner(this);
        owner->setOwnerId(QString::fromLatin1("unknown app %1").arg((intptr_t)stream));
        owner->setDomain("unknown app domain");
        owner->setAllowAll(true);
        owner->setStorageQuota(-1);
        mOwners[device] = owner;
    }
#endif
    Q_ASSERT(mOwners[device]);
    return mOwners[device];
}

/*!
    This will create a dummy owner. Used only when the policies are not enforced.
    When policies are not enforced the JsonDbOwner allows access to everybody.
*/
JsonDbOwner *DBServer::createDummyOwner( JsonStream *stream)
{
    if (jsondbSettings->enforceAccessControl())
        return 0;

    JsonDbOwner *owner = mOwners.value(stream->device());
    if (owner)
        return owner;

    owner = new JsonDbOwner(this);
    owner->setOwnerId(QString("unknown app"));
    owner->setDomain(QString("unknown domain"));
    mOwners[stream->device()] = owner;

    return mOwners[stream->device()];
}

void DBServer::notified(const QString &notificationId, quint32 stateNumber, const QJsonObject &object, const QString &action)
{
    if (jsondbSettings->debug())
        qDebug() << "notificationId" << notificationId << "object" << object;
    JsonStream *stream = mNotifications.value(notificationId);
    // if the notified signal() is delivered after the notification has been deleted,
    // then there is no stream to send to
    if (!stream)
        return;

    QJsonObject map, obj;
    obj.insert( JsonDbString::kObjectStr, object );
    obj.insert( JsonDbString::kActionStr, action );
    obj.insert( JsonDbString::kStateNumberStr, static_cast<int>(stateNumber));
    map.insert( JsonDbString::kNotifyStr, obj );
    map.insert( JsonDbString::kUuidStr, notificationId );

    if (stream && stream->device() && stream->device()->isWritable()) {
        if (jsondbSettings->debug())
            qDebug() << "Sending notify" << map;
        stream->send(map);
    } else {
        mNotifications.remove(notificationId);
        if (jsondbSettings->debug())
            qDebug() << "Invalid stream" << static_cast<void*>(stream);
    }
}

void DBServer::objectsUpdated(const QList<JsonDbUpdate> &objects)
{
    QString partitionName;
    JsonDbPartition *partition = 0;
    QList<JsonDbUpdate> updatesToEagerViews;
    QSet<QString> eagerViewTypes;
    bool isViewUpdate = false;

    if (sender() == mEphemeralPartition) {
        partitionName = mEphemeralPartition->name();
    } else {
        partition = qobject_cast<JsonDbPartition*>(sender());
        if (partition)
            partitionName = partition->name();
        else
            return;
    }
    quint32 partitionStateNumber = 0;
    if (partition)
        partitionStateNumber = partition->mainObjectTable()->stateNumber();
    else if (mDefaultPartition)
        partitionStateNumber =  mDefaultPartition->mainObjectTable()->stateNumber();

    if (jsondbSettings->debug())
        qDebug() << "objectsUpdated" << partitionName << partitionStateNumber;

    // FIXME: pretty good place to batch notifications
    foreach (const JsonDbUpdate &updated, objects) {

        JsonDbObject oldObject = updated.oldObject;
        JsonDbObject object = updated.newObject;
        JsonDbNotification::Action action = updated.action;

        // no notifications on notification
        if (object.type() == JsonDbString::kNotificationTypeStr)
            continue;

        QString oldObjectType = oldObject.value(JsonDbString::kTypeStr).toString();
        QString objectType = object.value(JsonDbString::kTypeStr).toString();
        quint32 stateNumber = 0;
        if (partition) {
            JsonDbObjectTable *objectTable = partition->findObjectTable(objectType);
            stateNumber = objectTable->stateNumber();
        } else if (partitionName != mEphemeralPartition->name())
            stateNumber = mDefaultPartition->mainObjectTable()->stateNumber();

        QStringList notificationKeys;
        if (object.contains(JsonDbString::kTypeStr)) {
            notificationKeys << objectType;

            // eagerly update views if this object that was created isn't a view type itself
            if (partition) {
                WeightedSourceViewGraph &sourceViewGraph = mEagerViewSourceGraph[partitionName];
                if (jsondbSettings->verbose()) qDebug() << "objectType" << objectType << sourceViewGraph.contains(oldObjectType) << sourceViewGraph.contains(objectType);
                if (partition->findView(objectType)) {
                    if (jsondbSettings->verbose()) qDebug() << "isViewUpdate" << objectType;
                    isViewUpdate = true;
                } else if ((sourceViewGraph.contains(oldObjectType))
                           || (sourceViewGraph.contains(objectType))) {
                    JsonDbUpdateList updateList;
                    if (oldObjectType == objectType) {
                        updateList.append(updated);
                    } else {
                        JsonDbObject tombstone(oldObject);
                        tombstone.insert(QLatin1String("_deleted"), true);
                        updateList.append(JsonDbUpdate(oldObject, tombstone, JsonDbNotification::Delete));
                        updateList.append(JsonDbUpdate(JsonDbObject(), object, JsonDbNotification::Create));
                    }
                    foreach (const JsonDbUpdate &splitUpdate, updateList) {
                        const QString updatedObjectType = splitUpdate.newObject.type();
                        const ViewEdgeWeights &edgeWeights = sourceViewGraph[updatedObjectType];
                        for (ViewEdgeWeights::const_iterator it = edgeWeights.begin(); it != edgeWeights.end(); ++it) {
                            if (jsondbSettings->verbose()) qDebug() << "edge weight" << updatedObjectType << it.key() << it.value().count;
                            if (it.value() > 0) {
                                eagerViewTypes.insert(it.key());
                                if (sourceViewGraph.contains(updatedObjectType))
                                    updatesToEagerViews.append(splitUpdate);
                            }
                        }
                    }
                }
            }
        }

        if (object.contains(JsonDbString::kUuidStr))
            notificationKeys << object.value(JsonDbString::kUuidStr).toString();
        notificationKeys << "__generic_notification__";

        QHash<QString, JsonDbObject> objectCache;
        for (int i = 0; i < notificationKeys.size(); i++) {
            QString key = notificationKeys[i];
            for (QMultiMap<QString, JsonDbNotification *>::const_iterator it = mKeyedNotifications.find(key);
                 (it != mKeyedNotifications.end()) && (it.key() == key);
                 ++it) {
                JsonDbNotification *n = it.value();
                if (jsondbSettings->debug())
                    qDebug() << "Notification" << n->query() << n->actions();
                objectUpdated(partitionName, stateNumber, n, action, oldObject, object);
            }
        }
    }

    if (isViewUpdate)
        return;
    if (updatesToEagerViews.isEmpty()) {
        updateEagerViewStateNumbers(partition ? partition : mDefaultPartition, partitionStateNumber);
        emitStateChanged(partition);
    } else {
        QMetaObject::invokeMethod(this, "updateEagerViews", Qt::QueuedConnection,
                                  Q_ARG(JsonDbPartition*, partition),
                                  Q_ARG(QSet<QString>, eagerViewTypes),
                                  Q_ARG(JsonDbUpdateList, updatesToEagerViews));
    }
}

// Updates the in-memory state numbers on each view so that we know it
// has seen all relevant updates from this transaction
void DBServer::updateEagerViewStateNumbers(JsonDbPartition *partition, quint32 partitionStateNumber)
{
    if (!partition)
        return;
    if (jsondbSettings->verbose())
        qDebug() << "updateEagerViewStateNumbers" << (partition ? partition->name() : "no partition") << partitionStateNumber << "{";
    const QString &partitionName = partition->name();
    WeightedSourceViewGraph &sourceViewGraph = mEagerViewSourceGraph[partitionName];
    foreach (const ViewEdgeWeights &edgeWeights, sourceViewGraph) {
        for (ViewEdgeWeights::const_iterator jt = edgeWeights.begin(); jt != edgeWeights.end(); ++jt) {
            const QString &viewType = jt.key();
            if (jt.value() == 0)
                continue;
            JsonDbView *view = partition->findView(viewType);
            if (view)
                view->updateViewStateNumber(partitionStateNumber);
            else
                if (jsondbSettings->debug())
                    qCritical() << "no view for" << viewType << partition->name();
        }
    }
    if (jsondbSettings->verbose())
        qDebug() << "updateEagerViewStateNumbers" << (partition ? partition->name() : "no partition") << partitionStateNumber << "}";
}

void DBServer::emitStateChanged(JsonDbPartition *partition)
{
    if (!partition)
        return;
    quint32 lastStateNumber = partition->mainObjectTable()->stateNumber();
    QJsonObject stateChange;
    stateChange.insert("_state", static_cast<int>(lastStateNumber));
    foreach (const JsonDbNotification *n, mNotificationMap) {
        if (n->lastStateNumber() == lastStateNumber
            && n->partition() == partition->name())
            emit notified(n->uuid(), lastStateNumber, stateChange, "stateChange");
    }
}

void DBServer::updateEagerViews(JsonDbPartition *partition, QSet<QString> viewTypes, QList<JsonDbUpdate> changeList)
{
    QSet<QString> viewsUpdated;

    if (jsondbSettings->verbose())
        qDebug() << "updateEagerViews {" << partition->mainObjectTable()->stateNumber() << viewTypes;
    quint32 partitionStateNumber = partition->mainObjectTable()->stateNumber();
    const QString &partitionName = partition->name();
    WeightedSourceViewGraph &sourceViewGraph = mEagerViewSourceGraph[partitionName];

    while (!viewTypes.isEmpty()) {
        bool madeProgress = false;
        foreach (const QString &targetType, viewTypes) {
            JsonDbView *view = partition->findView(targetType);
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
            if (jsondbSettings->verbose())
                qDebug() << "updated view" << targetType << additionalChanges.size() << additionalChanges;
            changeList.append(additionalChanges);
            viewsUpdated.insert(targetType);
            // if this triggers other eager types, we need to update that also
            if (sourceViewGraph.contains(targetType)) {
                const ViewEdgeWeights &edgeWeights = sourceViewGraph[targetType];
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

    updateEagerViewStateNumbers(partition, partitionStateNumber);
    emitStateChanged(partition);

    if (jsondbSettings->verbose())
        qDebug() << "updateEagerViews }" << partition->mainObjectTable()->stateNumber() << viewsUpdated;
}

void DBServer::objectUpdated(const QString &partitionName, quint32 stateNumber, JsonDbNotification *n,
                             JsonDbNotification::Action action, const JsonDbObject &oldObject, const JsonDbObject &object)
{
    JsonDbNotification::Action effectiveAction = action;
    if (n->partition() == partitionName) {
        JsonDbObject r;
        if (!n->query().isEmpty()) {
            if (jsondbSettings->debug())
                qDebug() << "Checking notification" << n->query() << endl
                         << "    for object" << object;
            JsonDbQuery *query  = n->parsedQuery();
            bool oldMatches = query->match(oldObject, 0 /* cache */, 0/*mStorage*/);
            bool newMatches = query->match(object, 0 /* cache */, 0/*mStorage*/);
            if (oldMatches || newMatches)
                r = object;
            if (!oldMatches && newMatches) {
                effectiveAction = JsonDbNotification::Create;
            } else if (oldMatches && (!newMatches || object.isDeleted())) {
                r = oldObject;
                if (object.isDeleted())
                    r.insert(JsonDbString::kDeletedStr, true);
                effectiveAction = JsonDbNotification::Delete;
            } else if (oldMatches && newMatches) {
                effectiveAction = JsonDbNotification::Update;
            }
            if (jsondbSettings->debug())
                qDebug() << "Got result" << r;
        } else {
            r = object;
        }
        if (!r.isEmpty()&& (n->actions() & effectiveAction)) {
            JsonDbPartition *partition = findPartition(partitionName);
            if (partition && !n->parsedQuery()->orderTerms.isEmpty()) {
                const QString &indexName = n->parsedQuery()->orderTerms[0].propertyName;
                QString objectType = r.type();
                JsonDbObjectTable *objectTable = partition->findObjectTable(objectType);
                IndexSpec *indexSpec = objectTable->indexSpec(indexName);
                if (indexSpec) {
                    QList<QJsonValue> indexValues = indexSpec->index->indexValues(r);
                    if (!indexValues.isEmpty())
                        r.insert(JsonDbString::kIndexValueStr, indexValues[0]);
                }
            }
            QString actionStr = (effectiveAction == JsonDbNotification::Create ? JsonDbString::kCreateStr :
                                 (effectiveAction == JsonDbNotification::Update ? JsonDbString::kUpdateStr :
                                  JsonDbString::kRemoveStr));
            notified(n->uuid(), stateNumber, r, actionStr);
            n->setLastStateNumber(stateNumber);
        }
    }
}

void DBServer::processWrite(JsonStream *stream, JsonDbOwner *owner, const JsonDbObjectList &objects,
                            JsonDbPartition::WriteMode mode, const QString &partitionName,  int id)
{
    QJsonObject response;
    response.insert(JsonDbString::kIdStr, id);

    JsonDbError::ErrorCode errorCode = JsonDbError::NoError;
    QString errorMsg;

    if (partitionName == mEphemeralPartition->name()) {
        // validate any notification objects before sending them off to be created
        foreach (const JsonDbObject &object, objects) {
            if (object.type() == JsonDbString::kNotificationTypeStr && !object.isDeleted()) {
                errorCode = validateNotification(object, errorMsg);
                if (errorCode != JsonDbError::NoError)
                    break;
            }
        }
    }

    if (errorCode == JsonDbError::NoError) {
        JsonDbWriteResult res = partitionName == mEphemeralPartition->name() ?
                    mEphemeralPartition->updateObjects(owner, objects, mode) :
                    mPartitions.value(partitionName, mDefaultPartition)->updateObjects(owner, objects, mode);
        errorCode = res.code;
        errorMsg = res.message;
        if (errorCode == JsonDbError::NoError) {
            QJsonArray data;
            foreach (const JsonDbObject &object, res.objectsWritten) {
                QJsonObject written = object;
                written.insert(JsonDbString::kUuidStr, object.uuid().toString());
                written.insert(JsonDbString::kVersionStr, object.version());
                data.append(written);

                // handle notifications
                if (object.type() == JsonDbString::kNotificationTypeStr) {
                    if (mNotificationMap.contains(object.uuid().toString()))
                        removeNotification(object);
                    if (!object.isDeleted())
                        createNotification(object, stream);
                }
            }

            QJsonObject result;
            result.insert(JsonDbString::kDataStr, data);
            result.insert(JsonDbString::kCountStr, data.count());
            result.insert(JsonDbString::kStateNumberStr, static_cast<int>(res.state));
            response.insert(JsonDbString::kResultStr, result);
            response.insert(JsonDbString::kErrorStr, QJsonValue());
        }
    }

    if (errorCode != JsonDbError::NoError) {
        QJsonObject error;
        error.insert(JsonDbString::kCodeStr, errorCode);
        error.insert(JsonDbString::kMessageStr, errorMsg);
        response.insert(JsonDbString::kErrorStr, error);
        response.insert(JsonDbString::kResultStr, QJsonValue());
    }

    stream->send(response);
}

void DBServer::processRead(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, const QString &partitionName, int id)
{
    if (object.type() != QJsonValue::Object) {
        QJsonObject errorResponse = createError(JsonDbError::InvalidRequest, "Invalid read request");
        errorResponse.insert(JsonDbString::kIdStr, id);
        stream->send(errorResponse);
        return;
    }

    QJsonObject response;

    QJsonObject request = object.toObject();
    QString query = request.value(JsonDbString::kQueryStr).toString();
    QJsonObject bindings = request.value("bindings").toObject();
    QScopedPointer<JsonDbQuery> parsedQuery(JsonDbQuery::parse(query, bindings));

    int limit = request.contains(JsonDbString::kLimitStr) ? request.value(JsonDbString::kLimitStr).toDouble() : -1;
    int offset = request.value(JsonDbString::kOffsetStr).toDouble();

    if (limit < -1)
        response = createError(JsonDbError::InvalidLimit, "Invalid limit");
    else if (offset < 0)
        response = createError(JsonDbError::InvalidOffset, "Invalid offset");
    else if (query.isEmpty())
        response = createError(JsonDbError::MissingQuery, "Missing query string");

    // response should only contain the id at this point
    if (response.contains(JsonDbString::kErrorStr)) {
        response.insert(JsonDbString::kIdStr, id);
        response.insert(JsonDbString::kResultStr, QJsonValue());
        stream->send(response);
        return;
    }

    JsonDbPartition *partition = mPartitions.value(partitionName, mDefaultPartition);
    JsonDbQueryResult queryResult = partitionName == mEphemeralPartition->name() ?
                mEphemeralPartition->queryObjects(owner, parsedQuery.data(), limit, offset) :
                partition->queryObjects(owner, parsedQuery.data(), limit, offset);

    if (jsondbSettings->debug())
        debugQuery(parsedQuery.data(), limit, offset, queryResult);

    if (queryResult.error.type() != QJsonValue::Null) {
        response.insert(JsonDbString::kErrorStr, queryResult.error);
        response.insert(JsonDbString::kResultStr, QJsonValue());
    } else {
        QJsonObject result;
        QJsonArray data;
        for (int i = 0; i < queryResult.data.size(); i++)
            data.append(queryResult.data.at(i));
        result.insert(JsonDbString::kDataStr, data);
        result.insert(JsonDbString::kLengthStr, data.size());
        result.insert(JsonDbString::kOffsetStr, queryResult.offset);
        result.insert(JsonDbString::kExplanationStr, queryResult.explanation);
        result.insert("sortKeys", queryResult.sortKeys);
        result.insert("state", queryResult.state);
        response.insert(JsonDbString::kResultStr, result);
        response.insert(JsonDbString::kErrorStr, QJsonValue());
    }

    response.insert(JsonDbString::kIdStr, id);
    stream->send(response);
}

void DBServer::processChangesSince(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, const QString &partitionName, int id)
{
    Q_UNUSED(owner);
    QJsonObject result;

    if (object.type() == QJsonValue::Object) {
        QJsonObject request(object.toObject());
        int stateNumber = request.value(JsonDbString::kStateNumberStr).toDouble();
        QSet<QString> limitTypes;

        if (request.contains(JsonDbString::kTypesStr)) {
            QJsonArray l = request.value(JsonDbString::kTypesStr).toArray();
            for (int i = 0; i < l.size(); i++)
                limitTypes.insert(l.at(i).toString());
        }

        JsonDbPartition *partition = mPartitions.value(partitionName, mDefaultPartition);
        result = partition->changesSince(stateNumber, limitTypes);
     } else {
        result = createError(JsonDbError::InvalidRequest, "Invalid changes since request");
    }

    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processFlush(JsonStream *stream, JsonDbOwner *owner, const QString &partitionName, int id)
{
    Q_UNUSED(owner);
    JsonDbPartition *partition = mPartitions.value(partitionName, mDefaultPartition);
    QJsonObject result = partition->flush();
    result.insert(JsonDbString::kIdStr, id);
    stream->send(result);
}

void DBServer::debugQuery(JsonDbQuery *query, int limit, int offset, const JsonDbQueryResult &result)
{
    const QList<OrQueryTerm> &orQueryTerms = query->queryTerms;
    for (int i = 0; i < orQueryTerms.size(); i++) {
        const OrQueryTerm &orQueryTerm = orQueryTerms[i];
        foreach (const QueryTerm &queryTerm, orQueryTerm.terms()) {
            if (jsondbSettings->verbose()) {
                qDebug() << __FILE__ << __LINE__
                         << QString("    %1%2%3 %4")
                            .arg(queryTerm.propertyName())
                            .arg(queryTerm.joinField().size() ? "->" : "")
                            .arg(queryTerm.joinField())
                            .arg(queryTerm.op())
                         << queryTerm.value();
            }
        }
    }

    QList<OrderTerm> &orderTerms = query->orderTerms;
    for (int i = 0; i < orderTerms.size(); i++) {
        const OrderTerm &orderTerm = orderTerms[i];
        if (jsondbSettings->verbose())
            qDebug() << __FILE__ << __LINE__ << QString("    %1 %2    ").arg(orderTerm.propertyName).arg(orderTerm.ascending ? "ascending" : "descending");
    }

    qDebug() << "  limit:      " << limit << endl
             << "  offset:     " << offset << endl
             << "  results:    " << result.data;
}

/*!
 * This is just for backwards compatability for create/remove. Once we drop the old
 * C++ API, we can drop this as well since everything will be an update.
 */
JsonDbObjectList DBServer::prepareWriteData(const QString &action, const QJsonValue &object)
{
    JsonDbObjectList result;

    // create/update handled by standard update
    if (action == JsonDbString::kCreateStr || action == JsonDbString::kUpdateStr) {
        if (object.type() == QJsonValue::Object) {
            result.append(object.toObject());
        } else if (object.type() == QJsonValue::Array) {
            QJsonArray array = object.toArray();
            for (int i = 0; i < array.count(); i++) {
                if (array.at(i).type() == QJsonValue::Object)
                    result.append(array.at(i).toObject());
            }
        }
    } else {

        // remove requires that the objects are tombstones
        if (object.type() == QJsonValue::Object) {
            QJsonObject tombstone = object.toObject();
            tombstone.insert(JsonDbString::kDeletedStr, true);
            result.append(tombstone);
        } else if (object.type() == QJsonValue::Array) {
            QJsonArray array = object.toArray();
            for (int i = 0; i < array.count(); i++) {
                if (array.at(i).type() == QJsonValue::Object) {
                    QJsonObject tombstone = array.at(i).toObject();
                    tombstone.insert(JsonDbString::kDeletedStr, true);
                    result.append(tombstone);
                }
            }
        }
    }

    return result;
}

JsonDbObjectList DBServer::checkForNotifications(const JsonDbObjectList &objects)
{
    JsonDbObjectList notifications;

    foreach (const JsonDbObject &object, objects) {
        if (object.type() == JsonDbString::kNotificationTypeStr)
            notifications.append(object);
    }

    return notifications;
}

void DBServer::createNotification(const JsonDbObject &object, JsonStream *stream)
{
    QString        uuid = object.value(JsonDbString::kUuidStr).toString();
    QStringList actions = QVariant(object.value(JsonDbString::kActionsStr).toArray().toVariantList()).toStringList();
    QString       query = object.value(JsonDbString::kQueryStr).toString();
    QJsonObject bindings = object.value("bindings").toObject();
    QString partitionName = object.value(JsonDbString::kPartitionStr).toString();
    quint32 stateNumber = 0;

    if (partitionName.isEmpty())
        partitionName = mDefaultPartition->name();
    JsonDbPartition *partition = findPartition(partitionName);

    JsonDbNotification *n = new JsonDbNotification(getOwner(stream), uuid, query, actions, partitionName);
    if (object.contains("initialStateNumber") && object.value("initialStateNumber").isDouble())
        stateNumber = static_cast<quint32>(object.value("initialStateNumber").toDouble());
    else if (partition)
        stateNumber = partition->mainObjectTable()->stateNumber();
    n->setInitialStateNumber(stateNumber);

    JsonDbQuery *parsedQuery = JsonDbQuery::parse(query, bindings);
    n->setCompiledQuery(parsedQuery);
    const QList<OrQueryTerm> &orQueryTerms = parsedQuery->queryTerms;

    bool generic = true;
    for (int i = 0; i < orQueryTerms.size(); i++) {
        const OrQueryTerm &orQueryTerm = orQueryTerms[i];
        const QList<QueryTerm> &terms = orQueryTerm.terms();
        if (terms.size() == 1) {
            const QueryTerm &term = terms[0];
            if (term.op() == "=") {
                if (term.propertyName() == JsonDbString::kUuidStr) {
                    mKeyedNotifications.insert(term.value().toString(), n);
                    generic = false;
                    break;
                } else if (term.propertyName() == JsonDbString::kTypeStr) {
                    QString objectType = term.value().toString();
                    mKeyedNotifications.insert(objectType, n);
                    generic = false;
                    break;
                }
            }
        }
    }
    if (generic)
        mKeyedNotifications.insert("__generic_notification__", n);

    mNotificationMap[uuid] = n;
    mNotifications[uuid] = stream;

    foreach (const QString &objectType, parsedQuery->matchedTypes())
        updateEagerViewTypes(objectType, mPartitions.value(partitionName, mDefaultPartition), stateNumber, 1);

    if (partition)
        notifyHistoricalChanges(n);
}

void DBServer::removeNotification(const JsonDbObject &object)
{
    if (mNotificationMap.contains(object.uuid().toString())) {
        JsonDbNotification *n = mNotificationMap.value(object.uuid().toString());
        mNotificationMap.remove(object.uuid().toString());
        mNotifications.remove(object.uuid().toString());
        const JsonDbQuery *parsedQuery = n->parsedQuery();
        const QList<OrQueryTerm> &orQueryTerms = parsedQuery->queryTerms;
        for (int i = 0; i < orQueryTerms.size(); i++) {
            const OrQueryTerm &orQueryTerm = orQueryTerms[i];
            const QList<QueryTerm> &terms = orQueryTerm.terms();
            if (terms.size() == 1) {
                const QueryTerm &term = terms[0];
                if (term.op() == "=") {
                    if (term.propertyName() == JsonDbString::kTypeStr) {
                        mKeyedNotifications.remove(term.value().toString(), n);
                    } else if (term.propertyName() == JsonDbString::kUuidStr) {
                        QString objectType = term.value().toString();
                        mKeyedNotifications.remove(objectType, n);
                    }
                }
            }
        }

        mKeyedNotifications.remove("__generic_notification__", n);

        const QString &partitionName = n->partition();
        foreach (const QString &objectType, parsedQuery->matchedTypes())
            updateEagerViewTypes(objectType, mPartitions.value(partitionName, mDefaultPartition), 0, -1);

        delete n;
    }
}

JsonDbError::ErrorCode DBServer::validateNotification(const JsonDbObject &notificationDef, QString &message)
{
    message.clear();

    QScopedPointer<JsonDbQuery> query(JsonDbQuery::parse(notificationDef.value(JsonDbString::kQueryStr).toString()));
    if (!(query->queryTerms.size() || query->orderTerms.size())) {
        message = QString::fromLatin1("Missing query: %1").arg(query->queryExplanation.join(QStringLiteral("\n")));
        return JsonDbError::MissingQuery;
    }

    if (notificationDef.contains(JsonDbString::kPartitionStr) &&
            notificationDef.value(JsonDbString::kPartitionStr).toString() != mEphemeralPartition->name()) {
        QString partitionName = notificationDef.value(JsonDbString::kPartitionStr).toString();
        JsonDbPartition *partition = findPartition(partitionName);
        if (!partition) {
            message = QString::fromLatin1("Invalid partition specified: %1").arg(partitionName);
            return JsonDbError::InvalidPartition;
        }
    }

    return JsonDbError::NoError;
}

void DBServer::notifyHistoricalChanges(JsonDbNotification *n)
{
    JsonDbPartition *partition = findPartition(n->partition());
    quint32 stateNumber = n->initialStateNumber();
    quint32 lastStateNumber = stateNumber;
    JsonDbQuery *parsedQuery = n->parsedQuery();
    QSet<QString> matchedTypes = parsedQuery->matchedTypes();
    bool matchAnyType = matchedTypes.isEmpty();
    if (stateNumber == 0) {
        QString indexName = JsonDbString::kTypeStr;
        if (matchAnyType) {
            matchedTypes.insert(QString());
            // faster to walk the _uuid index if no type is specified
            indexName = JsonDbString::kUuidStr;
        }
        foreach (const QString matchedType, matchedTypes) {
            JsonDbObjectTable *objectTable = partition->findObjectTable(matchedType);
            lastStateNumber = objectTable->stateNumber();
            if (lastStateNumber == stateNumber)
                continue;

            // views dont have a _type index
            if (partition->findView(matchedType))
                indexName = JsonDbString::kUuidStr;

            lastStateNumber = objectTable->stateNumber();
            QScopedPointer<JsonDbIndexQuery> indexQuery(JsonDbIndexQuery::indexQuery(partition, objectTable,
                                                                        indexName, QString("string"),
                                                                        n->owner()));
            if (!matchAnyType) {
                indexQuery.data()->setMin(matchedType);
                indexQuery.data()->setMax(matchedType);
            }

            JsonDbObject oldObject;
            int c = 0;
            for (JsonDbObject o = indexQuery.data()->first(); !o.isEmpty(); o = indexQuery.data()->next()) {
                JsonDbNotification::Action action = JsonDbNotification::Create;
                objectUpdated(partition->name(), lastStateNumber, n, action, oldObject, o);
                c++;
            }
        }
    } else {
        foreach (const QString matchedType, matchedTypes) {
            JsonDbObjectTable *objectTable = partition->findObjectTable(matchedType);
            if (objectTable->stateNumber() == stateNumber)
                continue;
            QList<JsonDbUpdate> updateList;
            lastStateNumber = objectTable->changesSince(stateNumber, matchedTypes, &updateList);
            foreach (const JsonDbUpdate &update, updateList) {
                QJsonObject before = update.oldObject;
                QJsonObject after = update.newObject;

                JsonDbNotification::Action action = JsonDbNotification::Update;
                if (before.isEmpty())
                    action = JsonDbNotification::Create;
                else if (after.contains(JsonDbString::kDeletedStr))
                    action = JsonDbNotification::Delete;
                objectUpdated(partition->name(), lastStateNumber, n, action, before, after);
            }
        }
    }
    QJsonObject stateChange;
    stateChange.insert("_state", static_cast<int>(lastStateNumber));
    emit notified(n->uuid(), lastStateNumber, stateChange, "stateChange");
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
void DBServer::updateEagerViewTypes(const QString &viewType, JsonDbPartition *partition, quint32 stateNumber, int increment)
{
    JsonDbView *view = partition->findView(viewType);
    if (!view)
        return;
    QString partitionName = partition->name();
    WeightedSourceViewGraph &sourceViewGraph = mEagerViewSourceGraph[partitionName];

    // An update of the Map/Reduce definition also causes the view to
    // need to be updated, so we count it as a view source.

    // this is a bit conservative, since we add both Map and Reduce as
    // sources, but it shortens the code
    sourceViewGraph[JsonDbString::kMapTypeStr][viewType] += increment;
    sourceViewGraph[JsonDbString::kReduceTypeStr][viewType] += increment;
    foreach (const QString sourceType, view->sourceTypes()) {
        sourceViewGraph[sourceType][viewType] += increment;
        if (jsondbSettings->verbose())
            qDebug() << "SourceView" << sourceType << viewType << sourceViewGraph[sourceType][viewType].count;
        // now recurse until we get to a non-view sourceType
        updateEagerViewTypes(sourceType, partition, stateNumber, increment);
    }
    if (stateNumber)
        partition->updateView(viewType, stateNumber);
}

JsonDbPartition *DBServer::findPartition(const QString &partitionName)
{
    JsonDbPartition *partition = mDefaultPartition;
    if (!partitionName.isEmpty()) {
        if (mPartitions.contains(partitionName))
            partition = mPartitions[partitionName];
        else
            partition = 0;
    }

    return partition;
}

QList<QJsonObject> DBServer::findPartitionDefinitions() const
{
    QList<QJsonObject> partitions;

    bool defaultSpecified = false;

    foreach (const QString &path, jsondbSettings->configSearchPath()) {
        QDir searchPath(path);
        if (!searchPath.exists())
            continue;

        if (jsondbSettings->debug())
            qDebug() << QString("Searching %1 for partition definition files").arg(path);

        QStringList files = searchPath.entryList(QStringList() << "partitions*.json",
                                                 QDir::CaseSensitive | QDir::Files | QDir::Readable);
        foreach (const QString file, files) {
            if (jsondbSettings->debug())
                qDebug() << QString("Loading partition definitions from %1").arg(file);

            QFile partitionFile(searchPath.absoluteFilePath(file));
            partitionFile.open(QFile::ReadOnly);

            QJsonArray partitionList = QJsonDocument::fromJson(partitionFile.readAll()).array();
            if (partitionList.isEmpty())
                continue;

            for (int i = 0; i < partitionList.count(); i++) {
                QJsonObject def = partitionList[i].toObject();
                if (def.contains(JsonDbString::kNameStr)) {
                    if (!def.contains(JsonDbString::kPathStr))
                        def.insert(JsonDbString::kPathStr, QDir::currentPath());
                    if (def.contains(JsonDbString::kDefaultStr))
                        defaultSpecified = true;
                    partitions.append(def);
                }
            }
        }
    }

    // if no partitions are specified just make a partition in the current working
    // directory and call it "default"
    if (partitions.isEmpty()) {
        QJsonObject defaultPartition;
        defaultPartition.insert(JsonDbString::kNameStr, QLatin1String("default"));
        defaultPartition.insert(JsonDbString::kPathStr, QDir::currentPath());
        defaultPartition.insert(JsonDbString::kDefaultStr, true);
        partitions.append(defaultPartition);
        defaultSpecified = true;
    }

    // ensure that at least one partition is marked as default
    if (!defaultSpecified) {
        QJsonObject defaultPartition = partitions.takeFirst();
        defaultPartition.insert(JsonDbString::kDefaultStr, true);
        partitions.append(defaultPartition);
    }

    return partitions;
}

void DBServer::receiveMessage(const QJsonObject &message)
{
    JsonStream *stream = qobject_cast<JsonStream *>(sender());
    QString  action = message.value(JsonDbString::kActionStr).toString();
    QJsonValue object = message.value(JsonDbString::kObjectStr);
    int id = message.value(JsonDbString::kIdStr).toDouble();
    QString partitionName = message.value(JsonDbString::kPartitionStr).toString();

    JsonDbPartition *partition = findPartition(partitionName);
    if (!(partitionName.isEmpty() || partition || partitionName == mEphemeralPartition->name())) {
        sendError(stream, JsonDbError::InvalidPartition,
                  QString("Invalid partition '%1'").arg(partitionName), id);
        return;
    }

    QElapsedTimer timer;
    QHash<QString, qint64> fileSizes;

    JsonDbStat startStat;
    if (jsondbSettings->performanceLog()) {
        if (jsondbSettings->verbose() && partitionName != mEphemeralPartition->name())
            fileSizes = partition->fileSizes();
        startStat = stat();
        timer.start();
    }

    JsonDbOwner *owner = getOwner(stream);
    if (!owner) {
        sendError(stream, JsonDbError::InvalidRequest, QLatin1String("Authentication error"), id);
        // Close the socket in case of authentication error
        QLocalSocket *socket = qobject_cast<QLocalSocket *>(stream->device());
        if (socket) {
            socket->disconnectFromServer();
        } else {
            QTcpSocket *tcpSocket = qobject_cast<QTcpSocket *>(stream->device());
            if (tcpSocket)
                tcpSocket->disconnectFromHost();
        }
        return;
    }

    if (action == JsonDbString::kCreateStr || action == JsonDbString::kRemoveStr || action == JsonDbString::kUpdateStr) {
        JsonDbPartition::WriteMode writeMode = JsonDbPartition::OptimisticWrite;
        QString writeModeRequested = message.value(QStringLiteral("writeMode")).toString();

        if (writeModeRequested == QLatin1String("merge"))
            writeMode = JsonDbPartition::ReplicatedWrite;
        else if (writeModeRequested == QLatin1String("replace") || !jsondbSettings->rejectStaleUpdates())
            writeMode = JsonDbPartition::ForcedWrite;

        // TODO: remove at the same time that clientcompat is dropped
        if (action == JsonDbString::kRemoveStr && object.toObject().contains(JsonDbString::kQueryStr)) {
            QScopedPointer<JsonDbQuery> parsedQuery(JsonDbQuery::parse(object.toObject().value(JsonDbString::kQueryStr).toString()));
            JsonDbQueryResult res;
            if (partition)
                res = partition->queryObjects(owner, parsedQuery.data());
            else
                res = mEphemeralPartition->queryObjects(owner, parsedQuery.data());

            QJsonArray toRemove;
            foreach (const QJsonValue &value, res.data)
                toRemove.append(value);
            object = toRemove;
        }

        JsonDbObjectList toWrite = prepareWriteData(action, object);

        // check if the objects to write contain any notifications. If the specified partition
        // is not the ephemeral one, then switch the ephemeral partition provided that all of the
        // objects to write are notifications.
        if (partitionName != mEphemeralPartition->name()) {
            JsonDbObjectList notifications = checkForNotifications(toWrite);
            if (!notifications.isEmpty()) {
                if (notifications.count() == toWrite.count()) {
                    partitionName = mEphemeralPartition->name();
                } else {
                    sendError(stream, JsonDbError::InvalidRequest,
                              QLatin1String("Mixing objects of type Notification with others can only be done in the ephemeral partition"), id);
                    return;
                }
            }
        }
        processWrite(stream, owner, toWrite, writeMode, partitionName, id);
    } else if (action == JsonDbString::kFindStr) {
        processRead(stream, owner, object, partitionName, id);
    } else if (action == JsonDbString::kChangesSinceStr) {
        if (partitionName == mEphemeralPartition->name()) {
            sendError(stream, JsonDbError::InvalidRequest,
                      QString("Invalid partition for changesSince '%1'").arg(partitionName), id);
            return;
        }
        processChangesSince(stream, owner, object, partitionName, id);
    } else if (action == JsonDbString::kFlushStr) {
        processFlush(stream, owner, partitionName, id);
    } else if (action == JsonDbString::kLogStr) {
        if (jsondbSettings->debug() || jsondbSettings->performanceLog())
            qDebug() << object.toObject().value(JsonDbString::kMessageStr).toString();
    }

    if (jsondbSettings->performanceLog()) {
        QString additionalInfo;
        JsonDbStat stats = stat();
        stats -= startStat;
        if ( action == JsonDbString::kFindStr ) {
            additionalInfo = object.toObject().value("query").toString();
        } else if (object.type() == QJsonValue::Array) {
            additionalInfo = QString::fromLatin1("%1 objects").arg(object.toArray().size());
        } else {
            additionalInfo = object.toObject().value(JsonDbString::kTypeStr).toString();
        }
        qDebug().nospace() << "+ JsonDB Perf: [id]" << id << "[id]:[action]" << action
                           << "[action]:[ms]" << timer.elapsed() << "[ms]:[details]" << additionalInfo << "[details]"
                           << ":[reads]" << stats.reads << "[reads]:[hits]" << stats.hits << "[hits]:[writes]" << stats  .writes << "[writes]";
        if (jsondbSettings->verbose() && partitionName != mEphemeralPartition->name()) {
            QHash<QString, qint64> newSizes = partition->fileSizes();
            QHashIterator<QString, qint64> files(fileSizes);

            while (files.hasNext()) {
                files.next();
                if (newSizes[files.key()] != files.value())
                    qDebug().nospace() << "\t [file]" << files.key() << "[file]:[size]" << (newSizes[files.key()] - files.value()) << "[size]";

            }
        }
    }
}

void DBServer::handleConnectionError()
{
    QIODevice *connection = qobject_cast<QIODevice *>(sender());
    qWarning() << connection->errorString();
}

void DBServer::removeConnection()
{
    QIODevice *connection = qobject_cast<QIODevice *>(sender());
    JsonDbOwner *owner = mOwners[connection];
    QMutableMapIterator<QString, JsonStream *> iter(mNotifications);
    while (iter.hasNext()) {
        iter.next();
        if (iter.value()
            && (iter.value()->device() == connection)) {
            QString notificationId = iter.key();
            QJsonObject notificationObject;
            notificationObject.insert(JsonDbString::kUuidStr, notificationId);
            notificationObject.insert(JsonDbString::kDeletedStr, true);
            mEphemeralPartition->updateObjects(owner, JsonDbObjectList() << notificationObject, JsonDbPartition::ForcedWrite);
            mNotifications.remove(notificationId);
        }
    }

    if (mConnections.contains(connection)) {
        JsonStream *stream = mConnections.value(connection);
        if (stream)
            stream->deleteLater();

        mConnections.remove(connection);
    }
    if (mOwners.contains(connection)) {
        JsonDbOwner *owner = mOwners.value(connection);
        if (owner)
            owner->deleteLater();
        mOwners.remove(connection);
    }

    connection->deleteLater();
}

#include "moc_dbserver.cpp"
