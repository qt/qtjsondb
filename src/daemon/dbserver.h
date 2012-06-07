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

#include "clientjsonstream.h"
#include "jsondbnotification.h"
#include "jsondbpartition.h"
#include "jsondbpartitionspec.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE
class QIODevice;
class QLocalServer;
class QTcpServer;
QT_END_NAMESPACE

class JsonDbEphemeralPartition;

QT_USE_NAMESPACE_JSONDB_PARTITION

class DBServer : public QObject
{
    Q_OBJECT
public:
    DBServer(const QString &searchPath, QObject *parent = 0);
    ~DBServer();

    void setTcpServerPort(quint16 port) { mTcpServerPort = port; }
    quint16 tcpServerPort() const { return mTcpServerPort; }

    bool start();
    bool socket();
    bool clear();
    void close();

    inline bool compactOnClose() const { return mCompactOnClose; }
    inline void setCompactOnClose(bool compact) { mCompactOnClose = compact; }

public slots:
    void sigTERM();
    void sigHUP();
    void sigINT();
    void sigUSR1();

protected slots:
    void handleConnection();
    void handleTcpConnection();
    void receiveMessage(const QJsonObject &document);
    void handleConnectionError();
    void removeConnection();

private:
    bool loadPartitions();
    void reduceMemoryUsage();
    void closeIndexes();
    JsonDbStat stat() const;

    void processWrite(ClientJsonStream *stream, JsonDbOwner *owner, const JsonDbObjectList &objects, JsonDbPartition::ConflictResolutionMode mode, const QString &partitionName, int id);
    void processRead(ClientJsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, const QString &partitionName, int id);
    void processChangesSince(ClientJsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, const QString &partitionName, int id);
    void processFlush(ClientJsonStream *stream, JsonDbOwner *owner, const QString &partitionName, int id);
    void processLog(ClientJsonStream *stream, const QString &message, int id);

    void debugQuery(const QString partitionName, const JsonDbQuery &query, int limit, int offset, const JsonDbQueryResult &result);
    JsonDbObjectList prepareWriteData(const QString &action, const QJsonValue &object);
    JsonDbObjectList checkForNotifications(const JsonDbObjectList &objects);
    void createNotification(const JsonDbObject &object, ClientJsonStream *stream);
    void removeNotification(const JsonDbObject &object, ClientJsonStream *stream);
    JsonDbError::ErrorCode validateNotification(const JsonDbObject &notificationDef, QString &message);
    void removeNotificationsByPartition(JsonDbPartition *partition);
    void enableNotificationsByPartition(JsonDbPartition *partition);

    JsonDbPartition* findPartition(const QString &partitionName);
    QList<JsonDbPartitionSpec> findPartitionDefinitions() const;
    void updatePartitionDefinition(JsonDbPartition *partition, bool remove = false, bool isDefault = false);

    JsonDbOwner *getOwner(ClientJsonStream *stream);
    JsonDbOwner *createDummyOwner(ClientJsonStream *stream);
    void sendError(ClientJsonStream *stream, JsonDbError::ErrorCode code,
                   const QString& message, int id);

    QHash<QString, JsonDbPartition *> mPartitions;
    JsonDbPartition *mDefaultPartition;
    JsonDbEphemeralPartition *mEphemeralPartition;

    quint16                          mTcpServerPort;
    QLocalServer                    *mServer;
    QTcpServer                      *mTcpServer;
    QMap<QIODevice*, ClientJsonStream *> mConnections;
    JsonDbOwner                     *mOwner;

    // per connection owner info
    class OwnerInfo {
    public:
        OwnerInfo() : owner(0), pid(0) {}
        JsonDbOwner *owner;
        int          pid;
        QString      processName;
    };
    QMap<QIODevice*,OwnerInfo>       mOwners;
    bool mCompactOnClose;
};

QT_END_HEADER

#endif // DBSERVER_H
