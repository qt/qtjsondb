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

#include "qjsondbconnection_p.h"
#include "qjsondbrequest_p.h"
#include "qjsondbwriterequest.h"
#include "qjsondbwriterequest_p.h"
#include "qjsondbreadrequest.h"
#include "qjsondbreadrequest_p.h"
#include "qjsondbwatcher_p.h"
#include "qjsondbstrings_p.h"
#include "qjsondbobject.h"
#include "qjsondbprivatepartition_p.h"

#include "jsondbsocketname_p.h"

#include "jsonstream.h"

#include <qcoreevent.h>
#include <qtimer.h>
#include <qjsonarray.h>
#include <qthreadstorage.h>

/*!
    \macro QT_USE_NAMESPACE_JSONDB
    \inmodule QtJsonDb

    This macro expands to using jsondb namespace and makes jsondb namespace
    visible to C++ source code.

    \code
        #include <QtJsonDb/qjsondbglobal.h>
        QT_USE_NAMESPACE_JSONDB
    \endcode

    To declare the class without including the declaration of the class:

    \code
        #include <QtJsonDb/qjsondbglobal.h>
        QT_BEGIN_NAMESPACE_JSONDB
        class QJsonDbConnection;
        QT_END_NAMESPACE_JSONDB
        QT_USE_NAMESPACE_JSONDB
    \endcode
*/

/*!
    \macro QT_BEGIN_NAMESPACE_JSONDB
    \inmodule QtJsonDb

    This macro begins a jsondb namespace. All forward declarations of QtJsonDb
    classes need to be wrapped in \c QT_BEGIN_NAMESPACE_JSONDB and \c
    QT_END_NAMESPACE_JSONDB.

    This macro includes QT_BEGIN_NAMESPACE, if Qt was compiled with namespace.

    \sa QT_USE_NAMESPACE_JSONDB, QT_END_NAMESPACE_JSONDB
*/

/*!
    \macro QT_END_NAMESPACE_JSONDB
    \inmodule QtJsonDb

    This macro ends a jsondb namespace. All forward declarations of QtJsonDb classes need to
    be wrapped in \c QT_BEGIN_NAMESPACE_JSONDB and \c QT_END_NAMESPACE_JSONDB.

    This macro includes QT_END_NAMESPACE, if Qt was compiled with namespace.

    \sa QT_USE_NAMESPACE_JSONDB, QT_BEGIN_NAMESPACE_JSONDB
*/

QT_BEGIN_NAMESPACE_JSONDB

Q_GLOBAL_STATIC(QThreadStorage<QJsonDbConnection *>, _q_defaultConnection);

/*!
    \class QJsonDbConnection
    \inmodule QtJsonDb

    \brief The QJsonDbConnection class provides a client interface which connects to the QJsonDb server.

    The connection is done using the local socket (unix domain socket)
    \l{QJsonDbConnection::socketName}{socketName}.

    The connection has a queue of requests, and each request that is meant to
    be \l{QJsonDbConnection::send()}{sent} is appended to the queue, which
    guarantees order of execution of requests. It is possible to add requests
    to the queue, even if the connection to the database is not yet established
    - requests will be queued locally and executed after the connection is
    complete.

    The connection has to be initiated explicitly by calling connectToServer().
    If \l{QJsonDbConnection::autoReconnectEnabled}{autoReconnectEnabled} property is set to
    true (by default), QJsonDbConnection object attempts to maintain the
    connection alive - by reconnecting again whenever the local socket
    connection is dropped, unless the connection was dropped due to
    a critical error (see \l{QJsonDbConnection::error()}{error()} signal).

    \code
        QtJsonDb::QJsonDbConnection *connection = new QtJsonDb::QJsonDbConnection;
        QObject::connect(connection, SIGNAL(error(QtJsonDb::QJsonDbConnection::ErrorCode,QString))),
                         this, SLOT(onConnectionError(QtJsonDb::QJsonDbConnection::ErrorCode,QString)));
        connection->connectToServer();
    \endcode
*/
/*!
    \enum QJsonDbConnection::ErrorCode

    This enum describes database connection errors.

    \value NoError
*/
/*!
    \enum QJsonDbConnection::Status

    This enum describes current database connection status.

    \value Unconnected Not connected.
    \value Connecting Connection to the database is being established.
    \value Authenticating Authentication is in process.
    \value Connected Connection established.
*/

