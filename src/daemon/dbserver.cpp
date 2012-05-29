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
#include "jsondbobjecttable.h"
#include "jsondbsettings.h"
#include "jsondbview.h"
#include "jsondbsocketname_p.h"
#include "jsondbqueryparser.h"
#include "dbserver.h"
#include "private/jsondbutils_p.h"

#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <pwd.h>
#include <errno.h>
#endif

QT_USE_NAMESPACE_JSONDB_PARTITION

static const int gReadBufferSize = 65536;

void DBServer::sendError(ClientJsonStream *stream, JsonDbError::ErrorCode code,
                         const QString& message, int id)
{
    QJsonObject map;
    map.insert( JsonDbString::kResultStr, QJsonValue());
    QJsonObject errormap;
    errormap.insert(JsonDbString::kCodeStr, code);
    errormap.insert(JsonDbString::kMessageStr, message);
    map.insert( JsonDbString::kErrorStr, errormap );
    map.insert( JsonDbString::kIdStr, id );

    stream->send(map);

    if (jsondbSettings->verboseErrors()) {
        if (mOwners.contains(stream->device())) {
            OwnerInfo &ownerInfo = mOwners[stream->device()];
            qDebug() << JSONDB_ERROR << "error for pid" << ownerInfo.pid << ownerInfo.processName << "error.code" << code << message;
        } else
            qDebug() << JSONDB_ERROR << "client error" << map;
    }
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
    // If a search path has been provided, then search that for partitions.json files
    // Otherwise search whatever's been specified in JSONDB_CONFIG_SEARCH_PATH
    // Otherwise search the CWD and /etc/jsondb
    QStringList searchPaths;
    if (!searchPath.isEmpty())
        searchPaths << searchPath;
    else if (!jsondbSettings->configSearchPath().isEmpty())
        searchPaths << jsondbSettings->configSearchPath();
    else
        searchPaths << QDir::currentPath() << QStringLiteral("/etc/jsondb");

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

void DBServer::sigHUP()
{
    if (jsondbSettings->debug())
        qDebug() << JSONDB_INFO << "SIGHUP received";
    loadPartitions();
}

void DBServer::sigTERM()
{
    if (jsondbSettings->debug())
        qDebug() << JSONDB_INFO << "SIGTERM received";
    close();
}

void DBServer::sigINT()
{
    if (jsondbSettings->debug())
        qDebug() << JSONDB_INFO << "SIGINT received";
    close();
}

void DBServer::sigUSR1()
{
    if (jsondbSettings->debug())
        qDebug() << JSONDB_INFO << "SIGUSR1 received";
    reduceMemoryUsage();
    closeIndexes();
}

bool DBServer::socket()
{
    QString socketName = ::getenv("JSONDB_SOCKET");
    if (socketName.isEmpty())
        socketName = QStringLiteral(JSONDB_SOCKET_NAME_STRING);
    if (jsondbSettings->debug())
        qDebug() << JSONDB_INFO << "listening on socket" << socketName;
    QLocalServer::removeServer(socketName);
    mServer = new QLocalServer(this);
    connect(mServer, SIGNAL(newConnection()), this, SLOT(handleConnection()));
    if (mServer->listen(socketName)) {
        if (jsondbSettings->verbose())
            qDebug() << JSONDB_INFO << "set permissions on" << mServer->fullServerName();
        QFile::setPermissions(mServer->fullServerName(), QFile::ReadUser | QFile::WriteUser | QFile::ExeUser |
                                                        QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
                                                        QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
    } else {
        qCritical() << JSONDB_ERROR << "unable to open" << socketName << "socket";
    }

    if (mTcpServerPort) {
        mTcpServer = new QTcpServer(this);
        connect(mTcpServer, SIGNAL(newConnection()), this, SLOT(handleTcpConnection()));
        if (!mTcpServer->listen(QHostAddress::Any, mTcpServerPort))
            qCritical() << JSONDB_ERROR << QString("unable to open jsondb remote socket on port %1").arg(mTcpServerPort);
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
    if (jsondbSettings->debug())
        qDebug() << JSONDB_INFO << "set owner id to" << username;

    if (!loadPartitions()) {
        qCritical() << JSONDB_ERROR << "unable to load partitions";
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
    QCoreApplication::exit();
}

bool DBServer::loadPartitions()
{
    if (!mEphemeralPartition)
        mEphemeralPartition = new JsonDbEphemeralPartition("Ephemeral", this);

    QHash<QString, JsonDbPartition*> partitions;
    QList<JsonDbPartitionSpec> definitions = findPartitionDefinitions();
    QString defaultPartitionName;

    foreach (const JsonDbPartitionSpec &definition, definitions) {
        QString name = definition.name;
        bool removable = definition.isRemovable;

        if (definition.isDefault && defaultPartitionName.isEmpty())
            defaultPartitionName = name;

        if (mPartitions.contains(name)) {
            partitions[name] = mPartitions.take(name);

            if (jsondbSettings->debug())
                qDebug() << JSONDB_INFO << "loading partition" << name;

            if (removable) {
                JsonDbPartition *removablePartition = partitions[name];
                bool updateDefinition = false;

                // for existing removable partitions, we need to check:
                // 1. if the partition is already open, check if it's
                // main file exists to make sure it's still available
                // 2. if the partition isn't open, call open on it to see
                // if it can be made available
                if (removablePartition->isOpen()) {
                    if (jsondbSettings->verbose())
                        qDebug() << JSONDB_INFO << "determining if partition" << removablePartition->partitionSpec().name << "is still available";
                    if (!QFile::exists(removablePartition->filename())) {
                        if (jsondbSettings->debug())
                            qDebug() << JSONDB_INFO << "marking partition" << removablePartition->partitionSpec().name << "as unavailable";
                        removablePartition->close();
                        updateDefinition = true;
                    } else if (jsondbSettings->debug()) {
                        qDebug() << JSONDB_INFO << "marking partition" << removablePartition->partitionSpec().name << "as available";
                    }
                } else {
                    if (jsondbSettings->verbose())
                        qDebug() << JSONDB_INFO << "determining if partition" << removablePartition->partitionSpec().name << "has become available";
                    if (removablePartition->open()) {
                        if (jsondbSettings->debug())
                            qDebug() << JSONDB_INFO << "marking partition" << removablePartition->partitionSpec().name << "as available";
                        updateDefinition = true;

                        // re-install any notifications on this partition
                        enableNotificationsByPartition(removablePartition);
                    }
                }

                if (updateDefinition)
                    updatePartitionDefinition(removablePartition);
            }
        } else {
            Q_ASSERT(!partitions.contains(name));

            JsonDbPartition *partition = new JsonDbPartition(this);
            partition->setPartitionSpec(definition);
            partition->setDefaultOwner(mOwner);

            if (jsondbSettings->debug())
                qDebug() << JSONDB_INFO << "creating partition" << name;

            partitions[name] = partition;

            if (!(partition->open() || removable)) {
                close();
                return false;
            }

            updatePartitionDefinition(partitions[name]);
        }
    }

    // close any partitions that were declared previously but are no longer present
    foreach (JsonDbPartition *partition, mPartitions.values()) {
        if (mDefaultPartition == partition)
            mDefaultPartition = 0;

        updatePartitionDefinition(partition, true);

        // remove any notifications for the partition being closed
        removeNotificationsByPartition(partition);

        partition->close();
        delete partition;
    }

    mPartitions = partitions;

    if (!mDefaultPartition) {
        mDefaultPartition = mPartitions[defaultPartitionName];
        updatePartitionDefinition(mDefaultPartition, false, true);
    }

    return true;
}

void DBServer::reduceMemoryUsage()
{
    foreach (JsonDbPartition *partition, mPartitions.values())
        partition->flushCaches();
}

void DBServer::closeIndexes()
{
    foreach (JsonDbPartition *partition, mPartitions.values())
        partition->closeIndexes();
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
            qDebug() << JSONDB_INFO << "client connected to jsondb server" << connection;
        connect(connection, SIGNAL(disconnected()), this, SLOT(removeConnection()));
        ClientJsonStream *stream = new ClientJsonStream(this);
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
            qDebug() << JSONDB_INFO << "remote client connected to jsondb server" << connection;
        connect(connection, SIGNAL(disconnected()), this, SLOT(removeConnection()));
        ClientJsonStream *stream = new ClientJsonStream(this);
        stream->setDevice(connection);
        connect(stream, SIGNAL(receive(QJsonObject)), this,
                SLOT(receiveMessage(QJsonObject)));
        mConnections.insert(connection, stream);
    }
}

JsonDbOwner *DBServer::getOwner(ClientJsonStream *stream)
{
    QIODevice *device = stream->device();

    if (!(jsondbSettings->enforceAccessControl() || mOwners.contains(stream->device()))) {
        // We are not enforcing policies here, allow requests
        // from all applications.
        mOwners[device].owner = createDummyOwner(stream);
    }

    // now capture pid and process name
    // and owner if we are enforcing policies
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    if (!mOwners.contains(device) || !mOwners.value(device).pid) {
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
            qWarning() << JSONDB_WARN << "socketDescriptor () does not return a valid descriptor.";
            return 0;
        }
        if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &peercred, &peercredlen)) {
            qWarning() << JSONDB_WARN << "getsockopt(...SO_PEERCRED...) failed" << ::strerror(errno) << errno;
            return 0;
        }

        OwnerInfo &ownerInfo = mOwners[stream->device()];
        ownerInfo.pid = peercred.pid;
        QFile cmdline(QString("/proc/%2/cmdline").arg(peercred.pid));
        if (cmdline.open(QIODevice::ReadOnly)) {
            QByteArray rawProcessName = cmdline.readAll();
            rawProcessName.replace(0, ' ');
            ownerInfo.processName = QString::fromLatin1(rawProcessName);
        }

        if (jsondbSettings->enforceAccessControl()) {
            QScopedPointer<JsonDbOwner> owner(new JsonDbOwner(this));
            if (owner->setOwnerCapabilities (peercred.uid, mDefaultPartition))
                ownerInfo.owner = owner.take();
            else return 0;
        }
    }
#else
    // security hole
    // TODO: Mac socket authentication
    if (!mOwners.contains(device)) {
        JsonDbOwner *owner = new JsonDbOwner(this);
        owner->setOwnerId(QString::fromLatin1("unknown app %1").arg((intptr_t)stream));
        owner->setDomain("unknown app domain");
        owner->setAllowAll(true);
        mOwners[device].owner = owner;
    }
#endif
    Q_ASSERT(mOwners[device].owner);
    return mOwners[device].owner;
}

