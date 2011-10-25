/****************************************************************************
**
** Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef DBSERVER_H
#define DBSERVER_H

#include <QObject>
#include <QVariant>
#include <QAbstractSocket>

#include "qsonstream.h"
#include "notification.h"
#include "jsondb.h"

class QIODevice;
class QLocalServer;
class QTcpServer;

namespace QtAddOn { namespace JsonDb {

class DBServer : public QObject
{
    Q_OBJECT
public:
    DBServer(const QString &fileName, QObject *parent = 0);
    void setTcpServerPort(quint16 port) { mTcpServerPort = port; }
    quint16 tcpServerPort() const { return mTcpServerPort; }

    bool start();
    bool socket();
    bool clear();
    void load(const QString &jsonFileName);

public slots:
    void sigTerm();
    void sigHUP();
    void sigINT();

protected slots:
    void handleConnection();
    void handleTcpConnection();
    void receiveMessage( QsonObject );
    void handleConnectionError();
    void removeConnection();

    void notified( const QString &id, QsonMap object, const QString &action );
    void updateView( const QString &viewType, const QString &partitionName );

private:
    void processFind( QsonStream *stream, QsonObject object, int id );
    void processCreate( QsonStream *stream, QsonObject object, int id );
    void processUpdate( QsonStream *stream, QsonObject object, int id, bool replication = false );
    void processRemove( QsonStream *stream, QsonObject object, int id );
    void processToken( QsonStream *stream, QVariant object, int id );
    void processChangesSince( QsonStream *stream, QsonObject object, int id );

    JsonDbOwner *getOwner( QsonStream *stream );
    bool validateToken( QsonStream *stream, const QsonMap &securityObject );
    void createDummyOwner( QsonStream *stream, int id );

    quint16                          mTcpServerPort;
    QLocalServer                    *mServer;
    QTcpServer                      *mTcpServer;
    QMap<QIODevice*,QsonStream *>    mConnections;
    QMap<QIODevice*,JsonDbOwner*>    mOwners;
    QMap<QString,QsonStream *>       mNotifications; // maps notification Id to socket
    JsonDb                          *mJsonDb;
    QString                          mMasterToken;
    QString mFileName;
};

} } // end namespace QtAddOn::JsonDb

#endif // DBSERVER_H
