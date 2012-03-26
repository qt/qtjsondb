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

#include "jsondb-client.h"
#include "jsondb-client_p.h"
#include "jsondb-strings.h"

#include "jsondb-connection_p.h"

#include <QEvent>
#include <QUuid>

/*!
    \macro QT_USE_NAMESPACE_JSONDB
    \inmodule QtJsonDb

    This macro expands to using jsondb namespace and makes jsondb namespace
    visible to C++ source code.

    \code
        #include <jsondb-client.h>
        QT_USE_NAMESPACE_JSONDB
    \endcode

    To declare the class without including the declaration of the class:

    \code
        #include <jsondb-global.h>
        QT_BEGIN_NAMESPACE_JSONDB
        class JsonDbClient;
        QT_END_NAMESPACE_JSONDB
        QT_USE_NAMESPACE_JSONDB
    \endcode
*/

/*!
    \macro QT_BEGIN_NAMESPACE_JSONDB
    \inmodule QtJsonDb

    This macro begins a jsondb namespace. All forward declarations of QtJsonDb classes need to
    be wrapped in \c QT_BEGIN_NAMESPACE_JSONDB and \c QT_END_NAMESPACE_JSONDB.

    \sa QT_USE_NAMESPACE_JSONDB, QT_END_NAMESPACE_JSONDB
*/

/*!
    \macro QT_END_NAMESPACE_JSONDB
    \inmodule QtJsonDb

    This macro ends a jsondb namespace. All forward declarations of QtJsonDb classes need to
    be wrapped in \c QT_BEGIN_NAMESPACE_JSONDB and \c QT_END_NAMESPACE_JSONDB.

    \sa QT_USE_NAMESPACE_JSONDB, QT_BEGIN_NAMESPACE_JSONDB
*/

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \class JsonDbClient

    \brief The JsonDbClient class provides a client interface which connects to the JsonDb server.
*/

/*!
    \enum JsonDbClient::NotifyType

    This type is used to subscribe to certain notification actions.

    \value NotifyCreate Send notification when an object is created.
    \value NotifyUpdate Send notification when an object is updated.
    \value NotifyRemove Send notification when an object is removed.

    \sa registerNotification()
*/

/*!
    \enum JsonDbClient::Status

    This enum describes current database connection status.

    \value Null Not connected.
    \value Connecting Connection to the database is being established.
    \value Ready Connection established.
    \value Error Disconnected due to an error.
*/


/*! \internal
    Returns the index of a method, even if method is not normalized.
    Returns -1 if there's no such method.

    \a mo must not be 0
    \a method can be 0
*/
static int indexOfMethod(const QMetaObject *mo, const char *method)
{
    Q_ASSERT(mo);
    if (!method)
        return -1;

    int idx = mo->indexOfMethod(method + 1);
    if (idx < 0) {
        QByteArray norm = QMetaObject::normalizedSignature(method);
        idx = mo->indexOfMethod(norm.constData() + 1);
    }

    return idx;
}

/*!
    \internal

    Constructs a new client object using a given \a connection and \a parent.
*/
JsonDbClient::JsonDbClient(JsonDbConnection *connection, QObject *parent)
    : QObject(parent), d_ptr(new JsonDbClientPrivate(this))
{
    Q_D(JsonDbClient);
    d->init(connection);
}

/*!
    Constructs a new client object and connects via the local socket with a
    given name \a socketName and a given \a parent.
*/
JsonDbClient::JsonDbClient(const QString &socketName, QObject *parent)
    : QObject(parent)
{
    JsonDbConnection *connection = new JsonDbConnection(this);
    d_ptr.reset(new JsonDbClientPrivate(this));
    Q_D(JsonDbClient);
    d->init(connection);
    connection->connectToServer(socketName);
}

/*!
    Constructs a new client object and connects via the default local socket
    and a given \a parent.
*/
JsonDbClient::JsonDbClient(QObject *parent)
    :  QObject(parent), d_ptr(new JsonDbClientPrivate(this))
{
    Q_D(JsonDbClient);
    JsonDbConnection *connection = JsonDbConnection::instance();
    if (connection->thread() != thread())
        connection = new JsonDbConnection(this);
    d->init(connection);
    d->connection->connectToServer();
}

