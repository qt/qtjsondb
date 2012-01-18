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

#include "qjsondbwatcher_p.h"
#include "qjsondbstrings_p.h"
#include "qjsondbconnection_p.h"

#include <QUuid>
#include <QDebug>

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \class QJsonDbNotification
    \inmodule QtJsonDb

    \brief The QJsonDbNotification class describes a database notification event.

    This is a single notification event that is emitted by QJsonDbWatcher class
    when an object matching a query string was changed in the database.

    \sa QJsonDbWatcher::notificationsAvailable(), QJsonDbWatcher::takeNotifications()
*/

/*!
    \internal
*/
QJsonDbNotification::QJsonDbNotification(const QJsonObject &object, QJsonDbWatcher::Action action, quint32 stateNumber)
    : obj(object), act(action), state(stateNumber)
{
}

/*!
    Returns the object that matched notification.

    \list

    \o If the action() is QJsonDbWatcher::Created, the object contains the full
    object that started matching the watcher query string.

    \o If the action() is QJsonDbWatcher::Updated, the object contains the
    latest version of the object.

    \o If the action() is QJsonDbWatcher::Removed, the object contains the
    \c{_uuid} and \c{_version} of the object that no longer matches the watcher
    query string.

    \endlist

    \sa QJsonDbObject
*/
QJsonObject QJsonDbNotification::object() const
{
    return obj;
}

/*!
    Returns the notification action.
*/
QJsonDbWatcher::Action QJsonDbNotification::action() const
{
    return act;
}

/*!
    Returns the state number that corresponds to the object in notification.
*/
quint32 QJsonDbNotification::stateNumber() const
{
    return state;
}

QJsonDbWatcherPrivate::QJsonDbWatcherPrivate(QJsonDbWatcher *q)
    : q_ptr(q), status(QJsonDbWatcher::Inactive),
      actions(QJsonDbWatcher::All), initialStateNumber(0), lastStateNumber(0)
{
    uuid = QUuid::createUuid().toString();
}

/*!
    \class QJsonDbWatcher
    \inmodule QtJsonDb

    \brief The QJsonDbWatcher class allows to watch live database changes.

    This class is used to configure what live database changes should result in
    notification events. The notificationsAvailable() signal is emitted
    whenever an object matching the given \l{QJsonDbWatcher::query}{query} was
    created or updated or removed from the database (depending on the
    \l{QJsonDbWatcher::watchedActions}{action}). Notification events can be
    retrieved using the takeNotifications() functions.

    Each change in the database corresponds to the identifier of the change - a
    database state number - and QJsonDbWatcher class can be used to start
    watching for notifications events starting from the given
    initialStateNumber. The last state number that was known to the watcher can
    be retrieved using \l{QJsonDbWatcher::lastStateNumber}{lastStateNumber}
    property.

    QJsonDbWatcher should be registered within the database connection with
    QJsonDbConnection::addWatcher() function and only starts receiving database
    change notifications after it was successfully registered and
    \l{QJsonDbWatcher::Status}{status} was switched to QJsonDbWatcher::Active
    state.

    Whenever the database connection is established it re-activates all
    registered watchers, hence the watcher will be re-activated automatically
    if the connection to the database was dropped and established again.

    \code
        QtJsonDb::QJsonDbWatcher *watcher = new QtJsonDb::QJsonDbWatcher;
        watcher->setQuery(QStringLiteral("[?_type=\"Foo\"]"));
        QObject::connect(watcher, SIGNAL(notificationsAvailable(int)),
                         this, SLOT(onNotificationsAvailable(int)));
        QObject::connect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
                         this, SLOT(onNotificationError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
        QtJsonDb::QJsonDbConnection *connection = new QtJsonDb::QJsonDbConnection;
        connection->addWatcher(watcher);
    \endcode
*/
/*!
    \enum QJsonDbWatcher::Action

    This enum describes possible actions that can be watched. By default
    watcher is set to watch All actions.

    \value Created Watches for objects to start matching the given query string.
    \value Updated Watches for modifications of objects matching the given query.
    \value Removed Watches for objects that stop matching the given query string.
    \value All A convenience value that specifies to watch for all possible actions.
*/
/*!
    \enum QJsonDbWatcher::Status

    This enum describes watcher status.

    \value Inactive The watcher does not current watch database changes.
    \value Activating The watcher is being activated.
    \value Active The watcher is successfully setup to watch for live database changes.
*/
/*!
    \enum QJsonDbWatcher::ErrorCode

    This enum describes possible errors that can happen when activating the watcher.

    \value NoError
    \value InvalidActions the set of actions that was given is incorrect.
    \value InvalidQuery the given query is incorrect.
    \value InvalidPartition the given partition is incorrect.
    \value InvalidStateNumber the given initialStateNumber is incorrect.
*/

