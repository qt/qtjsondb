/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
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
#include "jsondbbtreestorage.h"
#include "dbserver.h"

#ifdef Q_OS_UNIX
#include <sys/socket.h>
#endif

namespace QtAddOn { namespace JsonDb {

#ifndef QT_NO_DEBUG_OUTPUT
extern bool gDebug;
#define DBG() if (gDebug) qDebug() << Q_FUNC_INFO
#else
#define DBG() if (0) qDebug() << Q_FUNC_INFO
#endif

/*********************************************/

#if 0
static void sendResult( QsonStream *stream, const QVariant& result, const QVariant& id )
{
    QsonObject map;
    map.insert( JsonDbString::kResultStr, result);
    map.insert( JsonDbString::kErrorStr, QVariant());
    map.insert( JsonDbString::kIdStr, id );
    stream->send(map);
}
#endif

static QsonMap createError( JsonDbError::ErrorCode code, const QString& message )
{
    QsonMap map;
    map.insert(JsonDbString::kResultStr, QsonMap::NullValue);
    QsonMap errormap;
    errormap.insert(JsonDbString::kCodeStr, (int)code);
    errormap.insert(JsonDbString::kMessageStr, message);
    map.insert( JsonDbString::kErrorStr, errormap );
    return map;
}

static void sendError( QsonStream *stream, JsonDbError::ErrorCode code,
                       const QString& message, int id )
{
    QsonMap map;
    map.insert( JsonDbString::kResultStr, QsonObject::NullValue);
    QsonMap errormap;
    errormap.insert(JsonDbString::kCodeStr, code);
    errormap.insert(JsonDbString::kMessageStr, message);
    map.insert( JsonDbString::kErrorStr, errormap );
    map.insert( JsonDbString::kIdStr, id );

    DBG() << "Sending error" << map;
    stream->send(map);
}

/*********************************************/

DBServer::DBServer(const QString &fileName, QObject *parent)
    : QObject(parent),
      mTcpServerPort(0),
      mServer(NULL),
      mTcpServer(NULL),
      mJsonDb(NULL),
      mFileName(fileName)
{
    mMasterToken = ::getenv("JSONDB_TOKEN");
    DBG() << "Master JSON-DB token" << mMasterToken;

    if (mFileName.isEmpty()) {
        QDir defaultDir(QDir::homePath() + QDir::separator() + QLatin1String(".jsondb"));
        if (!defaultDir.exists())
            defaultDir.mkpath(defaultDir.path());
        mFileName = defaultDir.path();
        mFileName += QDir::separator() + QLatin1String("database.db");
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
    if (!mServer->listen(socketName))
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
    mJsonDb = new JsonDb(mFileName, this);

    if (!connect(mJsonDb, SIGNAL(notified(QString,QsonMap,QString)),
                 this, SLOT(notified(QString,QsonMap,QString)),
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
    return true;
}

bool DBServer::clear()
{
    return mJsonDb->clear();
}

void DBServer::load(const QString &jsonFileName)
{
    mJsonDb->load(jsonFileName);
}

void DBServer::sigHUP()
{
    DBG() << "SIGHUP received";
    mJsonDb->checkValidity();
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
        //connect(connection, SIGNAL(error(QLocalSocket::LocalSocketError)),
        //        this, SLOT(handleConnectionError()));
        DBG() << "client connected to jsondb server" << connection;
        connect(connection, SIGNAL(disconnected()), this, SLOT(removeConnection()));
        QsonStream *stream = new QsonStream(connection, this);
        connect(stream, SIGNAL(receive(QsonObject)), this,
                SLOT(receiveMessage(QsonObject)));
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
        QsonStream *stream = new QsonStream(connection, this);
        connect(stream, SIGNAL(receive(QsonObject)), this,
                SLOT(receiveMessage(QsonObject)));
        mConnections.insert(connection, stream);
    }
}

JsonDbOwner *DBServer::getOwner(QsonStream *stream)
{
    QIODevice *device = stream->device();
    if (!mOwners.contains(device)) {
        // security hole
        JsonDbOwner *owner = new JsonDbOwner(this);
        owner->setOwnerId("unknown app");
        owner->setDomain("unknown app domain");
        mOwners[stream->device()] = owner;
    }
    Q_ASSERT(mOwners[device]);
    return mOwners[device];
}

void DBServer::notified( const QString &notificationId, QsonMap object, const QString &action )
{
    DBG() << "DBServer::notified" << "notificationId" << notificationId << "object" << object;
    QsonStream *stream = mNotifications.value(notificationId);
    // if the notified signal() is delivered after the notification has been deleted,
    // then there is no stream to send to
    if (!stream)
        return;
    QsonMap map, obj;
    obj.insert( JsonDbString::kObjectStr, object );
    obj.insert( JsonDbString::kActionStr, action );
    map.insert( JsonDbString::kNotifyStr, obj );
    map.insert( JsonDbString::kUuidStr, notificationId );

    DBG() << "Sending notify" << map;
    stream->send(map);
}

void DBServer::updateView( const QString &viewType, const QString &partitionName )
{
    mJsonDb->updateView(viewType, partitionName);
}


void DBServer::processCreate(QsonStream *stream, const QsonObject &object, int id, const QString &partitionName)
{
    QsonMap result;
    JsonDbOwner *owner = getOwner(stream);
    switch (object.type()) {
    case QsonObject::ListType: {
        // TODO:  Properly handle creating notifications from a list
        QsonList list = object.toList();
        result = mJsonDb->createList(owner, list, partitionName);
        break; }
    case QsonObject::MapType: {
        QsonMap obj = object.toMap();
        result = mJsonDb->create(owner, obj, partitionName);
        QString uuid = result.value<QsonMap>(JsonDbString::kResultStr).valueString(JsonDbString::kUuidStr);
        if (!uuid.isEmpty() && partitionName == JsonDbString::kEphemeralPartitionName &&
                obj.valueString(JsonDbString::kTypeStr) == JsonDbString::kNotificationTypeStr) {
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

void DBServer::processUpdate(QsonStream *stream, const QsonObject &object,
                             int id, const QString &partitionName, bool replication)
{
    QsonMap result;
    JsonDbOwner *owner = getOwner(stream);
    switch (object.type()) {
    case QsonObject::ListType: {
        // TODO:  Properly handle updating notifications from a list
        QsonList list = object.toList();
        result = mJsonDb->updateList(owner, list, partitionName, replication);
        break; }
    case QsonObject::MapType: {
        QsonMap obj = object.toMap();

        // The user could be changing a _type='notification' object to some other type
        QString uuid = obj.valueString(JsonDbString::kUuidStr);
        if (!uuid.isEmpty() && partitionName == JsonDbString::kEphemeralPartitionName)
            mNotifications.remove(uuid);

        result = mJsonDb->update(owner, obj, partitionName, replication);
        if (result.contains(JsonDbString::kErrorStr) && !result.isNull(JsonDbString::kErrorStr))
            qCritical() << "UPDATE:" << obj << " Error Message : " << result.subObject(JsonDbString::kErrorStr).valueString(JsonDbString::kMessageStr);

        if (!uuid.isEmpty() && partitionName == JsonDbString::kEphemeralPartitionName &&
                obj.valueString(JsonDbString::kTypeStr) == JsonDbString::kNotificationTypeStr)
            mNotifications.insert(uuid, stream);
        break; }
    default:
        result = createError(JsonDbError::MissingObject, "Update requires list or object");
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processRemove(QsonStream *stream, const QsonObject &object, int id, const QString &partitionName)
{
    JsonDbOwner *owner = getOwner(stream);
    QsonMap result;
    switch (object.type()) {
    case QsonObject::ListType:
        // TODO: Properly handle removing notifications from a list
        result = mJsonDb->removeList(owner, object.toList(), partitionName);
        break;
    case QsonObject::MapType: {
        QsonMap obj = object.toMap();
        if (obj.contains(JsonDbString::kUuidStr)) {
            // removing a single item
            QString uuid = obj.valueString(JsonDbString::kUuidStr);
            mNotifications.remove(uuid);
            result = mJsonDb->remove(owner, obj, partitionName);
        } else {
            // treat input as a query and remove the result of it
            if (obj.valueString(JsonDbString::kQueryStr).isEmpty()) {
                result = createError(JsonDbError::MissingQuery, "Remove requires a query or an object");
                break;
            }
            QsonMap queryResult = mJsonDb->find(owner, obj, partitionName);
            if (queryResult.contains(JsonDbString::kErrorStr)
                && !queryResult.isNull(JsonDbString::kErrorStr)) {
                result = queryResult;
                break;
            }
            QsonList itemsToRemove = queryResult.subObject(JsonDbString::kResultStr).subList(QLatin1String("data"));
            // TODO: Properly handle removing notifications from a list
            result = mJsonDb->removeList(owner, itemsToRemove, partitionName);
        }
        break; }
    default:
        result = createError(JsonDbError::MissingObject, "Remove requires list or object");
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processFind(QsonStream *stream, const QsonObject &object, int id, const QString &partitionName)
{
    JsonDbOwner *owner = getOwner(stream);
    QsonMap result;
    switch (object.type()) {
    case QsonObject::MapType:
        result = mJsonDb->find(owner, object, partitionName);
        break;
    default:
        result = createError(JsonDbError::InvalidRequest, "Invalid find object");
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processChangesSince(QsonStream *stream, const QsonObject &object, int id, const QString &partitionName)
{
    JsonDbOwner *owner = getOwner(stream);
    QsonMap result;
    switch (object.type()) {
    case QsonObject::MapType:
        result = mJsonDb->changesSince(owner, object, partitionName);
        break;
    default:
        result = createError(JsonDbError::InvalidRequest, "Invalid find object");
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

bool DBServer::validateToken( QsonStream *stream, const QsonMap &securityObject )
{
    bool valid = false;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    pid_t pid = securityObject.valueUInt("pid");
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
    if (!getsockopt(s, SOL_SOCKET, SO_PEERCRED, &peercred, &peercredlen)) {
        valid = (pid == peercred.pid);
    }
#else
    Q_UNUSED(stream);
    Q_UNUSED(securityObject);
    valid = true;
#endif
    return valid;
}

// This will create a dummy owner. Used only when the policies are not enforced.
// When policies are not enforced the JsonDbOwner allows access to everybody.
void DBServer::createDummyOwner( QsonStream *stream, int id )
{
    if (gEnforceAccessControlPolicies)
        return;

    JsonDbOwner *owner = new JsonDbOwner(this);
    owner->setOwnerId(QString("unknown app"));
    owner->setDomain(QString("unknown domain"));
    QsonMap capabilities;
    owner->setCapabilities(capabilities, mJsonDb);
    mOwners[stream->device()] = owner;

    QsonMap result;
    result.insert( JsonDbString::kResultStr, QLatin1String("Okay") );
    result.insert( JsonDbString::kErrorStr, QsonObject::NullValue);
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processToken(QsonStream *stream, const QVariant &object, int id)
{
    if (!gEnforceAccessControlPolicies) {
        // We are not enforcing policies here, allow 'token' requests
        // from all applications.
        // ### TODO: We will have to remove this afterwards
        createDummyOwner(stream, id);
        return;
    }

    bool tokenValidated = false;
    QsonMap result;
    QString errorStr(QLatin1String("Invalid token"));
    if (object.type() == QVariant::String) {
        QString token = object.toString();

        // grant all access to applications with the master token.
        if (!mMasterToken.isEmpty() && mMasterToken == token) {
            JsonDbOwner *owner = new JsonDbOwner(this);
            owner->setAllowAll(true);
            owner->setStorageQuota(-1);
            mOwners[stream->device()] = owner;
            tokenValidated = true;
        } else {
            QsonMap request;
            request.insert(JsonDbString::kQueryStr, (QString("[?%1=\"com.nokia.mp.core.Security\"][?%2=\"%3\"]")
                                                     .arg(JsonDbString::kTypeStr)
                                                     .arg(JsonDbString::kTokenStr).arg(token)));

            QsonMap result = mJsonDb->find(mJsonDb->owner(), request);
            QsonList securityObjects = result.subObject(JsonDbString::kResultStr).subList(JsonDbString::kDataStr);
            if (securityObjects.size() == 1) {
                QsonMap securityObject = securityObjects.at<QsonMap>(0);
                if (validateToken(stream, securityObject)) {
                    QString identifier = securityObject.valueString("identifier");
                    QString domain = securityObject.valueString("domain");
                    QsonMap capabilities = securityObject.subObject("capabilities");

                    JsonDbOwner *owner = new JsonDbOwner(this);
                    owner->setOwnerId(identifier);
                    owner->setDomain(domain);
                    owner->setCapabilities(capabilities, mJsonDb);

                    QStringList keys = capabilities.keys();
                    if (keys.contains("quotas")) {
                        QsonMap quotas = capabilities.subObject("quotas");
                        int storageQuota = quotas.valueInt("storage");
                        owner->setStorageQuota(storageQuota);
                    }
                    mOwners[stream->device()] = owner;
                    tokenValidated = true;
                }
            } else {
                if (securityObjects.size()) {
                    errorStr = QLatin1String("Wrong number of security objects found.");
                    DBG() << Q_FUNC_INFO << "Wrong number of security objects found." << securityObjects.size();
                } else {
                    errorStr = QLatin1String("No security object for token");
                    DBG() << Q_FUNC_INFO << "No security object for token" << token << "Using default owner and access control";
                }
            }
        }
    }
    if (tokenValidated) {
        QsonMap resultObject;
        resultObject.insert(JsonDbString::kResultStr, QLatin1String("Okay"));
        result.insert(JsonDbString::kResultStr, resultObject);
        result.insert(JsonDbString::kErrorStr, QsonObject::NullValue);
    } else {
        result = createError(JsonDbError::InvalidRequest, errorStr);
    }
    result.insert(JsonDbString::kIdStr, id);
    stream->send(result);
    // in case of an invalid token, close the connection
    if (!tokenValidated) {
        QLocalSocket *socket = qobject_cast<QLocalSocket *>(stream->device());
        if (socket) {
            socket->disconnectFromServer();
        } else {
            QTcpSocket *tcpSocket = qobject_cast<QTcpSocket *>(stream->device());
            if (tcpSocket)
                tcpSocket->disconnectFromHost();
        }
    }
}

void DBServer::receiveMessage(const QsonObject &message)
{
    QsonMap map = message.toMap();
    QsonStream *stream = qobject_cast<QsonStream *>(sender());

    QString  action = map.valueString(JsonDbString::kActionStr);
    QsonObject object = map.value<QsonElement>(JsonDbString::kObjectStr);
    int id = map.valueInt(JsonDbString::kIdStr);
    QString partitionName = map.valueString(JsonDbString::kPartitionStr);

    QElapsedTimer timer;
    if (gPerformanceLog)
        timer.start();

    if (action == JsonDbString::kCreateStr)
        processCreate(stream, object, id, partitionName);
    else if (action == JsonDbString::kUpdateStr)
        processUpdate(stream, object, id, partitionName, map.valueBool("replication"));
    else if (action == JsonDbString::kRemoveStr)
        processRemove(stream, object, id, partitionName);
    else if (action == JsonDbString::kFindStr)
        processFind(stream, object, id, partitionName);
    else if (action == JsonDbString::kTokenStr)
        processToken(stream, qsonToVariant(map.value<QsonElement>(JsonDbString::kObjectStr)), id);
    else if (action == JsonDbString::kChangesSinceStr)
        processChangesSince(stream, object, id, partitionName);
    else {
        const QMetaObject *mo = mJsonDb->metaObject();
        QsonMap result;
        // DBG() << QString("using QMetaObject to invoke method %1").arg(action);
        if (mo->invokeMethod(mJsonDb, action.toLatin1().data(),
                             Qt::DirectConnection,
                             Q_RETURN_ARG(QsonObject, result),
                             Q_ARG(JsonDbOwner *, getOwner(stream)),
                             Q_ARG(QsonObject, object.toMap()))) {
            result.insert( JsonDbString::kIdStr, id );
            stream->send(result);
        } else {
            sendError(stream, JsonDbError::InvalidRequest,
                      QString("Invalid request '%1'").arg(action), id);
            return;
        }
    }
    if (gPerformanceLog) {
        QString additionalInfo;
        if ( action == JsonDbString::kFindStr ) {
            additionalInfo = object.toMap().valueString("query");
        } else if (object.type() == QsonObject::ListType) {
            additionalInfo = QString::fromLatin1("%1 objects").arg(object.toList().size());
        } else {
            additionalInfo = object.toMap().valueString(JsonDbString::kTypeStr);
        }
        qDebug() << "jsondb" << "processed" << action << "ms" << timer.elapsed() << additionalInfo;
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
    QMutableMapIterator<QString, QsonStream *> iter(mNotifications);
    while (iter.hasNext()) {
        iter.next();
        if (iter.value()
            && (iter.value()->device() == connection)) {
            QString notificationId = iter.key();
            QsonMap notificationObject;
            notificationObject.insert(JsonDbString::kUuidStr, notificationId);
            mJsonDb->remove(owner, notificationObject, JsonDbString::kEphemeralPartitionName);
            mNotifications.remove(notificationId);
        }
    }

    if (mConnections.contains(connection)) {
        QsonStream *stream = mConnections.value(connection);
        if (stream)
            stream->deleteLater();

        mConnections.remove(connection);
    }
    if (mOwners.contains(connection)) {
        JsonDbOwner *owner =mOwners.value(connection);
        if (owner)
            owner->deleteLater();
        mOwners.remove(connection);
    }
}

} } // end namespace QtAddOn::JsonDb