/*!
    Destroys the JsonDbClient object.
*/
JsonDbClient::~JsonDbClient()
{
}

/*!
    \internal
*/
bool JsonDbClient::event(QEvent *event)
{
    if (event->type() == QEvent::ThreadChange) {
        Q_D(JsonDbClient);
        if (JsonDbConnection::instance() == d->connection) {
            d->init(new JsonDbConnection(this));
            d->connection->connectToServer();
        }
    } else if (event->type() == QEvent::Timer) {
        Q_D(JsonDbClient);
        QTimerEvent *te = static_cast<QTimerEvent *>(event);
        if (te->timerId() == d->timeoutTimerId) {
            killTimer(d->timeoutTimerId);
            d->timeoutTimerId = -1;
            d->_q_timeout();
        }
    }
    return QObject::event(event);
}

/*!
    \fn void JsonDbClient::statusChanged()
    The signal is emitted when the connection status is changed.
    \sa status, connectToServer(), errorString()
*/
/*!
    \property JsonDbClient::status
    \brief Returns the current database connection status.
    \sa disconnected()
*/
JsonDbClient::Status JsonDbClient::status() const
{
    return d_func()->status;
}

void JsonDbClientPrivate::_q_statusChanged()
{
    Q_Q(JsonDbClient);
    JsonDbClient::Status newStatus = status;
    switch (connection->status()) {
    case JsonDbConnection::Disconnected:
        if (status != JsonDbClient::Error) {
            requestQueue.unite(sentRequestQueue);
            sentRequestQueue.clear();
            if (autoReconnect) {
                newStatus = JsonDbClient::Connecting;
                if (timeoutTimerId == -1)
                    timeoutTimerId = q->startTimer(5000);
            } else {
                newStatus = JsonDbClient::Error;
            }
        }
        break;
    case JsonDbConnection::Connecting:
    case JsonDbConnection::Authenticating:
        newStatus = JsonDbClient::Connecting;
        break;
    case JsonDbConnection::Ready:
        newStatus = JsonDbClient::Ready;
        _q_processQueue();
        break;
    case JsonDbConnection::Error:
        newStatus = JsonDbClient::Error;
        break;
    }
    if (status != newStatus) {
        status = newStatus;
        emit q->statusChanged();
    }
}

void JsonDbClientPrivate::_q_timeout()
{
    if (status != JsonDbClient::Error)
        connection->connectToServer();
}

void JsonDbClientPrivate::_q_processQueue()
{
    Q_Q(JsonDbClient);

    if (requestQueue.isEmpty())
        return;
    if (connection->status() != JsonDbConnection::Ready)
        return;

    QMap<int, QVariantMap>::iterator it = requestQueue.begin();
    int requestId = it.key();
    QVariantMap request = it.value();
    requestQueue.erase(it);

    if (!connection->request(requestId, request)) {
        requestQueue.insert(requestId, request);
        qWarning("qtjsondb: Cannot send request to the server from processQueue!");
        return;
    } else {
        sentRequestQueue.insert(requestId, request);
    }

    if (!requestQueue.isEmpty())
        QMetaObject::invokeMethod(q, "_q_processQueue", Qt::QueuedConnection);
}

bool JsonDbClientPrivate::send(int requestId, const QVariantMap &request)
{
    if (requestQueue.isEmpty() && connection->request(requestId, request)) {
        sentRequestQueue.insert(requestId, request);
        return true;
    }
    requestQueue.insert(requestId, request);
    return false;
}

/*!
    Returns true if the client is connected to the database.
    \sa status
*/
bool JsonDbClient::isConnected() const
{
    Q_D(const JsonDbClient);
    return d->status == JsonDbClient::Ready;
}

