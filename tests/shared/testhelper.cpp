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

#include "testhelper.h"

#include <QJsonDbWatcher>

#include <QCoreApplication>
#include <QDir>
#include <QLocalSocket>
#include <QProcess>
#include <QTest>
#include <QTimer>
#include <QJsonArray>

QT_USE_NAMESPACE_JSONDB

TestHelper::TestHelper(QObject *parent) :
    QObject(parent)
  , mProcess(0)
  , mConnection(0)
  , mNotificationsReceived(0)
  , mNotificationsExpected(0)
  , mLastStateChangedExpected(0)
  , mLastStateChangedReceived(0)
  , mRequestsPending(0)
{
}

QJsonDocument TestHelper::readJsonFile(const QString &filename, QJsonParseError *error)
{
    QString filepath = filename;
    QFile jsonFile(filepath);
    if (!jsonFile.exists()) {
        if (error) {
            error->error = QJsonParseError::MissingObject;
            error->offset = 0;
        }
        return QJsonDocument();
    }
    jsonFile.open(QIODevice::ReadOnly);
    QByteArray json = jsonFile.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(json, error));
    return doc;
}

void TestHelper::launchJsonDbDaemon(const QStringList &args, const char *sourceFile)
{
    QStringList partitionPath;
    QFileInfo partitionsFile(QFINDTESTDATA("partitions.json"));
    partitionPath << partitionsFile.absolutePath()
                  << QFileInfo(QString::fromLatin1(sourceFile)).absolutePath()
                  << QCoreApplication::applicationDirPath();
    qputenv("JSONDB_CONFIG_SEARCH_PATH", partitionPath.join(QStringLiteral(":")).toUtf8());

    if (dontLaunch())
        return;

    QString jsondb_app = QDir(QString::fromLocal8Bit(JSONDB_DAEMON_BASE)).absoluteFilePath(QLatin1String("jsondb"));
    if (!QFile::exists(jsondb_app))
        jsondb_app = QLatin1String("jsondb"); // rely on the PATH

    mProcess = new QProcess;
    mProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(mProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(processFinished(int,QProcess::ExitStatus)));

    QString socketName = QString("testjsondb_%1").arg(getpid());

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("JSONDB_SOCKET", socketName);
    mProcess->setProcessEnvironment(env);
    ::setenv("JSONDB_SOCKET", qPrintable(socketName), 1);

    QStringList argList = args;
    argList << QLatin1String("-reject-stale-updates");

    qDebug() << "Starting process" << jsondb_app << argList << "with socket" << socketName;

    if (useValgrind()) {
        QStringList args1 = argList;
        args1.prepend(jsondb_app);
        mProcess->start("valgrind", args1);
    } else {
        mProcess->start(jsondb_app, argList);
    }

    if (!mProcess->waitForStarted())
        qFatal("Unable to start jsondb database process");

    /* Wait until the jsondb is accepting connections */
    int tries = 0;
    bool connected = false;
    while (!connected && tries++ < 100) {
        QLocalSocket socket;
        socket.connectToServer(socketName);
        if (socket.waitForConnected()) {
            connected = true;
            socket.close();
        }
        QTest::qWait(250);
    }

    if (!connected)
        qFatal("Unable to connect to jsondb process");
}

