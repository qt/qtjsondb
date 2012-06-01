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

#ifndef JSONDB_TESTHELPER_H
#define JSONDB_TESTHELPER_H

#include <QJsonDbConnection>
#include <QJsonDbRequest>
#include <QJsonDbWatcher>

#include <QEventLoop>
#include <QHash>
#include <QMap>
#include <QJsonDocument>
#include <QJsonValue>
#include <QObject>
#include <QProcess>
#include <QStringList>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

class TestHelper : public QObject
{
    Q_OBJECT
public:
    explicit TestHelper(QObject *parent = 0);

    QJsonDocument readJsonFile(const QString &filename, QJsonParseError *error = 0);
    QString findFile(const QString &filename);
    QString findFile(const char *filename);

    void launchJsonDbDaemon(const QStringList &args, const char *sourceFile, bool skipConnection = false);
    qint64 launchJsonDbDaemonDetached(const QStringList &args, const char *sourceFile, bool skipConnection = false);
    void stopDaemon();

    void connectToServer();
    void disconnectFromServer();

    void removeDbFiles(const QStringList &additionalFiles = QStringList());

    bool waitForResponse(QtJsonDb::QJsonDbRequest *request);
    bool waitForResponse(QList<QtJsonDb::QJsonDbRequest*> requests);
    bool waitForResponseAndNotifications(QtJsonDb::QJsonDbRequest *request,
                                         QtJsonDb::QJsonDbWatcher *watcher,
                                         int notificationsExpected,
                                         int lastStateChangedExpected = 0);
    bool waitForStatus(QtJsonDb::QJsonDbWatcher *watcher,
                       QtJsonDb::QJsonDbWatcher::Status status);
    bool waitForStatusAndNotifications(QtJsonDb::QJsonDbWatcher *watcher,
                                       QtJsonDb::QJsonDbWatcher::Status status,
                                       int notificationsExpected);
    bool waitForError(QtJsonDb::QJsonDbWatcher *watcher,
                      QtJsonDb::QJsonDbWatcher::ErrorCode error);

    QtJsonDb::QJsonDbConnection *connection() const
    { return mConnection; }
    const QString &workingDirectory() const { return mWorkingDirectory; }
    void setWorkingDirectory(const QString &workingDirectory) { mWorkingDirectory = workingDirectory; }

    void clearHelperData();

protected:
    QProcess *mProcess;
    QtJsonDb::QJsonDbConnection *mConnection;
    QEventLoop mEventLoop;
    int mNotificationsReceived;
    int mNotificationsExpected;
    int mLastStateChangedExpected;
    int mLastStateChangedReceived;
    QHash<QtJsonDb::QJsonDbRequest *, QtJsonDb::QJsonDbRequest::ErrorCode> mRequestErrors;
    QMap<QtJsonDb::QJsonDbRequest *, QList<QtJsonDb::QJsonDbRequest::Status> > mRequestStatuses;

    void blockWithTimeout();

protected Q_SLOTS:
    void connectionError(QtJsonDb::QJsonDbConnection::ErrorCode code, QString msg);

    void processFinished(int,QProcess::ExitStatus);

    void requestFinished();
    void requestError(QtJsonDb::QJsonDbRequest::ErrorCode code, QString msg);
    void requestStatusChanged(QtJsonDb::QJsonDbRequest::Status status);

    void watcherNotificationsAvailable(int count);
    void watcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status status);
    void watcherError(QtJsonDb::QJsonDbWatcher::ErrorCode code, QString msg);
    void watcherLastStateNumberChanged(int stateNumber);
    void timeout();

private:
    static bool dontLaunch();
    static bool useValgrind();

    qint64 launchJsonDbDaemon_helper(const QStringList &args, const char *sourceFile, bool skipConnection, bool detached);

    QString mWorkingDirectory;
    int mRequestsPending;
    static int mProcessIndex;
    QtJsonDb::QJsonDbWatcher::Status mReceivedStatus;
    QtJsonDb::QJsonDbWatcher::Status mExpectedStatus;
    QtJsonDb::QJsonDbWatcher::ErrorCode mReceivedError;
    QtJsonDb::QJsonDbWatcher::ErrorCode mExpectedError;
};

#endif // JSONDB_TESTHELPER_H