void JsonDbClientPrivate::init(JsonDbConnection *c)
{
    Q_Q(JsonDbClient);

    if (connection) {
        q->disconnect(q, SLOT(_q_statusChanged()));
        q->disconnect(q, SLOT(_q_handleNotified(QString,QVariant,QString)));
        q->disconnect(q, SLOT(_q_handleResponse(int,QVariant)));
        q->disconnect(q, SLOT(_q_handleError(int,int,QString)));
        q->disconnect(q, SIGNAL(disconnected()));
    }
    connection = c;

    _q_statusChanged();

    q->connect(connection, SIGNAL(statusChanged()), q, SLOT(_q_statusChanged()));
    q->connect(connection, SIGNAL(notified(QString,QVariant,QString)),
               SLOT(_q_handleNotified(QString,QVariant,QString)));
    q->connect(connection, SIGNAL(response(int,QVariant)),
               SLOT(_q_handleResponse(int,QVariant)));
    q->connect(connection, SIGNAL(error(int,int,QString)),
               SLOT(_q_handleError(int,int,QString)));
    q->connect(connection, SIGNAL(disconnected()),  SIGNAL(disconnected()));
}

void JsonDbClientPrivate::_q_handleNotified(const QString &notifyUuid, const QVariant &v, const QString &action)
{
    Q_Q(JsonDbClient);
    QVariantMap vdata = v.toMap();
    if (notifyCallbacks.contains(notifyUuid)) {
        NotifyCallback c = notifyCallbacks.value(notifyUuid);
        QList<QByteArray> params = c.method.parameterTypes();

        JsonDbClient::NotifyType type;
        if (action == JsonDbString::kCreateStr) {
            type = JsonDbClient::NotifyCreate;
        } else if (action == JsonDbString::kUpdateStr) {
            type = JsonDbClient::NotifyUpdate;
        } else if (action == JsonDbString::kRemoveStr) {
            type = JsonDbClient::NotifyRemove;
        } else if (action == QLatin1String("stateChange")) {
            return;
        } else {
            Q_ASSERT(false);
            return;
        }

        quint32 stateNumber = quint32(0); // ### TODO

        JsonDbNotification notification(vdata, type, stateNumber);
        if (params.size() == 2 && params.at(1) == QByteArray("QtAddOn::JsonDb::JsonDbNotification")) {
            c.method.invoke(c.object.data(), Q_ARG(QString, notifyUuid), Q_ARG(JsonDbNotification, notification));
        } else {
            c.method.invoke(c.object.data(), Q_ARG(QString, notifyUuid), Q_ARG(QVariant, vdata), Q_ARG(QString, action));
        }
        emit q->notified(notifyUuid, vdata, action);
        emit q->notified(notifyUuid, notification);
    }
}

void JsonDbClientPrivate::_q_handleResponse(int id, const QVariant &data)
{
    Q_Q(JsonDbClient);

    sentRequestQueue.remove(id);
    QHash<int, NotifyCallback>::iterator it = unprocessedNotifyCallbacks.find(id);
    if (it != unprocessedNotifyCallbacks.end()) {
        NotifyCallback c = it.value();
        unprocessedNotifyCallbacks.erase(it);
        QString notifyUuid = data.toMap().value(JsonDbString::kUuidStr).toString();
        notifyCallbacks.insert(notifyUuid, c);
    }
    QHash<int, Callback>::iterator idsit = ids.find(id);
    if (idsit == ids.end())
        return;
    Callback c = idsit.value();
    ids.erase(idsit);
    if (QObject *object = c.object.data()) {
        const QMetaObject *mo = object->metaObject();
        int idx = indexOfMethod(mo, c.successSlot);
        if (idx >= 0) {
            QMetaMethod method = mo->method(idx);
            QList<QByteArray> params = method.parameterTypes();
            const QVariant vdata = data.toMap();
            method.invoke(object, Q_ARG(int, id), Q_ARG(QVariant, vdata));
        } else {
            qWarning() << "JsonDbClient: non existent slot"
                       << (c.successSlot ? QLatin1String(c.successSlot+1) : QLatin1String("<null>"))
                       << "on" << object;
        }
    }
    emit q->response(id, data);
}

