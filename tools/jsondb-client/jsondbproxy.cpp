#include "jsondbproxy.h"
#include "qjsondbobject.h"
#include "qjsondbreadrequest.h"
#include "qjsondbwriterequest.h"

#include <QDebug>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

QT_USE_NAMESPACE

QVariantMap _waitForResponse(QtJsonDb::QJsonDbRequest *request) {
    QEventLoop e;
    while (!(request->status() == QtJsonDb::QJsonDbRequest::Error || request->status() == QtJsonDb::QJsonDbRequest::Finished))
        e.processEvents();

    QVariantMap res;

    if (request->status() != QtJsonDb::QJsonDbRequest::Error) {
        QList<QJsonObject> results = request->takeResults();
        QVariantList data;
        foreach (const QJsonObject &obj, results)
            data.append(obj.toVariantMap());
        res.insert(QLatin1Literal("result"), data);
    } else {
        QVariantMap error;
        error.insert(QLatin1Literal("code"), 1);
        error.insert(QLatin1Literal("message"), "Error processing request");
        res.insert(QLatin1Literal("error"), error);
    }
    return res;
}


JsonDbProxy::JsonDbProxy(QtJsonDb::QJsonDbConnection *conn, QObject *parent) :
    QObject(parent)
  , mConnection(conn)
{
}

QVariantMap JsonDbProxy::find(QVariantMap object)
{
    QtJsonDb::QJsonDbReadRequest *request = new QtJsonDb::QJsonDbReadRequest(this);
    request->setQuery(object.value(QLatin1Literal("query")).toString());
    if (object.contains(QLatin1Literal("limit")))
        request->setQueryLimit(object.value(QLatin1Literal("limit")).toInt());
    mConnection->send(request);
    return _waitForResponse(request);
}

QVariantMap JsonDbProxy::create(QVariantMap object)
{
    // handle the to-be-deprecated _id property
    QtJsonDb::QJsonDbObject obj = QJsonObject::fromVariantMap(object);
    if (obj.uuid().isNull() && obj.contains(QLatin1Literal("_id"))) {
        obj.setUuid(QtJsonDb::QJsonDbObject::createUuidFromString(obj.value(QLatin1Literal("_id")).toString()));
        obj.remove(QLatin1String("_id"));
    }
    QtJsonDb::QJsonDbCreateRequest *request = new QtJsonDb::QJsonDbCreateRequest(QList<QJsonObject>() << obj,
                                                                                this);
    mConnection->send(request);
    return _waitForResponse(request);
}

QVariantMap JsonDbProxy::update(QVariantMap object)
{
    QtJsonDb::QJsonDbUpdateRequest *request = new QtJsonDb::QJsonDbUpdateRequest(QList<QJsonObject>() << QJsonObject::fromVariantMap(object),
                                                                                this);
    mConnection->send(request);
    return _waitForResponse(request);
}

QVariantMap JsonDbProxy::remove(QVariantMap object )
{
    QtJsonDb::QJsonDbRemoveRequest *request = new QtJsonDb::QJsonDbRemoveRequest(QList<QJsonObject>() << QJsonObject::fromVariantMap(object),
                                                                                this);
    mConnection->send(request);
    return _waitForResponse(request);
}

QVariantMap JsonDbProxy::createList(QVariantList list)
{
    QList<QJsonObject> objects;
    foreach (const QVariant &object, list) {
        QtJsonDb::QJsonDbObject obj = QJsonObject::fromVariantMap(object.toMap());
        if (!obj.uuid().isNull() && obj.contains(QLatin1Literal("_id"))) {
            obj.setUuid(QtJsonDb::QJsonDbObject::createUuidFromString(obj.value(QLatin1Literal("_id")).toString()));
            obj.remove(QLatin1String("_id"));
        }
        objects << obj;
    }
    QtJsonDb::QJsonDbCreateRequest *request = new QtJsonDb::QJsonDbCreateRequest(objects, this);
    mConnection->send(request);
    return _waitForResponse(request);
}

QVariantMap JsonDbProxy::updateList(QVariantList list)
{
    QList<QJsonObject> objects;
    foreach (const QVariant &obj, list)
        objects << QJsonObject::fromVariantMap(obj.toMap());
    QtJsonDb::QJsonDbUpdateRequest *request = new QtJsonDb::QJsonDbUpdateRequest(objects, this);
    mConnection->send(request);
    return _waitForResponse(request);
}

QVariantMap JsonDbProxy::removeList(QVariantList list)
{
    QList<QJsonObject> objects;
    foreach (const QVariant &obj, list)
        objects << QJsonObject::fromVariantMap(obj.toMap());
    QtJsonDb::QJsonDbRemoveRequest *request = new QtJsonDb::QJsonDbRemoveRequest(objects, this);
    mConnection->send(request);
    return _waitForResponse(request);
}

void JsonDbProxy::log(const QString &msg)
{
    qDebug() << msg;
}

void JsonDbProxy::debug(const QString &msg)
{
    qDebug() << msg;
}

