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

#ifndef JSONDBCLIENT_P_H
#define JSONDBCLIENT_P_H

#include "jsondb-global.h"

#include <QMetaMethod>
#include <QPointer>
#include <QTimer>

#include "jsondb-client.h"

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \class JsonDbClientPrivate
    \internal
*/
class JsonDbClientPrivate
{
    Q_DECLARE_PUBLIC(JsonDbClient)
public:
    JsonDbClientPrivate(JsonDbClient *q)
        : q_ptr(q), connection(0), status(JsonDbClient::Null), autoReconnect(true),
          timeoutTimerId(-1)
    { }

    ~JsonDbClientPrivate()
    { }

    void _q_statusChanged();
    void _q_handleNotified(const QString &, const QVariant &, const QString &);
    void _q_handleResponse(int id, const QVariant &data);
    void _q_handleError(int id, int code, const QString &message);
    void _q_timeout();
    void _q_processQueue();

    JsonDbClient *q_ptr;
    JsonDbConnection *connection;

    JsonDbClient::Status status;
    bool autoReconnect;

    QMap<int, QVariantMap> requestQueue;
    QMap<int, QVariantMap> sentRequestQueue;

    struct Callback
    {
        QPointer<QObject> object;
        const char *successSlot;
        const char *errorSlot;

        inline Callback(QObject *obj, const char *success, const char *error)
            : object(obj), successSlot(success), errorSlot(error)
        {
#ifdef _DEBUG
            if (obj) {
                if (successSlot && obj->metaObject()->indexOfMethod(QMetaObject::normalizedSignature(successSlot).constData()+1) < 0)
                    qWarning() << "JsonDbClient: passing non existent success slot" << QLatin1String(successSlot+1);
                if (errorSlot && obj->metaObject()->indexOfMethod(QMetaObject::normalizedSignature(errorSlot).constData()+1) < 0)
                    qWarning() << "JsonDbClient: passing non existent error slot" << QLatin1String(errorSlot+1);
            }
#endif
        }
    };
    QHash<int, Callback> ids;

    struct NotifyCallback
    {
        QPointer<QObject> object;
        QMetaMethod method;
        NotifyCallback(QObject *obj = 0, QMetaMethod m = QMetaMethod())
            : object(obj), method(m) { }
    };

    QHash<int, NotifyCallback> unprocessedNotifyCallbacks;
    QHash<QString, NotifyCallback> notifyCallbacks;

    int timeoutTimerId;

    void init(JsonDbConnection *connection);
    bool send(int requestId, const QVariantMap &request);
};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBCLIENT_P_H
