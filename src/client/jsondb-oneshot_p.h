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

#ifndef JSONDB_ONESHOT_P_H
#define JSONDB_ONESHOT_P_H

#include <QObject>
#include <QVariant>
#include <QDebug>
#include <QEventLoop>
#include <QThread>

#include <QtJsonDbQson/private/qson_p.h>

#include "jsondb-global.h"

namespace QtAddOn { namespace JsonDb {

/*
 * The one-shot class is strictly for the private use of the connection object
 */

class JsonDbOneShot : public QObject {
    Q_OBJECT
    friend class JsonDbConnection;
public slots:
    void handleResponse( int id, const QVariant& data ) {
	if (id == mId) { emit response(data); deleteLater(); }
    }
    void handleError( int id, int code, const QString& message ) {
	if (id == mId) { emit error(code, message); deleteLater(); }
    }
signals:
    void response(const QVariant& object);
    void error(int code, const QString& message);
private:
    int mId;
};


/*
 * The sync class forces a response before the program will continue
 */
class JsonDbConnection;
class JsonDbSyncCall : public QObject {
    Q_OBJECT
    friend class JsonDbConnection;
public:
    QT_DEPRECATED
    JsonDbSyncCall(const QVariantMap &dbrequest, QVariant &result);
    JsonDbSyncCall(const QVariantMap *dbrequest, QVariant *result);
    JsonDbSyncCall(const QsonMap *dbrequest, QsonObject *result);
    ~JsonDbSyncCall();
public slots:
    void createSyncRequest();
    void createSyncQsonRequest();
    void handleResponse( int id, const QVariant& data );
    void handleResponse( int id, const QsonObject& data );
    void handleError( int id, int code, const QString& message );
private:
    int                 mId;
    const QVariantMap   *mDbRequest;
    const QsonMap       *mDbQsonRequest;
    QVariant            *mResult;
    QsonObject          *mQsonResult;
    JsonDbConnection    *mSyncJsonDbConnection;
};

} } // end namespace QtAddOn::JsonDb

#endif // JSONDB_ONESHOT_P_H
