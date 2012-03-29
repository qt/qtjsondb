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
#include "jsondbpartition.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE
class QIODevice;
class QLocalServer;
class QTcpServer;
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE_JSONDB

using QtJsonDbJsonStream::JsonStream;

class JsonDbEphemeralPartition;

class DBServer : public QObject
{
    Q_OBJECT
public:
    DBServer(const QString &fileName, const QString &baseName, QObject *parent = 0);
    void setTcpServerPort(quint16 port) { mTcpServerPort = port; }
    quint16 tcpServerPort() const { return mTcpServerPort; }

    bool start();
    bool socket();
    bool clear();
    void close();

    inline bool compactOnClose() const { return mCompactOnClose; }
    inline void setCompactOnClose(bool compact) { mCompactOnClose = compact; }

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

    void notified(const QString &id, quint32 stateNumber, const QJsonObject &object, const QString &action);
    void objectsUpdated(const JsonDbUpdateList &objects);
    void viewUpdated(const QString &type);
    void updateEagerViews(JsonDbPartition *partition, const QSet<QString> &viewTypes);

private:
    void objectUpdated(const QString &partitionName, quint32 stateNumber, JsonDbNotification *n, JsonDbNotification::Action action, const JsonDbObject &oldObject, const JsonDbObject &object);

    bool loadPartitions();
    void reduceMemoryUsage();
    JsonDbStat stat() const;

    void processWrite(JsonStream *stream, JsonDbOwner *owner, const JsonDbObjectList &objects, JsonDbPartition::WriteMode mode, const QString &partitionName, int id);
    void processRead(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, const QString &partitionName, int id);
    void processChangesSince(JsonStream *stream, JsonDbOwner *owner, const QJsonValue &object, const QString &partitionName, int id);
    void processFlush(JsonStream *stream, JsonDbOwner *owner, const QString &partitionName, int id);

    void debugQuery(JsonDbQuery *query, int limit, int offset, const JsonDbQueryResult &result);
    JsonDbObjectList prepareWriteData(const QString &action, const QJsonValue &object);
    JsonDbObjectList checkForNotifications(const JsonDbObjectList &objects);
    void createNotification(const JsonDbObject &object, JsonStream *stream);
    void removeNotification(const JsonDbObject &object);
    void notifyHistoricalChanges(JsonDbNotification *n);
    void updateEagerViewTypes(const QString &objectType, JsonDbPartition *partition, quint32 stateNumber);

    JsonDbPartition* findPartition(const QString &partitionName);

    JsonDbOwner *getOwner( JsonStream *stream);
    JsonDbOwner *createDummyOwner( JsonStream *stream);

    QHash<QString, JsonDbPartition *> mPartitions;
    JsonDbPartition *mDefaultPartition;
    JsonDbEphemeralPartition *mEphemeralPartition;
    QMap<QString, JsonDbNotification *> mNotificationMap;
    QMultiMap<QString, JsonDbNotification *> mKeyedNotifications;
    QMap<QString,QSet<QString> > mEagerViewSourceTypes; // set of eager view types dependent on this source type
    quint16                          mTcpServerPort;
    QLocalServer                    *mServer;
    QTcpServer                      *mTcpServer;
    QMap<QIODevice*,JsonStream *>    mConnections;
    JsonDbOwner                     *mOwner;
    QMap<QIODevice*,JsonDbOwner*>    mOwners;
    QMap<QString,JsonStream *>       mNotifications; // maps notification Id to socket
    QString mFilePath; // Directory where database files shall be stored
    QString mBaseName; // Prefix to use in database file names
    bool mCompactOnClose;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // DBSERVER_H