void JsonDbClientPrivate::_q_handleError(int id, int code, const QString &message)
{
    Q_Q(JsonDbClient);

    sentRequestQueue.remove(id);
    unprocessedNotifyCallbacks.remove(id);

    QHash<int, Callback>::iterator it = ids.find(id);
    if (it == ids.end())
        return;
    Callback c = it.value();
    ids.erase(it);
    if (QObject *object = c.object.data()) {
        const QMetaObject *mo = object->metaObject();
        int idx = indexOfMethod(mo, c.errorSlot);
        if (idx >= 0)
            mo->method(idx).invoke(object, Q_ARG(int, id), Q_ARG(int, code), Q_ARG(QString, message));
    }
    emit q->error(id, (JsonDbError::ErrorCode)code, message);
}

/*!
  \deprecated
  \obsolete
*/
int JsonDbClient::find(const QVariant &object, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);
    int id = d->connection->makeRequestId();
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    QVariantMap request;
    request.insert(JsonDbString::kActionStr, JsonDbString::kFindStr);
    request.insert(JsonDbString::kObjectStr, object);
    request.insert(JsonDbString::kPartitionStr, QString());
    d->send(id, request);
    return id;
}

/*!
  \deprecated
  \obsolete
  Starts a query \a queryString with \a offset and \a limit in partition \a partitionName.
  In case of success \a successSlot is triggered on \a target, \a errorSlot is triggered otherwise.
*/
int JsonDbClient::query(const QString &queryString, int offset, int limit,
                        const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);
    int id = d->connection->makeRequestId();
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    QVariantMap request = JsonDbConnection::makeQueryRequest(queryString, offset, limit, QVariantMap(), partitionName);
    d->send(id, request);
    return id;
}

/*!
  \fn int JsonDbClient::query(const QString &query, int offset = 0, int limit = -1,
              const QVariantMap &bindings = QVariantMap(),
              const QString &partitionName = QString(),
              QObject *target = 0, const char *successSlot = 0, const char *errorSlot = 0);

  Starts a database \a query in a given \a partitionName.

  This function starts a query, skipping the first matching \a offset items,
  and asynchronously returning up to \a limit items. Returns the reference id
  of the query.

  \a bindings can be used to pass e.g. user-provided data to query constraints, e.g.
  \code
    void get(const QString &name)
    {
        QString queryString = QLatin1String("[?_type=\"Person\"][?name=%name]");
        QVariantMap bindings;
        bindings.insert(QLatin1String("name"), name);
        client->query(queryString, 0, -1, bindings);
    }
  \endcode

  If the query fails (e.g. there is a syntax error in the \a query string) \a
  errorSlot is called (if given) on \a target in addition to the error()
  signal. The \a errorSlot should be a slot with the following format: \c {void
  errorSlot(int id, int code, const QString &message)}

  When the query result is ready \a successSlot is called (if given) on \a
  target in addition to the response() signal. The \a successSlot should be a
  slot with the following format: \c {void successSlot(int id, const QVariant
  &object)}

  Note that this function returns the whole response and hence is very
  inefficient for large datasets, in general a query() function should be preferred instead.

  A successful response object will include the following properties:

  \list
  \li \c data: a list of objects matching the query
  \li \c length: the number of returned objects
  \li \c offset: the offset of the returned objects in the list of all objects matching the query.
  \endlist

  \sa query(), JsonDbQuery, response(), error()
*/
// \sa query() doesn't generate proper link, but I still mention it so that we
// can change it when qdoc3 is fixed
int JsonDbClient::query(const QString &queryString, int offset, int limit,
                        const QMap<QString,QVariant> &bindings,
                        const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);
    int id = d->connection->makeRequestId();
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    QVariantMap request = JsonDbConnection::makeQueryRequest(queryString, offset, limit, bindings, partitionName);
    d->send(id, request);
    return id;
}

/*!
  Constructs and returns an object for executing a query.

  \sa JsonDbQuery
*/
JsonDbQuery *JsonDbClient::query()
{
    return new JsonDbQuery(this, this);
}

