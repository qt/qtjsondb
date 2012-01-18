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

#ifndef JSONDB_REQUEST_H
#define JSONDB_REQUEST_H

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <QtJsonDb/qjsondbglobal.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbRequestPrivate;
class Q_JSONDB_EXPORT QJsonDbRequest : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString partition READ partition WRITE setPartition)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

public:
    ~QJsonDbRequest();

    enum ErrorCode {
        NoError = 0,
// ### TODO: clean me up and move me to ReadRequest and WriteRequest
//        InvalidMessage           = 1,
//        InvalidRequest           = 2,
        MissingObject            = 3,
//        DatabaseError            = 4,
//        MissingUUID              = 5,
        MissingType              = 6,
        MissingQuery             = 7,
        InvalidLimit             = 8,
//        InvalidOffset            = 9,
//        MismatchedNotifyId       = 10,
//        InvalidActions           = 11,
//        UpdatingStaleVersion     = 12,
//        OperationNotPermitted    = 13,
//        QuotaExceeded            = 14,
//        FailedSchemaValidation   = 15,
//        InvalidMap               = 16,
//        InvalidReduce            = 17,
//        InvalidSchemaOperation   = 18,
        InvalidPartition         = 19,
//        InvalidIndexOperation    = 20
        DatabaseConnectionError = 21
    };

    enum Status {
        Inactive = 0,
        Canceled = 1,
        Finished = 2,
        Error = 3,
        Queued = 4,
        Sent = 5,
        Receiving = 6
    };
    Status status() const;

    bool isActive() const;

    QString partition() const;
    void setPartition(const QString &);

    QList<QJsonObject> takeResults(int amount = -1);

Q_SIGNALS:
    void started();
    void resultsAvailable(int amount);
    void finished();
    void error(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message);

    // signals for properties
    void statusChanged(QtJsonDb::QJsonDbRequest::Status newStatus);

protected:
    QJsonDbRequest(QJsonDbRequestPrivate *d, QObject *parent = 0);
    QScopedPointer<QJsonDbRequestPrivate> d_ptr;

private:
    Q_DISABLE_COPY(QJsonDbRequest)
    Q_DECLARE_PRIVATE(QJsonDbRequest)
    friend class QJsonDbConnection;
    friend class QJsonDbConnectionPrivate;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_REQUEST_H
