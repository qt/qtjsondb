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

#include "jsondb-client.h"
#include "jsondb-client_p.h"
#include "jsondb-strings.h"

#include "jsondb-connection_p.h"

/*!
    \namespace QtAddOn
    \inmodule QtJsonDb
    \target Qt Namespace
*/

/*!
    \namespace QtAddOn::JsonDb
    \inmodule QtJsonDb
    \target QtAddOn::JsonDb Namespace

    \brief The QtAddOn::JsonDb namespace contains the C++ client for JsonDb.

    To use namespace QtAddOn::JsonDb from C++, use macro Q_USE_JSONDB_NAMESPACE.

    \code
        #include <jsondb-client.h>
        Q_USE_JSONDB_NAMESPACE
    \endcode

To declare the class without including the declaration of the class:
    \code
        #include <jsondb-global.h>
        Q_ADDON_JSONDB_BEGIN_NAMESPACE
        class JsonDbClient;
        Q_ADDON_JSONDB_END_NAMESPACE
        Q_USE_JSONDB_NAMESPACE
    \endcode

*/

/*!
    \macro Q_USE_JSONDB_NAMESPACE
    \inmodule QtJsonDb
    \brief Makes namespace QtAddOn::JsonDb visible to C++ source code.
*/

namespace QtAddOn { namespace JsonDb {

/*!
    \class QtAddOn::JsonDb::JsonDbClient

    \brief The JsonDbClient class provides a client interface which connects to the JsonDb server.
*/

/*!
    \internal
    \brief The JsonDbClient class provides a client interface which connects to the JsonDb server.

    Uses JsonDbConnection \a connection to connect.
*/
JsonDbClient::JsonDbClient(JsonDbConnection *connection, QObject *parent)
    : QObject(parent), d_ptr(new JsonDbClientPrivate(this, connection))
{
    Q_D(JsonDbClient);
    d->init();
}

/*!
    \brief The JsonDbClient class provides a client interface which connects to the JsonDb server.

    Creates a new JsonDbConnection and connects to the socket named \a socketName.
*/
JsonDbClient::JsonDbClient(const QString &socketName, QObject *parent)
    : QObject(parent)
{
    JsonDbConnection *connection = new JsonDbConnection(this);
    connection->connectToServer(socketName);
    d_ptr.reset(new JsonDbClientPrivate(this, connection));
    Q_D(JsonDbClient);
    d->init();
}

/*!
    \brief The JsonDbClient class provides a client interface which connects to the JsonDb server.

    Uses the singleton object returned by \c
    JsonDbConnection::instance() as for the database connection.
*/
JsonDbClient::JsonDbClient(QObject *parent)
    :  QObject(parent), d_ptr(new JsonDbClientPrivate(this, JsonDbConnection::instance()))
{
    Q_D(JsonDbClient);
    d->init();
    d->connection->connectToServer();
}

JsonDbClient::~JsonDbClient()
{
}

/*!
    Returns true if the client is connected to the database.
*/
bool JsonDbClient::isConnected() const
{
    Q_D(const JsonDbClient);
    Q_ASSERT(d->connection);
    return d->connection->isConnected();
}

JsonDbClientPrivate::JsonDbClientPrivate(JsonDbClient *q, JsonDbConnection *c)
    :q_ptr(q), connection(c)
{
     Q_ASSERT(connection);
}

JsonDbClientPrivate::~JsonDbClientPrivate()
{
}

void JsonDbClientPrivate::init(Qt::ConnectionType type)
{
    Q_Q(JsonDbClient);
    q->connect(connection, SIGNAL(notified(QString,QsonObject,QString)),
               SLOT(_q_handleNotified(QString,QsonObject,QString)),type);
    q->connect(connection, SIGNAL(response(int,QsonObject)),
               SLOT(_q_handleResponse(int,QsonObject)),type);
    q->connect(connection, SIGNAL(error(int,int,QString)),
               SLOT(_q_handleError(int,int,QString)),type);
    q->connect(connection, SIGNAL(disconnected()),  SIGNAL(disconnected()));
    q->connect(connection, SIGNAL(readyWrite()),  SIGNAL(readyWrite()));
}

void JsonDbClientPrivate::_q_handleNotified(const QString &notifyUuid, const QsonObject &data, const QString &action)
{
    Q_Q(JsonDbClient);
    if (notifyCallbacks.contains(notifyUuid)) {
        NotifyCallback c = notifyCallbacks.value(notifyUuid);
        QList<QByteArray> params = c.method.parameterTypes();
        if (params.size() >= 2 && params.at(1) == QByteArray("QsonObject")) {
            c.method.invoke(c.object.data(), Q_ARG(QString, notifyUuid), Q_ARG(QsonObject, data), Q_ARG(QString, action));
        } else {
            const QVariant vdata = qsonToVariant(data);
            c.method.invoke(c.object.data(), Q_ARG(QString, notifyUuid), Q_ARG(QVariant, vdata), Q_ARG(QString, action));
        }
        emit q->notified(notifyUuid, data, action);
        emit q->notified(notifyUuid, qsonToVariant(data), action);
    }
}

void JsonDbClientPrivate::_q_handleResponse(int id, const QsonObject &data)
{
    Q_Q(JsonDbClient);
    QHash<int, NotifyCallback>::iterator it = unprocessedNotifyCallbacks.find(id);
    if (it != unprocessedNotifyCallbacks.end()) {
        NotifyCallback c = it.value();
        unprocessedNotifyCallbacks.erase(it);
        QString notifyUuid = data.toMap().valueString(JsonDbString::kUuidStr);
        notifyCallbacks.insert(notifyUuid, c);
    }
    QHash<int, Callback>::iterator idsit = ids.find(id);
    if (idsit == ids.end())
        return;
    Callback c = idsit.value();
    ids.erase(idsit);
    if (QObject *object = c.object.data()) {
        const QMetaObject *mo = object->metaObject();
        int idx = mo->indexOfMethod(c.successSlot+1);
        if (idx < 0) {
            QByteArray norm = QMetaObject::normalizedSignature(c.successSlot);
            idx = mo->indexOfMethod(norm.constData()+1);
        }
        if (idx >= 0) {
            QMetaMethod method = mo->method(idx);
            QList<QByteArray> params = method.parameterTypes();
            if (params.size() >= 2 && params.at(1) == QByteArray("QsonObject")) {
                method.invoke(object, Q_ARG(int, id), Q_ARG(QsonObject, data));
            } else {
                const QVariant vdata = qsonToVariant(data);
                method.invoke(object, Q_ARG(int, id), Q_ARG(QVariant, vdata));
            }
        }
    }
    emit q->response(id, qsonToVariant(data));
    emit q->response(id, data);
}

void JsonDbClientPrivate::_q_handleError(int id, int code, const QString &message)
{
    Q_Q(JsonDbClient);
    unprocessedNotifyCallbacks.remove(id);
    QHash<int, Callback>::iterator it = ids.find(id);
    if (it == ids.end())
        return;
    Callback c = it.value();
    ids.erase(it);
    if (QObject *object = c.object.data()) {
        const QMetaObject *mo = object->metaObject();
        int idx = mo->indexOfMethod(c.errorSlot+1);
        if (idx < 0) {
            QByteArray norm = QMetaObject::normalizedSignature(c.errorSlot);
            idx = mo->indexOfMethod(norm.constData()+1);
        }
        if (idx >= 0)
            mo->method(idx).invoke(object, Q_ARG(int, id), Q_ARG(int, code), Q_ARG(QString, message));
    }
    emit q->error(id, (JsonDbError::ErrorCode)code, message);
}

/*!
  \deprecated

  Sends \a queryObject to the database. Returns the reference id of the query.

  The \a queryObject contains \c query: a query string and my
  optionally contain \c bindings, \c limit and \c offset values. If
  provided, \c bindings maps names in the query string to values
  used in the query.

  A successful response will include the following properties:

  \list
  \o \c data: a list of objects matching the query
  \o \c length: the number of returned objects
  \o \c offset: the offset of the returned objects in the list of all objects matching the query.
  \endlist

  \sa response(), error()
*/
int JsonDbClient::find(const QVariant &object)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeFindRequest(object));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback());
    return id;
}