/*!
  Sends a request to insert \a object into the database. Returns the reference id of the query.

  Given \a object is created in partition \a partitionName.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \li \c _uuid: the unique id of the created object
  \li \c _version: the version of the created object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::create(const QVariant &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);

    int id = d->connection->makeRequestId();

    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    if (object.type() == QVariant::Map
        && object.toMap().value(JsonDbString::kTypeStr).toString() == JsonDbString::kNotificationTypeStr)
        d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback());

    QVariantMap request = JsonDbConnection::makeCreateRequest(object, partitionName);
    d->send(id, request);

    return id;
}

/*!
  Sends a request to update \a object in partition \a partitionName. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \li \c _uuid: the unique id of the updated object
  \li \c _version: the version of the updated object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::update(const QVariant &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);

    int id = d->connection->makeRequestId();

    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));

    QVariantMap request = JsonDbConnection::makeUpdateRequest(object, partitionName);
    d->send(id, request);

    return id;
}

/*!
  Sends a request to remove \a object from partition \a partitionName. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \li \c _uuid: the unique id of the removed object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::remove(const QVariant &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);

    int id = d->connection->makeRequestId();

    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    if (object.toMap().value(JsonDbString::kTypeStr).toString() == JsonDbString::kNotificationTypeStr)
        d->notifyCallbacks.remove(object.toMap().value(JsonDbString::kUuidStr).toString());

    QVariantMap request;
    if (object.type() == QVariant::String) {
        QString queryString = object.toString();
        QVariantMap query;
        query.insert(JsonDbString::kQueryStr, queryString);
        request = JsonDbConnection::makeRemoveRequest(query, partitionName);
    } else {
        request = JsonDbConnection::makeRemoveRequest(object, partitionName);
    }
    d->send(id, request);

    return id;
}

/*!
  \deprecated
  \obsolete
  Creates a notification for a given \a query and notification \a types.

  When an object that is matched a \a query is created/update/removed (depending on the given
  \a types), the \a notifySlot will be invoken on \a notifyTarget.

  Upon success, invokes \a responseSuccessSlot of \a responseTarget, if provided, else emits \c response().
  On error, invokes \a responseErrorSlot of \a responseTarget, if provided, else emits \c error().
*/
int JsonDbClient::notify(NotifyTypes types, const QString &query,
                         const QString &partitionName,
                         QObject *notifyTarget, const char *notifySlot,
                         QObject *responseTarget, const char *responseSuccessSlot, const char *responseErrorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);

    QVariantList actions;
    if (types & JsonDbClient::NotifyCreate)
        actions.append(JsonDbString::kCreateStr);
    if (types & JsonDbClient::NotifyRemove)
        actions.append(JsonDbString::kRemoveStr);
    if (types & JsonDbClient::NotifyUpdate)
        actions.append(JsonDbString::kUpdateStr);

    QVariantMap create;
    create.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    create.insert(JsonDbString::kQueryStr, query);
    create.insert(JsonDbString::kActionsStr, actions);
    create.insert(JsonDbString::kPartitionStr, partitionName);

    int id = d->connection->makeRequestId();

    d->ids.insert(id, JsonDbClientPrivate::Callback(responseTarget, responseSuccessSlot, responseErrorSlot));
    if (notifyTarget) {
        const QMetaObject *mo = notifyTarget->metaObject();
        int idx = indexOfMethod(mo, notifySlot);
        if (idx < 0) {
            qWarning("JsonDbClient::notify: No such method %s::%s",
                     mo->className(), notifySlot ? notifySlot + 1 : "<null>");
        } else {
            d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback(notifyTarget, mo->method(idx)));
        }
    } else {
        d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback());
    }

    QVariantMap request = JsonDbConnection::makeCreateRequest(create);
    d->send(id, request);

    return id;
}

