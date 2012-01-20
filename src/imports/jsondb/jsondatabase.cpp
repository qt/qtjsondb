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
#include "jsondb-object.h"
#include <QJSEngine>
#include <qdebug.h>

/*!
    \qmlclass JsonDatabase
    \inqmlmodule QtJsonDb
    \since 1.x

    JsonDatabase allows to list and access different JsonDb partitons. This is exposed
    as a QML module, so that it can be used without creating a QML element.
*/

JsonDatabase::JsonDatabase(QObject *parent)
    :QObject(parent)
{
    connect(&jsonDb, SIGNAL(response(int,const QVariant&)),
            this, SLOT(dbResponse(int,const QVariant&)),
            Qt::QueuedConnection);
    connect(&jsonDb, SIGNAL(error(int,int,QString)),
            this, SLOT(dbErrorResponse(int,int,QString)),
            Qt::QueuedConnection);

}

JsonDatabase::~JsonDatabase()
{
}

/*!
    \qmlmethod object QtJsonDb::JsonDatabase::partition(partitionName, parentItem)

    Retrieve the Partition object for the specifed \a partitonName. The \a parentItem
    decides the life time of the returned object. If the \a parentItem is null, the
    script engine will destroy the object during garbage collection.

    \code
    import QtJsonDb 1.0 as JsonDb
    var nokiaDb = JsonDb.partition("com.nokia", parentItem)
    \endcode
*/


JsonDbPartition* JsonDatabase::partition(const QString &partitionName, QObject *parent)
{
    JsonDbPartition * newPartition = new JsonDbPartition(partitionName, parent);
    return newPartition;
}

/*!
    \qmlmethod QtJsonDb::JsonDatabase::listPartitions(listCallback, parentItem)

    List all the partions accessible by this application. The \a parentItem
    decides the life time of the returned objects. If the \a parentItem is null, the
    script engine will destroy the objects during garbage collection.

    The \a listCallback has the following signature.

    \code
    import QtJsonDb 1.0 as JsonDb
    JsonDb.listPartitions(listCallback, parentItem);

    function listCallback(error, result) {
        if (error) {
            // communication error or wrong parameters.
            // in case of error response will be  {status: Code, message: "plain text" }
        } else {
            // result is an array of objects describing the know partitions
        }
    }
    \endcode
*/

void JsonDatabase::listPartitions(const QJSValue &listCallback, QObject *parent)
{
    if (!listCallback.isFunction()) {
        qWarning() << "Invalid callback specified.";
        return;
    }
    QString query(QLatin1String("[?_type=\"Partition\"]"));
    int id = jsonDb.query(query, 0, -1, QLatin1String("com.nokia.qtjsondb.System"));
    listCallbacks.insert(id, listCallback);
    parentItems.insert(id, parent);
}

/*!
    \qmlmethod QtJsonDb::JsonDatabase::uuidFromObject(object)

    Returns a new uuid that can be used to identify a given \a object.

    Note that the returned uuid might be unique on every invocation on the same
    object, if the \a object doesn't have the \c{_id} property and there is no
    schema.
*/

QString JsonDatabase::uuidFromObject(const QVariant &object)
{
    QUuid objectid = JsonDbObject::uuidFromObject(object.toMap());
    return objectid.toString();
}

void JsonDatabase::dbResponse(int id, const QVariant &result)
{
    if (listCallbacks.contains(id)) {
        // Make sure that id exists in the map.
        QJSValue callback = listCallbacks[id];
        QJSEngine *engine = callback.engine();
        QJSValueList args;
        args << QJSValue(QJSValue::UndefinedValue);
        QVariantMap objectMap = result.toMap();
        if (objectMap.contains(QLatin1String("data"))) {
            QVariantList items = objectMap.value(QLatin1String("data")).toList();
            int count = items.count();
            QJSValue response = engine->newArray(count);
            for (int i = 0; i < count; ++i) {
                QVariantMap object = items.at(i).toMap();
                QString partitionName = object.value(QLatin1String("name")).toString();
                response.setProperty(i, engine->newQObject(partition(partitionName, parentItems[id])));
            }
            args << response;
        } else {
            args << engine->newArray();
        }
        callback.call(args);
        listCallbacks.remove(id);
        parentItems.remove(id);
    }
}

void JsonDatabase::dbErrorResponse(int id, int code, const QString &message)
{
    if (listCallbacks.contains(id)) {
        QJSValue callback = listCallbacks[id];
        QJSEngine *engine = callback.engine();

        QJSValueList args;
        QVariantMap error;
        error.insert(QLatin1String("code"), code);
        error.insert(QLatin1String("message"), message);

        args << engine->toScriptValue(QVariant(error))<< engine->newArray();

        callback.call(args);
        listCallbacks.remove(id);
        parentItems.remove(id);
    }
}