int JsonDbClient::find(const QsonObject &object, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeFindRequest(object));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    return id;
}

int JsonDbClient::query(const QString &queryString, int offset, int limit,
                        const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeQueryRequest(queryString, offset, limit, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    return id;
}

/*!
  \inmodule QtJsonDb
  \deprecated

  \brief Sends a request to insert \a object into the database. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \o \c _uuid: the unique id of the created object
  \o \c _version: the version of the created object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::create(const QsonObject &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeCreateRequest(object, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    if (object.toMap().valueString(JsonDbString::kTypeStr) == JsonDbString::kNotificationTypeStr)
        d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback());
    return id;
}

/*!
  \inmodule QtJsonDb

  \brief Sends a request to insert \a object into the database. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \o \c _uuid: the unique id of the created object
  \o \c _version: the version of the created object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::create(const QVariant &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeCreateRequest(object, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    if (object.toMap().value(JsonDbString::kTypeStr).toString() == JsonDbString::kNotificationTypeStr)
        d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback());
    return id;
}

/*!
  \inmodule QtJsonDb
  \deprecated

  \brief Sends a request to update \a object in the database. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \o \c _uuid: the unique id of the updated object
  \o \c _version: the version of the updated object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::update(const QsonObject &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeUpdateRequest(object, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    return id;
}

/*!
  \inmodule QtJsonDb

  \brief Sends a request to update \a object in the database. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \o \c _uuid: the unique id of the updated object
  \o \c _version: the version of the updated object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::update(const QVariant &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeUpdateRequest(object, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    return id;
}

/*!
  \inmodule QtJsonDb
  \deprecated

  \brief Sends a request to remove \a object from the database. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \o \c _uuid: the unique id of the removed object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::remove(const QsonObject &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeRemoveRequest(object, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    if (object.toMap().valueString(JsonDbString::kTypeStr) == JsonDbString::kNotificationTypeStr)
        d->notifyCallbacks.remove(object.toMap().valueString(JsonDbString::kUuidStr));
    return id;
}

/*!
  \inmodule QtJsonDb

  \brief Sends a request to remove \a object from the database. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:

  \list
  \o \c _uuid: the unique id of the removed object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::remove(const QVariant &object, const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeRemoveRequest(object, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    if (object.toMap().value(JsonDbString::kTypeStr).toString() == JsonDbString::kNotificationTypeStr)
        d->notifyCallbacks.remove(object.toMap().value(JsonDbString::kUuidStr).toString());
    return id;
}

/*!
  \inmodule QtJsonDb

  \brief Sends a request to remove from the database objects that match \a queryString. Returns the reference id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  A successful response will include the following properties:
  \list
  \o \c _uuid: the unique id of the removed object
  \endlist

  \sa response()
  \sa error()
*/
int JsonDbClient::remove(const QString &queryString, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeRemoveRequest(queryString));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    return id;
}

