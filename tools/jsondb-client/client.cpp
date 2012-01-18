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

#include <iostream>
#include <sstream>
#include <iomanip>

extern bool gDebug;

const char* InputThread::commands[] = { "changesSince",
                                        "create {\"",
                                        "help",
                                        "notify create [?",
                                        "notify remove [?",
                                        "notify update [?",
                                        "query [?",
                                        "quit",
                                        "remove",
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
    if (hist && !historyFile.isEmpty())
        history(hist, 0, H_SAVE, historyFile.toLocal8Bit().constData());
    if (hist) {
        history_end(hist);
        hist = 0;
    }
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
    historyFile = QDir::homePath() + QDir::separator() + QLatin1String(".jsondb/history");
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

Client::Client( QObject *parent )
    : QObject(parent), mNotifier(NULL), mInputThread(NULL)
{
}

Client::~Client()
{
    if (mInputThread) {
        mInputThread->terminate();
        mInputThread->wait(1000);
        delete mInputThread;
        mInputThread = 0;
    }
}

bool Client::connectToServer()
{
    QString socketName = ::getenv("JSONDB_SOCKET");
    mConnection = new QtJsonDb::QJsonDbConnection(this);
    if (!socketName.isEmpty())
        mConnection->setSocketName(socketName);

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

        QString message =  "Received notification: type " % actionString
                % " for " % watcher->query() % " object:\n" % QJsonDocument(n.object()).toJson();
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
    switch (error) {
    case QtJsonDb::QJsonDbConnection::NoError:
        Q_ASSERT(false);
        break;
    }
}

void Client::onRequestFinished()
{
    QtJsonDb::QJsonDbRequest *request = qobject_cast<QtJsonDb::QJsonDbRequest *>(sender());
    Q_ASSERT(request != 0);
    if (!request)
        return;

    QList<QJsonObject> objects = request->takeResults();

    QString message = QLatin1String("Received ") + QString::number(objects.size()) + QLatin1String(" object(s):\n");
    if (!objects.isEmpty()) {
        message += QJsonDocument(objects.front()).toJson().trimmed();
        for (int i = 1; i < objects.size(); ++i)
            message += QLatin1String(",\n") + QJsonDocument(objects.at(i)).toJson().trimmed();
    }
    InputThread::print(message);

    if (!mInputThread)
        QCoreApplication::exit(0);  // Non-interactive mode just stops
}

void Client::pushRequest(QtJsonDb::QJsonDbRequest *request)
{
    mRequests.append(request);
}

void Client::popRequest()
{
    QtJsonDb::QJsonDbRequest *request = mRequests.takeFirst();
    delete request;
    if (mRequests.isEmpty())
        emit requestsProcessed();
}

void Client::usage()
{
    std::stringstream out;
    out << "Valid commands:" << std::endl
              << "   connect" << std::endl
              << "   disconnect" << std::endl
              << "Direct database commands - these take an explict object" << std::endl
              << "   create [partition:<name>] OBJECT" << std::endl
              << "   update [partition:<name>] OBJECT" << std::endl
              << "   remove [partition:<name>] OBJECT" << std::endl
              << "   remove [partition:<name>] QUERY" << std::endl
              << "   find [partition:<name>] QUERY" << std::endl
              << "   changesSince [partition:<name>] STATENUMBER [type1 type2 ...]" << std::endl
              << std::endl
              << "Convenience functions" << std::endl
              << "   query STRING [limit]" << std::endl
              << "                  find {\"query\": STRING}" << std::endl
              << "   notify ACTIONS QUERY" << std::endl
              << "                  create { \"_type\": \"notification\"," << std::endl
              << "                           \"query\": QUERY," << std::endl
              << "                           \"actions\": ACTIONS }" << std::endl
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
              << "   notify create,remove [?_type=\"duck\"]" << std::endl;

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

    gDebug = true;

    QString partition;
    if (rest.startsWith(QLatin1String("partition:"))) {
        partition = rest.left(rest.indexOf(' '));
        rest.remove(0, partition.size());
        partition.remove(0, 10);
        rest = rest.trimmed();
    }

    if (cmd == "quit") {
        exit(0);
    } else if (cmd == "help") {
        usage();
    } else if (cmd == "query") {
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
        if (gDebug)
            qDebug() << "Sending query:" << rest;
        QtJsonDb::QJsonDbReadRequest *request = new QtJsonDb::QJsonDbReadRequest(this);
        request->setPartition(partition);
        request->setQuery(rest);
        request->setQueryLimit(limit);
        connect(request, SIGNAL(finished()), this, SLOT(onRequestFinished()));
        connect(request, SIGNAL(finished()), this, SLOT(popRequest()));
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
                InputThread::print("uknown notification type" % s);
                return false;
            }
            actions |= action;
        }
        QString query = rest.mid(s+1).trimmed();
        if (gDebug)
            qDebug() << "Creating notification:" << alist << ":" << query;
        QtJsonDb::QJsonDbWatcher *watcher = new QtJsonDb::QJsonDbWatcher(this);
        watcher->setPartition(partition);
        watcher->setQuery(query);
        watcher->setWatchedActions(actions);
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
        if (gDebug)
            qDebug() << "Sending" << cmd << ":" << doc;
        QList<QJsonObject> objects;
        if (doc.isObject()) {
            objects.append(doc.object());
        } else {
            foreach (const QJsonValue &value, doc.array())
                objects.append(value.toObject());
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
        pushRequest(request);
        mConnection->send(request);
    } else if (cmd == "changesSince") {
//        int stateNumber = 0;
//        QStringList types;
//        QStringList args = rest.split(" ");

//        if (args.isEmpty()) {
//            InputThread::print("Must specify the state number");
//            usage();
//            return false;
//        }

//        stateNumber = args.takeFirst().trimmed().toInt();

//        if (!args.isEmpty())
//            types = args;

//        if (gDebug)
//            qDebug() << "Sending changesSince: " << stateNumber << "types: " << types;

//        mRequests << mConnection->changesSince(stateNumber, types, partition);
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

bool Client::loadJsonFile(const QString &fileName)
{
//    QFile file(fileName);
//    if (!file.open(QFile::ReadOnly)) {
//        if (gDebug)
//            qDebug() << "Couldn't load file" << fileName;
//        return false;
//    }
//    JsonReader parser;
//    bool ok = parser.parse(file.readAll());
//    file.close();
//    if (!ok) {
//        std::cout << "Unable to parse the content of the file" << qPrintable(fileName) << ":"
//                  << qPrintable(parser.errorString()) << std::endl;
//        return false;
//    }
//    QVariant arg = parser.result();
//    mRequests << mConnection->create(arg);
    return true;
}
