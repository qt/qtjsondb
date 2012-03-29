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

#ifndef JSONDB_CONNECTION_P_H
#define JSONDB_CONNECTION_P_H

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
#include <QWeakPointer>
#include <QLocalSocket>
#include <QTimer>

#include "qjsondbglobal.h"
#include "qjsondbconnection.h"
#include "qjsondbrequest.h"

#include "jsonstream.h"

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbConnectionPrivate
{
    Q_DECLARE_PUBLIC(QJsonDbConnection)
public:
    QJsonDbConnectionPrivate(QJsonDbConnection *q);

    inline QString serverSocketName() const
    {
        if (socketName.isEmpty()) {
            QByteArray ba = qgetenv("JSONDB_SOCKET");
            return ba.isEmpty() ? QStringLiteral("qt5jsondb") : QString::fromLatin1(ba.constData(), ba.size());
        }
        return socketName;
    }
    inline bool shouldAutoReconnect() const
    { return autoReconnectEnabled && !explicitDisconnect; }

    void _q_onConnected();
    void _q_onDisconnected();
    void _q_onError(QLocalSocket::LocalSocketError);
    void _q_onTimer();
    void _q_onReceivedObject(const QJsonObject &);
    void _q_onAuthFinished();

    void handleRequestQueue();
    void initWatcher(QJsonDbWatcher *);
    void removeWatcher(QJsonDbWatcher *);
    void reactivateAllWatchers();

    QJsonDbConnection *q_ptr;
    QString socketName;
    QJsonDbConnection::Status status;
    bool autoReconnectEnabled;
    bool explicitDisconnect;
    QTimer timeoutTimer;

    QLocalSocket *socket;
    QtJsonDbJsonStream::JsonStream *stream;

    int lastRequestId;
    QWeakPointer<QJsonDbRequest> currentRequest;
    QList<QWeakPointer<QJsonDbRequest> > pendingRequests;

    QMap<QString, QWeakPointer<QJsonDbWatcher> > watchers; // uuid->watcher map
};

QT_END_NAMESPACE_JSONDB

#endif // JSONDB_CONNECTION_P_H
