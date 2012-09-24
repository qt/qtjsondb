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

#ifndef QJSONDB_CONNECTION_P_H
#define QJSONDB_CONNECTION_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtJsonDb API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QObject>
#include <QPointer>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>

#include "qjsondbglobal.h"
#include "qjsondbconnection.h"
#include "qjsondbrequest.h"

QT_BEGIN_NAMESPACE
namespace QtJsonDbJsonStream {
class JsonStream;
}
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbPrivatePartition;
class QJsonDbConnectionPrivate
{
    Q_DECLARE_PUBLIC(QJsonDbConnection)
    friend class QJsonDbPrivatePartition;

public:
    QJsonDbConnectionPrivate(QJsonDbConnection *q);

    QString serverSocketName() const;
    inline bool shouldAutoReconnect() const
    { return autoReconnectEnabled && !explicitDisconnect; }

    void _q_onConnected();
    void _q_onDisconnected();
    void _q_onError(QLocalSocket::LocalSocketError);
    void _q_onTimer();
    void _q_onReceivedObject(const QJsonObject &);
    void _q_onAuthFinished();

    void _q_privateReadRequestStarted(int requesId, quint32, const QString &);
    void _q_privateWriteRequestStarted(int requestId, quint32);
    void _q_privateFlushRequestStarted(int requestId, quint32);
    void _q_privateRequestFinished(int requestId);
    void _q_privateRequestError(int requestId, QtJsonDb::QJsonDbRequest::ErrorCode, const QString &);
    void _q_privateRequestResultsAvailable(int requestId, const QList<QJsonObject> &);

    void handleRequestQueue();
    void handlePrivatePartitionRequest(const QJsonObject &);
    bool initWatcher(QJsonDbWatcher *);
    void removeWatcher(QJsonDbWatcher *);
    void reactivateAllWatchers();

    QJsonDbConnection *q_ptr;
    QString socketName;
    QJsonDbConnection::Status status;
    bool autoConnect;
    bool autoReconnectEnabled;
    bool explicitDisconnect;
    QTimer timeoutTimer;

    QLocalSocket *socket;
    QtJsonDbJsonStream::JsonStream *stream;

    QPointer<QJsonDbRequest> currentRequest;
    QList<QPointer<QJsonDbRequest> > pendingRequests;

    QMap<QString, QPointer<QJsonDbWatcher> > watchers; // uuid->watcher map
    QJsonDbPrivatePartition *privatePartitionHandler; // weak pointer to global static object
};

class QPrivatePartitionThread : public QThread
{
    Q_OBJECT
public Q_SLOTS:
    void quitAndWait() { quit(); wait(); }
};

QT_END_NAMESPACE_JSONDB

#endif // QJSONDB_CONNECTION_P_H
