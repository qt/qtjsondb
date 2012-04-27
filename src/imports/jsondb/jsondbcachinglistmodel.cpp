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

//#define JSONDB_LISTMODEL_DEBUG
//#define JSONDB_LISTMODEL_BENCHMARK

#include "plugin.h"
#include "jsondbcachinglistmodel.h"

#include <QJSEngine>
#include <QJSValueIterator>
#include <QDebug>
#ifdef JSONDB_LISTMODEL_BENCHMARK
#include <QElapsedTimer>
#endif

/*!
  \internal
  \class JsonDbCachingListModel
*/

QT_BEGIN_NAMESPACE_JSONDB


/*!
    \qmlclass JsonDbCachingListModel JsonDbCachingListModel
    \inqmlmodule QtJsonDb
    \since 1.0

    The JsonDbCachingListModel provides a read-only ListModel usable with views such as
    ListView or GridView displaying data items matching a query. The sorting is done using
    an index set on the JsonDb server. If it doesn't find a matching index for the sortkey,
    the model goes into Error status. Maximum number of items in the model cache can be set
    by cacheSize property.

    When an item is not present in the internal cache, the model can return an 'undefined'
    object from data() method. It will be queued for retrieval and the model will notify its
    presence using the dataChanged() signal.

    The model is initialized by retrieving the result in chunks. After receiving the first
    chunk, the model is reset with items from it. The status will be "Querying" during
    fetching data and will be changed to "Ready".

    \note This is still a work in progress, so expect minor changes.

    \code
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        cacheSize: 100
        partitions: [JsonDb.Partition {
            name:"com.nokia.shared"
        }]
    }
    \endcode
*/

JsonDbCachingListModel::JsonDbCachingListModel(QObject *parent):
    QJsonDbQueryModel(QJsonDbConnection::defaultConnection(), parent)
{
    QJsonDbConnection::defaultConnection()->connectToServer();
}

JsonDbCachingListModel::~JsonDbCachingListModel()
{
}

//---------------- METHODS------------------------------

void JsonDbCachingListModel::classBegin()
{
}

void JsonDbCachingListModel::componentComplete()
{
    populate();
    connect (this, SIGNAL(objectAvailable(int,QJsonObject,QString)), this, SLOT(onObjectAvailable(int,QJsonObject,QString)));
}

/*!
    \qmlmethod  QtJsonDb::JsonDbCachingListModel::get(int index, function callback)
    Calls the callback with object at the specified \a index in the model. The result.object property
    contains the object in its raw form as returned by the query, the rolenames
    are not applied. The object.partition is the partition for the returned.
    If the index is out of range it returns an object with empty partition & object properties.

    \code
    function updateCallback(error, response) {
        if (error) {
            console.log("Update Error :"+JSON.stringify(error));
            return,
        }
        console.log("Response from Update");
        console.log("response.id = "+response.id +" count = "+response.items.length);
        for (var i = 0; i < response.items.length; i++) {
            console.log("_uuid = "+response.items[i]._uuid +" ._version = "+response.items[i]._version);
        }
    }
    function updateItemCallback(index, response) {
        if (response) {
            response.object.firstName = response.object.firstName+ "*";
            response.partition.update(response.object, updateCallback);
        }
    }
    onClicked: {
        contacts.get(listView.currentIndex, updateItemCallback);
    }
    \endcode
*/
void JsonDbCachingListModel::get(int index, const QJSValue &callback)
{
    getCallbacks.insertMulti(index, callback);
    fetchObject(index);
}

/*!
    \qmlmethod int QtJsonDb::JsonDbCachingListModel::indexOf(string uuid)

    Returns the index of the object with the \a uuid in the model. If the object is
    not found it returns -1
*/
int JsonDbCachingListModel::indexOf(const QString &uuid) const
{
    return QJsonDbQueryModel::indexOf(uuid);
}

/*!
    \qmlmethod object QtJsonDb::JsonDbCachingListModel::getPartition(int index)

    Returns the partition object at the specified \a index in the model. If
    the index is out of range it returns an empty object.
*/