/*!
    This will create a dummy owner. Used only when the policies are not enforced.
    When policies are not enforced the JsonDbOwner allows access to everybody.
*/
JsonDbOwner *DBServer::createDummyOwner(ClientJsonStream *stream)
{
    if (jsondbSettings->enforceAccessControl())
        return 0;

    JsonDbOwner *owner = mOwners.value(stream->device()).owner;
    if (owner)
        return owner;

    owner = new JsonDbOwner(this);
    owner->setOwnerId(QString("unknown app"));
    owner->setDomain(QString("unknown domain"));
    mOwners[stream->device()].owner = owner;

    return owner;
}

void DBServer::processWrite(ClientJsonStream *stream, JsonDbOwner *owner, const JsonDbObjectList &objects,
                            JsonDbPartition::ConflictResolutionMode mode, const QString &partitionName,  int id)
{
    QJsonObject response;
    response.insert(JsonDbString::kIdStr, id);

    JsonDbError::ErrorCode errorCode = JsonDbError::NoError;
    QString errorMsg;

    if (partitionName == mEphemeralPartition->name()) {
        // prevent objects of type Partition from being created
        foreach (const JsonDbObject &object, objects) {
            if (object.type() == JsonDbString::kPartitionTypeStr && !object.isDeleted()) {
                errorCode = JsonDbError::OperationNotPermitted;
                errorMsg = QStringLiteral("Cannot create object of type 'Partition' in the ephemeral partition");
                break;
            }
        }

        if (errorCode == JsonDbError::NoError) {
            // validate any notification objects before sending them off to be created
            foreach (const JsonDbObject &object, objects) {
                if (object.type() == JsonDbString::kNotificationTypeStr && !object.isDeleted()) {
                    errorCode = validateNotification(object, errorMsg);
                    if (errorCode != JsonDbError::NoError)
                        break;
                }
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
                    removeNotification(object, stream);
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
        sendError(stream, errorCode, errorMsg, id);
        return;
    }

    stream->send(response);
}

void DBServer::processRead(ClientJsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, const QString &partitionName, int id)
{
    if (object.type() != QJsonValue::Object) {
        sendError(stream, JsonDbError::InvalidRequest, "Invalid read request", id);
        return;
    }

    QJsonObject response;

    QJsonObject request = object.toObject();
    QString query = request.value(JsonDbString::kQueryStr).toString();
    QJsonObject bindings = request.value("bindings").toObject();

    int limit = request.contains(JsonDbString::kLimitStr) ? request.value(JsonDbString::kLimitStr).toDouble() : -1;
    int offset = request.value(JsonDbString::kOffsetStr).toDouble();

    JsonDbError::ErrorCode errorCode = JsonDbError::NoError;
    QString errorMessage;

    JsonDbQueryParser parser;
    parser.setQuery(query);
    parser.setBindings(bindings);
    bool ok = parser.parse();
    JsonDbQuery parsedQuery = parser.result();
    if (!ok) {
        errorCode = JsonDbError::MissingQuery;
        errorMessage = QStringLiteral("Invalid query string");
    } else if (limit < -1) {
        errorCode = JsonDbError::InvalidLimit;
        errorMessage = QStringLiteral("Invalid limit");
    } else if (offset < 0) {
        errorCode = JsonDbError::InvalidOffset;
        errorMessage = QStringLiteral("Invalid offset");
    } else if (query.isEmpty()) {
        errorCode = JsonDbError::MissingQuery;
        errorMessage = QStringLiteral("Missing query string");
    }

    // response should only contain the id at this point
    if (errorCode != JsonDbError::NoError) {
        sendError(stream, errorCode, errorMessage, id);
        return;
    }

    JsonDbPartition *partition = mPartitions.value(partitionName, mDefaultPartition);
    JsonDbQueryResult queryResult = partitionName == mEphemeralPartition->name() ?
                mEphemeralPartition->queryObjects(owner, parsedQuery, limit, offset) :
                partition->queryObjects(owner, parsedQuery, limit, offset);

    if (jsondbSettings->debug())
        debugQuery(parsedQuery, limit, offset, queryResult);

    if (queryResult.code != JsonDbError::NoError) {
        sendError(stream, queryResult.code, queryResult.message, id);
        return;
    } else {
        QJsonObject result;
        QJsonArray data;
        for (int i = 0; i < queryResult.data.size(); i++)
            data.append(queryResult.data.at(i));

        QJsonArray sortKeys;
        foreach (const QString &sortKey, queryResult.sortKeys)
            sortKeys.append(sortKey);

        result.insert(JsonDbString::kDataStr, data);
        result.insert(JsonDbString::kLengthStr, data.size());
        result.insert(JsonDbString::kOffsetStr, queryResult.offset);
        result.insert("sortKeys", sortKeys);
        result.insert("state", static_cast<qint32>(queryResult.state));
        response.insert(JsonDbString::kResultStr, result);
        response.insert(JsonDbString::kErrorStr, QJsonValue());
    }

    response.insert(JsonDbString::kIdStr, id);
    stream->send(response);
}

void DBServer::processChangesSince(ClientJsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, const QString &partitionName, int id)
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
        JsonDbChangesSinceResult csResult = partition->changesSince(stateNumber, limitTypes);
        if (csResult.code == JsonDbError::NoError) {
            QJsonObject resultMap;

            resultMap.insert(QStringLiteral("count"), csResult.changes.count());
            resultMap.insert(QStringLiteral("startingStateNumber"), static_cast<qint32>(csResult.startingStateNumber));
            resultMap.insert(QStringLiteral("currentStateNumber"), static_cast<qint32>(csResult.currentStateNumber));

            QJsonArray changeArray;
            foreach (const JsonDbUpdate &update, csResult.changes) {
                QJsonObject change;
                change.insert(QStringLiteral("before"), update.oldObject);
                change.insert(QStringLiteral("after"), update.newObject);
                changeArray.append(change);
            }

            resultMap.insert(QStringLiteral("changes"), changeArray);
            result.insert(JsonDbString::kResultStr, resultMap);
            result.insert(JsonDbString::kErrorStr, QJsonValue());
        } else {
            QJsonObject errorMap;
            errorMap.insert(JsonDbString::kCodeStr, csResult.code);
            errorMap.insert(JsonDbString::kMessageStr, csResult.message);
            result.insert(JsonDbString::kResultStr, QJsonValue());
            result.insert(JsonDbString::kErrorStr, errorMap);
        }
     } else {
        sendError(stream, JsonDbError::InvalidRequest, "Invalid changes since request", id);
        return;
    }

    result.insert(JsonDbString::kIdStr, id);
    stream->send(result);
}

