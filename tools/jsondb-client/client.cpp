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

#include "client.h"
#ifndef QTJSONDB_NO_DEPRECATED
#include "jsondbproxy.h"
#endif

#include <iostream>
#include <sstream>
#include <iomanip>

#include <QQmlComponent>
#include <QQmlEngine>

QT_USE_NAMESPACE

const char* InputThread::commands[] = { "create {\"",
                                        "help",
                                        "load",
                                        "notify create [?",
                                        "notify remove [?",
                                        "notify update [?",
                                        "query [?",
                                        "quit",
                                        "remove",
                                        "remove [?",
                                        "update",
                                        0 };

InputThread *InputThread::threadInstance = 0;

InputThread *InputThread::instance()
{
    if (!threadInstance)
        threadInstance = new InputThread;
    return threadInstance;
}

InputThread::~InputThread()
{
}

char const* InputThread::prompt(EditLine *e)
{
    Q_UNUSED(e);
    char const* string = "jclient> ";
    return string;
}

QString InputThread::longestCommonPrefix(const QStringList &list)
{
    if (list.size() > 1) {
        QChar temp;
        int pos = 0;
        for (bool roundComplete = true; roundComplete; pos++) {
            for (int i = 0; i < list.size(); i++) {
                QString entry = list[i];
                if (entry.length() <= pos) {
                    roundComplete = false;
                    break;
                } else if (i == 0)
                    temp = list[i][pos];
                else {
                    if (temp != list[i][pos]) {
                        roundComplete = false;
                        break;
                    }
                }
            }
        }
        return list[0].left(pos-1);
    } else if (list.size() == 1)
        return list[0];
    else
        return QString();
}

unsigned char InputThread::console_tabkey(EditLine * el, int ch)
{
    Q_UNUSED(ch);
    QStringList matches;
    const LineInfo *lineInfo = el_line(el);
    for (int i = 0; commands[i] != 0; i++) {
        int compare = qstrncmp(commands[i], lineInfo->buffer, lineInfo->lastchar-lineInfo->buffer);
        if (compare == 0) {
            matches << QString::fromLocal8Bit(commands[i]);
        }
    }

    int m = matches.size();
    if (m == 1) {
        QString missing = matches[0].remove(0,lineInfo->lastchar-lineInfo->buffer);
        el_insertstr(el, qPrintable(missing));
        return CC_REFRESH;
    } else if (m > 1) {
        instance()->async_print("\n" % matches.join("\n"));
        QString missing = longestCommonPrefix(matches).remove(0,lineInfo->lastchar-lineInfo->buffer);
        el_insertstr(el, qPrintable(missing));
        return CC_REFRESH;
    }
    return CC_ARGHACK;
}

void InputThread::run()
{
    int count;
    const char *line;
    HistEvent ev;

    el = el_init("jsondb-client", stdin, stdout, stderr);
    el_set(el, EL_PROMPT, &prompt);
    el_set(el, EL_EDITOR, "emacs");
    hist = history_init();
    el_set(el, EL_ADDFN, "tab-key", "TAB KEY PRESS", console_tabkey);
    if (hist == 0) {
        qDebug() << "initializing the history failed.";
        exit(-1);
    }
    history(hist, &ev, H_SETSIZE, 800);
    QString dirName = QDir::homePath() + QDir::separator() + QLatin1String(".jsondb");
    QFileInfo fi(dirName);
    if (!fi.exists())
    {
        if (!QDir::home().mkdir(".jsondb"))
            qWarning() << "Cannot create" << dirName << ". History will not work.";
    }

    historyFile = dirName + QDir::separator() + "history";
    history(hist, &ev, H_LOAD, historyFile.toLocal8Bit().constData());
    el_set(el, EL_HIST, history, hist);
    el_set(el, EL_BIND, "\t", "tab-key", NULL);
    while (true) {
        line = el_gets(el, &count);

        if (count > 0) {
            QString cmd = QString(line).trimmed();
            if (cmd == "quit")
                break;
            else if (!cmd.isEmpty()) {
                emit (commandReceived(cmd));
                history(hist, &ev, H_ENTER, line);
            }
        }
    }

    history(hist, &ev, H_SAVE, historyFile.toLocal8Bit().constData());
    history_end(hist);
    hist = 0;
    el_end(el);

    exit(0);
}

