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

#include "jsondb-watcher.h"
#include "private/jsondb-strings_p.h"

/*!
  \class JsonDbWatcher
  \brief The JsonDbWatcher class monitors the state of a single JsonDb object.

  The JsonDbWatcher class is used to monitor a single JsonDb object and 
  provide notifications when it changes its values.

  The response() signal is when the watcher first connects to the JsonDb object
  and subsequently each time the object changes.  If there is no object or
  if the object is deleted, the response() signal is sent with a null QVariant
  object.

  This class is designed to work with singleton database objects.  If there
  is more than one object in the database that matches the query, only the
  first object found will be used.

 */

JsonDbWatcher::JsonDbWatcher(JsonDbConnection *connection, QObject *parent)
    : QObject(parent),
      mConnection(connection)
{
    Q_ASSERT(mConnection);
    init();
}

JsonDbWatcher::JsonDbWatcher(Qt::ConnectionType type, QObject *parent)
    : QObject(parent)
{
    mConnection = JsonDbConnection::instance();
    init(type);
}

JsonDbWatcher::JsonDbWatcher(QObject *parent)
    : QObject(parent)
{
    mConnection = JsonDbConnection::instance();
    init();
}

/*!
  Checks to see if the connection to the database is alive.
 */

bool JsonDbWatcher::connected() const 
{
    Q_ASSERT(mConnection);
    return mConnection->isConnected();
}

void JsonDbWatcher::init(Qt::ConnectionType type)
{
    connect(mConnection, SIGNAL(notified(QString,QVariant,QString)),
	    this, SLOT(handleNotification(QString,QVariant,QString)),type);
    connect(mConnection, SIGNAL(response(int,QsonObject)),
            this, SLOT(handleResponse(int,QsonObject)),type);
    connect(mConnection, SIGNAL(error(int,int,QString)),
	    this, SLOT(handleError(int,int,QString)),type);
    connect(mConnection, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
}

void JsonDbWatcher::handleResponse( int id, const QsonObject& data )
{
    if ( mNotifyId == id ) 
        mNotifyUuid = data.toMap().value(JsonDbString::kUuidStr, QString());
}

void JsonDbWatcher::handleError( int id, int code, const QString& message )
{
    if ( mNotifyId == id ) 
	qWarning() << Q_FUNC_INFO << id << code << message;
}

void JsonDbWatcher::handleNotification( const QString& notify_uuid,
                                        const QsonObject& object,
					const QString& action )
{
    if (notify_uuid == mNotifyUuid)
       emit response( action != JsonDbString::kRemoveStr ? qsonToVariant(object) : QVariant() );
}

/*!
  Start monitoring a JsonDb object that responds to \a queryString.
  The initial state of the object is retrieved synchronously with 
  this call, which means that the response() signal will be fired
  immediately.  

  Returns false if there is no JsonDb connection.  
 */

bool JsonDbWatcher::start( const QString& queryString)
{
    if (!mConnection || !mConnection->isConnected())
	return false;

    QVariant result;
    QsonObject data = mConnection->sync(variantToQson(JsonDbConnection::makeQueryRequest(queryString)));
    QsonList list = data.toMap().subList( JsonDbString::kDataStr );
    if (list.size())
        result = qsonToVariant(list.at<QsonElement>(0));

    if (result.isNull() && !mDefault.isNull()) { // Attempt to construct a default object
        QsonMap createResultMap = mConnection->sync(JsonDbConnection::makeCreateRequest(variantToQson(mDefault))).toMap();
        qDebug() << "JsonDbWatcher::start" << "creating default" << createResultMap;
        QVariantMap defaultMap = mDefault.toMap();
        defaultMap.insert(JsonDbString::kUuidStr, createResultMap.value(JsonDbString::kUuidStr, QString()));
        defaultMap.insert(JsonDbString::kVersionStr, createResultMap.value(JsonDbString::kVersionStr, QString()));
        mDefault = defaultMap;
        result = mDefault;
    }

    emit response(result);

    QsonList actions;
    actions.append(JsonDbString::kCreateStr);
    actions.append(JsonDbString::kUpdateStr);
    actions.append(JsonDbString::kRemoveStr);
    QsonMap notify_object = JsonDbConnection::makeNotification(queryString, actions);
    mNotifyId = mConnection->request(JsonDbConnection::makeCreateRequest(notify_object));
    return true;
}




