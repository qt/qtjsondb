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

#ifndef QJSONDB_READ_REQUEST_P_H
#define QJSONDB_READ_REQUEST_P_H

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
#include <QUuid>

#include "qjsondbrequest_p.h"
#include "qjsondbreadrequest.h"

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbReadRequestPrivate : public QJsonDbRequestPrivate
{
    Q_DECLARE_PUBLIC(QJsonDbReadRequest)
public:
    QJsonDbReadRequestPrivate(QJsonDbReadRequest *q);

    QJsonObject getRequest() const;
    void handleResponse(const QJsonObject &);
    void handleError(int, const QString &);

    void _q_privatePartitionStarted(quint32 state, const QString &key);

    QString query;
    int queryLimit;
    QMap<QString, QJsonValue> bindings;

    // query results
    quint32 stateNumber;
    QString sortKey;
};

class QJsonDbReadObjectRequestPrivate : public QJsonDbReadRequestPrivate
{
    Q_DECLARE_PUBLIC(QJsonDbReadObjectRequest)
public:
    QJsonDbReadObjectRequestPrivate(QJsonDbReadObjectRequest *q);

    void _q_onFinished();

    QUuid uuid;
};

QT_END_NAMESPACE_JSONDB

#endif // QJSONDB_READ_REQUEST_P_H
