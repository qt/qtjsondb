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

#ifndef JSONDB_WATCHER_H
#define JSONDB_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QJsonObject>
#include <QtCore/QUuid>

#include <QtJsonDb/qjsondbglobal.h>
#include <QtJsonDb/qjsondbrequest.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbNotification;
class QJsonDbWatcherPrivate;
class Q_JSONDB_EXPORT QJsonDbWatcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Actions watchedActions READ watchedActions WRITE setWatchedActions)
    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(QString partition READ partition WRITE setPartition)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(quint32 initialStateNumber READ initialStateNumber WRITE setInitialStateNumber)
    Q_PROPERTY(quint32 lastStateNumber READ lastStateNumber NOTIFY lastStateNumberChanged)

public:
    enum Action {
        Created = 0x01, // ### TODO: rename me to StartsMatching ?
        Updated = 0x02,
        Removed = 0x04,
        StateChanged = 0x08,

        All = 0xFF
    };
    Q_DECLARE_FLAGS(Actions, Action)

    enum Status {
        Inactive,
        Activating,
        Active
    };
    Status status() const;
    inline bool isInactive() const { return status() == QJsonDbWatcher::Inactive; }
    inline bool isActivating() const { return status() == QJsonDbWatcher::Activating; }
    inline bool isActive() const { return status() == QJsonDbWatcher::Active; }

    enum ErrorCode {
        NoError = QJsonDbRequest::NoError,
        InvalidRequest = QJsonDbRequest::InvalidRequest,
        OperationNotPermitted = QJsonDbRequest::OperationNotPermitted,
        InvalidPartition = QJsonDbRequest::InvalidPartition,
        DatabaseConnectionError = QJsonDbRequest::DatabaseConnectionError,
        MissingQuery = QJsonDbRequest::MissingQuery,
        InvalidStateNumber = QJsonDbRequest::InvalidStateNumber
    };

    QJsonDbWatcher(QObject *parent = 0);
    ~QJsonDbWatcher();

    void setWatchedActions(Actions actions);
    Actions watchedActions() const;

    void setQuery(const QString &query);
    QString query() const;

    void setPartition(const QString &partition);
    QString partition() const;

    void setInitialStateNumber(quint32 stateNumber);
    quint32 initialStateNumber() const;

    quint32 lastStateNumber() const;

    QList<QJsonDbNotification> takeNotifications(int amount = -1);

Q_SIGNALS:
    void notificationsAvailable(int count);
    void statusChanged(QtJsonDb::QJsonDbWatcher::Status newStatus);
    void error(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message);

    // signals for properties
    void lastStateNumberChanged(int stateNumber);

private:
    Q_DECLARE_PRIVATE(QJsonDbWatcher)
    QScopedPointer<QJsonDbWatcherPrivate> d_ptr;

    Q_PRIVATE_SLOT(d_func(), void _q_onFinished())
    Q_PRIVATE_SLOT(d_func(), void _q_onError(QtJsonDb::QJsonDbRequest::ErrorCode,QString))

    friend class QJsonDbConnection;
    friend class QJsonDbConnectionPrivate;
};

class Q_JSONDB_EXPORT QJsonDbNotification
{
public:
    QJsonDbNotification(const QJsonObject &object, QJsonDbWatcher::Action action, quint32 stateNumber);

    QJsonObject object() const;
    QJsonDbWatcher::Action action() const;
    quint32 stateNumber() const;

private:
    QJsonObject obj;
    QJsonDbWatcher::Action act;
    quint32 state;

    void *reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QJsonDbWatcher::Actions)

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_WATCHER_H