void DBServer::processFlush(ClientJsonStream *stream, JsonDbOwner *owner, const QString &partitionName, int id)
{
    Q_UNUSED(owner);
    JsonDbPartition *partition = mPartitions.value(partitionName, mDefaultPartition);

    QJsonObject resultmap, errormap;

    bool ok;
    int stateNumber = partition->flush(&ok);

    if (ok) {
        resultmap.insert(JsonDbString::kStateNumberStr, stateNumber);
    } else {
        errormap.insert(JsonDbString::kCodeStr, JsonDbError::FlushFailed);
        errormap.insert(JsonDbString::kMessageStr, QStringLiteral("Unable to flush partition"));
    }

    QJsonObject result;
    result.insert(JsonDbString::kResultStr, resultmap);
    result.insert(JsonDbString::kErrorStr, errormap);
    result.insert(JsonDbString::kIdStr, id);
    stream->send(result);
}

void DBServer::processLog(ClientJsonStream *stream, const QString &message, int id)
{
     if (jsondbSettings->debug() || jsondbSettings->performanceLog())
         qDebug() << message;

     QJsonObject result;
     result.insert(JsonDbString::kResultStr, QJsonObject());
     result.insert(JsonDbString::kErrorStr, QJsonObject());
     result.insert(JsonDbString::kIdStr, id);
     stream->send(result);
}

