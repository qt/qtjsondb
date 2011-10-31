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

namespace QtAddOn { namespace JsonDb {

/*!
    \class JsonDbClientPrivate
    \internal
*/
class JsonDbClientPrivate
{
    Q_DECLARE_PUBLIC(JsonDbClient)
public:
    JsonDbClientPrivate(JsonDbClient *q, JsonDbConnection *c);
    ~JsonDbClientPrivate();
    void init(Qt::ConnectionType type=Qt::AutoConnection);

    void _q_handleNotified(const QString &, const QsonObject &, const QString &);
    void _q_handleResponse(int id, const QsonObject &data);
    void _q_handleError(int id, int code, const QString &message);

    JsonDbClient *q_ptr;
    JsonDbConnection *connection;

    struct Callback
    {
        QWeakPointer<QObject> object;
        const char *successSlot;
        const char *errorSlot;

        inline Callback(QObject *obj = 0, const char *success = 0, const char *error = 0)
            : object(obj), successSlot(success), errorSlot(error) { }
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