/*!
    Constructs a new QJsonDbWatcher object with a given \a parent.

    \sa QJsonDbConnection::addWatcher()
*/
QJsonDbWatcher::QJsonDbWatcher(QObject *parent)
    : QObject(parent), d_ptr(new QJsonDbWatcherPrivate(this))
{

}
/*!
    Destroys the QJsonDbWatcher object.
*/
QJsonDbWatcher::~QJsonDbWatcher()
{
    Q_D(QJsonDbWatcher);
    if (d->status != QJsonDbWatcher::Inactive && d->connection)
        d->connection.data()->d_func()->removeWatcher(this);
}

/*!
    \property QJsonDbWatcher::status
    Specifies the current watcher status.
    \sa statusChanged()
*/
QJsonDbWatcher::Status QJsonDbWatcher::status() const
{
    Q_D(const QJsonDbWatcher);
    return d->status;
}

/*!
    \property QJsonDbWatcher::watchedActions
    Specifies which actions the watcher is registered for.
*/
void QJsonDbWatcher::setWatchedActions(QJsonDbWatcher::Actions actions)
{
    Q_D(QJsonDbWatcher);
    if (d->status != QJsonDbWatcher::Inactive)
        qWarning("QJsonDbWatcher: should not change already active watcher.");
    d->actions = actions;
}

QJsonDbWatcher::Actions QJsonDbWatcher::watchedActions() const
{
    Q_D(const QJsonDbWatcher);
    return d->actions;
}

/*!
    \property QJsonDbWatcher::query
    Specifies the query this watcher is registered for.
*/
void QJsonDbWatcher::setQuery(const QString &query)
{
    Q_D(QJsonDbWatcher);
    if (d->status != QJsonDbWatcher::Inactive)
        qWarning("QJsonDbWatcher: should not change already active watcher.");
    d->query = query;
}

QString QJsonDbWatcher::query() const
{
    Q_D(const QJsonDbWatcher);
    return d->query;
}

/*!
    \property QJsonDbWatcher::partition
    Specifies the partition this watcher is registered for.
*/
void QJsonDbWatcher::setPartition(const QString &partition)
{
    Q_D(QJsonDbWatcher);
    if (d->status != QJsonDbWatcher::Inactive)
        qWarning("QJsonDbWatcher: should not change already active watcher.");
    d->partition = partition;
}

QString QJsonDbWatcher::partition() const
{
    Q_D(const QJsonDbWatcher);
    return d->partition;
}

/*!
    \property QJsonDbWatcher::initialStateNumber

    Specifies from which database state number the watcher should start
    watching for notifications.

    \sa lastStateNumber()
*/
void QJsonDbWatcher::setInitialStateNumber(quint32 stateNumber)
{
    Q_D(QJsonDbWatcher);
    if (d->status != QJsonDbWatcher::Inactive)
        qWarning("QJsonDbWatcher: should not change already active watcher.");
    d->initialStateNumber = stateNumber;
    d->lastStateNumber = 0; // reset last seen state number
}

quint32 QJsonDbWatcher::initialStateNumber() const
{
    Q_D(const QJsonDbWatcher);
    return d->initialStateNumber;
}

/*!
    \property QJsonDbWatcher::lastStateNumber

    Specifies the last database state number the watcher received notifications for.

    This property contains valid data only after watcher was successfully
    activated (i.e. the watcher state was changed to QJsonDbWatcher::Active).

    \sa lastStateNumberChanged(), status
*/
quint32 QJsonDbWatcher::lastStateNumber() const
{
    Q_D(const QJsonDbWatcher);
    return d->lastStateNumber;
}

