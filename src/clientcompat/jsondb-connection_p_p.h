/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDB_CONNECTION_P_P_H
#define JSONDB_CONNECTION_P_P_H

#include <QObject>
#include <QVariant>
#include <QSet>
#include <QStringList>
#include <QLocalSocket>
#include <QTcpSocket>
#include <QScopedPointer>

#include <qjsonobject.h>

#include "jsondb-global.h"
#include "jsondb-connection_p.h"

#include "jsonstream.h"

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbConnectionPrivate
{
    Q_DECLARE_PUBLIC(JsonDbConnection)
public:
    JsonDbConnectionPrivate(JsonDbConnection *q)
        : q_ptr(q), socket(0), tcpSocket(0), mStream(q), mId(1), status(JsonDbConnection::Null)
    { }
    ~JsonDbConnectionPrivate()
    { }

    void _q_onConnected();
    void _q_onDisconnected();
    void _q_onReceiveMessage(const QJsonObject &);
    void _q_onError(QLocalSocket::LocalSocketError error);

    JsonDbConnection *q_ptr;
    QLocalSocket *socket;
    QTcpSocket *tcpSocket;
    QtJsonDbJsonStream::JsonStream mStream;
    int mId;
    JsonDbConnection::Status status;
    QString errorString;
    QList<int> protocolAdaptionRequests;
};

/*!
 * \internal
 * The sync class forces a response before the program will continue
 */
class JsonDbSyncCall : public QObject
{
    Q_OBJECT
    friend class JsonDbConnection;
public:
    QT_DEPRECATED
    JsonDbSyncCall(const QVariantMap &dbrequest, QVariant &result);
    JsonDbSyncCall(const QVariantMap *dbrequest, QVariant *result);
    ~JsonDbSyncCall();
public slots:
    void createSyncRequest();
    void handleResponse( int id, const QVariant& data );
    void handleError( int id, int code, const QString& message );
private:
    int                 mId;
    const QVariantMap   *mDbRequest;
    QVariant            *mResult;
    JsonDbConnection    *mSyncJsonDbConnection;
};

QT_END_NAMESPACE_JSONDB

#endif /* JSONDB_CONNECTION_P_P_H */