QJsonDbConnectionPrivate::QJsonDbConnectionPrivate(QJsonDbConnection *q)
    : q_ptr(q), status(QJsonDbConnection::Unconnected), autoConnect(false), autoReconnectEnabled(true),
      explicitDisconnect(false), timeoutTimer(q), stream(0), lastRequestId(0),
      privatePartitionProcessing(0), privatePartitionHandler(0)
{
    qRegisterMetaType<QtJsonDb::QJsonDbRequest::ErrorCode>("QtJsonDb::QJsonDbRequest::Status");
    qRegisterMetaType<QtJsonDb::QJsonDbRequest::ErrorCode>("QtJsonDb::QJsonDbRequest::ErrorCode");
    qRegisterMetaType<QtJsonDb::QJsonDbWatcher::ErrorCode>("QtJsonDb::QJsonDbWatcher::ErrorCode");
    qRegisterMetaType<QList<QJsonObject> >("QList<QJsonObject>");

    timeoutTimer.setSingleShot(true);
    timeoutTimer.setTimerType(Qt::VeryCoarseTimer);
    QObject::connect(&timeoutTimer, SIGNAL(timeout()), q, SLOT(_q_onTimer()));
    socket = new QLocalSocket(q);
    QObject::connect(socket, SIGNAL(disconnected()), q_ptr, SLOT(_q_onDisconnected()));
    QObject::connect(socket, SIGNAL(connected()), q_ptr, SLOT(_q_onConnected()));
    QObject::connect(socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
                     q_ptr, SLOT(_q_onError(QLocalSocket::LocalSocketError)));
    stream = new QtJsonDbJsonStream::JsonStream(q);
    stream->setDevice(socket, true);
    QObject::connect(stream, SIGNAL(receive(QJsonObject)),
                     q_ptr, SLOT(_q_onReceivedObject(QJsonObject)));
}

QString QJsonDbConnectionPrivate::serverSocketName() const
{
    if (socketName.isEmpty()) {
        QByteArray ba = qgetenv("JSONDB_SOCKET");
        return ba.isEmpty() ? QLatin1String(JSONDB_SOCKET_NAME_STRING) : QString::fromLatin1(ba.constData(), ba.size());
    }
    return socketName;
}

void QJsonDbConnectionPrivate::_q_onConnected()
{
    Q_Q(QJsonDbConnection);
    Q_ASSERT(status == QJsonDbConnection::Connecting);
    status = QJsonDbConnection::Connected;
    emit q->statusChanged(status);
    emit q->connected();

    // reactivate all watchers
    reactivateAllWatchers();
    handleRequestQueue();
}

void QJsonDbConnectionPrivate::_q_onDisconnected()
{
    Q_Q(QJsonDbConnection);
    if (currentRequest) {
        QJsonDbRequestPrivate *drequest = currentRequest.data()->d_func();
        drequest->setStatus(QJsonDbRequest::Error);
        emit currentRequest.data()->error(QJsonDbRequest::DatabaseConnectionError, QString());
        pendingRequests.prepend(currentRequest);
        currentRequest.clear();
    }
    // deactivate all watchers
    QMap<QString, QWeakPointer<QJsonDbWatcher> >::iterator it;
    for (it = watchers.begin(); it != watchers.end();) {
        QJsonDbWatcher *watcher = it.value().data();
        if (!watcher) {
            it = watchers.erase(it);
            continue;
        }
        QJsonDbWatcherPrivate *dwatcher = watcher->d_func();
        dwatcher->setStatus(QJsonDbWatcher::Activating); // emits stateChanged
        ++it;
    }

    if (status == QJsonDbConnection::Unconnected) {
        // an error occured (e.g. auth error), we should not reconnect.
        emit q->disconnected();
        return;
    }

    QJsonDbConnection::Status newStatus;
    if (shouldAutoReconnect()) {
        status = QJsonDbConnection::Unconnected;
        emit q->statusChanged(status);
        if (!explicitDisconnect && status == QJsonDbConnection::Unconnected) {
            newStatus = QJsonDbConnection::Connecting;
            if (!timeoutTimer.isActive()) {
                timeoutTimer.setInterval(5000);
                timeoutTimer.start();
            }
        }
    } else {
        newStatus = QJsonDbConnection::Unconnected;
    }
    if (status != newStatus) {
        status = newStatus;
        emit q->statusChanged(status);
    }
    emit q->disconnected();
}