JsonDbPartition* JsonDbCachingListModel::getPartition(int index)
{
    const QString partitionName = this->partitionName(index);
    if (!this->nameToJsonDbPartitionMap.contains(partitionName)) {
        this->nameToJsonDbPartitionMap.insert(partitionName,
                                           new JsonDbPartition(partitionName));
    }
    return this->nameToJsonDbPartitionMap.value(partitionName);
}

//---------------- PROPERTIES------------------------------

/*!
    \qmlproperty object QtJsonDb::JsonDbCachingListModel::bindings
    Holds the bindings for the placeholders used in the query string. Note that
    the placeholder marker '%' should not be included as part of the keys.

    \qml
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: '[?_type="Contact"][?name=%firstName]'
        bindings :{'firstName':'Book'}
        partitions:[ JsonDb.Partition {
            name:"com.nokia.shared"
        }]
    }
    \endqml

    \sa QtJsonDb::JsonDbCachingListModel::query

*/

/*!
    \qmlproperty int QtJsonDb::JsonDbCachingListModel::cacheSize
    Holds the maximum number of items cached by the model.
*/

/*!
    \qmlproperty object QtJsonDb::JsonDbCachingListModel::error
    \readonly

    This property holds the current error information for the object. It contains:
    \list
    \li error.code -  code for the current error.
    \li error.message - detailed explanation of the error
    \endlist
*/

/*!
    \qmlproperty string QtJsonDb::JsonDbCachingListModel::query

    The query string in JsonQuery format used by the model to fetch
    items from the database. Setting an empty query clears all the elements

    In the following example, the JsonDbCachingListModel would contain all
    the objects with \a _type "CONTACT" from partition called "com.nokia.shared"

    \qml
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        partitions:[ JsonDb.Partition {
            name:"com.nokia.shared"
        }]
    }
    \endqml

*/

/*!
    \qmlproperty int QtJsonDb1::JsonDbCachingListModel::rowCount
    The number of items in the model.
*/

/*!
    \qmlproperty ListOrObject QtJsonDb1::JsonDbCachingListModel::roleNames

    Controls which properties to expose from the objects matching the query.

    Setting \a roleNames to a list of strings causes the model to expose
    corresponding object values as roles to the delegate for each item viewed.

    \code
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"MyType\"]"
        partitions:[ JsonDb.Partition {
            name:"com.nokia.shared"
        }]
        roleNames: ['a', 'b']
    }
    ListView {
        model: listModel
        Text {
            text: a + ":" + b
        }
    \endcode

    Setting \a roleNames to a dictionary remaps properties in the object
    to the specified roles in the model.

    In the following example, role \a a would yield the value of
    property \a aLongName in the objects. Role \a liftedProperty would
    yield the value of \a o.nested.property for each matching object \a
    o in the database.

    \code
    function makeRoleNames() {
        return { 'a': 'aLongName', 'liftedProperty': 'nested.property' };
    }
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"MyType\"]"
        partitions: [JsonDb.Partition {
            name:"com.nokia.shared"
        }]
        roleNames: makeRoleNames()
    }
    ListView {
        model: listModel
        Text {
            text: a + " " + liftedProperty
        }
    }
    \endcode
*/

/*!
    \qmlproperty string QtJsonDb::JsonDbCachingListModel::sortOrder

    The order used by the model to sort the items. Make sure that there
    is a matching Index in the database for this sortOrder. This has to be
    specified in the JsonQuery format.

    In the following example, the JsonDbCachingListModel would contain all
    the objects of type \a "CONTACT" sorted by their \a firstName field

    \qml
    JsonDb.JsonDbCachingListModel {
        id: listModel
        query: "[?_type=\"CONTACT\"]"
        partitions:[ JsonDb.Partition {
            name:"com.nokia.shared"
        }]
        sortOrder: "[/firstName]"
    }
    \endqml

    \sa QtJsonDb1::JsonDbCachingListModel::bindings

*/