inline qint64 TestHelper::launchJsonDbDaemonDetached(const QStringList &args, const char *sourceFile)
{
    QStringList partitionPath;
    QFileInfo partitionsFile(QFINDTESTDATA("partitions.json"));
    partitionPath << partitionsFile.absolutePath()
                  << QFileInfo(QString::fromLatin1(sourceFile)).absolutePath()
                  << QCoreApplication::applicationDirPath();
    qputenv("JSONDB_CONFIG_SEARCH_PATH", partitionPath.join(QStringLiteral(":")).toUtf8());

    if (dontLaunch())
        return 0;

    QString jsondb_app = QDir(QString::fromLocal8Bit(JSONDB_DAEMON_BASE)).absoluteFilePath(QLatin1String("jsondb"));
    if (!QFile::exists(jsondb_app))
        jsondb_app = QLatin1String("jsondb"); // rely on the PATH

    QString socketName = QString("testjsondb_%1").arg(getpid());
    ::setenv("JSONDB_SOCKET", qPrintable(socketName), 1);

    QStringList argList = args;
    argList << QLatin1String("-reject-stale-updates");

    qDebug() << "Starting process" << jsondb_app << argList << "with socket" << socketName;
    qint64 pid;
    if (useValgrind()) {
        QStringList args1 = argList;
        args1.prepend(jsondb_app);
        QProcess::startDetached(jsondb_app, args1, QDir::currentPath(), &pid );
    } else {
        QProcess::startDetached(jsondb_app, argList, QDir::currentPath(), &pid);
    }

    /* Wait until the jsondb is accepting connections */
    int tries = 0;
    bool connected = false;
    while (!connected && tries++ < 100) {
        QLocalSocket socket;
        socket.connectToServer(socketName);
        if (socket.waitForConnected()) {
            connected = true;
            socket.close();
        }
        QTest::qWait(250);
    }
    if (!connected)
        qFatal("Unable to connect to jsondb process");

    return pid;
}

void TestHelper::stopDaemon()
{
    if (dontLaunch())
        return;

    if (mProcess) {
        mProcess->close();
        delete mProcess;
        mProcess = 0;
    }
}

void TestHelper::connectToServer()
{
    mConnection = new QJsonDbConnection(this);
    connect(mConnection, SIGNAL(error(QtJsonDb::QJsonDbConnection::ErrorCode,QString)),
            this, SLOT(connectionError(QtJsonDb::QJsonDbConnection::ErrorCode,QString)));

    mConnection->connectToServer();
}

void TestHelper::disconnectFromServer()
{
    connect(mConnection, SIGNAL(disconnected()), &mEventLoop, SLOT(quit()), Qt::QueuedConnection);
    mConnection->disconnectFromServer();
    blockWithTimeout();

    if (mConnection) {
        delete mConnection;
        mConnection = 0;
    }

    mRequestErrors.clear();
}

void TestHelper::removeDbFiles(const QStringList &additionalFiles)
{
    if (dontLaunch())
        return;

    QStringList files = QDir().entryList(QStringList() << QLatin1String("*.db"));
    files << additionalFiles;
    foreach (const QString &fileName, files)
        QFile::remove(fileName);
}

bool TestHelper::waitForResponse(QJsonDbRequest *request)
{
    mRequestsPending = 1;
    mNotificationsExpected = 0;
    mLastStateChangedExpected = 0;

    connect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));

    blockWithTimeout();

    disconnect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
    disconnect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
               this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));

    return !mRequestsPending;
}

bool TestHelper::waitForResponse(QList<QJsonDbRequest *> requests)
{
    mRequestsPending = requests.count();
    mNotificationsExpected = 0;
    mLastStateChangedExpected = 0;

    foreach (QJsonDbRequest *request, requests) {
        connect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    }

    blockWithTimeout();

    foreach (QJsonDbRequest *request, requests) {
        disconnect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
        disconnect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                   this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    }

    return !mRequestsPending;
}

bool TestHelper::waitForResponseAndNotifications(QJsonDbRequest *request,
                                                 QJsonDbWatcher *watcher,
                                                 int notificationsExpected,
                                                 int lastStateChangedExpected)
{
    mNotificationsExpected = notificationsExpected;
    mNotificationsReceived = 0;
    mLastStateChangedExpected = lastStateChangedExpected;
    mLastStateChangedReceived = 0;

    if (request) {
        mRequestsPending = 1;

        connect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    }

    connect(watcher, SIGNAL(notificationsAvailable(int)),
            this, SLOT(watcherNotificationsAvailable(int)));
    connect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
            this, SLOT(watcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    connect(watcher, SIGNAL(lastStateNumberChanged(int)),
            this, SLOT(watcherLastStateNumberChanged(int)));

    blockWithTimeout();

    if (request) {
        disconnect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
        disconnect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                   this, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    }

    disconnect(watcher, SIGNAL(notificationsAvailable(int)),
               this, SLOT(watcherNotificationsAvailable(int)));
    disconnect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
               this, SLOT(watcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    disconnect(watcher, SIGNAL(lastStateNumberChanged(int)),
            this, SLOT(watcherLastStateNumberChanged(int)));

    bool res = !mRequestsPending && mNotificationsReceived >= mNotificationsExpected
            && mLastStateChangedReceived >= mLastStateChangedExpected;

    mNotificationsExpected = 0;

    return res;
}

bool TestHelper::waitForStatus(QJsonDbWatcher *watcher, QJsonDbWatcher::Status status)
{
    mReceivedError = QJsonDbWatcher::NoError;
    mReceivedStatus = watcher->status();
    mExpectedStatus = status;

    if (mReceivedStatus == status)
        return true;

    connect(watcher, SIGNAL(statusChanged(QtJsonDb::QJsonDbWatcher::Status)),
            this, SLOT(watcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status)));
    blockWithTimeout();
    disconnect(watcher, SIGNAL(statusChanged(QtJsonDb::QJsonDbWatcher::Status)),
               this, SLOT(watcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status)));

    return mReceivedStatus == status;
}