void QJsonDbConnectionPrivate::_q_onError(QLocalSocket::LocalSocketError error)
{
    switch (error) {
    case QLocalSocket::ServerNotFoundError:
    case QLocalSocket::ConnectionRefusedError:
    case QLocalSocket::PeerClosedError:
    case QLocalSocket::SocketAccessError:
    case QLocalSocket::SocketResourceError:
    case QLocalSocket::SocketTimeoutError:
    case QLocalSocket::ConnectionError:
    case QLocalSocket::UnsupportedSocketOperationError:
    case QLocalSocket::UnknownSocketError:
    case QLocalSocket::OperationError:
        _q_onDisconnected();
        break;
    case QLocalSocket::DatagramTooLargeError:
        qWarning("QJsonDbConnectionPrivate: datagram is too large.");
        break;
    }
}

void QJsonDbConnectionPrivate::_q_onTimer()
{
    if (status == QJsonDbConnection::Connecting)
        socket->connectToServer(serverSocketName());
}

void QJsonDbConnectionPrivate::handleRequestQueue()
{
    if (currentRequest)
        return;
    if (status != QJsonDbConnection::Connected)
        return;

    if (pendingRequests.isEmpty())
        return;
    QWeakPointer<QJsonDbRequest> request;
    do { request = pendingRequests.takeFirst(); } while (!request && !pendingRequests.isEmpty());
    if (request) {
        Q_ASSERT(request.data()->status() == QJsonDbRequest::Queued);
        QJsonDbRequestPrivate *drequest = request.data()->d_func();
        Q_ASSERT(drequest != 0);
        drequest->setStatus(QJsonDbRequest::Sent);
        QJsonObject req = drequest->getRequest();
        if (!req.isEmpty()) {
            currentRequest = request;
            if (drequest->isPrivatePartition())
                handlePrivatePartitionRequest(req);
            else
                stream->send(req);
        } else {
            drequest->setStatus(QJsonDbRequest::Error);
            QMetaObject::invokeMethod(request.data(), "error", Qt::QueuedConnection,
                                      Q_ARG(QtJsonDb::QJsonDbRequest::ErrorCode, QJsonDbRequest::MissingObject),
                                      Q_ARG(QString, QLatin1String("Empty request")));
        }
    }
}