/*!
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
    if (!d->connection)
        return -1;
    QsonList actions;
    if (types & JsonDbClient::NotifyCreate)
        actions.append(JsonDbString::kCreateStr);
    if (types & JsonDbClient::NotifyRemove)
        actions.append(JsonDbString::kRemoveStr);
    if (types & JsonDbClient::NotifyUpdate)
        actions.append(JsonDbString::kUpdateStr);
    QsonMap create;
    create.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    create.insert(JsonDbString::kQueryStr, query);
    create.insert(JsonDbString::kActionsStr, actions);
    int id = d->connection->request(JsonDbConnection::makeCreateRequest(create, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(responseTarget, responseSuccessSlot, responseErrorSlot));
    if (notifyTarget && notifySlot) {
        const QMetaObject *mo = notifyTarget->metaObject();
        int idx = mo->indexOfMethod(notifySlot+1);
        if (idx < 0) {
            QByteArray norm = QMetaObject::normalizedSignature(notifySlot);
            idx = mo->indexOfMethod(norm.constData()+1);
        }
        if (idx < 0) {
            qWarning("JsonDbClient::notify: No such method %s::%s",
                     mo->className(), notifySlot);
        } else {
            d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback(notifyTarget, mo->method(idx)));
        }
    } else {
        d->unprocessedNotifyCallbacks.insert(id, JsonDbClientPrivate::NotifyCallback());
    }
    return id;
}

/*!
  \inmodule QtJsonDb

  \brief Sends a request to retrieve a description of changes since
  database state \a stateNumber. Limits the change descriptions to
  those for the types in \a types, if not empty. Returns the reference
  id of the query.

  Upon success, invokes \a successSlot of \a target, if provided, else emits \c response().
  On error, invokes \a errorSlot of \a target, if provided, else emits \c error().

  \sa response()
  \sa error()
*/
int JsonDbClient::changesSince(int stateNumber, QStringList types,
                               const QString &partitionName, QObject *target, const char *successSlot, const char *errorSlot)
{
    Q_D(JsonDbClient);
    if (!d->connection)
        return -1;
    int id = d->connection->request(JsonDbConnection::makeChangesSinceRequest(stateNumber, types, partitionName));
    if (id == -1)
        return -1;
    d->ids.insert(id, JsonDbClientPrivate::Callback(target, successSlot, errorSlot));
    return id;
}

/*!
    \fn void QtAddOn::JsonDb::JsonDbClient::notified(const QString &notifyUuid, const QVariant &object, const QString &action)

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
    \fn void QtAddOn::JsonDb::JsonDbClient::notified(const QString &notify_uuid, const QsonObject &object, const QString &action)

    \deprecated

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
    \fn void QtAddOn::JsonDb::JsonDbClient::response(int id, const QVariant &object)

    Signal that a response to a request has been received from the
    database.  The \a id parameter will match with the return result
    of the request to the database.  The \a object parameter depends
    on the type of the original request.

    \sa create(), update(), remove()
*/

/*!
    \fn void QtAddOn::JsonDb::JsonDbClient::response(int id, const QsonObject &object)

    \deprecated

    Signal that a response to a request has been received from the
    database.  The \a id parameter will match with the return result
    of the request to the database.  The \a object parameter depends
    on the type of the original request.

    \sa create(), update(), remove()
*/

/*!
    \fn void QtAddOn::JsonDb::JsonDbClient::error(int id, int code, const QString &message)

    Signals an error in the database request.  The \a id parameter
    will match the return result of the original request to the
    database.  The \a code and \a message parameters indicate the error.

    \sa create(), update(), remove(), JsonDbError::ErrorCode
*/

#include "moc_jsondb-client.cpp"

} } // end namespace QtAddOn::JsonDb
