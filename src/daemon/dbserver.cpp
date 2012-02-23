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
#include "jsondb.h"
#include "jsondbpartition.h"
#include "dbserver.h"

#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#endif

QT_BEGIN_NAMESPACE_JSONDB

#ifndef QT_NO_DEBUG_OUTPUT
extern bool gDebug;
#define DBG() if (gDebug) qDebug() << Q_FUNC_INFO
#else
#define DBG() if (0) qDebug() << Q_FUNC_INFO
#endif

static int gReadBufferSize = 65536;

/*********************************************/

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

    DBG() << "Sending error" << map;
    stream->send(map);
}

/*********************************************/
static int jsondbdocumentid = qRegisterMetaType<JsonDbObject>("JsonDbObject");

DBServer::DBServer(const QString &filePath, const QString &baseName, QObject *parent)
    : QObject(parent),
      mTcpServerPort(0),
      mServer(NULL),
      mTcpServer(NULL),
      mJsonDb(NULL),
      mFilePath(filePath),
      mBaseName(baseName)
{
    if (mFilePath.isEmpty()) {
        QDir defaultDir(QDir::homePath() + QDir::separator() + QLatin1String(".jsondb"));
        if (!defaultDir.exists())
            defaultDir.mkpath(defaultDir.path());
        mFilePath = defaultDir.path();
    }
}