bool TestHelper::waitForError(QJsonDbWatcher *watcher, QJsonDbWatcher::ErrorCode error)
{
    mExpectedError = error;
    connect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
            this, SLOT(watcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    blockWithTimeout();
    disconnect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
               this, SLOT(watcherError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));

    return mReceivedError == error;
}

bool TestHelper::dontLaunch()
{
    static const bool dontlaunch = qgetenv("AUTOTEST_DONT_LAUNCH_JSONDB").toInt() == 1;
    return dontlaunch;
}

bool TestHelper::useValgrind()
{
    static const bool usevalgrind = qgetenv("AUTOTEST_VALGRIND_JSONDB").toInt() == 1;
    return usevalgrind;
}

void TestHelper::blockWithTimeout()
{
    QTimer t;
    connect(&t, SIGNAL(timeout()), &mEventLoop, SLOT(quit()));
    connect(&t, SIGNAL(timeout()), this, SLOT(timeout()));

    t.start(10000);
    mEventLoop.exec(QEventLoop::AllEvents);
    t.stop();
}

void TestHelper::connectionError(QtJsonDb::QJsonDbConnection::ErrorCode code, QString msg)
{
    qCritical() << "Error from connection" << code << msg;
}

void TestHelper::processFinished(int code, QProcess::ExitStatus status)
{
    qDebug() << "jsondb process finished" << code << status;
}

void TestHelper::requestFinished()
{
    --mRequestsPending;
    if (!mRequestsPending && mNotificationsReceived >= mNotificationsExpected
        && mLastStateChangedReceived >= mLastStateChangedExpected)
        mEventLoop.quit();
}

void TestHelper::requestError(QtJsonDb::QJsonDbRequest::ErrorCode code, QString msg)
{
    qWarning() << "Request error:" << code << msg;
    QJsonDbRequest *request = qobject_cast<QJsonDbRequest*>(sender());
    if (request)
        mRequestErrors[request] = code;

    requestFinished();
}

void TestHelper::requestStatusChanged(QtJsonDb::QJsonDbRequest::Status status)
{
    Q_UNUSED(status);
}

void TestHelper::watcherNotificationsAvailable(int count)
{
    mNotificationsReceived = count;

    if (!mRequestsPending && mNotificationsReceived >= mNotificationsExpected
        && mLastStateChangedReceived >= mLastStateChangedExpected)
        mEventLoop.quit();
}

void TestHelper::watcherStatusChanged(QtJsonDb::QJsonDbWatcher::Status status)
{
    mReceivedStatus = status;
    if (status == mExpectedStatus)
        mEventLoop.quit();
}

void TestHelper::watcherLastStateNumberChanged(int stateNumber)
{
    Q_UNUSED(stateNumber);
    mLastStateChangedReceived++;
    if (!mRequestsPending && mNotificationsReceived >= mNotificationsExpected
        && mLastStateChangedReceived >= mLastStateChangedExpected)
        mEventLoop.quit();
}

void TestHelper::watcherError(QtJsonDb::QJsonDbWatcher::ErrorCode code, QString msg)
{
    qWarning() << "Watcher error:" << code << msg;
    mReceivedError = code;
    if (code == mExpectedError)
        mEventLoop.quit();
}

void TestHelper::timeout()
{
    qCritical() << "A timeout occurred";
}