/*!
  Creates a notification for a given notification \a types and \a query in a \a partition.

  When an object that is matched a \a query is created/update/removed (depending on the given
  \a types), the \a notifySlot will be invoken on \a notifyTarget.

  Upon success, invokes \a responseSuccessSlot of \a responseTarget, if provided, else emits \c response().
  On error, invokes \a responseErrorSlot of \a responseTarget, if provided, else emits \c error().

  \a notifySlot has the following signature notifySlot(const QString &notifyUuid, const JsonDbNotification &notification)

  Returns a uuid of a notification object that is passed as notifyUuid argument to the \a notifySlot.

  \sa unregisterNotification(), notified(), JsonDbNotification
*/
QString JsonDbClient::registerNotification(NotifyTypes types, const QString &query, const QString &partition,
                                           QObject *notifyTarget, const char *notifySlot,
                                           QObject *responseTarget, const char *responseSuccessSlot, const char *responseErrorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);

    QVariantList actions;
    if (types & JsonDbClient::NotifyCreate)
        actions.append(JsonDbString::kCreateStr);
    if (types & JsonDbClient::NotifyRemove)
        actions.append(JsonDbString::kRemoveStr);
    if (types & JsonDbClient::NotifyUpdate)
        actions.append(JsonDbString::kUpdateStr);

    QString uuid = QUuid::createUuid().toString();
    QVariantMap create;
    create.insert(JsonDbString::kUuidStr, uuid);
    create.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    create.insert(JsonDbString::kQueryStr, query);
    create.insert(JsonDbString::kActionsStr, actions);
    create.insert(JsonDbString::kPartitionStr, partition);

    int id = d->connection->makeRequestId();

    d->ids.insert(id, JsonDbClientPrivate::Callback(responseTarget, responseSuccessSlot, responseErrorSlot));
    if (notifyTarget) {
        const QMetaObject *mo = notifyTarget->metaObject();
        int idx = indexOfMethod(mo, notifySlot);
        if (idx < 0) {
            qWarning("JsonDbClient::notify: No such method %s::%s",
                     mo->className(), notifySlot ? notifySlot + 1 : "<null>");
        } else {
            d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback(notifyTarget, mo->method(idx)));
        }
    } else {
        d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback());
    }

    QVariantMap request = JsonDbConnection::makeUpdateRequest(create);
    d->send(id, request);

    return uuid;
}

/*!
  Deletes a notification for a given \a notifyUuid.

  \sa registerNotification()
*/
void JsonDbClient::unregisterNotification(const QString &notifyUuid)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);

    QVariantMap object;
    object.insert(JsonDbString::kUuidStr, notifyUuid);
    object.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);

    int id = d->connection->makeRequestId();
    QVariantMap request = JsonDbConnection::makeRemoveRequest(object);
    d->send(id, request);
}

/*!
  Sends a request to retrieve a description of changes since
  database state \a stateNumber. Limits the change descriptions to
  those for the types in \a types, if not empty in a given \a partitionName.
  Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  \sa response()
  \sa error()
*/
int JsonDbClient::changesSince(int stateNumber, QStringList types,
                               const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    Q_ASSERT(d->connection);

    int id = d->connection->makeRequestId();

    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));

    QVariantMap request = JsonDbConnection::makeChangesSinceRequest(stateNumber, types, partitionName);
    d->send(id, request);

    return id;
}

/*!
  Constructs and returns an object for executing a changesSince query.

  \sa JsonDbChangesSince
*/
JsonDbChangesSince *JsonDbClient::changesSince()
{
    return new JsonDbChangesSince(this, this);
}

/*!
  \property JsonDbClient::autoReconnect

  \brief Specifies whether to reconnect when server connection is lost.
*/
void JsonDbClient::setAutoReconnect(bool reconnect)
{
    Q_D(JsonDbClient);
    d->autoReconnect = reconnect;
}

bool JsonDbClient::autoReconnect() const
{
    return d_func()->autoReconnect;
}

/*!
  Connects to the server if not connected.
  \sa autoReconnect(), disconnectFromServer()
*/
void JsonDbClient::connectToServer()
{
    Q_D(JsonDbClient);
    if (d->status == JsonDbClient::Connecting || d->status == JsonDbClient::Ready)
        return;
    d->connection->connectToServer();
}

/*!
  Disconnects to the server if connected.
  \sa autoReconnect(), connectToServer(), errorString()
*/
void JsonDbClient::disconnectFromServer()
{
    Q_D(JsonDbClient);
    if (d->status != JsonDbClient::Ready)
        return;
    d->connection->disconnectFromServer();
}