void DBServer::debugQuery(const JsonDbQuery &query, int limit, int offset, const JsonDbQueryResult &result)
{
    const QList<JsonDbOrQueryTerm> &orQueryTerms = query.queryTerms;
    if (jsondbSettings->verbose()) {
        qDebug() << JSONDB_INFO << "query:" <<  query.query;
        if (orQueryTerms.size())
            qDebug() << JSONDB_INFO << "query terms:" <<  query.query;
    }
    for (int i = 0; i < orQueryTerms.size(); i++) {
        const JsonDbOrQueryTerm &orQueryTerm = orQueryTerms[i];
        foreach (const JsonDbQueryTerm &queryTerm, orQueryTerm.terms()) {
            if (jsondbSettings->verbose()) {
                qDebug().nospace() << QString("%1%2%3, op:%4, value:")
                                      .arg(queryTerm.propertyName())
                                      .arg(queryTerm.joinField().size() ? "->" : "")
                                      .arg(queryTerm.joinField())
                                      .arg(queryTerm.op())
                                   << query.termValue(queryTerm);
            }
        }
    }

    const QList<JsonDbOrderTerm> &orderTerms = query.orderTerms;
    if (jsondbSettings->verbose() && query.orderTerms.size())
        qDebug() << JSONDB_INFO << "order terms:" <<  query.query;
    for (int i = 0; i < orderTerms.size(); i++) {
        const JsonDbOrderTerm &orderTerm = orderTerms.at(i);
        if (jsondbSettings->verbose())
            qDebug() << QString("%1 %2").arg(orderTerm.propertyName).arg(orderTerm.ascending ? "ascending" : "descending");
    }

    qDebug() << "* limit:" << limit << endl
             << "* offset:" << offset << endl
             << "* results:" << result.data;
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

void DBServer::createNotification(const JsonDbObject &object, ClientJsonStream *stream)
{
    QString uuid = object.value(JsonDbString::kUuidStr).toString();
    QStringList actions = QVariant(object.value(JsonDbString::kActionsStr).toArray().toVariantList()).toStringList();
    QString query = object.value(JsonDbString::kQueryStr).toString();
    QJsonObject bindings = object.value("bindings").toObject();
    QString partitionName = object.value(JsonDbString::kPartitionStr).toString();

    JsonDbQueryParser parser;
    parser.setQuery(query);
    parser.setBindings(bindings);
    parser.parse();
    JsonDbQuery parsedQuery = parser.result();

    JsonDbNotification *n = new JsonDbNotification(getOwner(stream), parsedQuery, actions);
    if (object.contains("initialStateNumber") && object.value("initialStateNumber").isDouble())
         n->setInitialStateNumber(static_cast<qint32>(object.value("initialStateNumber").toDouble()));

    stream->addNotification(uuid, n);

    if (partitionName.isEmpty())
        partitionName = mDefaultPartition->partitionSpec().name;
    JsonDbPartition *partition = findPartition(partitionName);

    if (partition) {
        n->setPartition(partition);

        if (partition->isOpen())
            partition->addNotification(n);
    } else {
        mEphemeralPartition->addNotification(n);
    }
}

void DBServer::removeNotification(const JsonDbObject &object, ClientJsonStream *stream)
{
    QString uuid = object.value(JsonDbString::kUuidStr).toString();
    JsonDbNotification *n = stream->takeNotification(uuid);
    if (!n)
        return;

    if (n->partition())
        n->partition()->removeNotification(n);
    else
        mEphemeralPartition->removeNotification(n);

    delete n;
}

JsonDbError::ErrorCode DBServer::validateNotification(const JsonDbObject &notificationDef, QString &message)
{
    message.clear();

    JsonDbQueryParser parser;
    parser.setQuery(notificationDef.value(JsonDbString::kQueryStr).toString());
    bool ok = parser.parse();
    JsonDbQuery query = parser.result();
    if (!ok || !(query.queryTerms.size() || query.orderTerms.size())) {
        message = QStringLiteral("Missing query: %1").arg(parser.errorString());
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

void DBServer::removeNotificationsByPartition(JsonDbPartition *partition)
{
    QList<JsonDbObject> toRemove;
    QMap<QString, QJsonValue> bindings;
    bindings.insert(QStringLiteral("notification"), JsonDbString::kNotificationTypeStr);
    bindings.insert(QStringLiteral("partition"), partition->partitionSpec().name);

    JsonDbQueryParser parser;
    parser.setQuery(QStringLiteral("[?_type=%notification][?partition=%partition]"));
    parser.setBindings(bindings);
    parser.parse();
    JsonDbQuery query = parser.result();
    JsonDbQueryResult results = mEphemeralPartition->queryObjects(mOwner, query);
    foreach (const JsonDbObject &result, results.data) {
        JsonDbObject notification = result;
        notification.markDeleted();
        toRemove.append(notification);
    }

    mEphemeralPartition->updateObjects(mOwner, toRemove, JsonDbPartition::Replace);

    // remove the notifications from each of the connections
    foreach (ClientJsonStream *stream, mConnections.values()) {
        foreach (JsonDbNotification *n, stream->notificationsByPartition(partition)) {
            stream->removeNotification(n);
            delete n;
        }
    }
}

void DBServer::enableNotificationsByPartition(JsonDbPartition *partition)
{
    // re-install the notifications from each of the connections
    foreach (ClientJsonStream *stream, mConnections.values()) {
        foreach (JsonDbNotification *n, stream->notificationsByPartition(partition))
            partition->addNotification(n);
    }
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

QList<JsonDbPartitionSpec> DBServer::findPartitionDefinitions() const
{
    QList<JsonDbPartitionSpec> partitions;

    bool defaultSpecified = false;

    QSet<QString> names;
    foreach (const QString &path, jsondbSettings->configSearchPath()) {
        QDir searchPath(path);
        if (!searchPath.exists())
            continue;

        if (jsondbSettings->debug())
            qDebug() << JSONDB_INFO << QString("searching %1 for partition definition files").arg(path);

        QStringList files = searchPath.entryList(QStringList() << QStringLiteral("partitions*.json"),
                                                 QDir::Files | QDir::Readable);
        foreach (const QString &file, files) {
            if (jsondbSettings->debug())
                qDebug() << JSONDB_INFO << QString("loading partition definitions from %1").arg(file);

            QFile partitionFile(searchPath.absoluteFilePath(file));
            partitionFile.open(QFile::ReadOnly);
            QByteArray ba = partitionFile.readAll();
            partitionFile.close();

            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(ba, &error);

            if (error.error != QJsonParseError::NoError) {
                qDebug() << JSONDB_WARN << "Couldn't parse configuration file" << partitionFile.fileName()
                         << "at" << error.offset << ":" << error.errorString();
            } else if (!doc.isArray()) {
                qDebug() << JSONDB_WARN << "Couldn't parse configuration file" << partitionFile.fileName()
                         << "at 0 : content should be a JSON array";
            }

            QJsonArray partitionList = doc.array();

            if (partitionList.isEmpty()) {
                if (jsondbSettings->verbose())
                    qDebug() << JSONDB_WARN << QString("%1 is empty").arg(file);
                continue;
            }

            for (int i = 0; i < partitionList.count(); i++) {
                QJsonObject definition = partitionList.at(i).toObject();
                QJsonValue name = definition.value(JsonDbString::kNameStr);
                if (!name.isString()) {
                    qDebug() << JSONDB_WARN << partitionFile.fileName() << ": partition name should be a string, skipping. Type:" << name.type();
                    continue;
                }

                if (jsondbSettings->debug())
                    qDebug() << JSONDB_INFO << "adding partition" << name.toString();

                QJsonValue path = definition.value(JsonDbString::kPathStr);
                if (path.isUndefined() || !path.isString()) {
                    qDebug() << JSONDB_WARN << partitionFile.fileName() << ":" << name.toString()
                             << "partition path should be a string containing directory path, using current directory."
                             << "Setting path to" << QDir::currentPath();
                    path = QDir::currentPath();
                }
                if (!path.isString())
                    definition.insert(JsonDbString::kPathStr, QDir::currentPath());

                QJsonValue isDefault = definition.value(JsonDbString::kDefaultStr);
                if (!isDefault.isUndefined() && !isDefault.isBool())
                    qDebug() << JSONDB_WARN << partitionFile.fileName() << ": partition" << name.toString()
                             << "'default' property should be a boolean";
                if (isDefault.toBool()) {
                    if (defaultSpecified) {
                        qDebug() << JSONDB_WARN << partitionFile.fileName() << ": parition" << name.toString()
                                 << "unset as default. Default already specified.";
                        isDefault = QJsonValue(false);
                    } else {
                        defaultSpecified = true;
                    }
                }

                QJsonValue isRemovable = definition.value(JsonDbString::kRemovableStr);
                if (!isRemovable.isUndefined() && !isRemovable.isBool())
                    qDebug() << JSONDB_WARN << partitionFile.fileName() << ": partition" << name.toString()
                             << "'removable' property should be a boolean";

                JsonDbPartitionSpec spec;
                spec.name = name.toString();
                spec.path = path.toString();
                spec.isDefault = isDefault.toBool();
                spec.isRemovable = isRemovable.toBool();

                if (names.contains(spec.name)) {
                    qDebug() << partitionFile.fileName() << ": parition" << name.toString()
                             << "is a duplicate definition, skipping.";
                    continue;
                }
                names.insert(spec.name);

                if (jsondbSettings->debug())
                    qDebug() << JSONDB_INFO << QString(QLatin1String("appending partition [name:%1, path:%2, removable:%3, default:%4"))
                                .arg(spec.name)
                                .arg(spec.path)
                                .arg(spec.isRemovable)
                                .arg(spec.isDefault);
                partitions.append(spec);
            }
        }

        if (!partitions.isEmpty())
            break;
    }

    // if no partitions are specified just make a partition in the current working
    // directory and call it "default"
    if (partitions.isEmpty()) {
        if (jsondbSettings->debug())
            qDebug() << JSONDB_INFO << "no partitions were defined, creating partition spec \"default\" at"
                     << QDir::currentPath();
        JsonDbPartitionSpec defaultPartition;
        defaultPartition.name = QStringLiteral("default");
        defaultPartition.path = QDir::currentPath();
        defaultPartition.isDefault = true;
        partitions.append(defaultPartition);
        defaultSpecified = true;
    }

    // ensure that at least one partition is marked as default
    if (!defaultSpecified) {
        partitions[0].isDefault = true;
        if (jsondbSettings->debug())
            qDebug() << JSONDB_INFO << QString(QLatin1String("setting %1 as default")).arg(partitions[0].name);
    }

    return partitions;
}

void DBServer::updatePartitionDefinition(JsonDbPartition *partition, bool remove, bool isDefault)
{
    QUuid uuid = JsonDbObject::createUuidFromString(QStringLiteral("Partition:%1").arg(partition->partitionSpec().name));
    JsonDbObject partitionRecord;
    partitionRecord.insert(JsonDbString::kUuidStr, uuid.toString());
    partitionRecord.insert(JsonDbString::kTypeStr, JsonDbString::kPartitionTypeStr);
    partitionRecord.insert(JsonDbString::kNameStr, partition->partitionSpec().name);
    partitionRecord.insert(JsonDbString::kPathStr, QFileInfo(partition->filename()).absolutePath());
    partitionRecord.insert(JsonDbString::kAvailableStr, partition->isOpen());

    if (isDefault)
        partitionRecord.insert(JsonDbString::kDefaultStr, true);

    if (remove)
        partitionRecord.markDeleted();

    mEphemeralPartition->updateObjects(mOwner, JsonDbObjectList() << partitionRecord, JsonDbPartition::Replace);
}

void DBServer::receiveMessage(const QJsonObject &message)
{
    ClientJsonStream *stream = qobject_cast<ClientJsonStream *>(sender());
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
        JsonDbPartition::ConflictResolutionMode writeMode = JsonDbPartition::RejectStale;
        QString conflictModeRequested = message.value(JsonDbString::kConflictResolutionModeStr).toString();

        if (conflictModeRequested == QLatin1String("merge"))
            writeMode = JsonDbPartition::ReplicatedWrite;
        else if (conflictModeRequested == QLatin1String("replace") || !jsondbSettings->rejectStaleUpdates())
            writeMode = JsonDbPartition::Replace;

        // TODO: remove at the same time that clientcompat is dropped
        if (action == JsonDbString::kRemoveStr && object.toObject().contains(JsonDbString::kQueryStr)) {
            JsonDbQueryParser parser;
            parser.setQuery(object.toObject().value(JsonDbString::kQueryStr).toString());
            parser.parse();
            JsonDbQuery parsedQuery = parser.result();
            JsonDbQueryResult res;
            if (partition)
                res = partition->queryObjects(owner, parsedQuery);
            else
                res = mEphemeralPartition->queryObjects(owner, parsedQuery);

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
                              QStringLiteral("Mixing objects of type Notification with others can only be done in the ephemeral partition"), id);
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
        processLog(stream, object.toObject().value(JsonDbString::kMessageStr).toString(), id);
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
        qDebug().nospace() << "+ JsonDB Perf: [id]" << id << "[id]";
        if (mOwners.contains(stream->device())) {
            const OwnerInfo &ownerInfo = mOwners[stream->device()];
            qDebug().nospace() << ":[pid]" << ownerInfo.pid << "[pid]:[process]" << ownerInfo.processName << "[process]";
        }
        qDebug().nospace() << ":[action]" << action
                           << "[action]:[ms]" << timer.elapsed() << "[ms]:[details]" << additionalInfo << "[details]"
                           << ":[partition]" << partitionName << "[partition]"
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

    if (mConnections.contains(connection)) {
        ClientJsonStream *stream = mConnections.value(connection);

        QList<JsonDbNotification *>  notifications = stream->takeAllNotifications();
        foreach (JsonDbNotification *n, notifications) {
            if (n->partition()) {
                n->partition()->removeNotification(n);
            } else {
                mEphemeralPartition->removeNotification(n);
            }
            n->deleteLater();
        }

        if (stream)
            stream->deleteLater();

        mConnections.remove(connection);
    }
    if (mOwners.contains(connection)) {
        JsonDbOwner *owner = mOwners.value(connection).owner;
        if (owner)
            owner->deleteLater();
        mOwners.remove(connection);
    }

    connection->deleteLater();
}

#include "moc_dbserver.cpp"