/*!
    \qmlproperty Status QtJsonDb::JsonDbCachingListModel::status
    The current status of the model.
    \list
    \li Status.None - The model is not initialized
    \li Status.Querying - It is querying the results from server
    \li Status.Ready - Results are ready
    \li Status.Error - Cannot find a matching index on the server
    \endlist
*/

/*!
    \qmlproperty list QtJsonDb1::JsonDbCachingListModel::partitions
    Holds the list of partition objects for the model.
    \code
    JsonDb.JsonDbCachingListModel {
        id: contacts
        query: '[?_type="Contact"]'
        partitions :[nokiaPartition, nokiaPartition2]
        roleNames: ["firstName", "lastName", "_uuid", "_version"]
        sortOrder:"[/firstName]"
    }

    \endcode
*/

QQmlListProperty<JsonDbPartition> JsonDbCachingListModel::partitions()
{
    return QQmlListProperty<JsonDbPartition>(this, 0
                                                     , &partitions_append
                                                     , &partitions_count
                                                     , &partitions_at
                                             , &partitions_clear);
}

void JsonDbCachingListModel::partitions_append(QQmlListProperty<JsonDbPartition> *p, JsonDbPartition *v)
{
    if (!v) {
        qWarning("Invalid partition object");
        return;
    }
    JsonDbCachingListModel *q = qobject_cast<JsonDbCachingListModel *>(p->object);
    if (q) {
        const QString partitionName = v->name();
        q->appendPartitionName(partitionName);
        if (!q->nameToJsonDbPartitionMap.contains(partitionName)) {
            q->nameToJsonDbPartitionMap.insert(partitionName,
                                               new JsonDbPartition(partitionName));
        }
    }
}

int JsonDbCachingListModel::partitions_count(QQmlListProperty<JsonDbPartition> *p)
{
    JsonDbCachingListModel *q = qobject_cast<JsonDbCachingListModel *>(p->object);
    if (q) {
        return q->partitionNames().size();
    }
    return 0;
}

JsonDbPartition* JsonDbCachingListModel::partitions_at(QQmlListProperty<JsonDbPartition> *p, int idx)
{
    JsonDbCachingListModel *q = qobject_cast<JsonDbCachingListModel *>(p->object);
    if (q && idx < q->partitionNames().size()) {
        QString partitionName = q->partitionName(idx);
        if (q->nameToJsonDbPartitionMap.contains(partitionName))
            return q->nameToJsonDbPartitionMap.value(partitionName);
    }
    return 0;
}

void JsonDbCachingListModel::partitions_clear(QQmlListProperty<JsonDbPartition> *p)
{
    JsonDbCachingListModel *q = qobject_cast<JsonDbCachingListModel *>(p->object);
    if (q) {
        q->setPartitionNames(QStringList());
        foreach (const QString &partitionName, q->nameToJsonDbPartitionMap.keys()) {
            q->nameToJsonDbPartitionMap.take(partitionName)->deleteLater();
        }
    }
}

void JsonDbCachingListModel::onObjectAvailable(int index, QJsonObject availableObject, QString partitionName)
{
    if (getCallbacks.contains(index)) {
        QVariant object = QVariant (availableObject.toVariantMap());
        JsonDbPartition *partition = new JsonDbPartition(partitionName);
        QJSValue result = g_declEngine->newObject();
        result.setProperty(QLatin1String("object"), g_declEngine->toScriptValue(object));
        result.setProperty(QLatin1String("partition"), g_declEngine->newQObject(partition));

        QMap<int, QJSValue>::iterator callbacksIter = getCallbacks.find(index);

        while ((callbacksIter != getCallbacks.end()) && (callbacksIter.key() == index)) {
            if (callbacksIter.value().isCallable()) {
                QJSValueList args;
                args << QJSValue(index) << result;
                callbacksIter.value().call(args);
            }
            callbacksIter ++;
        }
        getCallbacks.remove(index);
    }
}

#include "moc_jsondbcachinglistmodel.cpp"
QT_END_NAMESPACE_JSONDB
