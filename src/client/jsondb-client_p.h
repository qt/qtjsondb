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

#ifndef JSONDBCLIENT_P_H
#define JSONDBCLIENT_P_H

#include "jsondb-global.h"

#include <QtJsonDbQson/private/qson_p.h>

#include <QMetaMethod>
#include <QWeakPointer>
#include <QTimer>

#include "jsondb-client.h"

namespace QtAddOn { namespace JsonDb {

/*!
    \class JsonDbClientPrivate
    \internal
*/
class JsonDbClientPrivate
{
    Q_DECLARE_PUBLIC(JsonDbClient)
public:
    JsonDbClientPrivate(JsonDbClient *q, JsonDbConnection *c)
        : q_ptr(q), connection(c)
    {
         Q_ASSERT(connection);
    }

    ~JsonDbClientPrivate()
    { }

    void init(Qt::ConnectionType type=Qt::AutoConnection);
    bool send(int requestId, const QVariantMap &request);

    void _q_statusChanged();
    void _q_handleNotified(const QString &, const QsonObject &, const QString &);
    void _q_handleResponse(int id, const QsonObject &data);
    void _q_handleError(int id, int code, const QString &message);
    void _q_timeout();
    void _q_processQueue();

    JsonDbClient *q_ptr;
    JsonDbConnection *connection;

    QTimer reconnectionTimer;
    JsonDbClient::Status status;

    QMap<int, QVariantMap> requestQueue;
    QMap<int, QVariantMap> sentRequestQueue;

    struct Callback
    {
        QWeakPointer<QObject> object;
        const char *successSlot;
        const char *errorSlot;

        inline Callback(QObject *obj = 0, const char *success = 0, const char *error = 0)
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
        QWeakPointer<QObject> object;
        QMetaMethod method;
        NotifyCallback(QObject *obj = 0, QMetaMethod m = QMetaMethod())
            : object(obj), method(m) { }
    };

    QHash<int, NotifyCallback> unprocessedNotifyCallbacks;
    QHash<QString, NotifyCallback> notifyCallbacks;
};

} } // end namespace QtAddOn::JsonDb

#endif // JSONDBCLIENT_P_H