void InputThread::print(const QString &message)
{
    if (threadInstance) {
        instance()->async_print(message);
    } else {
        std::cout << qPrintable(message) << std::endl;
    }
}

void InputThread::async_print(const QString &message)
{
    QString clear(strlen(prompt(0)), QChar('\b'));
    std::cout << qPrintable(clear) << qPrintable(message) << std::endl;
    el_set(el, EL_REFRESH);
}

Client::Client( QObject *parent ) :
    QObject(parent)
  , mNotifier(0)
  , mInputThread(0)
  , mTerminate(false)
  , mDebug(false)
  , mEngine(0)
{
}

Client::~Client()
{
    if (mInputThread && mInputThread->isRunning()) {
        mInputThread->quit();
        mInputThread->wait(1000);
        delete mInputThread;
        mInputThread = 0;
    }
}

bool Client::connectToServer()
{
    mConnection = new QtJsonDb::QJsonDbConnection(this);

    connect(mConnection, SIGNAL(error(QtJsonDb::QJsonDbConnection::ErrorCode,QString)),
            this, SLOT(error(QtJsonDb::QJsonDbConnection::ErrorCode,QString)));
    connect(mConnection, SIGNAL(statusChanged(QtJsonDb::QJsonDbConnection::Status)),
            this, SLOT(statusChanged(QtJsonDb::QJsonDbConnection::Status)));

    mConnection->connectToServer();
    return true;
}

void Client::interactiveMode()
{
    mInputThread = InputThread::instance();
    connect(mInputThread, SIGNAL(commandReceived(QString)), this, SLOT(processCommand(QString)));
    connect(mInputThread, SIGNAL(finished()), QCoreApplication::instance(), SLOT(quit()));
    connect(mInputThread, SIGNAL(terminated()), QCoreApplication::instance(), SLOT(quit()));
    mInputThread->start();
}

void Client::onNotificationsAvailable(int)
{
    QtJsonDb::QJsonDbWatcher *watcher = qobject_cast<QtJsonDb::QJsonDbWatcher *>(sender());
    Q_ASSERT(watcher);
    if (!watcher)
        return;
    QList<QtJsonDb::QJsonDbNotification> notifications = watcher->takeNotifications();
    foreach (const QtJsonDb::QJsonDbNotification &n, notifications) {
        QString actionString;
        switch (n.action()) {
        case QtJsonDb::QJsonDbWatcher::Created:
            actionString = QStringLiteral("create"); break;
        case QtJsonDb::QJsonDbWatcher::Updated:
            actionString = QStringLiteral("update"); break;
        case QtJsonDb::QJsonDbWatcher::Removed:
            actionString = QStringLiteral("remove"); break;
        case QtJsonDb::QJsonDbWatcher::All: break;
        }

        QString message = QString("Received %1 notification for %2 [state %3]\n%4\n")
                .arg(actionString)
                .arg(watcher->query())
                .arg(n.stateNumber())
                .arg(QString::fromUtf8(QJsonDocument(n.object()).toJson()));
        InputThread::print(message);
    }
    if (!mInputThread)
        QCoreApplication::exit(0);  // Non-interactive mode just stops
}

void Client::onNotificationError(QtJsonDb::QJsonDbWatcher::ErrorCode error, const QString &message)
{
    QtJsonDb::QJsonDbWatcher *watcher = qobject_cast<QtJsonDb::QJsonDbWatcher *>(sender());
    Q_ASSERT(watcher);
    if (!watcher)
        return;
    qDebug() << "Failed to create watcher:" << watcher->query() << error << message;
}

void Client::onNotificationStatusChanged(QtJsonDb::QJsonDbWatcher::Status)
{
    QtJsonDb::QJsonDbWatcher *watcher = qobject_cast<QtJsonDb::QJsonDbWatcher *>(sender());
    Q_ASSERT(watcher);
    if (!watcher)
        return;
    if (watcher->isActive())
        qDebug() << "Watcher created:" << watcher->query();
}

