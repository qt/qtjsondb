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

#include <QtCore>
#include <QtNetwork>
#include <QElapsedTimer>

#include <json.h>

#include "jsondb-strings.h"
#include "jsondb-error.h"

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

QT_BEGIN_NAMESPACE_JSONDB

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

DBServer::DBServer(const QString &filePath, const QString &baseName, QObject *parent)
    : QObject(parent),
      mDefaultPartition(0),
      mEphemeralPartition(0),
      mTcpServerPort(0),
      mServer(0),
      mTcpServer(0),
      mOwner(new JsonDbOwner(this)),
      mFilePath(filePath),
      mBaseName(baseName)
{

    QFileInfo info(filePath);

    if (QString::compare(info.suffix(), QLatin1String("db"), Qt::CaseInsensitive) == 0) {
        mFilePath = info.absolutePath();
        if (mBaseName.isEmpty())
            mBaseName = info.baseName();
    }

    if (mFilePath.isEmpty())
        mFilePath = QDir::currentPath();
    if (mBaseName.isEmpty())
        mBaseName = QLatin1String("default.System");
    if (!mBaseName.endsWith(QLatin1String(".System")))
        mBaseName += QLatin1String(".System");

    QDir(mFilePath).mkpath(QString("."));

    mOwner->setAllowAll(true);
}

void DBServer::sigHUP()
{
    if (jsondbSettings->debug())
        qDebug() << "SIGHUP received";
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

    QCoreApplication::exit();
}

