/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
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

QT_BEGIN_NAMESPACE_JSONDB

#ifndef QT_NO_DEBUG_OUTPUT
extern bool gDebug;
#define DBG() if (gDebug) qDebug() << Q_FUNC_INFO
#else
#define DBG() if (0) qDebug() << Q_FUNC_INFO
#endif

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

bool DBServer::start(bool compactOnClose)
{
    mJsonDb = new JsonDb(mFileName, this);
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
    return true;
}

bool DBServer::clear()
{
    return mJsonDb->clear();
}

bool DBServer::load(const QString &jsonFileName)
{
    return mJsonDb->load(jsonFileName);
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
    if (!mOwners.contains(device)) {
        // security hole
        JsonDbOwner *owner = new JsonDbOwner(this);
        owner->setOwnerId(QString::fromLatin1("unknown app %1").arg((intptr_t)stream));
        owner->setDomain("unknown app domain");
        mOwners[stream->device()] = owner;
    }
    Q_ASSERT(mOwners[device]);
    return mOwners[device];
}

void DBServer::notified(const QString &notificationId, JsonDbObject object, const QString &action)
{
    DBG() << "DBServer::notified" << "notificationId" << notificationId << "object" << object;
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

    DBG() << "Sending notify" << map;
    stream->send(map);
}

void DBServer::updateView( const QString &viewType, const QString &partitionName )
{
    mJsonDb->updateView(viewType, partitionName);
}

