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

#include "jsondb-strings.h"
#include "jsondb-error.h"
#include "jsondb-oneshot_p.h"
#include "jsondb-connection_p.h"
#include "qsonstream.h"

namespace QtAddOn { namespace JsonDb {

Q_GLOBAL_STATIC(JsonDbConnection, qtjsondbConnection)

static QString           sDefaultToken;

class JsonDbConnectionPrivate
{
    Q_DECLARE_PUBLIC(JsonDbConnection)
public:
    JsonDbConnectionPrivate(JsonDbConnection *q);
    ~JsonDbConnectionPrivate();

    JsonDbConnection *q_ptr;
    QsonStream mStream;
    int mId;
    QString mToken;
};

JsonDbConnectionPrivate::JsonDbConnectionPrivate(JsonDbConnection *q)
    : q_ptr(q), mId(1)
{
}

JsonDbConnectionPrivate::~JsonDbConnectionPrivate()
{

}

/*!
  \class QtAddOn::JsonDb::JsonDbConnection

  \brief The JsonDbConnection class provides a connection to the
  database server. Generally used via \c class
  QtAddOn::JsonDb::JsonDbClient.

  \sa QtAddOn::JsonDb::JsonDbClient
*/

/*!
  \fn JsonDbConnection *QtAddOn::JsonDb::JsonDbConnection::instance()
  Returns a singleton instance of \c JsonDbConnection.
*/
JsonDbConnection *JsonDbConnection::instance()
{
    JsonDbConnection *c = qtjsondbConnection();
    c->connectToServer();
    return c;
}

/*!
  \fn void JsonDbConnection::setDefaultToken( const QString& token )
  Sets the default security token to \a token.
*/
void JsonDbConnection::setDefaultToken( const QString& token )
{
    sDefaultToken = token;
}

/*!
  \fn QString JsonDbConnection::defaultToken()
  Returns the default security for the connection.
*/
QString JsonDbConnection::defaultToken()
{
    return sDefaultToken;
}

/*!
  \fn bool JsonDbConnection::waitForConnected(int msecs)
  Waits up to \a msecs milliseconds for the connection to the database to be completed.
  Returns true if connected.
*/
bool JsonDbConnection::waitForConnected(int msecs)
{
    Q_D(JsonDbConnection);
    QIODevice *device = d->mStream.device();
    if (QLocalSocket *socket = qobject_cast<QLocalSocket *>(device))
        return socket->waitForConnected(msecs);
    else if (QAbstractSocket *socket = qobject_cast<QAbstractSocket *>(device))
        return socket->waitForConnected(msecs);
    return false;
}

/*!
  \fn bool JsonDbConnection::waitForDisconnected(int msecs)
  Waits up to \a msecs milliseconds for the connection to the database to be closed.
  Returns true if disconnected.
*/
bool JsonDbConnection::waitForDisconnected(int msecs)
{
    Q_D(JsonDbConnection);
    QIODevice *device = d->mStream.device();
    if (QLocalSocket *socket = qobject_cast<QLocalSocket *>(device))
        return socket->waitForDisconnected(msecs);
    else if (QAbstractSocket *socket = qobject_cast<QAbstractSocket *>(device))
        return socket->waitForDisconnected(msecs);
    return false;
}

bool JsonDbConnection::waitForBytesWritten(int msecs)
{
    Q_D(JsonDbConnection);
    QIODevice *device = d->mStream.device();
    if (QLocalSocket *socket = qobject_cast<QLocalSocket *>(device))
        return socket->waitForBytesWritten(msecs);
    else if (QAbstractSocket *socket = qobject_cast<QAbstractSocket *>(device))
        return socket->waitForBytesWritten(msecs);
    return false;
}

QVariantMap JsonDbConnection::makeFindRequest( const QVariant& object )
{
    QVariantMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kFindStr);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QsonObject JsonDbConnection::makeFindRequest( const QsonObject& object )
{
    QsonMap request;

    request.insert(JsonDbString::kActionStr, JsonDbString::kFindStr);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QVariantMap JsonDbConnection::makeQueryRequest( const QString& queryString, int offset, int limit )
{
    QVariantMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kFindStr);
    QVariantMap object;
    object.insert(JsonDbString::kQueryStr, queryString);
    if (offset != 0)
        object.insert(JsonDbString::kOffsetStr, offset);
    if (limit != -1)
        object.insert(JsonDbString::kLimitStr, limit);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QVariantMap JsonDbConnection::makeCreateRequest( const QVariant& object )
{
    QVariantMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kCreateStr);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QsonObject JsonDbConnection::makeCreateRequest( const QsonObject& object )
{
    QsonMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kCreateStr);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QVariantMap JsonDbConnection::makeUpdateRequest( const QVariant& object )
{
    QVariantMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kUpdateStr);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QsonObject JsonDbConnection::makeUpdateRequest( const QsonObject& object )
{
    QsonMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kUpdateStr);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QVariantMap JsonDbConnection::makeRemoveRequest( const QVariant& object )
{
    QVariantMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kRemoveStr);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QsonObject JsonDbConnection::makeRemoveRequest( const QsonObject& object )
{
    QsonMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kRemoveStr);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QVariantMap JsonDbConnection::makeRemoveRequest( const QString &queryString)
{
    QVariantMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kRemoveStr);
    QVariantMap object;
    object.insert(JsonDbString::kQueryStr, queryString);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

QVariantMap JsonDbConnection::makeNotification( const QString& query,
                                                const QVariantList& actions )
{
    QVariantMap notification;
    notification.insert(JsonDbString::kTypeStr,
                        JsonDbString::kNotificationTypeStr);
    notification.insert(JsonDbString::kQueryStr, query);
    notification.insert(JsonDbString::kActionsStr, actions);
    return notification;
}

QsonObject JsonDbConnection::makeNotification( const QString& query,
                                               const QsonObject& actions )
{
    QsonMap notification;
    notification.insert(JsonDbString::kTypeStr,
                        JsonDbString::kNotificationTypeStr);
    notification.insert(JsonDbString::kQueryStr, query);
    notification.insert(JsonDbString::kActionsStr, actions);
    return notification;
}

QVariantMap JsonDbConnection::makeChangesSinceRequest(int stateNumber, const QStringList &types)
{
    QVariantMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kChangesSinceStr);
    QVariantMap object;
    object.insert(JsonDbString::kStateNumberStr, stateNumber);
    if (!types.isEmpty())
        object.insert(JsonDbString::kTypesStr, types);
    request.insert(JsonDbString::kObjectStr, object);
    return request;
}

/***************************************************************************/

/*!
 \fn JsonDbConnection::JsonDbConnection(QObject *parent)
 Constructs a \c JsonDbConnection.
*/
JsonDbConnection::JsonDbConnection(QObject *parent)
    : QObject(parent), d_ptr(new JsonDbConnectionPrivate(this))
{
    Q_D(JsonDbConnection);
    if (!sDefaultToken.isEmpty())
        d->mToken = sDefaultToken;
    else
        d->mToken = QLatin1String(::getenv("JSONDB_TOKEN"));
}

JsonDbConnection::~JsonDbConnection()
{
    Q_D(JsonDbConnection);
    // QsonStreams don't own the socket
    QIODevice *device = d->mStream.device();
    d->mStream.setDevice(0);
    if (device)
        delete device;
}

void JsonDbConnection::init(QIODevice *device)
{
    Q_D(JsonDbConnection);
    d->mStream.setDevice(device);
    connect(&d->mStream, SIGNAL(receive(QsonObject)),
            this, SLOT(receiveMessage(QsonObject)));
    connect(&d->mStream, SIGNAL(readyWrite()),
            this, SIGNAL(readyWrite()));

    if (!d->mToken.isEmpty()) {
        QsonMap request;
        request.insert(JsonDbString::kIdStr, makeRequestId());
        request.insert(JsonDbString::kActionStr, JsonDbString::kTokenStr);
        request.insert(JsonDbString::kObjectStr, d->mToken);
        d->mStream << request;
    }
}

/*
 * Connects to the named local socket.
 */

/*!
  \fn void JsonDbConnection::connectToServer(const QString &socketName)
  Connects to the named local socket \a socketName.
 */
void JsonDbConnection::connectToServer(const QString &socketName)
{
    Q_D(JsonDbConnection);

    QString name = socketName;
    if (name.isEmpty())
        name = QLatin1String(::getenv("JSONDB_SOCKET"));
    if (name.isEmpty())
        name = QLatin1String("qt5jsondb");

    if (d->mStream.device() != 0) {
        qWarning() << "JsonDbConnection" << "already connected";
        return;
    }
    QLocalSocket *socket = new QLocalSocket(this);
    connect(socket, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
    connect(socket, SIGNAL(connected()), this, SIGNAL(connected()));
    socket->connectToServer(name);
    if (socket->waitForConnected())
        init(socket);
    else
        qCritical() << "JsonDbConnection: Unable to connect to socket" << name << socket->errorString();
}

/*
 * Connects to the named remote host.
 */

/*!
  \fn void JsonDbConnection::connectToServer(const QString &hostname, quint16 port)
  Connects to the databsae via a TCP connection to \a hostname and \a port.
 */
void JsonDbConnection::connectToHost(const QString &hostname, quint16 port)
{
    Q_D(JsonDbConnection);
    Q_ASSERT(d->mStream.device() == 0);
    Q_UNUSED(d);

    QTcpSocket *socket = new QTcpSocket(this);
    connect(socket, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
    connect(socket, SIGNAL(connected()), this, SIGNAL(connected()));
    socket->connectToHost(hostname, port);
    if (socket->waitForConnected())
        init(socket);
    else
        qCritical() << "JsonDbConnection: Unable to connect to remote server: "
                    << hostname << port << socket->errorString();
}

/*!
  \fn int JsonDbConnection::request(const QVariantMap &dbrequest)
  \deprecated

  Sends request \a dbrequest to the database and returns the request identification number.
*/
int JsonDbConnection::request(const QVariantMap &dbrequest)
{
    if(!isConnected())
        return -1;
    Q_D(JsonDbConnection);
    QVariantMap r = dbrequest;
    int newid = makeRequestId();
    r.insert(JsonDbString::kIdStr, newid);
    if (!d->mStream.send(variantToQson(r)))
        return -1;
    d->mId = newid;
    return newid;
}

/*!
  \fn int JsonDbConnection::request(const QsonObject &dbrequest)

  Sends request \a dbrequest to the database and returns the request identification number.
*/
int JsonDbConnection::request(const QsonObject &dbrequest)
{
    if(!isConnected())
        return -1;
    Q_D(JsonDbConnection);
    QsonMap r = dbrequest.toMap();
    int newid = makeRequestId();
    r.insert(JsonDbString::kIdStr, newid);
    if (!d->mStream.send(r))
        return -1;
    d->mId = newid;
    return newid;
}

/*!
    Returns a new request id.
*/
int JsonDbConnection::makeRequestId()
{
    return ++d_func()->mId;
}


void JsonDbConnection::receiveMessage(const QsonObject &msg)
{
    QsonMap qsonMsg = msg.toMap();
    if (qsonMsg.contains(JsonDbString::kNotifyStr)) {
        QsonMap nmap = qsonMsg.subObject(JsonDbString::kNotifyStr).toMap();
        emit notified(qsonMsg.valueString(JsonDbString::kUuidStr),
                      qsonToVariant(nmap.subObject(JsonDbString::kObjectStr)),
                      nmap.valueString(JsonDbString::kActionStr));
        emit notified(qsonMsg.valueString(JsonDbString::kUuidStr),
                      nmap.subObject(JsonDbString::kObjectStr),
                      nmap.valueString(JsonDbString::kActionStr));
    } else {
        int id = qsonMsg.valueInt(JsonDbString::kIdStr);
        QsonObject qsonResult = qsonMsg.subObject(JsonDbString::kResultStr);
        QVariantMap result = qsonToVariant(qsonResult).toMap();
        if (result.size()) {
            emit response(id, qsonResult);
            emit response(id, result);
        } else {
            QVariantMap emap  = qsonToVariant(qsonMsg.subObject(JsonDbString::kErrorStr)).toMap();
            emit error(id,
                       emap.value(JsonDbString::kCodeStr).toInt(),
                       emap.value(JsonDbString::kMessageStr).toString());
        }
    }
}


/*!
  \fn void JsonDbConnection::oneShot(const QVariantMap &dbrequest, QObject *receiver, const char *responseSlot, const char *errorSlot)
  \deprecated

  \sa JsonDbClient
*/
void JsonDbConnection::oneShot(const QVariantMap &dbrequest, QObject *receiver,
                               const char *responseSlot, const char *errorSlot)
{
    JsonDbOneShot *oneShot = new JsonDbOneShot;
    oneShot->setParent(this);

    connect(this, SIGNAL(response(int,QVariant)),
            oneShot, SLOT(handleResponse(int,QVariant)));
    connect(this, SIGNAL(error(int,int,QString)),
            oneShot, SLOT(handleError(int,int,QString)));

    if (responseSlot && !connect(oneShot, SIGNAL(response(QVariant)), receiver, responseSlot))
        qCritical() << Q_FUNC_INFO << "failed to connect to slot" << responseSlot;
    if (errorSlot && !connect(oneShot, SIGNAL(error(int,QString)), receiver, errorSlot))
        qCritical() << Q_FUNC_INFO << "failed to connect to slot" << errorSlot;

    oneShot->mId = request(dbrequest);
    if (!responseSlot && !errorSlot)
        oneShot->deleteLater();
}

QVariant JsonDbConnection::sync(const QVariantMap &dbrequest)
{
    QVariant result;
    QThread syncThread;
    JsonDbSyncCall *call = new JsonDbSyncCall(&dbrequest, &result);

    connect(&syncThread, SIGNAL(started()),
            call, SLOT(createSyncRequest()));
    connect(&syncThread, SIGNAL(finished()),
            call, SLOT(deleteLater()));
    call->moveToThread(&syncThread);
    syncThread.start();
    syncThread.wait();
    return result;
}

/*!
  \fn QsonObject JsonDbConnection::sync(const QsonMap &dbrequest)
  Sends request \a dbrequest to the database, waits synchronously for it to complete, and returns the response.

  This operation creates a new thread with a new connection to the
  database and blocks the thread in which it is running. This should
  only be used from agents and daemons. It should never be called from
  a user interface thread.
*/
QsonObject JsonDbConnection::sync(const QsonMap &dbrequest)
{
    QsonObject result;
    QThread syncThread;
    JsonDbSyncCall *call = new JsonDbSyncCall(&dbrequest, &result);

    connect(&syncThread, SIGNAL(started()),
            call, SLOT(createSyncQsonRequest()));
    connect(&syncThread, SIGNAL(finished()),
            call, SLOT(deleteLater()));
    call->moveToThread(&syncThread);
    syncThread.start();
    syncThread.wait();
    return result;
}

/*!
  \fn void JsonDbConnection::setToken(const QString &token)
  Sets the security token for this connection.
*/
void JsonDbConnection::setToken(const QString &token)
{
    d_func()->mToken = token;
}

/*!
  \fn bool JsonDbConnection::isConnected() const
  Returns true if connected to the database.
*/
bool JsonDbConnection::isConnected() const
{
    Q_D(const JsonDbConnection);
    QIODevice *device = d->mStream.device();
    if (!device)
        return false;
    if (QLocalSocket *socket = qobject_cast<QLocalSocket *>(device))
        return socket->state() == QLocalSocket::ConnectedState;
    else if (QAbstractSocket *socket = qobject_cast<QAbstractSocket *>(device))
        return socket->state() == QAbstractSocket::ConnectedState;

    return false;
}

/*!
  \fn void disconnected()
  This signal is emitted when the connection to the database is lost.
*/

/*!
    \class JsonDbSyncCall
    \internal
*/
JsonDbSyncCall::JsonDbSyncCall(const QVariantMap &dbrequest, QVariant &result)
    : mDbRequest(&dbrequest), mDbQsonRequest(0), mResult(&result), mQsonResult(0), mSyncJsonDbConnection(0)
{
}

JsonDbSyncCall::JsonDbSyncCall(const QVariantMap *dbrequest, QVariant *result)
    : mDbRequest(dbrequest), mDbQsonRequest(0), mResult(result), mQsonResult(0), mSyncJsonDbConnection(0)
{
}

JsonDbSyncCall::JsonDbSyncCall(const QsonMap *dbrequest, QsonObject *result)
    : mDbRequest(0), mDbQsonRequest(dbrequest), mResult(0), mQsonResult(result), mSyncJsonDbConnection(0)
{
}

JsonDbSyncCall::~JsonDbSyncCall()
{
    if (mSyncJsonDbConnection)
        delete mSyncJsonDbConnection;
}

void JsonDbSyncCall::createSyncRequest()
{
    mSyncJsonDbConnection = new JsonDbConnection;
    mSyncJsonDbConnection->connectToServer();

    connect(mSyncJsonDbConnection, SIGNAL(response(int,QVariant)),
            this, SLOT(handleResponse(int,QVariant)));
    connect(mSyncJsonDbConnection, SIGNAL(error(int,int,QString)),
            this, SLOT(handleError(int,int,QString)));
    mId = mSyncJsonDbConnection->request(*mDbRequest);
}

void JsonDbSyncCall::createSyncQsonRequest()
{
    mSyncJsonDbConnection = new JsonDbConnection;
    mSyncJsonDbConnection->connectToServer();

    connect(mSyncJsonDbConnection, SIGNAL(response(int,QsonObject)),
            this, SLOT(handleResponse(int,QsonObject)));
    connect(mSyncJsonDbConnection, SIGNAL(error(int,int,QString)),
            this, SLOT(handleError(int,int,QString)));
    mId = mSyncJsonDbConnection->request(*mDbQsonRequest);
}


void JsonDbSyncCall::handleResponse(int id, const QVariant& data)
{
    if (id == mId) {
        *mResult  = data;
        QThread::currentThread()->quit();
    }
}

void JsonDbSyncCall::handleResponse(int id, const QsonObject& data)
{
    if (id == mId) {
        *mQsonResult  = data;
        QThread::currentThread()->quit();
    }
}

void JsonDbSyncCall::handleError(int id, int code, const QString& message)
{
    if (id == mId) {
        qCritical() << "Illegal result" << code << message;
        QThread::currentThread()->quit();
    }
}

} } // end namespace QtAddOn::JsonDb