/*!
  Returns a human readable description of the last database connection error.
*/
QString JsonDbClient::errorString() const
{
    return d_func()->connection->errorString();
}

/*!
    \fn void JsonDbClient::notified(const QString &notifyUuid, const JsonDbNotification &notification)

    Signal that a notification has been received. The notification object must
    have been created previously, usually with the \c registerNotification()
    function. The \a notifyUuid field is the uuid of the notification object.
    The \a notification object contains information describing the event.

    \sa registerNotification()
*/

/*!
  \fn void JsonDbClient::notified(const QString &notifyUuid, const QVariant &object, const QString &action)

    \deprecated
    \obsolete

    Signal that a notification has been received.  The notification
    object must have been created previously, usually with the
    \c create() function (an object with ``_type="notification"``).  The
    \a notifyUuid field is the uuid of the notification object.  The
    \a object field is the actual database object and the \a action
    field is the action that started the notification (one of
    "create", "update", or "remove").

    \sa notify(), create()
*/

/*!
    \fn void JsonDbClient::notified(const QString &notify_uuid, const QsonObject &object, const QString &action)

    \deprecated
    \obsolete

    Signal that a notification has been received.  The notification
    object must have been created previously, usually with the
    \c create() function (an object with ``_type="notification"``).  The
    \a notify_uuid field is the uuid of the notification object.  The
    \a object field is the actual database object and the \a action
    field is the action that started the notification (one of
    "create", "update", or "remove").

    \sa notify(), create()
*/

/*!
    \fn void JsonDbClient::response(int id, const QVariant &object)

    Signal that a response to a request has been received from the
    database.  The \a id parameter will match with the return result
    of the request to the database.  The \a object parameter depends
    on the type of the original request.

    \sa create(), update(), remove()
*/

/*!
    \fn void JsonDbClient::response(int id, const QsonObject &object)

    \deprecated
    \obsolete

    Signal that a response to a request has been received from the
    database.  The \a id parameter will match with the return result
    of the request to the database.  The \a object parameter depends
    on the type of the original request.

    \sa create(), update(), remove()
*/

/*!
    \fn void JsonDbClient::error(int id, int code, const QString &message)

    Signals an error in the database request.  The \a id parameter
    will match the return result of the original request to the
    database.  The \a code and \a message parameters indicate the error.

    \a code is an error code from JsonDbError::ErrorCode

    \sa create(), update(), remove(), JsonDbError::ErrorCode
*/

/*!
    \fn void JsonDbClient::disconnected()

    This signal is emitted when the client connection is broken. JsonDbClient
    automatically will try to reconnect, so this signal is just a convenience.

    \sa status, errorString(), connectToServer()
*/

/*!
    \fn int JsonDbClient::changesSince(int stateNumber, QStringList types, QObject *target, const char *successSlot, const char *errorSlot)
    \deprecated
    \obsolete
*/
/*!
    \fn int JsonDbClient::notify(NotifyTypes types, const QString &query,
           QObject *notifyTarget = 0, const char *notifySlot = 0,
           QObject *responseTarget = 0, const char *responseSuccessSlot = 0, const char *responseErrorSlot = 0)
    \deprecated
    \obsolete
    Registers notification for a \a query on \a types.
    Callback \a notifySlot is triggered on \a notifyTarget.
    When notification is created, \a responseSuccessSlot or \a responseErrorSlot is triggered on \a responseTarget.
    \sa registerNotification()
*/
/*!
    \fn int JsonDbClient::query(const QString &query, int offset, int limit, QObject *target, const char *successSlot, const char *errorSlot)
    \deprecated
    \obsolete
*/
/*!
    \fn int JsonDbClient::remove(const QsonObject &object, QObject *target = 0, const char *successSlot = 0, const char *errorSlot = 0)
    \deprecated
    \obsolete
*/
/*!
    \fn int JsonDbClient::update(const QsonObject &object, QObject *target = 0, const char *successSlot = 0, const char *errorSlot = 0)
    \deprecated
    \obsolete
*/

#include "moc_jsondb-client.cpp"

QT_END_NAMESPACE_JSONDB
