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

#include "qjsondbwriterequest_p.h"
#include "qjsondbstrings_p.h"
#include "qjsondbobject.h"

#include <QJsonArray>
#include <QDebug>

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \class QJsonDbWriteRequest
    \inmodule QtJsonDb

    \brief The QJsonDbWriteRequest class allows to put objects into the database.

    Every write request to JsonDb is essentially an update to the object store. You can
    update the database by adding a new object, modifying an existing object
    or removing an existing object.

    All updates are carried out by passing a QJsonObject list to the setObjects() function
    and then sending the write request object to the server via the QJsonDbConnection::send()
    function. How JsonDb processes each object depends on which properties the object has.
    For a description of these properties, please see the setObjects() function.

    \code
        QJsonObject object;
        object.insert(QStringLiteral("_type"), QLatin1String("Foo"));
        object.insert(QStringLiteral("firstName"), QLatin1String("Malcolm"));
        object.insert(QStringLiteral("lastName"), QLatin1String("Reinolds"));
        QList<QJsonObject> objects;
        objects.append(object);

        QJsonDbWriteRequest *request = new QJsonDbWriteRequest;
        request->setObjects(objects);
        connect(request, SIGNAL(finished()), this, SLOT(onCreateFinished()));
        connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onCreateError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                request, SLOT(deleteLater()));
        QJsonDbConnection *connection = new QJsonDbConnection;
        connection->send(request);
    \endcode

    See derived classes that provide some optional convenience -
    QJsonDbRemoveRequest to remove a list of objects, QJsonDbUpdateRequest to
    modify a list of objects and QJsonDbCreateRequest to create a list of
    objects.

    \sa QJsonDbObject
    \sa setObjects()
*/
/*!
    \enum QJsonDbWriteRequest::ErrorCode

    This enum describes database connection errors for write requests that can
    be emitted by the error() signal.

    \value NoError


    \value InvalidRequest
    \value OperationNotPermitted
    \value InvalidPartition
    \value DatabaseConnectionError
    \value PartitionUnavailable
    \value MissingObject Missing object field.
    \value DatabaseError
    \value MissingUUID
    \value MissingType
    \value UpdatingStaleVersion The value of _version supplied to this write request
        does not match the current value of _version stored for this object in the
        database.
    \value FailedSchemaValidation
    \value InvalidMap
    \value InvalidReduce
    \value InvalidSchemaOperation
    \value InvalidIndexOperation
    \value InvalidType

    \sa error(), QJsonDbRequest::ErrorCode
*/
/*!
    \enum QJsonDbWriteRequest::ConflictResolutionMode

    This enum describes the conflict resolution mode that is used for write requests.

    \value RejectStale Object updates that do not have consecutive versions are
    rejected. This is the default mode.

    \value Replace Forcefully updates the object even if there is another
    version in the database.

    Each object in the database is identified by UUID (which is stored in
    \c{_uuid} property) and each change to the object is identified by object
    version (which is stored in \c{_version} property). The version field
    contains the update counter and identifier for this object version. This
    enum specifies how to treat object updates that happen "at the same time"
    on the same object.

    For example lets assume there is object "{123}" with version "1-abc" in the
    database. Assume there are two clients A and B both reading and writing
    from the database, and both clients read object "{123}" and got object with
    version "1-abc". When client A updates the object from version "1-abc" to
    "2-def", and then client B attempts to update his object from version
    "1-abc" to "2-xyz", jsondb detects stale update which is rejected by
    default.

    However if the client B sets the conflict resolution mode to
    QJsonDbWriteRequest::Replace, the write will be accepted and object version
    "3-xyz" will be put into the database, effectively forcefully replacing
    existing object.
*/

QJsonDbWriteRequestPrivate::QJsonDbWriteRequestPrivate(QJsonDbWriteRequest *q)
    : QJsonDbRequestPrivate(q), conflictResolutionMode(QJsonDbWriteRequest::RejectStale), stateNumber(0)
{
}

/*!
    Constructs a new write request object with the given \a parent.
*/
QJsonDbWriteRequest::QJsonDbWriteRequest(QObject *parent)
    : QJsonDbRequest(new QJsonDbWriteRequestPrivate(this), parent)
{
}

/*!
    Destroys the request object.
*/
QJsonDbWriteRequest::~QJsonDbWriteRequest()
{
}