bool DBServer::loadPartitions()
{
    if (!mEphemeralPartition) {
        mEphemeralPartition = new JsonDbEphemeralPartition("Ephemeral", this);
        connect(mEphemeralPartition, SIGNAL(objectsUpdated(JsonDbUpdateList)),
                this, SLOT(objectsUpdated(JsonDbUpdateList)));
    }

    QHash<QString, JsonDbPartition*> oldPartitions = mPartitions;
    oldPartitions.remove(mBaseName);

    if (!mDefaultPartition) {
        mDefaultPartition = new JsonDbPartition(QDir(mFilePath).absoluteFilePath(mBaseName + QLatin1String(".db")),
                                                mBaseName, mOwner, this);
        connect(mDefaultPartition, SIGNAL(objectsUpdated(JsonDbUpdateList)),
                            this, SLOT(objectsUpdated(JsonDbUpdateList)));

        if (!mDefaultPartition->open())
            return false;

        mPartitions[mBaseName] = mDefaultPartition;
    }

    JsonDbQueryResult partitions = mDefaultPartition->queryObjects(mOwner, JsonDbQuery::parse(QLatin1String("[?_type=\"Partition\"]")));

    foreach (const JsonDbObject &partition, partitions.data) {
        if (partition.contains(JsonDbString::kNameStr)) {
            QString name = partition.value(JsonDbString::kNameStr).toString();

            if (!mPartitions.contains(name)) {
                QString filename = partition.contains(QLatin1String("file")) ?
                            partition.value(QLatin1String("file")).toString() :
                            QDir(mFilePath).absoluteFilePath(name + QLatin1String(".db"));
                JsonDbPartition *p = new JsonDbPartition(filename, name, mOwner, this);
                connect(p, SIGNAL(objectsUpdated(JsonDbUpdateList)),
                        this, SLOT(objectsUpdated(JsonDbUpdateList)));

                if (!p->open())
                    return false;

                mPartitions[name] = p;
            }

            oldPartitions.remove(name);
        }
    }

    // close any partitions that were declared previously but are no longer present
    foreach (JsonDbPartition *p, oldPartitions.values())
        p->close();

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
        JsonStream *stream = new JsonStream(connection, this);
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
        JsonStream *stream = new JsonStream(connection, this);
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

    if (sender() == mEphemeralPartition) {
        partitionName = mEphemeralPartition->name();
    } else {
        partition = qobject_cast<JsonDbPartition*>(sender());
        if (partition)
            partitionName = partition->name();
        else
            return;
    }

    // FIXME: pretty good place to batch notifications
    foreach (const JsonDbUpdate &updated, objects) {

        JsonDbObject oldObject = updated.oldObject;
        JsonDbObject object = updated.newObject;
        JsonDbNotification::Action action = updated.action;

        // no notifications on notification
        if (object.type() == JsonDbString::kNotificationTypeStr)
            continue;

        QString objectType = object.value(JsonDbString::kTypeStr).toString();
        quint32 stateNumber;
        if (partition) {
            JsonDbObjectTable *objectTable = partition->findObjectTable(objectType);
            stateNumber = objectTable->stateNumber();
        } else
            stateNumber = mDefaultPartition->mainObjectTable()->stateNumber();

        QStringList notificationKeys;
        if (object.contains(JsonDbString::kTypeStr)) {
            notificationKeys << objectType;
            if (mEagerViewSourceTypes.contains(objectType)) {
                const QSet<QString> &targetTypes = mEagerViewSourceTypes[objectType];
                for (QSet<QString>::const_iterator it = targetTypes.begin(); it != targetTypes.end(); ++it) {
                    if (partition)
                        partition->updateView(*it);
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
            QString actionStr = (effectiveAction == JsonDbNotification::Create ? JsonDbString::kCreateStr :
                                 (effectiveAction == JsonDbNotification::Update ? JsonDbString::kUpdateStr :
                                  JsonDbString::kRemoveStr));
            notified(n->uuid(), stateNumber, r, actionStr);
        }
    }
}

void DBServer::processWrite(JsonStream *stream, JsonDbOwner *owner, const JsonDbObjectList &objects,
                            JsonDbPartition::WriteMode mode, const QString &partitionName,  int id)
{
    QJsonObject response;
    response.insert(JsonDbString::kIdStr, id);

    JsonDbWriteResult res = partitionName == mEphemeralPartition->name() ?
                mEphemeralPartition->updateObjects(owner, objects, mode) :
                mPartitions.value(partitionName, mDefaultPartition)->updateObjects(owner, objects, mode);

    if (res.code != JsonDbError::NoError) {
        QJsonObject error;
        error.insert(JsonDbString::kCodeStr, res.code);
        error.insert(JsonDbString::kMessageStr, res.message);
        response.insert(JsonDbString::kErrorStr, error);
        response.insert(JsonDbString::kResultStr, QJsonValue());
    } else {
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

            // handle partitions
            } else if (object.type() == JsonDbString::kPartitionTypeStr) {
                loadPartitions();
            }
        }

        QJsonObject result;
        result.insert(JsonDbString::kDataStr, data);
        result.insert(JsonDbString::kCountStr, data.count());
        result.insert(JsonDbString::kStateNumberStr, static_cast<int>(res.state));
        response.insert(JsonDbString::kResultStr, result);
        response.insert(JsonDbString::kErrorStr, QJsonValue());
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
        if (queryResult.values.size()) {
            result.insert(JsonDbString::kDataStr, queryResult.values);
            result.insert(JsonDbString::kLengthStr, queryResult.values.size());
        } else {
            QJsonArray values;
            for (int i = 0; i < queryResult.data.size(); i++) {
                JsonDbObject d = queryResult.data.at(i);
                values.append(d);
            }
            result.insert(JsonDbString::kDataStr, values);
            result.insert(JsonDbString::kLengthStr, values.size());
        }
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
            if (jsondbSettings->verbose())
                qDebug() << __FILE__ << __LINE__
                         << QString("    %1%2%3 %4 %5    ")
                             .arg(queryTerm.propertyName())
                             .arg(queryTerm.joinField().size() ? "->" : "")
                             .arg(queryTerm.joinField())
                             .arg(queryTerm.op())
                             .arg(JsonWriter().toString(queryTerm.value().toVariant()));
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
    QString partition = object.value(JsonDbString::kPartitionStr).toString();
    quint32 stateNumber = object.value("initialStateNumber").toDouble();
    if (partition.isEmpty())
        partition = mDefaultPartition->name();

    JsonDbNotification *n = new JsonDbNotification(getOwner(stream), uuid, query, actions, partition);
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
        updateEagerViewTypes(objectType, mPartitions.value(partition, mDefaultPartition), stateNumber);

    if (stateNumber)
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

        delete n;
    }
}