/*!
    Returns the first \a amount of notification events from the watcher.

    If amount is -1, retrieves all available notifications events.

    \sa notificationsAvailable()
*/
QList<QJsonDbNotification> QJsonDbWatcher::takeNotifications(int amount)
{
    Q_D(QJsonDbWatcher);
    QList<QJsonDbNotification> list;
    if (amount < 0 || amount >= d->notifications.size()) {
        list.swap(d->notifications);
    } else {
        list = d->notifications.mid(0, amount);
        d->notifications.erase(d->notifications.begin(), d->notifications.begin() + amount);
    }
    return list;
}

void QJsonDbWatcherPrivate::setStatus(QJsonDbWatcher::Status newStatus)
{
    Q_Q(QJsonDbWatcher);
    if (status != newStatus) {
        status = newStatus;
        emit q->statusChanged(status);
    }
}

void QJsonDbWatcherPrivate::_q_onFinished()
{
    Q_Q(QJsonDbWatcher);
    Q_ASSERT(status != QJsonDbWatcher::Inactive);
    if (status == QJsonDbWatcher::Activating) {
        // got a success reply to notification creation
        QJsonDbRequest *request = qobject_cast<QJsonDbRequest *>(q->sender());
        Q_ASSERT(request != 0);
        if (request) {
            QList<QJsonObject> objects = request->takeResults();
            Q_ASSERT(objects.size() == 1);
            version = objects.at(0).value(JsonDbStrings::Property::version()).toString();
        }
        setStatus(QJsonDbWatcher::Active);
    } else {
        // got a success reply to notification deletion
        setStatus(QJsonDbWatcher::Inactive);
    }
}

void QJsonDbWatcherPrivate::_q_onError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    Q_Q(QJsonDbWatcher);
    Q_UNUSED(code);
    QJsonDbWatcher::ErrorCode error = QJsonDbWatcher::InvalidQuery; // ### TODO:
    emit q->error(error, message);
}

void QJsonDbWatcherPrivate::handleNotification(QJsonDbWatcher::Action action, const QJsonObject &object)
{
    Q_Q(QJsonDbWatcher);
    if (!actions.testFlag(action))
        return;
    Q_ASSERT(!object.isEmpty());
    quint32 stateNumber = 0; // ### TODO:
    // lastStateNumber = stateNumber;
    // emit q->lastStateNumberChanged(stateNumber);
    QJsonDbNotification n(object, action, stateNumber);
    notifications.append(n);
    emit q->notificationsAvailable(notifications.size());
}

/*!
    \fn bool QJsonDbWatcher::isInactive() const
    Returns true if the status of the watcher is QJsonDbWatcher::Inactive.
    \sa status
*/
/*!
    \fn bool QJsonDbWatcher::isActivating() const
    Returns true if the status of the watcher is QJsonDbWatcher::Activating.
    \sa status
*/
/*!
    \fn bool QJsonDbWatcher::isActive() const
    Returns true if the status of the watcher is QJsonDbWatcher::Active.
    \sa status
*/
/*!
    \fn void QJsonDbWatcher::notificationsAvailable(int count)

    This signal is emitted when the \l{QJsonDbWatcher::isActive()}{active}
    watcher receives database notification events. \a count specifies how many
    events are available so far.

    \sa takeNotifications()
*/
/*!
    \fn void QJsonDbWatcher::statusChanged(QtJsonDb::QJsonDbWatcher::Status newStatus);
    This signal is emitted when state of the watcher changed to \a newStatus.
    \sa status
*/
/*!
    \fn void QJsonDbWatcher::error(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message);

    This signal is emitted when an error occured while activating the watcher.
    \a code and \a message describe the error.
*/
/*!
    \fn void QJsonDbWatcher::lastStateNumberChanged(int stateNumber);

    This signal is emitted when the given \a stateNumber is fully processed -
    all notifications for it were successfully received.

    \sa lastStateNumber
*/

#include "moc_qjsondbwatcher.cpp"

QT_END_NAMESPACE_JSONDB
