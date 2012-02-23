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

#ifndef DBSERVER_H
#define DBSERVER_H

#include <QObject>
#include <QVariant>
#include <QAbstractSocket>

#include "jsonstream.h"
#include "jsondbnotification.h"
#include "jsondb.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE
class QIODevice;
class QLocalServer;
class QTcpServer;
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE_JSONDB

class DBServer : public QObject
{
    Q_OBJECT
public:
    DBServer(const QString &fileName, const QString &baseName, QObject *parent = 0);
    void setTcpServerPort(quint16 port) { mTcpServerPort = port; }
    quint16 tcpServerPort() const { return mTcpServerPort; }

    bool start(bool compactOnClose);
    bool socket();
    bool clear();

public slots:
    void sigTerm();
    void sigHUP();
    void sigINT();

protected slots:
    void handleConnection();
    void handleTcpConnection();
    void receiveMessage(const QJsonObject &document);
    void handleConnectionError();
    void removeConnection();

    void notified( const QString &id, JsonDbObject document, const QString &action );
    void updateView( const QString &viewType, const QString &partitionName );

private:
    void processFind(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName);
    void processCreate(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName, JsonDb::WriteMode writeMode);
    void processUpdate(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName, JsonDb::WriteMode writeMode);
    void processRemove(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName, JsonDb::WriteMode writeMode);
    void processToken(JsonStream *stream, const QJsonValue &object, int id);
    void processChangesSince(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, int id, const QString &partitionName);
    void processFlush(JsonStream *stream, JsonDbOwner *owner, const QString &partition, int id);

    JsonDbOwner *getOwner( JsonStream *stream);
    JsonDbOwner *createDummyOwner( JsonStream *stream);

    quint16                          mTcpServerPort;
    QLocalServer                    *mServer;
    QTcpServer                      *mTcpServer;
    QMap<QIODevice*,JsonStream *>    mConnections;
    QMap<QIODevice*,JsonDbOwner*>    mOwners;
    QMap<QString,JsonStream *>       mNotifications; // maps notification Id to socket
    JsonDb                          *mJsonDb;
    QString mFilePath; // Directory where database files shall be stored
    QString mBaseName; // Prefix to use in database file names
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // DBSERVER_H
