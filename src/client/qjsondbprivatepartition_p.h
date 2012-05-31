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

#ifndef QJSONDB_PRIVATE_PARTITION_P_H
#define QJSONDB_PRIVATE_PARTITION_P_H

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

#include <QJsonObject>
#include <QObject>

#include "qjsondbglobal.h"
#include "qjsondbrequest.h"
#include <QtJsonDbPartition/jsondbpartitionglobal.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION
class JsonDbOwner;
class JsonDbPartition;
QT_END_NAMESPACE_JSONDB_PARTITION

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbConnectionPrivate;

class Q_JSONDB_EXPORT QJsonDbPrivatePartition : public QObject
{
    Q_OBJECT
public:
    QJsonDbPrivatePartition(QObject *parent = 0);
    ~QJsonDbPrivatePartition();

public Q_SLOTS:
    void handleRequest(const QJsonObject &request);

Q_SIGNALS:
    void readRequestStarted(int requestId, quint32 state, const QString &sortKey);
    void writeRequestStarted(int requestId, quint32 state);
    void flushRequestStarted(int requestId, quint32 state);
    void resultsAvailable(int requestId, const QList<QJsonObject> &results);
    void finished(int requestId);
    void error(int requestId, QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message);

private:
    QtJsonDb::QJsonDbRequest::ErrorCode ensurePartition(const QString &partitionName, QString &message);

    Partition::JsonDbOwner *partitionOwner;
    Partition::JsonDbPartition *privatePartition;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER
#endif // QJSONDB_PRIVATE_PARTITION_P_H
