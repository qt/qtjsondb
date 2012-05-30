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

#include "jsondatabase.h"
#include "jsondbpartition.h"
#include <QJSEngine>
#include <QQmlEngine>
#include <qjsondbobject.h>
#include <qjsondbconnection.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \qmlclass JsonDatabase JsonDatabase
    \inqmlmodule QtJsonDb 1.0
    \since 1.x

    JsonDatabase allows to list and access different JsonDb partitions. This is exposed
    as a QML module, so that it can be used without creating a QML element.
*/

/*!
    \internal
 */
JsonDatabase::JsonDatabase(QObject *parent)
    :QObject(parent)
{
}

/*!
    \internal
 */
JsonDatabase::~JsonDatabase()
{
}

/*!
    \qmlmethod object QtJsonDb1::JsonDatabase::partition(partitionName)

    Retrieve the Partition object for the specifed \a partitionName. The script engine
    decides the life time of the returned object. The returned object can be saved
    in a 'property var' until it is required.

    \code
    import QtJsonDb 1.0 as JsonDb
    var nokiaDb = JsonDb.partition("com.nokia")
    \endcode
*/
JsonDbPartition* JsonDatabase::partition(const QString &partitionName)
{
    JsonDbPartition* newPartition = new JsonDbPartition(partitionName);
    QQmlEngine::setObjectOwnership(newPartition, QQmlEngine::JavaScriptOwnership);
    return newPartition;
}

/*!
    \qmlmethod QtJsonDb1::JsonDatabase::listPartitions(listCallback)

    Lists all partitions excluding private partitions. The script engine will
    destroy the objects during garbage collection.

    The \a listCallback has the following signature.

    \code
    import QtJsonDb 1.0 as JsonDb
    JsonDb.listPartitions(listCallback);

    function listCallback(error, result) {
        if (error) {
            // communication error or wrong parameters.
            // in case of error response will be  {status: Code, message: "plain text" }
        } else {
            // result is an array of objects describing the known partitions
        }
    }
    \endcode
*/

void JsonDatabase::listPartitions(const QJSValue &listCallback)
{
    if (!listCallback.isCallable()) {
        qWarning() << "Invalid callback specified.";
        return;
    }

    QJsonDbReadRequest *request = new QJsonDbReadRequest;
    request->setQuery(QStringLiteral("[?_type=\"Partition\"]"));
    request->setPartition(QStringLiteral("Ephemeral"));
    connect(request, SIGNAL(finished()), this, SLOT(onQueryFinished()));
    connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(onQueryError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            request, SLOT(deleteLater()));
    sharedConnection().send(request);
    listCallbacks.insert(request, listCallback);
}

/*!
    \qmlmethod QtJsonDb1::JsonDatabase::uuidFromString(string)

    Returns deterministic uuid that can be used to identify given \a identifier.

    The uuid is generated using QtJsonDb UUID namespace on a value of the
    given \a identifier.

    \code
    var uuid1 = JsonDb.uuidFromString("person:Name1");
    var uuid2 = JsonDb.uuidFromString("person:Name2");
    myPartition.update([{"_uuid" : uuid1, "_type": "Person", "name" : "Name1", "knows": uuid2},
                        {"_uuid" : uuid2, "_type": "Person", "name" : "Name2", "knows": uuid1}]);
    \endcode
*/

QString JsonDatabase::uuidFromString(const QString &identifier)
{
    return QJsonDbObject::createUuidFromString(identifier).toString();
}

void JsonDatabase::onQueryFinished()
{
    QJsonDbReadRequest *request = qobject_cast<QJsonDbReadRequest *>(sender());
    if (listCallbacks.contains(request)) {
        QJSValue callback = listCallbacks[request];
        QJSEngine *engine = callback.engine();
        QJSValueList args;
        args << QJSValue(QJSValue::UndefinedValue);
        QList<QJsonObject> objects = request->takeResults();
        int count = objects.count();
        if (count) {
            QJSValue response = engine->newArray(count);
            for (int i = 0; i < count; ++i) {
                QString partitionName = objects[i].value(QStringLiteral("name")).toString();
                response.setProperty(i, engine->newQObject(partition(partitionName)));
            }
            args << response;
        } else {
            args << engine->newArray();
        }
        callback.call(args);
        listCallbacks.remove(request);
    }
}

void JsonDatabase::onQueryError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    QJsonDbReadRequest *request = qobject_cast<QJsonDbReadRequest *>(sender());
    if (listCallbacks.contains(request)) {
        QJSValue callback = listCallbacks[request];
        QJSEngine *engine = callback.engine();

        QJSValueList args;
        QVariantMap error;
        error.insert(QStringLiteral("code"), code);
        error.insert(QStringLiteral("message"), message);

        args << engine->toScriptValue(QVariant(error)) << engine->newArray();

        callback.call(args);
        listCallbacks.remove(request);
    }
}

QJsonDbConnection& JsonDatabase::sharedConnection()
{
    QJsonDbConnection *connection = QJsonDbConnection::defaultConnection();
    Q_ASSERT(connection);
    connection->connectToServer();
    return *connection;
}

#include "moc_jsondatabase.cpp"
QT_END_NAMESPACE_JSONDB
