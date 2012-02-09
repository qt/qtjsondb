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
*/
/*!
    \enum QJsonDbWriteRequest::ErrorCode

    This enum describes database connection errors for write requests that can
    be emitted by the error() signal.

    \value NoError
    \value MissingObject Missing object field.

    \sa error(), QJsonDbRequest::ErrorCode
*/

QJsonDbWriteRequestPrivate::QJsonDbWriteRequestPrivate(QJsonDbWriteRequest *q)
    : QJsonDbRequestPrivate(q)
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
*/
void QJsonDbWriteRequest::setObjects(const QList<QJsonObject> &objects)
{
    Q_D(QJsonDbWriteRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    d->objects = objects;
}

QList<QJsonObject> QJsonDbWriteRequest::objects() const
{
    Q_D(const QJsonDbWriteRequest);
    return d->objects;
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
    request.insert(JsonDbStrings::Protocol::partition(), partition);
    request.insert(JsonDbStrings::Protocol::requestId(), requestId);
    return request;
}

void QJsonDbWriteRequestPrivate::handleResponse(const QJsonObject &response)
{
    Q_Q(QJsonDbWriteRequest);
    // sigh, fix the server response
    if (response.contains(JsonDbStrings::Protocol::data())) {
        QJsonArray data = response.value(JsonDbStrings::Protocol::data()).toArray();
        foreach (const QJsonValue &v, data) {
            QJsonObject object = v.toObject();
            stateNumber = static_cast<quint32>(object.value(JsonDbStrings::Protocol::stateNumber()).toDouble());
            QJsonObject obj;
            obj.insert(JsonDbStrings::Property::uuid(), object.value(JsonDbStrings::Property::uuid()));
            obj.insert(JsonDbStrings::Property::version(), object.value(JsonDbStrings::Property::version()));
            results.append(obj);
        }
    } else {
        stateNumber = static_cast<quint32>(response.value(JsonDbStrings::Protocol::stateNumber()).toDouble());
        QJsonObject obj;
        obj.insert(JsonDbStrings::Property::uuid(), response.value(JsonDbStrings::Property::uuid()));
        obj.insert(JsonDbStrings::Property::version(), response.value(JsonDbStrings::Property::version()));
        results.append(obj);
    }
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
    setObjects(objects);
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