void DBServer::notifyHistoricalChanges(JsonDbNotification *n)
{
    JsonDbPartition *partition = findPartition(n->partition());
    quint32 stateNumber = n->initialStateNumber();
    quint32 lastStateNumber = stateNumber;
    JsonDbQuery *parsedQuery = n->parsedQuery();
    QSet<QString> matchedTypes = parsedQuery->matchedTypes();
    bool matchAnyType = matchedTypes.isEmpty();
    if (stateNumber == static_cast<quint32>(-1)) {
        QString indexName = JsonDbString::kTypeStr;
        if (matchAnyType) {
            matchedTypes.insert(QString());
            // faster to walk the _uuid index if no type is specified
            indexName = JsonDbString::kUuidStr;
        }
        foreach (const QString matchedType, matchedTypes) {
            JsonDbObjectTable *objectTable = partition->findObjectTable(matchedType);
            lastStateNumber = objectTable->stateNumber();
            JsonDbIndexQuery *indexQuery = JsonDbIndexQuery::indexQuery(partition, objectTable,
                                                                        indexName, QString("string"),
                                                                        n->owner());
            if (!matchAnyType) {
                indexQuery->setMin(matchedType);
                indexQuery->setMax(matchedType);
            }

            JsonDbObject oldObject;
            for (JsonDbObject o = indexQuery->first(); !o.isEmpty(); o = indexQuery->next()) {
                JsonDbNotification::Action action = JsonDbNotification::Create;
                objectUpdated(partition->name(), stateNumber, n, action, oldObject, o);
            }
        }
    } else {
        QJsonObject changesSince = partition->changesSince(stateNumber, matchedTypes);
        QJsonObject changes(changesSince.value("result").toObject());
        lastStateNumber = changes.value("currentStateNumber").toDouble();
        QJsonArray changeList(changes.value("changes").toArray());
        quint32 count = changeList.size();
        for (quint32 i = 0; i < count; i++) {
            QJsonObject change = changeList.at(i).toObject();
            QJsonObject before = change.value("before").toObject();
            QJsonObject after = change.value("after").toObject();

            JsonDbNotification::Action action = JsonDbNotification::Update;
            if (before.isEmpty())
                action = JsonDbNotification::Create;
            else if (after.contains(JsonDbString::kDeletedStr))
                action = JsonDbNotification::Delete;
            objectUpdated(partition->name(), stateNumber, n, action, before, after);
        }
    }
    QJsonObject stateChange;
    stateChange.insert("_state", static_cast<int>(lastStateNumber));
    emit notified(n->uuid(), stateNumber, stateChange, "stateChange");
}

void DBServer::updateEagerViewTypes(const QString &objectType, JsonDbPartition *partition, quint32 stateNumber)
{
    // FIXME: eager view types should be broken down by partition
    JsonDbView *view = partition->findView(objectType);
    if (!view)
        return;
    foreach (const QString sourceType, view->sourceTypes()) {
        mEagerViewSourceTypes[sourceType].insert(objectType);
        // now recurse until we get to a non-view sourceType
        updateEagerViewTypes(sourceType, partition, stateNumber);
    }
    partition->updateView(objectType, stateNumber);
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

void DBServer::receiveMessage(const QJsonObject &message)
{
    JsonStream *stream = qobject_cast<JsonStream *>(sender());
    QString  action = message.value(JsonDbString::kActionStr).toString();
    QJsonValue object = message.value(JsonDbString::kObjectStr);
    int id = message.value(JsonDbString::kIdStr).toDouble();
    QString partitionName = message.value(JsonDbString::kPartitionStr).toString();

    JsonDbPartition *partition = findPartition(partitionName);
    if (!(partitionName.isEmpty() || partition || partitionName == mEphemeralPartition->name())) {
        sendError(stream, JsonDbError::InvalidRequest,
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
            JsonDbQuery *query = JsonDbQuery::parse(object.toObject().value(JsonDbString::kQueryStr).toString());
            JsonDbQueryResult res;
            if (partition)
                res = partition->queryObjects(owner, query);
            else
                res = mEphemeralPartition->queryObjects(owner, query);

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

QT_END_NAMESPACE_JSONDB