void Client::statusChanged(QtJsonDb::QJsonDbConnection::Status)
{
    switch (mConnection->status()) {
    case QtJsonDb::QJsonDbConnection::Unconnected:
        qCritical() << "Lost connection to the server";
        break;
    case QtJsonDb::QJsonDbConnection::Connecting:
        qCritical() << "Connecting to the server...";
        break;
    case QtJsonDb::QJsonDbConnection::Authenticating:
        qCritical() << "Authenticating...";
        break;
    case QtJsonDb::QJsonDbConnection::Connected:
        qCritical() << "Connected to the server.";
        break;
    }
}

void Client::error(QtJsonDb::QJsonDbConnection::ErrorCode error, const QString &message)
{
    Q_UNUSED(error);
    Q_UNUSED(message);

    switch (error) {
    case QtJsonDb::QJsonDbConnection::NoError:
        Q_ASSERT(false);
        break;
    }
}

void Client::onRequestFinished()
{
    quint32 stateNumber;
    QtJsonDb::QJsonDbRequest *request = qobject_cast<QtJsonDb::QJsonDbReadRequest*>(sender());

    if (request) {
        stateNumber = qobject_cast<QtJsonDb::QJsonDbReadRequest*>(request)->stateNumber();
    } else {
        request = qobject_cast<QtJsonDb::QJsonDbWriteRequest*>(sender());
        if (!request)
            return;
        stateNumber = qobject_cast<QtJsonDb::QJsonDbWriteRequest*>(request)->stateNumber();
    }

    QList<QJsonObject> objects = request->takeResults();

    QString message = QString("Received %1 object(s) [state %2]\n").arg(objects.size()).arg(stateNumber);

    if (!objects.isEmpty()) {
        message += QJsonDocument(objects.front()).toJson().trimmed();
        for (int i = 1; i < objects.size(); ++i)
            message += QLatin1String(",\n") + QJsonDocument(objects.at(i)).toJson().trimmed();
    }
    InputThread::print(message);
}