void DBServer::processCreate(JsonStream *stream, const QJsonValue &object, int id, const QString &partitionName)
{
    QJsonObject result;
    JsonDbOwner *owner = getOwner(stream);
    switch (object.type()) {
    case QJsonValue::Array: {
        // TODO:  Properly handle creating notifications from a list
        QJsonArray list = object.toArray();
        JsonDbObjectList olist;
        for (int i = 0; i < list.size(); i++) {
            QJsonObject o(list.at(i).toObject());
            olist.append(o);
        }
        result = mJsonDb->createList(owner, olist, partitionName);
        break; }
    case QJsonValue::Object: {
        QJsonObject o = object.toObject();
        JsonDbObject document(o);

        result = mJsonDb->create(owner, document, partitionName);
        QString uuid = result.value(JsonDbString::kResultStr).toObject().value(JsonDbString::kUuidStr).toString();
        if (!uuid.isEmpty()
              // && partitionName == JsonDbString::kEphemeralPartitionName ### TODO: uncomment me
            && document.value(JsonDbString::kTypeStr) == JsonDbString::kNotificationTypeStr) {
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

void DBServer::processUpdate(JsonStream *stream, const QJsonValue &object,
                             int id, const QString &partitionName)
{
    QJsonObject result;
    JsonDbOwner *owner = getOwner(stream);
    switch (object.type()) {
    case QJsonValue::Array: {
        // TODO:  Properly handle updating notifications from a list
        QJsonArray list = object.toArray();
        JsonDbObjectList olist;
        for (int i = 0; i < list.size(); i++) {
            QJsonObject o(list.at(i).toObject());
            olist.append(o);
        }
        result = mJsonDb->updateList(owner, olist, partitionName);
        break; }
    case QJsonValue::Object: {
        QJsonObject o = object.toObject();
        JsonDbObject document(o);

        // The user could be changing a _type='notification' object to some other type
        QString uuid = document.value(JsonDbString::kUuidStr).toString();
        if (!uuid.isEmpty() && partitionName == JsonDbString::kEphemeralPartitionName)
            mNotifications.remove(uuid);

        result = mJsonDb->update(owner, document, partitionName);
        if (result.contains(JsonDbString::kErrorStr)
                && (!result.value(JsonDbString::kErrorStr).isNull())
                && !result.value(JsonDbString::kErrorStr).toObject().size())
            qCritical() << "UPDATE:" << document << " Error Message : " << result.value(JsonDbString::kErrorStr).toObject().value(JsonDbString::kMessageStr).toString();

        if (!uuid.isEmpty() && partitionName == JsonDbString::kEphemeralPartitionName &&
                document.value(JsonDbString::kTypeStr) == JsonDbString::kNotificationTypeStr)
            mNotifications.insert(uuid, stream);
        break; }
    default:
        result = createError(JsonDbError::MissingObject, "Update requires list or object");
        break;
    }
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processRemove(JsonStream *stream, const QJsonValue &object, int id, const QString &partitionName)
{
    JsonDbOwner *owner = getOwner(stream);
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
        result = mJsonDb->removeList(owner, olist, partitionName);
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
            result = mJsonDb->removeList(owner, itemsToRemove, partitionName);
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

void DBServer::processFind(JsonStream *stream, const QJsonValue &object, int id, const QString &partitionName)
{
    JsonDbOwner *owner = getOwner(stream);
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

void DBServer::processChangesSince(JsonStream *stream, const QJsonValue &object, int id, const QString &partitionName)
{
    JsonDbOwner *owner = getOwner(stream);
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

bool DBServer::validateToken( JsonStream *stream, const JsonDbObject &securityObject )
{
    bool valid = false;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    pid_t pid = securityObject.value("pid").toDouble();
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
void DBServer::createDummyOwner( JsonStream *stream, int id )
{
    if (gEnforceAccessControlPolicies)
        return;

    JsonDbOwner *owner = new JsonDbOwner(this);
    owner->setOwnerId(QString("unknown app"));
    owner->setDomain(QString("unknown domain"));
    QJsonObject capabilities;
    owner->setCapabilities(capabilities, mJsonDb);
    mOwners[stream->device()] = owner;

    QJsonObject result;
    result.insert( JsonDbString::kResultStr, QLatin1String("Okay") );
    result.insert( JsonDbString::kErrorStr, QJsonValue(QJsonValue::Null));
    result.insert( JsonDbString::kIdStr, id );
    stream->send(result);
}

void DBServer::processToken(JsonStream *stream, const QJsonValue &object, int id)
{
    if (!gEnforceAccessControlPolicies) {
        // We are not enforcing policies here, allow 'token' requests
        // from all applications.
        // ### TODO: We will have to remove this afterwards
        createDummyOwner(stream, id);
        return;
    }

    bool tokenValidated = false;
    QJsonObject result;
    QString errorStr(QLatin1String("Invalid token"));
    if (object.type() == QJsonValue::String) {
        QString token = object.toString();

        // grant all access to applications with the master token.
        if (!mMasterToken.isEmpty() && mMasterToken == token) {
            JsonDbOwner *owner = new JsonDbOwner(this);
            owner->setAllowAll(true);
            owner->setStorageQuota(-1);
            mOwners[stream->device()] = owner;
            tokenValidated = true;
        } else {
            GetObjectsResult result = mJsonDb->getObjects(JsonDbString::kTypeStr, QString("com.nokia.mp.core.Security"));
            JsonDbObjectList securityObjects;
            for (int i = 0; i < result.data.size(); i++) {
                JsonDbObject doc = result.data.at(i);
                if (doc.value(JsonDbString::kTokenStr).toString() == token)
                    securityObjects.append(doc);
            }
            if (securityObjects.size() == 1) {
                JsonDbObject securityObject = securityObjects.at(0);
                if (validateToken(stream, securityObject)) {
                    QString identifier = securityObject.value("identifier").toString();
                    QString domain = securityObject.value("domain").toString();
                    QJsonObject capabilities = securityObject.value("capabilities").toObject();

                    JsonDbOwner *owner = new JsonDbOwner(this);
                    owner->setOwnerId(identifier);
                    owner->setDomain(domain);
                    owner->setCapabilities(capabilities, mJsonDb);

                    QStringList keys = capabilities.keys();
                    if (keys.contains("quotas")) {
                        QJsonObject quotas = capabilities.value("quotas").toObject();
                        int storageQuota = quotas.value("storage").toDouble();
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
        QJsonObject resultObject;
        resultObject.insert(JsonDbString::kResultStr, QLatin1String("Okay"));
        result.insert(JsonDbString::kResultStr, resultObject);
        result.insert(JsonDbString::kErrorStr, QJsonValue(QJsonValue::Null));
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

void DBServer::receiveMessage(const QJsonObject &message)
{
    JsonStream *stream = qobject_cast<JsonStream *>(sender());
    QString  action = message.value(JsonDbString::kActionStr).toString();
    QJsonValue object = message.value(JsonDbString::kObjectStr);
    int id = message.value(JsonDbString::kIdStr).toDouble();
    QString partitionName = message.value(JsonDbString::kPartitionStr).toString();

    QElapsedTimer timer;
    if (gPerformanceLog)
        timer.start();

    if (action == JsonDbString::kCreateStr)
        processCreate(stream, object, id, partitionName);
    else if (action == JsonDbString::kUpdateStr)
        processUpdate(stream, object, id, partitionName);
    else if (action == JsonDbString::kRemoveStr)
        processRemove(stream, object, id, partitionName);
    else if (action == JsonDbString::kFindStr)
        processFind(stream, object, id, partitionName);
    else if (action == JsonDbString::kTokenStr)
        processToken(stream, object, id);
    else if (action == JsonDbString::kChangesSinceStr)
        processChangesSince(stream, object, id, partitionName);
    else {
        const QMetaObject *mo = mJsonDb->metaObject();
        QJsonObject result;
        // DBG() << QString("using QMetaObject to invoke method %1").arg(action);
        if (mo->invokeMethod(mJsonDb, action.toLatin1().data(),
                             Qt::DirectConnection,
                             Q_RETURN_ARG(QJsonObject, result),
                             Q_ARG(JsonDbOwner *, getOwner(stream)),
                             Q_ARG(QJsonValue, object.toObject()))) {
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
            additionalInfo = object.toObject().value("query").toString();
        } else if (object.type() == QJsonValue::Array) {
            additionalInfo = QString::fromLatin1("%1 objects").arg(object.toArray().size());
        } else {
            additionalInfo = object.toObject().value(JsonDbString::kTypeStr).toString();
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
    QMutableMapIterator<QString, JsonStream *> iter(mNotifications);
    while (iter.hasNext()) {
        iter.next();
        if (iter.value()
            && (iter.value()->device() == connection)) {
            QString notificationId = iter.key();
            QJsonObject notificationObject;
            notificationObject.insert(JsonDbString::kUuidStr, notificationId);
            mJsonDb->remove(owner, notificationObject, JsonDbString::kEphemeralPartitionName);
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
        JsonDbOwner *owner =mOwners.value(connection);
        if (owner)
            owner->deleteLater();
        mOwners.remove(connection);
    }
}

#include "moc_dbserver.cpp"

QT_END_NAMESPACE_JSONDB
