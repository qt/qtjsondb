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

#ifndef JSONDB_CONNECTION_H
#define JSONDB_CONNECTION_H

#include <QObject>
#include <QVariant>
#include <QSet>
#include <QStringList>
#include <QLocalSocket>
#include <QTcpSocket>
#include <QScopedPointer>

#include "jsondb-global.h"

namespace QtAddOn { namespace JsonDb {

class QsonObject;
class QsonMap;

class JsonDbConnectionPrivate;
class Q_ADDON_JSONDB_EXPORT JsonDbConnection : public QObject
{
    Q_OBJECT
public:
    static JsonDbConnection *instance();
    static void              setDefaultToken( const QString& token );
    static QString           defaultToken();

    static QVariantMap makeFindRequest( const QVariant& );
    static QsonObject makeFindRequest( const QsonObject& );
    static QVariantMap makeQueryRequest( const QString&, int offset = 0, int limit = -1);
    static QVariantMap makeCreateRequest( const QVariant& );
    static QsonObject makeCreateRequest( const QsonObject& );
    static QVariantMap makeUpdateRequest( const QVariant& );
    static QsonObject makeUpdateRequest( const QsonObject& );
    static QVariantMap makeRemoveRequest( const QVariant& );
    static QsonObject makeRemoveRequest( const QsonObject& );
    static QVariantMap makeRemoveRequest( const QString& );
    static QVariantMap makeNotification( const QString&, const QVariantList& );
    static QsonObject makeNotification( const QString&, const QsonObject& );
    static QVariantMap makeChangesSinceRequest(int stateNumber, const QStringList &types=QStringList());

    JsonDbConnection(QObject *parent = 0);
    ~JsonDbConnection();

    // One-shot functions allow you to avoid constructing a JsonDbClient
    QT_DEPRECATED
    void oneShot( const QVariantMap& dbrequest, QObject *receiver=0,
                  const char *responseSlot=0, const char *errorSlot=0);
    // Synchronized calls pause execution until successful
    QsonObject sync(const QsonMap &dbrequest);
    QVariant sync(const QVariantMap &dbrequest);

    void setToken(const QString &token);
    void connectToServer(const QString &socketName = QString());
    void connectToHost(const QString &hostname, quint16 port);

    // General purpose request
    int  request(const QVariantMap &request);
    int  request(const QsonObject &request);
    bool isConnected() const;
    Q_DECL_DEPRECATED inline bool connected() const
    { return isConnected(); }

    bool waitForConnected(int msecs = 30000);
    bool waitForDisconnected(int msecs = 30000);
    bool waitForBytesWritten(int msecs = 30000);

signals:
    void notified(const QString &notify_uuid, const QVariant &object, const QString &action);
    void notified(const QString &notify_uuid, const QsonObject &object, const QString &action);
    void response(int id, const QVariant &data);
    void response(int id, const QsonObject &data);
    void error(int id, int code, const QString &message);
    void connected();
    void disconnected();
    void readyWrite();

private slots:
    void receiveMessage(const QsonObject &msg);

private:
    void init(QIODevice *);

private:
    Q_DISABLE_COPY(JsonDbConnection)
    Q_DECLARE_PRIVATE(JsonDbConnection)
    QScopedPointer<JsonDbConnectionPrivate> d_ptr;
};

} } // end namespace QtAddOn::JsonDb

#endif /* JSONDB_H */
