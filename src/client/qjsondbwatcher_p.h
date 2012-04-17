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

#ifndef QJSONDB_WATCHER_P_H
#define QJSONDB_WATCHER_P_H

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
#include <QMap>
#include <QWeakPointer>
#include <qjsonvalue.h>

#include "qjsondbwatcher.h"
#include "qjsondbrequest.h"

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbConnection;

class QJsonDbWatcherPrivate
{
    Q_DECLARE_PUBLIC(QJsonDbWatcher)
public:
    QJsonDbWatcherPrivate(QJsonDbWatcher *q);

    void _q_onFinished();
    void _q_onError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message);

    void handleNotification(quint32 stateNumber, QJsonDbWatcher::Action action, const QJsonObject &object);
    void handleStateChange(quint32 stateNumber);
    void setStatus(QJsonDbWatcher::Status newStatus);

    QJsonDbWatcher *q_ptr;
    QWeakPointer<QJsonDbConnection> connection;
    QJsonDbWatcher::Status status;
    QJsonDbWatcher::Actions actions;
    QString query;
    QMap<QString, QJsonValue> bindings;
    QString partition;
    enum {
        UnspecifiedInitialStateNumber = -1
    };
    quint32 initialStateNumber;
    quint32 lastStateNumber;

    QList<QJsonDbNotification> notifications;

    QString uuid; // ### TODO: make me QUuid after QJsonObject can store binary blob efficiently
    QString version;
};

QT_END_NAMESPACE_JSONDB

#endif // QJSONDB_WATCHER_P_H