void QJsonDbConnectionPrivate::handlePrivatePartitionRequest(const QJsonObject &request)
{
    Q_Q(QJsonDbConnection);

    if (!privatePartitionProcessing.isRunning())
        privatePartitionProcessing.start();

    if (!privatePartitionHandler) {
        privatePartitionHandler = new QJsonDbPrivatePartition;
        privatePartitionHandler->moveToThread(&privatePartitionProcessing);
        QObject::connect(&privatePartitionProcessing, SIGNAL(finished()),
                         privatePartitionHandler, SLOT(deleteLater()));
        QObject::connect(privatePartitionHandler, SIGNAL(readRequestStarted(int,quint32,QString)),
                         q, SLOT(_q_privateReadRequestStarted(int,quint32,QString)));
        QObject::connect(privatePartitionHandler, SIGNAL(writeRequestStarted(int,quint32)),
                         q, SLOT(_q_privateWriteRequestStarted(int,quint32)));
        QObject::connect(privatePartitionHandler, SIGNAL(finished(int)),
                         q, SLOT(_q_privateRequestFinished(int)));
        QObject::connect(privatePartitionHandler, SIGNAL(error(int,QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                         q, SLOT(_q_privateRequestError(int,QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        QObject::connect(privatePartitionHandler, SIGNAL(resultsAvailable(int,QList<QJsonObject>)),
                         q, SLOT(_q_privateRequestResultsAvailable(int,QList<QJsonObject>)));
    }

    QMetaObject::invokeMethod(privatePartitionHandler, "handleRequest", Qt::QueuedConnection, Q_ARG(QJsonObject, request));
}

void QJsonDbConnectionPrivate::_q_onReceivedObject(const QJsonObject &object)
{
    if (object.contains(JsonDbStrings::Property::notify())) {
        QString notifyUuid = object.value(JsonDbStrings::Property::uuid()).toString();
        QJsonObject sub = object.value(JsonDbStrings::Property::notify()).toObject();
        QString action = sub.value(JsonDbStrings::Protocol::action()).toString();
        QJsonObject notificationObject = sub.value(JsonDbStrings::Protocol::object()).toObject();
        quint32 stateNumber = sub.value(JsonDbStrings::Protocol::stateNumber()).toDouble();
        QMap<QString, QWeakPointer<QJsonDbWatcher> >::iterator it = watchers.find(notifyUuid);
        if (it != watchers.end()) {
            QJsonDbWatcher *watcher = it.value().data();
            if (!watcher) {
                qWarning("QJsonDbConnection: received notification for already deleted watcher");
                watchers.erase(it);
                return;
            }
            // initialize actionType to silence compiler warnings.
            QJsonDbWatcher::Action actionType = QJsonDbWatcher::All;
            bool stateChanged = false;
            if (action == JsonDbStrings::Notification::actionCreate())
                actionType = QJsonDbWatcher::Created;
            else if (action == JsonDbStrings::Notification::actionUpdate())
                actionType = QJsonDbWatcher::Updated;
            else if (action == JsonDbStrings::Notification::actionRemove())
                actionType = QJsonDbWatcher::Removed;
            else if (action == JsonDbStrings::Notification::actionStateChange())
                stateChanged = true;
            else
                qWarning() << "Unknown action" << action << "received for notification" << notifyUuid;

            if (stateChanged)
                watcher->d_func()->handleStateChange(stateNumber);
            else if (actionType != QJsonDbWatcher::All)
                watcher->d_func()->handleNotification(stateNumber, actionType, notificationObject);
        } else {
            // received notification for unknown watcher, just ignore it.
        }
    } else if (currentRequest) {
        QJsonDbRequestPrivate *drequest = currentRequest.data()->d_func();
        int requestId = static_cast<int>(object.value(JsonDbStrings::Protocol::requestId()).toDouble());
        if (requestId != drequest->requestId)
            return;
        currentRequest.clear();
        QJsonValue result = object.value(JsonDbStrings::Protocol::result());
        if (result.isObject()) {
            drequest->handleResponse(result.toObject());
        } else {
            QJsonObject error = object.value(JsonDbStrings::Protocol::error()).toObject();
            int code = static_cast<int>(error.value(JsonDbStrings::Protocol::errorCode()).toDouble());
            QString message = error.value(JsonDbStrings::Protocol::errorMessage()).toString();
            drequest->handleError(code, message);
        }
        handleRequestQueue();
    } else {
        handleRequestQueue();
    }
}

void QJsonDbConnectionPrivate::_q_onAuthFinished()
{
    Q_Q(QJsonDbConnection);
    Q_ASSERT(status == 2);
    status = QJsonDbConnection::Connected;
    emit q->statusChanged(status);
    reactivateAllWatchers();
    handleRequestQueue();
}

void QJsonDbConnectionPrivate::_q_privateReadRequestStarted(int requestId, quint32 stateNumber, const QString &sortKey)
{
    if (!currentRequest)
        return;
    QJsonDbReadRequest *request = qobject_cast<QJsonDbReadRequest *>(currentRequest.data());
    if (!request || request->d_func()->requestId != requestId)
        return;
    QJsonDbReadRequestPrivate *drequest = request->d_func();
    drequest->stateNumber = stateNumber;
    drequest->sortKey = sortKey;
    drequest->setStatus(QJsonDbRequest::Receiving);
    emit request->started();
}

void QJsonDbConnectionPrivate::_q_privateWriteRequestStarted(int requestId, quint32 stateNumber)
{
    if (!currentRequest)
        return;
    QJsonDbWriteRequest *request = qobject_cast<QJsonDbWriteRequest *>(currentRequest.data());
    if (!request || request->d_func()->requestId != requestId)
        return;
    QJsonDbWriteRequestPrivate *drequest = request->d_func();
    drequest->stateNumber = stateNumber;
    drequest->setStatus(QJsonDbRequest::Receiving);
    emit request->started();
}

void QJsonDbConnectionPrivate::_q_privateRequestFinished(int requestId)
{
    if (currentRequest) {
        QJsonDbRequest *request = currentRequest.data();
        QJsonDbRequestPrivate *drequest = request->d_func();
        if (drequest->requestId == requestId) {
            drequest->setStatus(QJsonDbRequest::Finished);
            emit request->finished();
            currentRequest.clear();
        }
    }
    handleRequestQueue();
}

void QJsonDbConnectionPrivate::_q_privateRequestError(int requestId, QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    if (currentRequest) {
        QJsonDbRequest *request = currentRequest.data();
        QJsonDbRequestPrivate *drequest = request->d_func();
        if (drequest->requestId == requestId) {
            drequest->setStatus(QJsonDbRequest::Error);
            emit request->error(code, message);
            currentRequest.clear();
        }
    }
    handleRequestQueue();
}

void QJsonDbConnectionPrivate::_q_privateRequestResultsAvailable(int requestId, const QList<QJsonObject> &results)
{
    if (!currentRequest)
        return;
    QJsonDbRequest *request = currentRequest.data();
    QJsonDbRequestPrivate *drequest = request->d_func();
    if (drequest->requestId != requestId)
        return;
    Q_ASSERT(drequest->status == QJsonDbRequest::Receiving);
    drequest->results += results;
    emit request->resultsAvailable(drequest->results.size());
}

/*!
    Constructs a new connection object with a given \a parent.
*/
QJsonDbConnection::QJsonDbConnection(QObject *parent)
    : QObject(parent), d_ptr(new QJsonDbConnectionPrivate(this))
{
}

/*!
    Destroys the JsonDbConnection object.
*/
QJsonDbConnection::~QJsonDbConnection()
{
    disconnectFromServer();
}

/*!
    \property QJsonDbConnection::socketName
    Specifies the local socket name that is used to establish the connection.

    If this property is not set, the default socket name is used.

    \sa connectToServer()
*/
void QJsonDbConnection::setSocketName(const QString &socketName)
{
    Q_D(QJsonDbConnection);
    if (d->status != QJsonDbConnection::Unconnected)
        qWarning("QJsonDbConnection: cannot set socket name on active connection");
    d->socketName = socketName;
}

QString QJsonDbConnection::socketName() const
{
    Q_D(const QJsonDbConnection);
    return d->socketName;
}

/*!
    \property QJsonDbConnection::status
    Specifies the current connection state.

    The statusChanged() signal is emitted when the connection status changes.

    \sa connected(), disconnected(), error()
*/
QJsonDbConnection::Status QJsonDbConnection::status() const
{
    Q_D(const QJsonDbConnection);
    return d->status;
}

/*!
    \property QJsonDbConnection::autoReconnectEnabled
    Specifies whether the connection should be re-established if it was droppped.

    This property is set to true by default.

    \sa connectToServer(), disconnectFromServer()
*/
void QJsonDbConnection::setAutoReconnectEnabled(bool value)
{
    Q_D(QJsonDbConnection);
    d->autoReconnectEnabled = value;
}

bool QJsonDbConnection::isAutoReconnectEnabled() const
{
    Q_D(const QJsonDbConnection);
    return d->autoReconnectEnabled;
}

/*!
    Returns the queue of request objects that are pending to be sent.
    \sa cancel()
*/
QList<QJsonDbRequest *> QJsonDbConnection::pendingRequests() const
{
    Q_D(const QJsonDbConnection);
    QList<QJsonDbRequest *> requests;
    requests.reserve(d->pendingRequests.size());
    foreach (const QWeakPointer<QJsonDbRequest> &request, d->pendingRequests) {
        if (request && !request.data()->d_func()->internal)
            requests.append(request.data());
    }
    return requests;
}

/*!
    Attempts to establish a connection to the database.

    \sa autoReconnectEnabled, status, disconnectFromServer()
*/
void QJsonDbConnection::connectToServer()
{
    Q_D(QJsonDbConnection);
    if (d->status != QJsonDbConnection::Unconnected)
        return;
    Q_ASSERT(d->socket->state() == QLocalSocket::UnconnectedState);

    d->explicitDisconnect = false;
    d->status = QJsonDbConnection::Connecting;
    emit statusChanged(d->status);

    d->socket->connectToServer(d->serverSocketName());
}

/*!
    Disconnects from the server.

    \sa autoReconnectEnabled, status, connectToServer()
*/
void QJsonDbConnection::disconnectFromServer()
{
    Q_D(QJsonDbConnection);
    if (d->status == QJsonDbConnection::Unconnected)
        return;
    d->explicitDisconnect = true;
    d->socket->disconnectFromServer();

    if (d->privatePartitionProcessing.isRunning()) {
        d->privatePartitionProcessing.quit();
        d->privatePartitionProcessing.wait();
    }
}

/*!
    Appends the given \a request to the queue and attempts to send it.

    Returns true if the request was successfully added to the queue.

    This function does not take ownership of the passed \a request object.

    \sa cancel()
*/
bool QJsonDbConnection::send(QJsonDbRequest *request)
{
    Q_D(QJsonDbConnection);
    if (!request)
        return false;
    if (request->isActive()) {
        qWarning("QJsonDbConnection: cannot send request that is currently active.");
        return false;
    }
    if (d->autoConnect) {
        d->autoConnect = false;
        connectToServer();
    }
    QJsonDbRequestPrivate *drequest = request->d_func();
    drequest->setStatus(QJsonDbRequest::Queued);
    if (drequest->internal)
        d->pendingRequests.prepend(QWeakPointer<QJsonDbRequest>(request));
    else
        d->pendingRequests.append(QWeakPointer<QJsonDbRequest>(request));
    drequest->setRequestId(++d->lastRequestId);
    if (d->status == QJsonDbConnection::Connected)
        d->handleRequestQueue();
    return true;
}

/*!
    Cancels the given \a request.

    It is only possible to cancel request that was queued, but not sent to the
    server yet, i.e. a request in the QJsonDbRequest::Queued state.

    Returns true if the request was successfully canceled.

    \sa cancelPendingRequests(), pendingRequests(), QJsonDbRequest::status
*/
bool QJsonDbConnection::cancel(QJsonDbRequest *request)
{
    Q_D(QJsonDbConnection);
    if (!request)
        return false;
    switch (request->status()) {
    case QJsonDbRequest::Inactive:
    case QJsonDbRequest::Error:
    case QJsonDbRequest::Finished:
    case QJsonDbRequest::Canceled:
        qWarning("QJsonDbConnection: cannot cancel request that was not added to connection.");
        return false;
    case QJsonDbRequest::Sent:
    case QJsonDbRequest::Receiving:
        qWarning("QJsonDbConnection: cannot cancel request that was already sent.");
        return false;
    case QJsonDbRequest::Queued:
        if (!d->pendingRequests.removeOne(request)) {
            qWarning("QJsonDbConnection: cannot cancel request that doesn't belong to this connection.");
            return false;
        }
        request->d_func()->setStatus(QJsonDbRequest::Canceled);
        return true;
    }
    return false;
}

/*!
    Cancels all pending requests that were in the request queue.

    \sa pendingRequests(), cancel()
*/
void QJsonDbConnection::cancelPendingRequests()
{
    Q_D(QJsonDbConnection);
    QList<QWeakPointer<QJsonDbRequest> > list;
    list.swap(d->pendingRequests);
    for (int i = 0; i < list.size(); ++i) {
        QWeakPointer<QJsonDbRequest> request = list.at(i);
        if (request && request.data()->d_func()->internal)
            d->pendingRequests.append(request);
    }
    for (int i = 0; i < list.size(); ++i) {
        QWeakPointer<QJsonDbRequest> request = list.at(i);
        if (request) {
            QJsonDbRequestPrivate *drequest = request.data()->d_func();
            if (!drequest->internal)
                drequest->setStatus(QJsonDbRequest::Canceled);
        }
    }
}

void QJsonDbConnectionPrivate::reactivateAllWatchers()
{
    QMap<QString, QWeakPointer<QJsonDbWatcher> >::iterator it;
    for (it = watchers.begin(); it != watchers.end();) {
        QJsonDbWatcher *watcher = it.value().data();
        if (!watcher) {
            it = watchers.erase(it);
            continue;
        }
        initWatcher(watcher);
        ++it;
    }
}

bool QJsonDbConnectionPrivate::initWatcher(QJsonDbWatcher *watcher)
{
    Q_Q(QJsonDbConnection);
    QJsonDbWatcherPrivate *dwatcher = watcher->d_func();

    if (dwatcher->partition == JsonDbStrings::Partition::privatePartition() ||
            dwatcher->partition.endsWith(QString::fromLatin1(".%1").arg(JsonDbStrings::Partition::privatePartition()))) {
        qWarning() << "QJsonDbWatcher does not support private partitions";
        return false;
    }

    // disconnect the internal signals so that if the watcher was in the process
    // of being activated/deactivated, we don't handle it anymore.
    foreach (QObject *child, watcher->children()) {
        QJsonDbRequest *request = qobject_cast<QJsonDbRequest *>(child);
        if (request && request->d_func()->internal)
            QObject::disconnect(request, 0, watcher, 0);
    }

    dwatcher->connection = q;
    dwatcher->setStatus(QJsonDbWatcher::Activating);

    // make notification object
    QJsonDbObject object;
    object.insert(JsonDbStrings::Property::type(), QJsonValue(JsonDbStrings::Types::notification()));
    object.insert(JsonDbStrings::Property::query(), QJsonValue(dwatcher->query));
    if (!dwatcher->bindings.isEmpty()) {
        QJsonObject b;
        QMap<QString, QJsonValue>::const_iterator it, e;
        for (it = dwatcher->bindings.begin(), e = dwatcher->bindings.end(); it != e; ++it)
            b.insert(it.key(), it.value());
        object.insert(JsonDbStrings::Property::bindings(), b);
    }
    bool initialStateNumberSpecified = (dwatcher->initialStateNumber != static_cast<quint32>(QJsonDbWatcherPrivate::UnspecifiedInitialStateNumber));
    if (dwatcher->lastStateNumber != 0 || initialStateNumberSpecified) {
        quint32 initialStateNumber;
        if (initialStateNumberSpecified)
            initialStateNumber = qMax(dwatcher->lastStateNumber, dwatcher->initialStateNumber);
        else
            initialStateNumber = dwatcher->lastStateNumber;
        object.insert(JsonDbStrings::Property::initialStateNumber(), static_cast<int>(initialStateNumber));
    }
    QJsonArray actions;
    if (dwatcher->actions & QJsonDbWatcher::Created)
        actions.append(JsonDbStrings::Notification::actionCreate());
    if (dwatcher->actions & QJsonDbWatcher::Updated)
        actions.append(JsonDbStrings::Notification::actionUpdate());
    if (dwatcher->actions & QJsonDbWatcher::Removed)
        actions.append(JsonDbStrings::Notification::actionRemove());
    object.insert(JsonDbStrings::Property::actions(), actions);
    object.insert(JsonDbStrings::Protocol::partition(), QJsonValue(dwatcher->partition));
    object.insert(JsonDbStrings::Property::uuid(), QJsonValue(dwatcher->uuid));

    Q_ASSERT(!dwatcher->uuid.isEmpty());
    Q_ASSERT(!QUuid(dwatcher->uuid).isNull());
    watchers.insert(dwatcher->uuid, QWeakPointer<QJsonDbWatcher>(watcher));

    // now make a request object
    QJsonDbWriteRequest *request = new QJsonDbWriteRequest(watcher);
    request->setObjects(QList<QJsonObject>() << object);
    request->QJsonDbRequest::d_func()->internal = true;
    QObject::connect(request, SIGNAL(finished()), watcher, SLOT(_q_onFinished()));
    QObject::connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     watcher, SLOT(_q_onError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    // auto delete request after it's complete
    QObject::connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    QObject::connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     request, SLOT(deleteLater()));
    q->send(request);

    return true;
}

void QJsonDbConnectionPrivate::removeWatcher(QJsonDbWatcher *watcher)
{
    Q_Q(QJsonDbConnection);
    if (!watcher)
        return;

    QJsonDbWatcherPrivate *dwatcher = watcher->d_func();
    if (!watchers.take(dwatcher->uuid))
        return;

    QJsonObject object;
    object.insert(JsonDbStrings::Property::type(), QJsonValue(JsonDbStrings::Types::notification()));
    object.insert(JsonDbStrings::Property::uuid(), dwatcher->uuid);
    object.insert(JsonDbStrings::Property::version(), dwatcher->version);
    object.insert(JsonDbStrings::Property::deleted(), true);

    // disconnect the internal signals so that if the watcher was in the process
    // of being activated/deactivated, we don't handle it anymore.
    foreach (QObject *child, watcher->children()) {
        QJsonDbRequest *request = qobject_cast<QJsonDbRequest *>(child);
        if (request && request->d_func()->internal)
            QObject::disconnect(request, 0, watcher, 0);
    }

    // create a request to remove notification object
    // This time we don't care about the response to that removal request.
    QJsonDbWriteRequest *request = new QJsonDbWriteRequest(watcher);
    request->setObjects(QList<QJsonObject>() << object);
    request->QJsonDbRequest::d_func()->internal = true;
    QObject::connect(request, SIGNAL(finished()), watcher, SLOT(_q_onFinished()));
    // auto delete request after it's complete
    QObject::connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    QObject::connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                     request, SLOT(deleteLater()));
    q->send(request);

    dwatcher->connection.clear();
}

/*!
    Registers and activates the given \a watcher withing the database connection.

    Returns true if the watcher was registered.

    Note however that registering a watcher doesn't necessery mean it was
    activated sucessfully, but QJsonDbWatcher::status property should be used to
    detect that.

    This function does not take ownership of the passed \a watcher object.

    \sa removeWatcher()
*/
bool QJsonDbConnection::addWatcher(QJsonDbWatcher *watcher)
{
    Q_D(QJsonDbConnection);
    if (!watcher)
        return false;
    if (d->autoConnect) {
        d->autoConnect = false;
        connectToServer();
    }
    QJsonDbWatcherPrivate *dwatcher = watcher->d_func();
    if (dwatcher->status != QJsonDbWatcher::Inactive) {
        qWarning("QJsonDbConnection: cannot add watcher that is already active.");
        return false;
    }
    return d->initWatcher(watcher);
}

/*!
    Unregisters and deactivates the given \a watcher.

    After being unregistered, the watcher no longer emits notifications.

    Returns true if succeeds (e.g. if watcher was previously registered).

    \sa addWatcher()
*/
bool QJsonDbConnection::removeWatcher(QJsonDbWatcher *watcher)
{
    Q_D(QJsonDbConnection);
    if (!watcher)
        return true;
    QJsonDbWatcherPrivate *dwatcher = watcher->d_func();
    if (dwatcher->status == QJsonDbWatcher::Inactive) {
        qWarning("QJsonDbConnection: cannot remove watcher that is not active.");
        return false;
    }
    if (!d->watchers.contains(dwatcher->uuid)) {
        qWarning("QJsonDbConnection: cannot remove watcher that was not added.");
        return false;
    }
    d->removeWatcher(watcher);
    return true;
}

/*!
    \nonreentrant

    Sets the default jsondb connection for the current thread to \a connection.
    This transfers ownership of \a connection to QtJsonDb, it will be deleted
    on thread exit, or on the next call to setDefaultConnection().

    The given default connection should typically be connected, so that the
    users of defaultConnection() could assume there is no need to explicitly
    call connectToServer().

    \sa QJsonDbConnection::defaultConnection()
*/
void QJsonDbConnection::setDefaultConnection(QJsonDbConnection *connection)
{
    _q_defaultConnection()->setLocalData(connection);
}

/*!
    Returns the default jsondb connection object for the current thread. If no
    connection has been set for the current thread with setDefaultConnection(),
    a new connection is created.

    The returned default connection can be assumed to be connected, so
    typically there is no need to call connectToServer() on it.

    The returned object is owned by QtJsonDb and should not be deleted.

    \sa QJsonDbConnection::setDefaultConnection()
*/
QJsonDbConnection *QJsonDbConnection::defaultConnection()
{
    QThreadStorage<QJsonDbConnection *> *storage = _q_defaultConnection();
    QJsonDbConnection *defaultConnection = storage->localData();
    if (!defaultConnection) {
        defaultConnection = new QJsonDbConnection;
        defaultConnection->d_func()->autoConnect = true;
        storage->setLocalData(defaultConnection);
    }
    return defaultConnection;
}

/*!
    \fn void QJsonDbConnection::connected()

    This signal is emitted when the connection to the database was established.

    \sa status, disconnected(), error()
*/
/*!
    \fn void QJsonDbConnection::disconnected()

    This signal is emitted when the connection to the database was dropped. If
    the autoReconnectEnabled property is set to true, a new connection attempt will be
    made.

    \sa autoReconnectEnabled, status, connected(), error()
*/
/*!
    \fn QJsonDbConnection::error(QtJsonDb::QJsonDbConnection::ErrorCode error, const QString &message)

    This signal is emitted when a connection error occured. \a error and \a
    message give more information on the cause of the error.

    Note that if the autoReconnectEnabled property is set to true, a new connection
    attempt will be made.

    \sa autoReconnectEnabled, status, connectToServer()
*/
#include "moc_qjsondbconnection.cpp"

QT_END_NAMESPACE_JSONDB
