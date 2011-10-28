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

#ifndef JSONDB_WATCHER_H
#define JSONDB_WATCHER_H

#include <QObject>
#include <QVariant>
#include <QList>

#include <QtJsonDbQson/private/qson_p.h>

#include "private/jsondb-connection_p.h"

Q_USE_JSONDB_NAMESPACE

class JsonDbWatcher : public QObject 
{
    Q_OBJECT
public:
    JsonDbWatcher(JsonDbConnection *connection, QObject *parent=0);
    JsonDbWatcher(Qt::ConnectionType type, QObject *parent=0);
    JsonDbWatcher(QObject *parent=0);

public slots:
    bool connected() const;
    void setDefault(const QVariant& value)  { mDefault = value; }
    bool start(const QString& query);
		     
signals:
    void response( const QVariant& object );
    void disconnected();
					    
private slots:
    void handleResponse( int id, const QsonObject& data );
    void handleError( int id, int code, const QString& message );
    void handleNotification( const QString& notify_uuid, 
                             const QsonObject& object, const QString& action );

private:
    void init(Qt::ConnectionType type=Qt::AutoConnection);
    
private:
    JsonDbConnection  *mConnection;
    int                mNotifyId;
    QString            mNotifyUuid;
    QVariant           mDefault;
};

#endif // JSONDB_WATCHER_H
