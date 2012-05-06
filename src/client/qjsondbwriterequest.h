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

#ifndef QJSONDB_WRITE_REQUEST_H
#define QJSONDB_WRITE_REQUEST_H

#include <QtCore/QObject>
#include <QtCore/QJsonObject>
#include <QtCore/QDebug>

#include <QtJsonDb/qjsondbrequest.h>
#include <QtJsonDb/qjsondbobject.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbWriteRequestPrivate;
class Q_JSONDB_EXPORT QJsonDbWriteRequest : public QJsonDbRequest
{
    Q_OBJECT
    Q_PROPERTY(QList<QJsonObject> objects READ objects WRITE setObjects)
    Q_PROPERTY(QJsonDbWriteRequest::ConflictResolutionMode conflictResolutionMode READ conflictResolutionMode WRITE setConflictResolutionMode)

    Q_PROPERTY(quint32 stateNumber READ stateNumber)

public:
    QJsonDbWriteRequest(QObject *parent = 0);
    ~QJsonDbWriteRequest();

    enum ErrorCode {
        NoError = QJsonDbRequest::NoError,
        InvalidRequest = QJsonDbRequest::InvalidRequest,
        OperationNotPermitted = QJsonDbRequest::OperationNotPermitted,
        InvalidPartition = QJsonDbRequest::InvalidPartition,
        DatabaseConnectionError = QJsonDbRequest::DatabaseConnectionError,
        PartitionUnavailable = QJsonDbRequest::PartitionUnavailable,
        MissingObject = QJsonDbRequest::MissingObject,
        DatabaseError = QJsonDbRequest::DatabaseError,
        MissingUUID = QJsonDbRequest::MissingUUID,
        MissingType = QJsonDbRequest::MissingType,
        UpdatingStaleVersion = QJsonDbRequest::UpdatingStaleVersion,
        FailedSchemaValidation = QJsonDbRequest::FailedSchemaValidation,
        InvalidMap = QJsonDbRequest::InvalidMap,
        InvalidReduce = QJsonDbRequest::InvalidReduce,
        InvalidSchemaOperation = QJsonDbRequest::InvalidSchemaOperation,
        InvalidIndexOperation = QJsonDbRequest::InvalidIndexOperation,
        InvalidType = QJsonDbRequest::InvalidType
    };

    enum ConflictResolutionMode {
        RejectStale = 0,
        Replace = 1
        //Merge = 2
    };

    void setObjects(const QList<QJsonObject> &);
    QList<QJsonObject> objects() const;

    void setConflictResolutionMode(ConflictResolutionMode mode);
    ConflictResolutionMode conflictResolutionMode() const;

    // read request results. Data is only available after started() was emitted.
    quint32 stateNumber() const;

private:
    Q_DISABLE_COPY(QJsonDbWriteRequest)
    Q_DECLARE_PRIVATE(QJsonDbWriteRequest)

    Q_PRIVATE_SLOT(d_func(), void _q_privatePartitionStarted(quint32))
};

class QJsonDbCreateRequest : public QJsonDbWriteRequest
{
public:
    inline QJsonDbCreateRequest(const QJsonObject &object, QObject *parent = 0);
    inline QJsonDbCreateRequest(const QList<QJsonObject> &objects, QObject *parent = 0);
};

class QJsonDbUpdateRequest : public QJsonDbWriteRequest
{
public:
    inline QJsonDbUpdateRequest(const QJsonObject &object, QObject *parent = 0);
    inline QJsonDbUpdateRequest(const QList<QJsonObject> &objects, QObject *parent = 0);
};

class QJsonDbRemoveRequest : public QJsonDbWriteRequest
{
public:
    inline QJsonDbRemoveRequest(const QJsonObject &object, QObject *parent = 0);
    inline QJsonDbRemoveRequest(const QList<QJsonObject> &objects, QObject *parent = 0);
};

inline QJsonDbCreateRequest::QJsonDbCreateRequest(const QJsonObject &objectToCreate, QObject *parentObject)
    : QJsonDbWriteRequest(parentObject)
{
    QJsonDbObject obj = objectToCreate;
    if (obj.uuid().isNull())
        obj.setUuid(QJsonDbObject::createUuid());
    QList<QJsonObject> list;
    list.append(obj);
    setObjects(list);
}

inline QJsonDbCreateRequest::QJsonDbCreateRequest(const QList<QJsonObject> &objectsToCreate, QObject *parentObject)
    : QJsonDbWriteRequest(parentObject)
{
    QList<QJsonObject> objs = objectsToCreate;
    for (int i = 0; i < objs.size(); ++i) {
        QJsonObject &obj = objs[i];
        if (!obj.contains(QStringLiteral("_uuid")))
            obj.insert(QStringLiteral("_uuid"), QUuid::createUuid().toString());
    }
    setObjects(objs);
}

inline QJsonDbUpdateRequest::QJsonDbUpdateRequest(const QJsonObject &objectToUpdate, QObject *parentObject)
    : QJsonDbWriteRequest(parentObject)
{
    QJsonDbObject obj = objectToUpdate;
    if (obj.uuid().isNull()) {
        qWarning() << "QJsonDbUpdateRequest: couldn't update an object that doesn't have uuid";
        return;
    }
    QList<QJsonObject> list;
    list.append(obj);
    setObjects(list);
}

inline QJsonDbUpdateRequest::QJsonDbUpdateRequest(const QList<QJsonObject> &objectsToUpdate, QObject *parentObject)
    : QJsonDbWriteRequest(parentObject)
{
    for (int i = 0; i < objectsToUpdate.size(); ++i) {
        QJsonDbObject obj = objectsToUpdate.at(i);
        if (obj.uuid().isNull()) {
            qWarning() << "QJsonDbUpdateRequest: couldn't update an object that doesn't have uuid";
            return;
        }
    }
    setObjects(objectsToUpdate);
}

inline QJsonDbRemoveRequest::QJsonDbRemoveRequest(const QJsonObject &objectToRemove, QObject *parentObject)
    : QJsonDbWriteRequest(parentObject)
{
    QJsonObject obj = objectToRemove;
    if (!obj.value(QStringLiteral("_deleted")).toBool())
        obj.insert(QStringLiteral("_deleted"), QJsonValue(true));
    QList<QJsonObject> list;
    list.append(obj);
    setObjects(list);
}

inline QJsonDbRemoveRequest::QJsonDbRemoveRequest(const QList<QJsonObject> &objectsToRemove, QObject *parentObject)
    : QJsonDbWriteRequest(parentObject)
{
    QList<QJsonObject> objs = objectsToRemove;
    for (int i = 0; i < objs.size(); ++i) {
        QJsonObject &obj = objs[i];
        if (!obj.value(QStringLiteral("_deleted")).toBool())
            obj.insert(QStringLiteral("_deleted"), QJsonValue(true));
    }
    setObjects(objs);
}

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // QJSONDB_WRITE_REQUEST_H