/*!
    \property QJsonDbWriteRequest::objects

    \brief the list of objects to be written to the database

    There are essentially 3 different types of requests which depend on the properties of the \a object being passed in.
    \list
    \li Create: To create an object you have to set the object's \c{_uuid} property. The object
    will then be created if it does not exist in JsonDb. If it exists then what you want is
    an update.
    \li Update: To update an existing object you have to supply the \c{_uuid} and \c{_version}
    properties that tell JsonDb which object and which version of the object to update. You
    may also set the conflict resolution mode to tell JsonDb how to update the object.
    \li Remove: To remove an object from JsonDb you have to ensure the \c{_deleted} property is
    set to true, and you have to also ensure the \c{_uuid} and \c{_version} properties are supplied.
    \endlist

    \note If you pass an object or list of objects as input to convenience class QJsonDbCreateRequest,
    the constructor ensures that \c{_uuid} is defined.

    \note If you pass an object or list of objects as input to convenience class QJsonDbRemoveRequest,
    the constructor ensures that \c{_deleted} is set to true.

    \warning it is much more efficient to pass in a list of objects to be batch processed then to add a single
    object to the database.

    \sa setConflictResolutionMode(), setObject()
*/

void QJsonDbWriteRequest::setObjects(const QList<QJsonObject> &objects)
{
    Q_D(QJsonDbWriteRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    d->objects = objects;
}

/*!
    \fn QJsonDbWriteRequest::setObject(const QJsonObject &object)

    Sets the object to be processsed by JsonDb

    \warning it is inefficient to process one object.

    \sa setObjects()
*/

QList<QJsonObject> QJsonDbWriteRequest::objects() const
{
    Q_D(const QJsonDbWriteRequest);
    return d->objects;
}

/*!
    \property QJsonDbWriteRequest::conflictResolutionMode

    \brief defines the conflict handling mode.

    In case when writing to the database results in an update that is not based
    on the same version of the object that is stored in the database, this
    property specifies the conflict handling mode.
*/
void QJsonDbWriteRequest::setConflictResolutionMode(QJsonDbWriteRequest::ConflictResolutionMode mode)
{
    Q_D(QJsonDbWriteRequest);
    d->conflictResolutionMode = mode;
}

QJsonDbWriteRequest::ConflictResolutionMode QJsonDbWriteRequest::conflictResolutionMode() const
{
    Q_D(const QJsonDbWriteRequest);
    return d->conflictResolutionMode;
}

/*!
    \property QJsonDbWriteRequest::stateNumber

    Returns a database state number that the write request was executed on.

    The property is populated after started() signal was emitted.

    \sa started()
*/
quint32 QJsonDbWriteRequest::stateNumber() const
{
    Q_D(const QJsonDbWriteRequest);
    return d->stateNumber;
}

QJsonObject QJsonDbWriteRequestPrivate::getRequest() const
{
    QJsonObject request;
    request.insert(JsonDbStrings::Protocol::action(), JsonDbStrings::Protocol::update());
    if (objects.size() == 0) {
        return QJsonObject();
    } else if (objects.size() == 1) {
        request.insert(JsonDbStrings::Protocol::object(), objects.at(0));
    } else {
        QJsonArray array;
        foreach (const QJsonObject &obj, objects)
            array.append(obj);
        request.insert(JsonDbStrings::Protocol::object(), array);
    }
    switch ((int)conflictResolutionMode) {
    case QJsonDbWriteRequest::RejectStale:
        request.insert(JsonDbStrings::Protocol::conflictResolutionMode(), JsonDbStrings::Protocol::rejectStale());
        break;
    case QJsonDbWriteRequest::Replace:
        request.insert(JsonDbStrings::Protocol::conflictResolutionMode(), JsonDbStrings::Protocol::replace());
        break;
    case 2: // internal type Merge
        request.insert(JsonDbStrings::Protocol::conflictResolutionMode(), JsonDbStrings::Protocol::merge());
        break;
    }
    request.insert(JsonDbStrings::Protocol::partition(), partition);
    request.insert(JsonDbStrings::Protocol::requestId(), requestId);
    return request;
}

void QJsonDbWriteRequestPrivate::handleResponse(const QJsonObject &response)
{
    Q_Q(QJsonDbWriteRequest);

    if (response.contains(JsonDbStrings::Protocol::data())) {
        QJsonArray data = response.value(JsonDbStrings::Protocol::data()).toArray();
        foreach (const QJsonValue &v, data) {
            QJsonObject object = v.toObject();
            QJsonObject obj;
            obj.insert(JsonDbStrings::Property::uuid(), object.value(JsonDbStrings::Property::uuid()));
            obj.insert(JsonDbStrings::Property::version(), object.value(JsonDbStrings::Property::version()));
            results.append(obj);
        }
    }

    stateNumber = static_cast<quint32>(response.value(JsonDbStrings::Protocol::stateNumber()).toDouble());

    setStatus(QJsonDbRequest::Receiving);
    emit q->started();
    emit q->resultsAvailable(results.size());
    setStatus(QJsonDbRequest::Finished);
    emit q->finished();
}

void QJsonDbWriteRequestPrivate::handleError(int code, const QString &message)
{
    Q_Q(QJsonDbWriteRequest);
    setStatus(QJsonDbRequest::Error);
    emit q->error(QJsonDbRequest::ErrorCode(code), message);
}

/*!
    \class QJsonDbCreateRequest
    \inmodule QtJsonDb

    \brief The QJsonDbCreateRequest class allows to create objects in the database.

    This is a convenience api for QJsonDbWriteRequest that generates uuid for
    the given object and puts it into QJsonDbWriteRequest.
*/
/*!
    Creates a new QJsonDbCreateRequest object with the given \a parent to create
    the given \a object in the database.
*/
QJsonDbCreateRequest::QJsonDbCreateRequest(const QJsonObject &object, QObject *parent)
    : QJsonDbWriteRequest(parent)
{
    QJsonDbObject obj = object;
    if (obj.uuid().isNull())
        obj.setUuid(QJsonDbObject::createUuid());
    QList<QJsonObject> list;
    list.append(obj);
    setObjects(list);
}

/*!
    Creates a new QJsonDbCreateRequest object with the given \a parent to create
    the given list of \a objects in the database.
*/
QJsonDbCreateRequest::QJsonDbCreateRequest(const QList<QJsonObject> &objects, QObject *parent)
    : QJsonDbWriteRequest(parent)
{
    QList<QJsonObject> objs = objects;
    for (int i = 0; i < objs.size(); ++i) {
        QJsonObject &obj = objs[i];
        if (!obj.contains(JsonDbStrings::Property::uuid()))
            obj.insert(JsonDbStrings::Property::uuid(), QUuid::createUuid().toString());
    }
    setObjects(objs);
}

/*!
    \class QJsonDbUpdateRequest
    \inmodule QtJsonDb

    \brief The QJsonDbUpdateRequest class allows to modify objects in the database.

    This is a convenience api for QJsonDbWriteRequest that passes all given
    objects to the write request.
*/
/*!
    Creates a new QJsonDbUpdateRequest object with the given \a parent to update
    the given \a object in the database.
*/
QJsonDbUpdateRequest::QJsonDbUpdateRequest(const QJsonObject &object, QObject *parent)
    : QJsonDbWriteRequest(parent)
{
    QJsonDbObject obj = object;
    if (obj.uuid().isNull()) {
        qWarning() << "QJsonDbUpdateRequest: couldn't update an object that doesn't have uuid";
        return;
    }
    QList<QJsonObject> list;
    list.append(obj);
    setObjects(list);
}

/*!
    Creates a new QJsonDbUpdateRequest object with the given \a parent to update
    the given list of \a objects in the database.
*/
QJsonDbUpdateRequest::QJsonDbUpdateRequest(const QList<QJsonObject> &objects, QObject *parent)
    : QJsonDbWriteRequest(parent)
{
    for (int i = 0; i < objects.size(); ++i) {
        QJsonDbObject obj = objects.at(i);
        if (obj.uuid().isNull()) {
            qWarning() << "QJsonDbUpdateRequest: couldn't update an object that doesn't have uuid";
            return;
        }
    }
    setObjects(objects);
}

/*!
    \class QJsonDbRemoveRequest
    \inmodule QtJsonDb

    \brief The QJsonDbRemoveRequest class allows to delete objects from the database.

    This is a convenience api for QJsonDbWriteRequest that marks the given
    objects to be deleted and puts them into QJsonDbWriteRequest.
*/
/*!
    Creates a new QJsonDbRemoveRequest object with the given \a parent to remove
    the given \a object from the database.
*/
QJsonDbRemoveRequest::QJsonDbRemoveRequest(const QJsonObject &object, QObject *parent)
    : QJsonDbWriteRequest(parent)
{
    QJsonObject obj =  object;
    if (!obj.value(JsonDbStrings::Property::deleted()).toBool())
        obj.insert(JsonDbStrings::Property::deleted(), QJsonValue(true));
    QList<QJsonObject> list;
    list.append(obj);
    setObjects(list);
}

/*!
    Creates a new QJsonDbRemoveRequest object with the given \a parent to remove
    the given list of \a objects from the database.
*/
QJsonDbRemoveRequest::QJsonDbRemoveRequest(const QList<QJsonObject> &objects, QObject *parent)
    : QJsonDbWriteRequest(parent)
{
    QList<QJsonObject> objs = objects;
    for (int i = 0; i < objs.size(); ++i) {
        QJsonObject &obj = objs[i];
        if (!obj.value(JsonDbStrings::Property::deleted()).toBool())
            obj.insert(JsonDbStrings::Property::deleted(), QJsonValue(true));
    }
    setObjects(objs);
}

#include "moc_qjsondbwriterequest.cpp"

QT_END_NAMESPACE_JSONDB