bool DBServer::socket()
{
    QString socketName = ::getenv("JSONDB_SOCKET");
    if (socketName.isEmpty())
        socketName = "qt5jsondb";
    DBG() << "Listening on socket" << socketName;
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

bool DBServer::start(bool compactOnClose)
{
    QElapsedTimer timer;
    if (gPerformanceLog)
        timer.start();

    QString username;
#if defined(Q_OS_UNIX)
    struct passwd *pwdent = ::getpwent();
    username = QString::fromLocal8Bit(pwdent->pw_name);
#else
    username = QStringLiteral("com.example.JsonDb");
#endif
    if (mBaseName.isEmpty()) {
        int pos = username.lastIndexOf(QLatin1Char('.'));
        if (pos > 1)
            mBaseName = username.left(pos);
        else
            mBaseName = QStringLiteral("database");
    }

    mJsonDb = new JsonDb(mFilePath, mBaseName, username, this);
    mJsonDb->setCompactOnClose(compactOnClose);
    if (!connect(mJsonDb, SIGNAL(notified(QString,JsonDbObject,QString)),
                 this, SLOT(notified(QString,JsonDbObject,QString)),
                 Qt::QueuedConnection))
        qWarning() << "DBServer::start - failed to connect SIGNAL(notified)";
    if (!connect(mJsonDb, SIGNAL(requestViewUpdate(QString,QString)),
                 this, SLOT(updateView(QString,QString)),
                 Qt::QueuedConnection))
        qWarning() << "DBServer::start - failed to connect SIGNAL(requestViewUpdate)";

    if (!mJsonDb->open()) {
      qCritical() << "DBServer::start - Unable to open database";
      return false;
    }
    if (gPerformanceLog) {
        JsonDbStat stat = mJsonDb->stat();
        qDebug().nospace() << "+ JsonDB Perf: " << "[action]" << "start" << "[action]:[ms]" << timer.elapsed()
                           << "[ms]:[reads]" << stat.reads << "[reads]:[hits]" << stat.hits
                           << "[hits]:[writes]" << stat.writes << "[writes]";
    }
    return true;
}

bool DBServer::clear()
{
    return mJsonDb->clear();
}

void DBServer::sigHUP()
{
    DBG() << "SIGHUP received";
    mJsonDb->reduceMemoryUsage();
}

void DBServer::sigTerm()
{
    DBG() << "SIGTERM received";
    mJsonDb->close();
    QCoreApplication::exit();
}

void DBServer::sigINT()
{
    DBG() << "SIGINT received";
    mJsonDb->close();
    QCoreApplication::exit();
}

void DBServer::handleConnection()
{
    if (QIODevice *connection = mServer->nextPendingConnection()) {
        qobject_cast<QLocalSocket *>(connection)->setReadBufferSize(gReadBufferSize);

        //connect(connection, SIGNAL(error(QLocalSocket::LocalSocketError)),
        //        this, SLOT(handleConnectionError()));
        DBG() << "client connected to jsondb server" << connection;
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
        //connect(connection, SIGNAL(error(QLocalSocket::LocalSocketError)),
        //        this, SLOT(handleConnectionError()));
        DBG() << "remote client connected to jsondb server" << connection;
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

    if (!gEnforceAccessControlPolicies) {
        // We are not enforcing policies here, allow requests
        // from all applications.
        // ### TODO: We will have to remove this afterwards
        return createDummyOwner(stream);
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
        struct passwd *pwd = ::getpwuid(peercred.uid);
        if (!pwd) {
            qWarning() << Q_FUNC_INFO << "pwd entry for" << peercred.uid <<
                          "not found" << strerror(errno);
            return 0;
        }
        QString username = QString::fromLocal8Bit(pwd->pw_name);
        // OwnerId == username
        owner->setOwnerId(username);

        // Parse domain from username
        // TODO: handle reverse domains like co.uk.foo, co.au.bar, co.nz.foo , .... correctly
        QStringList domainParts = username.split(QLatin1Char('.'), QString::SkipEmptyParts);
        if (domainParts.count() > 2)
            owner->setDomain (domainParts.at(0)+QLatin1Char('.')+domainParts.at(1));
        else
            owner->setDomain(QStringLiteral("public"));
        DBG() << "username" << username << "uid" << peercred.uid << "domain set to" <<
                 owner->domain();

        // Get capabilities from supplementary groups
        if (peercred.uid) {
            int ngroups = 128;
            gid_t groups[128];
            bool setOwner = false;
            QJsonObject capabilities;
            if (::getgrouplist(pwd->pw_name, pwd->pw_gid, groups, &ngroups) != -1) {
                struct group *gr;
                for (int i = 0; i < ngroups; i++) {
                    gr = ::getgrgid(groups[i]);
                    if (gr && ::strcasecmp (gr->gr_name, "identity") == 0)
                        setOwner = true;
                }
                // Start from 1 to omit the primary group
                for (int i = 1; i < ngroups; i++) {
                    gr = ::getgrgid(groups[i]);
                    QJsonArray value;
                    if (!gr || ::strcasecmp (gr->gr_name, "identity") == 0)
                        continue;
                    if (setOwner)
                        value.append(QJsonValue(QLatin1String("setOwner")));
                    value.append(QJsonValue(QLatin1String("rw")));
                    capabilities.insert(QString::fromLocal8Bit(gr->gr_name), value);
                    DBG() << "Adding capability" << QString::fromLocal8Bit(gr->gr_name) <<
                             "to user" << owner->ownerId() << "setOwner =" << setOwner;
                }
                if (ngroups)
                    owner->setCapabilities(capabilities, mJsonDb);
            } else {
                qWarning() << Q_FUNC_INFO << owner->ownerId() << "belongs to too many groups (>128)";
            }
        } else {
            // root can access all
            owner->setAllowAll(true);
            owner->setStorageQuota(-1);
        }

        // Read quota from security object
        GetObjectsResult result = mJsonDb->getObjects(JsonDbString::kTypeStr, QString("Quota"));
        JsonDbObjectList securityObjects;
        for (int i = 0; i < result.data.size(); i++) {
            JsonDbObject doc = result.data.at(i);
            if (doc.value(JsonDbString::kTokenStr).toString() == username)
                securityObjects.append(doc);
        }
        if (securityObjects.size() == 1) {
            QJsonObject securityObject = securityObjects.at(0);
            QJsonObject capabilities = securityObject.value("capabilities").toObject();
            QStringList keys = capabilities.keys();
            if (keys.contains("quotas")) {
                QJsonObject quotas = capabilities.value("quotas").toObject();
                int storageQuota = quotas.value("storage").toDouble();
                owner->setStorageQuota(storageQuota);
            }
        } else if (!securityObjects.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "Wrong number of security objects found." << securityObjects.size();
            return 0;
        }

        mOwners[stream->device()] = owner.take();
    }
#else
    // security hole
    // TODO: Mac socket authentication
    if (!mOwners.contains(device)) {
        JsonDbOwner *owner = new JsonDbOwner(this);
        owner->setOwnerId(QString::fromLatin1("unknown app %1").arg((intptr_t)stream));
        owner->setDomain("unknown app domain");
        mOwners[device] = owner;
    }
#endif
    Q_ASSERT(mOwners[device]);
    return mOwners[device];
}

void DBServer::notified(const QString &notificationId, JsonDbObject object, const QString &action)
{
    DBG() << "notificationId" << notificationId << "object" << object;
    JsonStream *stream = mNotifications.value(notificationId);
    // if the notified signal() is delivered after the notification has been deleted,
    // then there is no stream to send to
    if (!stream)
        return;

    QJsonObject map, obj;
    obj.insert( JsonDbString::kObjectStr, object );
    obj.insert( JsonDbString::kActionStr, action );
    map.insert( JsonDbString::kNotifyStr, obj );
    map.insert( JsonDbString::kUuidStr, notificationId );

    if (stream && stream->device() && stream->device()->isWritable()) {
        DBG() << "Sending notify" << map;
        stream->send(map);
    }
    else {
        mNotifications.remove(notificationId);
        DBG() << "Invalid stream" << static_cast<void*>(stream);
    }
}

void DBServer::updateView( const QString &viewType, const QString &partitionName )
{
    mJsonDb->updateView(viewType, partitionName);
}

void DBServer::processCreate(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName, JsonDb::WriteMode writeMode)
{
    QJsonObject result;
    switch (object.type()) {
    case QJsonValue::Array: {
        // TODO:  Properly handle creating notifications from a list
        QJsonArray list = object.toArray();
        JsonDbObjectList olist;
        for (int i = 0; i < list.size(); i++) {
            QJsonObject o(list.at(i).toObject());
            olist.append(o);
        }
        result = mJsonDb->createList(owner, olist, partitionName, writeMode);
        break; }
    case QJsonValue::Object: {
        QJsonObject o = object.toObject();
        JsonDbObject document(o);

        result = mJsonDb->create(owner, document, partitionName, writeMode);
        QString uuid = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kUuidStr).toString();
        if (!uuid.isEmpty()
              // && partitionName == mJsonDb->mEphemeralPartitionName ### TODO: uncomment me
            && document.value(JsonDbString::kTypeStr) == JsonDbString::kNotificationTypeStr) {
            if (stream->device() && mConnections.value(stream->device()) == stream)
                mNotifications.insert(uuid, stream);
        }
        break; }
    default:
        result = createError( JsonDbError::MissingObject, "Create requires list or object" );
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processUpdate(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object,
                             int id, const QString &partitionName, JsonDb::WriteMode writeMode)
{
    QJsonObject result;
    switch (object.type()) {
    case QJsonValue::Array: {
        // TODO:  Properly handle updating notifications from a list
        QJsonArray list = object.toArray();
        JsonDbObjectList olist;
        for (int i = 0; i < list.size(); i++) {
            QJsonObject o(list.at(i).toObject());
            olist.append(o);
        }
        result = mJsonDb->updateList(owner, olist, partitionName, writeMode);
        break; }
    case QJsonValue::Object: {
        QJsonObject o = object.toObject();
        JsonDbObject document(o);

        QString partition = partitionName;
        // Always use ephemeral partition for notifications
        if (document.value(JsonDbString::kTypeStr).toString() == JsonDbString::kNotificationTypeStr)
            partition = mJsonDb->ephemeralPartitionName();

        // The user could be changing a _type='notification' object to some other type
        QString uuid = document.value(JsonDbString::kUuidStr).toString();
        if (!uuid.isEmpty() && partition == mJsonDb->ephemeralPartitionName())
            mNotifications.remove(uuid);

        result = mJsonDb->update(owner, document, partitionName, writeMode);
        if (result.contains(JsonDbString::kErrorStr)
                && (!result.value(JsonDbString::kErrorStr).isNull())
                && !result.value(JsonDbString::kErrorStr).toObject().size())
            qCritical() << "UPDATE:" << document << " Error Message : " << result.value(JsonDbString::kErrorStr).toObject().value(JsonDbString::kMessageStr).toString();

        if (!uuid.isEmpty() && partition == mJsonDb->ephemeralPartitionName() &&
                document.value(JsonDbString::kTypeStr) == JsonDbString::kNotificationTypeStr) {
            if (stream->device() && mConnections.value(stream->device()) == stream)
                mNotifications.insert(uuid, stream);
        }
        break; }
    default:
        result = createError(JsonDbError::MissingObject, "Update requires list or object");
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processRemove(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName, JsonDb::WriteMode writeMode)
{
    QJsonObject result;
    switch (object.type()) {
    case QJsonValue::Array: {
        // TODO: Properly handle removing notifications from a list
        QJsonArray list = object.toArray();
        JsonDbObjectList olist;
        for (int i = 0; i < list.size(); i++) {
            QJsonObject o(list.at(i).toObject());
            olist.append(o);
        }
        result = mJsonDb->removeList(owner, olist, partitionName, writeMode);
    } break;
    case QJsonValue::Object: {
        QJsonObject o = object.toObject();
        if (o.contains(JsonDbString::kQueryStr)
            && !o.contains(JsonDbString::kUuidStr)) {
            QJsonObject query = o;
            JsonDbQueryResult queryResult = mJsonDb->find(owner, query, partitionName);
            if (queryResult.error.type() == QJsonValue::Object) {
                result = createError((JsonDbError::ErrorCode)queryResult.error.toObject().value("code").toDouble(),
                                     queryResult.error.toObject().value("message").toString());
                break;
            }
            JsonDbObjectList itemsToRemove = queryResult.data;
            result = mJsonDb->removeList(owner, itemsToRemove, partitionName, writeMode);
        } else {
            JsonDbObject document(o);        // removing a single item
            QString uuid = document.value(JsonDbString::kUuidStr).toString();
            mNotifications.remove(uuid);
            result = mJsonDb->remove(owner, document, partitionName);
        }
    } break;
    default:
        result = createError(JsonDbError::MissingObject, "Remove requires object, list of objects, or query string");
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processFind(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName)
{
    QJsonObject response;
    switch (object.type()) {
    case QJsonValue::Object: {
        JsonDbQueryResult findResult = mJsonDb->find(owner, object.toObject(), partitionName);
        if (findResult.error.type() != QJsonValue::Null) {
            response.insert(JsonDbString::kErrorStr, findResult.error);
            response.insert(JsonDbString::kResultStr, QJsonValue());
        } else {
            QJsonObject result;
            if (findResult.values.size()) {
                result.insert(JsonDbString::kDataStr, findResult.values);
                result.insert(JsonDbString::kLengthStr, findResult.values.size());
            } else {
                QJsonArray values;
                for (int i = 0; i < findResult.data.size(); i++) {
                    JsonDbObject d = findResult.data.at(i);
                    values.append(d);
                }
                result.insert(JsonDbString::kDataStr, values);
                result.insert(JsonDbString::kLengthStr, values.size());
            }
            result.insert(JsonDbString::kOffsetStr, findResult.offset);
            result.insert(JsonDbString::kExplanationStr, findResult.explanation);
            result.insert("sortKeys", findResult.sortKeys);
            result.insert("state", findResult.state);
            response.insert(JsonDbString::kResultStr, result);
            response.insert(JsonDbString::kErrorStr, QJsonValue());
        }
    } break;
    default:
        response = createError(JsonDbError::InvalidRequest, "Invalid find object");
        break;
    }
    response.insert( JsonDbString::kIdStr, id );
    stream->send(response);
}

void DBServer::processChangesSince(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName)
{
    QJsonObject request(object.toObject());
    QJsonObject result;
    switch (object.type()) {
    case QJsonValue::Object:
        result = mJsonDb->changesSince(owner, request, partitionName);
        break;
    default:
        result = createError(JsonDbError::InvalidRequest, "Invalid find object");
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

// This will create a dummy owner. Used only when the policies are not enforced.
// When policies are not enforced the JsonDbOwner allows access to everybody.
JsonDbOwner *DBServer::createDummyOwner( JsonStream *stream)
{
    if (gEnforceAccessControlPolicies)
        return 0;

    JsonDbOwner *owner = mOwners.value(stream->device());
    if (owner)
        return owner;

    owner = new JsonDbOwner(this);
    owner->setOwnerId(QString("unknown app"));
    owner->setDomain(QString("unknown domain"));
    QJsonObject capabilities;
    owner->setCapabilities(capabilities, mJsonDb);
    mOwners[stream->device()] = owner;

    return mOwners[stream->device()];
}

void DBServer::processToken(JsonStream *stream, const QJsonValue &object, int id)
{
    // Always succeed. Authentication is done using the socket peer credentials in getOwner()
    Q_UNUSED(object);
    QJsonObject resultObject;
    resultObject.insert(JsonDbString::kResultStr, QLatin1String("Okay"));
    QJsonObject result;
    result.insert(JsonDbString::kResultStr, resultObject);
    result.insert(JsonDbString::kErrorStr, QJsonValue(QJsonValue::Null));
    result.insert(JsonDbString::kIdStr, id);
    stream->send(result);
}

void DBServer::receiveMessage(const QJsonObject &message)
{
    JsonStream *stream = qobject_cast<JsonStream *>(sender());
    QString  action = message.value(JsonDbString::kActionStr).toString();
    QJsonValue object = message.value(JsonDbString::kObjectStr);
    int id = message.value(JsonDbString::kIdStr).toDouble();
    QString partitionName = message.value(JsonDbString::kPartitionStr).toString();

    QElapsedTimer timer;
    QHash<QString, qint64> fileSizes;

    JsonDbStat startStat;
    if (gPerformanceLog) {
        if (gVerbose)
            fileSizes = mJsonDb->fileSizes(partitionName);
        startStat = mJsonDb->stat();
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

    if (action == JsonDbString::kUpdateStr || action == JsonDbString::kRemoveStr || action == JsonDbString::kCreateStr) {
        JsonDb::WriteMode writeMode = JsonDb::DefaultWrite;
        QString writeModeRequested = message.value(QStringLiteral("writeMode")).toString();

        if (writeModeRequested == QStringLiteral("optimistic"))
            writeMode = JsonDb::OptimisticWrite;
        else if (writeModeRequested == QStringLiteral("forced"))
            writeMode = JsonDb::ForcedWrite;
        else if (writeModeRequested == QStringLiteral("replicated"))
            writeMode = JsonDb::ReplicatedWrite;

        if (action == JsonDbString::kUpdateStr)
            processUpdate(stream, owner, object, id, partitionName, writeMode);
        else if (action == JsonDbString::kRemoveStr)
            processRemove(stream, owner, object, id, partitionName, writeMode);
        else if (action == JsonDbString::kCreateStr)
            processCreate(stream, owner, object, id, partitionName, writeMode);
    } else if (action == JsonDbString::kFindStr) {
        processFind(stream, owner, object, id, partitionName);
    } else if (action == JsonDbString::kTokenStr) {
        processToken(stream, object, id);
    } else if (action == JsonDbString::kChangesSinceStr) {
        processChangesSince(stream, owner, object, id, partitionName);
    } else {
        const QMetaObject *mo = mJsonDb->metaObject();
        QJsonObject result;
        // DBG() << QString("using QMetaObject to invoke method %1").arg(action);
        if (mo->invokeMethod(mJsonDb, action.toLatin1().data(),
                             Qt::DirectConnection,
                             Q_RETURN_ARG(QJsonObject, result),
                             Q_ARG(JsonDbOwner *, owner),
                             Q_ARG(QJsonValue, object.toObject()))) {
            if (!result.isEmpty()) {
                result.insert(JsonDbString::kIdStr, id);
                stream->send(result);
            }
        } else {
            sendError(stream, JsonDbError::InvalidRequest,
                      QString("Invalid request '%1'").arg(action), id);
            return;
        }
    }
    if (gPerformanceLog) {
        QString additionalInfo;
        JsonDbStat stat = mJsonDb->stat();
        stat -= startStat;
        if ( action == JsonDbString::kFindStr ) {
            additionalInfo = object.toObject().value("query").toString();
        } else if (object.type() == QJsonValue::Array) {
            additionalInfo = QString::fromLatin1("%1 objects").arg(object.toArray().size());
        } else {
            additionalInfo = object.toObject().value(JsonDbString::kTypeStr).toString();
        }
        qDebug().nospace() << "+ JsonDB Perf: [id]" << id << "[id]:[action]" << action
                           << "[action]:[ms]" << timer.elapsed() << "[ms]:[details]" << additionalInfo << "[details]"
                           << ":[reads]" << stat.reads << "[reads]:[hits]" << stat.hits << "[hits]:[writes]" << stat.writes << "[writes]";
        if (gVerbose) {
            QHash<QString, qint64> newSizes = mJsonDb->fileSizes(partitionName);
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
            mJsonDb->remove(owner, notificationObject, mJsonDb->ephemeralPartitionName());
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
}

#include "moc_dbserver.cpp"

QT_END_NAMESPACE_JSONDB
