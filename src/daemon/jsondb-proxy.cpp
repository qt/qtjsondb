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

#include "jsondb-proxy.h"
#include "jsondb-strings.h"

namespace QtAddOn { namespace JsonDb {

extern bool gDebug;

JsonDbProxy::JsonDbProxy( const JsonDbOwner *owner, JsonDb *jsonDb, QObject *parent )
  : QObject(parent)
  , mOwner(owner)
  , mJsonDb(jsonDb)
{
}
JsonDbProxy::~JsonDbProxy()
{
}

QVariantMap JsonDbProxy::find(QVariantMap object)
{
    QsonMap bson = variantToQson(object).toMap();
    return qsonToVariant(mJsonDb->find(mOwner, bson)).toMap();
}
QVariantMap JsonDbProxy::create(QVariantMap object )
{
    QsonMap bson = variantToQson(object).toMap();
    return qsonToVariant(mJsonDb->create(mOwner, bson)).toMap();
}
QVariantMap JsonDbProxy::update(QVariantMap object )
{
    QsonMap bson = variantToQson(object).toMap();
    return qsonToVariant(mJsonDb->update(mOwner, bson)).toMap();
}
QVariantMap JsonDbProxy::remove(QVariantMap object )
{
    QsonMap bson = variantToQson(object).toMap();
    return qsonToVariant(mJsonDb->remove(mOwner, bson)).toMap();
}
QVariantMap JsonDbProxy::notification(QString query, QStringList actions, QString script)
{
    QsonList actionsList;
    foreach (const QString &action, actions)
        actionsList.append(action);

    QsonMap notificationObject;
    notificationObject.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    notificationObject.insert(JsonDbString::kQueryStr, query);
    notificationObject.insert(JsonDbString::kActionsStr, actionsList);
    notificationObject.insert("script", script);
    return qsonToVariant(mJsonDb->create(mOwner, notificationObject)).toMap();
}

QVariantMap JsonDbProxy::createList(QVariantList list)
{
    QsonList blist;
    for (int i = 0; i < list.size(); i++) {
        QsonObject bson = variantToQson(list[i]);
        blist.append(bson);
    }
    return qsonToVariant(mJsonDb->createList(mOwner, blist)).toMap();
}

QVariantMap JsonDbProxy::updateList(QVariantList list)
{
    QsonList blist;
    for (int i = 0; i < list.size(); i++) {
        QsonObject bson = variantToQson(list[i]);
        blist.append(bson);
    }
    return qsonToVariant(mJsonDb->removeList(mOwner, blist)).toMap();
}

QVariantMap JsonDbProxy::removeList(QVariantList list)
{
    QsonList blist;
    for (int i = 0; i < list.size(); i++) {
        QsonObject bson = variantToQson(list[i]);
        blist.append(bson);
    }
    return qsonToVariant(mJsonDb->removeList(mOwner, blist)).toMap();
}

JsonDbMapProxy::JsonDbMapProxy( const JsonDbOwner *owner, JsonDb *jsonDb, QObject *parent )
  : QObject(parent)
  , mOwner(owner)
  , mJsonDb(jsonDb)
{
}
JsonDbMapProxy::~JsonDbMapProxy()
{
}

void JsonDbMapProxy::emitViewObject(const QString &key, const QJSValue &v)
{
    //qDebug() << "emitViewItem" << key << v.toVariant();
    QJSValue object = v.engine()->newObject();
    object.setProperty("key", key);
    object.setProperty("value", v);
    emit viewObjectEmitted(object);
}

void JsonDbMapProxy::lookup(const QString &key, const QJSValue &value, const QJSValue &context)
{
    QJSValue query = value.engine()->newObject();
    query.setProperty("index", key);
    query.setProperty("value", value);

    emit lookupRequested(query, context);
}

void JsonDbMapProxy::lookupWithType(const QString &key, const QJSValue &value, const QJSValue &objectType, const QJSValue &context)
{
    QJSValue query = value.engine()->newObject();
    query.setProperty("index", key);
    query.setProperty("value", value);
    query.setProperty("objectType", objectType);
    emit lookupRequested(query, context);
}

JsonDbJoinProxy::JsonDbJoinProxy( const JsonDbOwner *owner, JsonDb *jsonDb, QObject *parent )
  : QObject(parent)
  , mOwner(owner)
  , mJsonDb(jsonDb)
{
}
JsonDbJoinProxy::~JsonDbJoinProxy()
{
}

void JsonDbJoinProxy::create(const QJSValue &v)
{
    emit viewObjectEmitted(v);
}

void JsonDbJoinProxy::lookup(const QJSValue &spec, const QJSValue &context)
{
    emit lookupRequested(spec, context);
}

Console::Console()
{
}

void Console::log(const QString &s)
{
    qDebug() << s;
}

void Console::debug(const QString &s)
{
//    if (gDebug)
        qDebug() << s;
}

} } // end namespace QtAddOn::JsonDb