void Client::aboutToRemove(void)
{

    QtJsonDb::QJsonDbRequest *queryRequest = qobject_cast<QtJsonDb::QJsonDbRequest *>(sender());
    Q_ASSERT(queryRequest != 0);
    if (!queryRequest)
        return;

    QList<QJsonObject> objects = queryRequest->takeResults();
    QString message = QLatin1String("Query result: received ") + QString::number(objects.size()) + QLatin1String(" object(s):\n");
    InputThread::print(message);

    if (objects.isEmpty()) {
        InputThread::print("No object matches your query. Nothing to remove.");
        popRequest();
        return;
    }

    // remove the query from the request list
    mRequests.takeFirst()->deleteLater();

    InputThread::print("The object(s) matching your query are about to be removed.");
    //we get the objects, remove them now
    QtJsonDb::QJsonDbRemoveRequest* request = new QtJsonDb::QJsonDbRemoveRequest(objects);
    request->setPartition(queryRequest->partition());

    connect(request, SIGNAL(finished()), this, SLOT(onRequestFinished()));
    connect(request, SIGNAL(finished()), this, SLOT(popRequest()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    pushRequest(request);
    mConnection->send(request);
}

void Client::onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    Q_UNUSED(code);
    InputThread::print(message);
}

void Client::pushRequest(QtJsonDb::QJsonDbRequest *request)
{
    mRequests.append(request);
}

void Client::popRequest()
{
    mRequests.takeFirst()->deleteLater();
    if (mRequests.isEmpty() && mTerminate)
        emit terminate();
}

void Client::usage()
{
    std::stringstream out;
    out << "Valid commands:" << std::endl
              << "   connect" << std::endl
              << "   disconnect" << std::endl
              << "Direct database commands - these take an explicit object" << std::endl
              << "   create [partition:<name>] OBJECT" << std::endl
              << "   update [partition:<name>] OBJECT" << std::endl
              << "   remove [partition:<name>] OBJECT" << std::endl
              << "   remove [partition:<name>] STRING [limit]" << std::endl
              << "   query [partition:<name>] STRING [limit]" << std::endl
              << "   notify [partition:<name>] ACTIONS QUERY [starting-state]" << std::endl
              << "   load FILE1 FILE2 ..." << std::endl
              << "   help" << std::endl
              << "   quit" << std::endl
              << std::endl
              << "ACTIONS: comma separated list. Valid values: create, update, remove" << std::endl
              << "OBJECT:  Valid JSON object" << std::endl
              << "QUERY:   Valid JSONQuery command" << std::endl
              << std::endl
              << "Sample commands: " << std::endl
              << "   create {\"_type\": \"duck\", \"name\": \"Fred\"}" << std::endl
              << "   query [?_type=\"duck\"]" << std::endl
              << "   remove {\"_uuid\": \"{18c9d905-5860-464e-a6dd-951464e366de}\", \"_version\": \"1-134f23dbb2\"}" << std::endl
              << "   remove [?_type=\"duck\"]" << std::endl
              << "   notify create,remove [?_type=\"duck\"]" << std::endl
              << "   notify create,remove [?_type=\"duck\"] 53" << std::endl;


    QString usageInfo = QString::fromStdString(out.str());
    InputThread::print(usageInfo);
}

bool Client::processCommand(const QString &command)
{
    QString cmd = command.trimmed();
    QString rest;

    int space = command.indexOf(' ');
    if (space > 0) {
        cmd = command.left(space);
        rest = command.mid(space+1).trimmed();
    }

    QString partition;
    if (rest.startsWith(QLatin1String("partition:"))) {
        partition = rest.left(rest.indexOf(' '));
        rest.remove(0, partition.size());
        partition.remove(0, 10);
        rest = rest.trimmed();
    } else {
        partition = mDefaultPartition;
    }

    if (cmd == "quit") {
        exit(0);
    } else if (cmd == "help") {
        usage();
    } else if (cmd == "query" || ((cmd == "remove") && (rest.left(2) == "[?"))) {
        int limit = -1;
        int idx = rest.lastIndexOf(']');
        if (idx != -1) {
            QStringList list = rest.mid(idx+1).split(' ');
            int i = 0;
            for (; i < list.size(); ++i) {
                if (!list.at(i).trimmed().isEmpty()) {
                    limit = list.at(i).toInt();
                    break;
                }
            }
            rest.truncate(idx+1);
        }
        if (mDebug)
            qDebug() << "Sending query:" << rest;

        QtJsonDb::QJsonDbReadRequest *request = new QtJsonDb::QJsonDbReadRequest(this);
        request->setPartition(partition);
        request->setQuery(rest);
        request->setQueryLimit(limit);
        if (cmd == "remove") {
            connect(request, SIGNAL(finished()), this, SLOT(aboutToRemove()));
        }
        else {
            connect(request, SIGNAL(finished()), this, SLOT(onRequestFinished()));
            connect(request, SIGNAL(finished()), this, SLOT(popRequest()));
        }
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(popRequest()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        pushRequest(request);
        mConnection->send(request);
    } else if (cmd == "notify") {
        int s = rest.indexOf(' ');
        if (s <= 0)
            return false;
        QStringList alist = rest.left(s).split(QRegExp(","), QString::SkipEmptyParts);
        QtJsonDb::QJsonDbWatcher::Actions actions;
        foreach (const QString &s, alist) {
            QtJsonDb::QJsonDbWatcher::Action action = QtJsonDb::QJsonDbWatcher::Action(0);
            if (s == QLatin1String("create"))
                action = QtJsonDb::QJsonDbWatcher::Created;
            else if (s == QLatin1String("remove"))
                action = QtJsonDb::QJsonDbWatcher::Removed;
            else if (s == QLatin1String("update"))
                action = QtJsonDb::QJsonDbWatcher::Updated;
            if (action == QtJsonDb::QJsonDbWatcher::Action(0)) {
                InputThread::print("unknown notification type" % s);
                return false;
            }
            actions |= action;
        }

        int startingState = 0;
        QString query = rest.mid(s+1).trimmed();
        if (!query.endsWith(']')) {
            bool ok;
            int index = query.lastIndexOf(' ');
            int state = query.right(query.length() - index).trimmed().toInt(&ok);
            if (ok)
                startingState = state;
            query = query.left(index);
        }

        if (mDebug)
            qDebug() << "Creating notification:" << alist << ":" << query;

        QtJsonDb::QJsonDbWatcher *watcher = new QtJsonDb::QJsonDbWatcher(this);
        watcher->setPartition(partition);
        watcher->setQuery(query);
        watcher->setWatchedActions(actions);
        if (startingState != 0)
            watcher->setInitialStateNumber(startingState);

        connect(watcher, SIGNAL(notificationsAvailable(int)), this, SLOT(onNotificationsAvailable(int)));
        connect(watcher, SIGNAL(statusChanged(QtJsonDb::QJsonDbWatcher::Status)),
                this, SLOT(onNotificationStatusChanged(QtJsonDb::QJsonDbWatcher::Status)));
        connect(watcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
                this, SLOT(onNotificationError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
        mConnection->addWatcher(watcher);
    } else if (cmd == "create" || cmd == "update" || cmd == "remove") {
        QJsonDocument doc = QJsonDocument::fromJson(rest.toUtf8());
        if (doc.isEmpty()) {
            InputThread::print("Unable to parse: " % rest);
            usage();
            return false;
        }

        if (mDebug)
            qDebug() << "Sending" << cmd << ":" << doc;

        QList<QJsonObject> objects;
        if (doc.isObject()) {
            objects.append(doc.object());
        } else {
            foreach (const QJsonValue &value, doc.array()) {
                if (value.isObject())
                    objects.append(value.toObject());
            }
        }

        QtJsonDb::QJsonDbWriteRequest *request = 0;
        if (cmd == "create")
            request = new QtJsonDb::QJsonDbCreateRequest(objects);
        else if (cmd == "update")
            request = new QtJsonDb::QJsonDbUpdateRequest(objects);
        else if (cmd == "remove")
            request = new QtJsonDb::QJsonDbRemoveRequest(objects);
        else
            Q_ASSERT(false);
        request->setPartition(partition);
        connect(request, SIGNAL(finished()), this, SLOT(onRequestFinished()));
        connect(request, SIGNAL(finished()), this, SLOT(popRequest()));
        connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        pushRequest(request);
        mConnection->send(request);
    } else if (cmd == "load") {
        QStringList filenames = rest.split(' ');
        for (int i = 0; i < filenames.size(); i++) {
            QString filename = filenames[i];
            if (filename.startsWith('"'))
                filenames[i] = filename.mid(1, filename.size()-2);
        }
        loadFiles(filenames);
    } else if (cmd == "connect") {
        mConnection->connectToServer();
    } else if (cmd == "disconnect") {
        mConnection->disconnectFromServer();
    } else if (!cmd.isEmpty()) {
        InputThread::print("Unrecognized command: " % cmd);
        usage();
        return false;
    } else {
        return false;
    }

    return true;
}

void Client::loadFiles(const QStringList &files)
{
    mFilesToLoad = files;
    loadNextFile();
}

void Client::fileLoadSuccess()
{
    // make sure it's a request so we don't accidently delete the declarative engine
    QtJsonDb::QJsonDbRequest *request = qobject_cast<QtJsonDb::QJsonDbRequest *>(sender());
    if (request)
        request->deleteLater();

    qDebug() << "Successfully loaded:" << mFilesToLoad.takeFirst();
    loadNextFile();
}

void Client::fileLoadError()
{
    // Could be a QJsonDbWriteRequest or a QTimer, either way it needs to be cleaned up
    if (sender())
        sender()->deleteLater();

    qDebug() << "Error loading:" << mFilesToLoad.takeFirst();
    loadNextFile();
}

void Client::loadNextFile()
{
    if (mFilesToLoad.isEmpty()) {
        if (mTerminate)
            emit terminate();
        else if (!mInputThread)
            interactiveMode();
        return;
    }

    QFileInfo info(mFilesToLoad.first());
    if (info.suffix() == QLatin1String("json")) {
        loadJsonFile(info.filePath());
    } else if (info.suffix() == QLatin1String("qml")) {
        loadQmlFile(info.filePath());
#ifndef QTJSONDB_NO_DEPRECATED
    } else if (info.suffix() == QLatin1String("js")) {
        loadJavaScriptFile(info.filePath());
#endif
    } else {
        qDebug() << "Unknown file type:" << mFilesToLoad.takeFirst();
        loadNextFile();
    }
}

void Client::loadJsonFile(const QString &jsonFile)
{
    QFile json(jsonFile);

    if (!json.exists()) {
        qDebug() << "File not found:" << jsonFile;
        fileLoadError();
        return;
    }

    json.open(QFile::ReadOnly);
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.readAll(), &error);
    json.close();

    if (error.error != QJsonParseError::NoError) {
        qDebug() << "Unable to parse file:" << error.errorString();
        fileLoadError();
        return;
    }

    QList<QJsonObject> objects;
    if (doc.isArray()) {
        QJsonArray objectArray = doc.array();
        for (QJsonArray::const_iterator it = objectArray.begin(); it != objectArray.end(); it++) {
            QJsonValue val = *it;
            if (val.isObject())
                objects.append(val.toObject());
        }
    } else {
        objects.append(doc.object());
    }

    QtJsonDb::QJsonDbCreateRequest *write = new QtJsonDb::QJsonDbCreateRequest(objects, this);
    write->setPartition(mDefaultPartition);
    connect(write, SIGNAL(finished()), this, SLOT(fileLoadSuccess()));
    connect(write, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(fileLoadError()));
    connect(write, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    mConnection->send(write);
}

void Client::loadQmlFile(const QString &qmlFile)
{
    QFile qml(qmlFile);

    if (!qml.exists()) {
        qDebug() << "File not found:" << qmlFile;
        fileLoadError();
        return;
    }

    if (!mEngine) {
        mEngine = new QQmlEngine(this);
        connect(mEngine, SIGNAL(quit()), this, SLOT(fileLoadSuccess()));
    }

    qml.open(QFile::ReadOnly);
    QQmlComponent *component = new QQmlComponent(mEngine, this);
    component->setData(qml.readAll(), QUrl());
    qml.close();

    // Time the qml loading out after 10 seconds
    QTimer *timeout = new QTimer(this);
    timeout->setSingleShot(true);
    connect(timeout, SIGNAL(timeout()), this, SLOT(fileLoadError()));
    connect(mEngine, SIGNAL(quit()), timeout, SLOT(stop()));
    timeout->start(10000);

    QObject *created = component->create();
    if (created) {
        connect(mEngine, SIGNAL(quit()), component, SLOT(deleteLater()));
        connect(mEngine, SIGNAL(quit()), created, SLOT(deleteLater()));
        return;
    }

    fileLoadError();
}

#ifndef QTJSONDB_NO_DEPRECATED
void Client::loadJavaScriptFile(const QString &jsFile)
{
    QFile js(jsFile);

    if (!js.exists()) {
        qDebug() << "File not found:" << jsFile;
        fileLoadError();
        return;
    }

    QJSEngine *scriptEngine = new QJSEngine(this);
    QJSValue globalObject = scriptEngine->globalObject();
    QJSValue proxy = scriptEngine->newQObject(new JsonDbProxy(mConnection, mDefaultPartition, this));
    globalObject.setProperty("jsondb", proxy);
    globalObject.setProperty("console", proxy);

    js.open(QFile::ReadOnly);
    QJSValue sv = scriptEngine->evaluate(js.readAll(), jsFile);
    scriptEngine->deleteLater();
    js.close();

    if (sv.isError()) {
        qDebug() << sv.toString();
        fileLoadError();
        return;
    }

    fileLoadSuccess();
}

#endif
