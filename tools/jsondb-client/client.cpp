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

#include <QSocketNotifier>
#include <QCoreApplication>
#include <QStringBuilder>
#include <QMetaObject>
#include <QThread>
#include <QVariant>
#include <QDir>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "json.h"

#include "client.h"

Q_USE_JSONDB_NAMESPACE

extern bool gDebug;

const char* InputThread::commands[] = { "changesSince",
                                        "create {\"",
                                        "find",
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
    if (socketName.isEmpty()) {
        mConnection = new QtAddOn::JsonDb::JsonDbClient(this);
    } else {
        mConnection = new QtAddOn::JsonDb::JsonDbClient(socketName, this);
    }

    connect(mConnection, SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(mConnection, SIGNAL(response(int,QVariant)),
            this, SLOT(response(int,QVariant)));
    connect(mConnection, SIGNAL(error(int,int,QString)),
            this, SLOT(error(int,int,QString)));
    connect(mConnection, SIGNAL(notified(QString,QVariant,QString)),
            this, SLOT(notified(QString,QVariant,QString)));
    connect(mConnection, SIGNAL(statusChanged()), this, SLOT(statusChanged()));

    if (!mConnection->isConnected())
        qCritical() << "Not connected to the server yet... retrying";

    return true;
}

void Client::interactiveMode()
{
    mInputThread = InputThread::instance();
    connect(mInputThread, SIGNAL(commandReceived(QString)), this, SLOT(processCommand(QString)));
    connect(mInputThread, SIGNAL(finished()), QCoreApplication::instance(), SLOT(quit()));
    mInputThread->start();
}

void Client::disconnected()
{
    qCritical() << "Lost connection to the server";
}

void Client::notified(const QString &notify_uuid, const QVariant &object, const QString &action)
{
    JsonWriter writer;
    writer.setAutoFormatting(true);
    QString buf = writer.toString(object);

    QString message =  "Received notification: type " % action
              % " for " % notify_uuid % " object:\n" % buf;
    InputThread::print(message);

    if (!mInputThread)
        QCoreApplication::exit(0);  // Non-interactive mode just stops
}

void Client::statusChanged()
{
    switch (mConnection->status()) {
    case JsonDbClient::Ready:
        qCritical() << "Connected to the server";
        break;
    case JsonDbClient::Error:
        qCritical() << "Cannot connect to the server";
        break;
    default:
        return;
    }
}

void Client::response(int id, const QVariant &msg)
{
    Q_UNUSED(id);
    JsonWriter writer;
    writer.setAutoFormatting(true);
    QString buf = writer.toString(msg);

    QString message = "Received message: " % buf;
    InputThread::print(message);

    mRequests.remove(id);
    if (mRequests.isEmpty())
        emit requestsProcessed();

    if (!mInputThread)
        QCoreApplication::exit(0);  // Non-interactive mode just stops
}

void Client::error(int, int code, const QString &msg)
{
    QString message = "Received error " % QString().setNum(code) % ":" % msg;
    InputThread::print(message);

    if (!mInputThread)
        QCoreApplication::exit(0);  // Non-interactive mode just stops
}

void Client::usage()
{
    std::stringstream out;
    out << "Valid commands:" << std::endl
              << std::endl
              << "Direct database commands - these take an explict object" << std::endl
              << "   create [partition:<name>] OBJECT" << std::endl
              << "   update [partition:<name>] OBJECT" << std::endl
              << "   remove [partition:<name>] OBJECT" << std::endl
              << "   remove [partition:<name>] QUERY" << std::endl
              << "   find [partition:<name>] QUERY" << std::endl
              << "   changesSince [partition:<name>] STATENUMBER [type1 type2 ...]" << std::endl
              << std::endl
              << "Convenience functions" << std::endl
              << "   query STRING [offset [limit]]" << std::endl
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
        int offset = 0, limit = -1;
        int idx = rest.lastIndexOf(']');
        if (idx != -1) {
            QStringList list = rest.mid(idx+1).split(' ');
            int i = 0;
            for (; i < list.size(); ++i) {
                if (!list.at(i).trimmed().isEmpty()) {
                    offset = list.at(i).toInt();
                    break;
                }
            }
            for (++i; i < list.size(); ++i) {
                if (!list.at(i).trimmed().isEmpty()) {
                    limit = list.at(i).toInt();
                    break;
                }
            }
            rest.truncate(idx+1);
        }
        if (gDebug)
            qDebug() << "Sending query:" << QVariant(rest);
        mRequests << mConnection->query(rest, offset, limit, partition);
    } else if (cmd == "notify") {
        int s = rest.indexOf(' ');
        if (s <= 0)
            return false;
        QStringList alist = rest.left(s).split(QRegExp(","), QString::SkipEmptyParts);
        QtAddOn::JsonDb::JsonDbClient::NotifyTypes actions;
        foreach (const QString &s, alist) {
            JsonDbClient::NotifyType type = JsonDbClient::NotifyType(0);
            if (s == QLatin1String("create"))
                type = JsonDbClient::NotifyCreate;
            if (s == QLatin1String("remove"))
                type = JsonDbClient::NotifyRemove;
            if (s == QLatin1String("update"))
                type = JsonDbClient::NotifyUpdate;
            if (type == JsonDbClient::NotifyType(0)) {
                InputThread::print("uknown notification type" % s);
                return false;
            }
            actions |= type;
        }
        QString query = rest.mid(s+1).trimmed();
        if (gDebug)
            qDebug() << "Creating notification:" << alist << ":" << query;
        mRequests << mConnection->notify(actions, query, partition);
    } else if (cmd == "remove") {
        rest = rest.trimmed();
        bool isquery = false;
        int i = 0;
        const int len = rest.length();
        if (i < len) {
            if (rest.at(i++) == QLatin1Char('[')) {
                while (i < len && rest.at(i) == QLatin1Char(' ')) ++i;
                if (i < len && rest.at(i) == QLatin1Char('?'))
                    isquery = true;
            }
        }
        if (isquery) {
            // if not json format, then it is a query
            if (gDebug)
                qDebug() << "Sending remove for:" << rest;
            mRequests << mConnection->remove(rest);
        } else {
            JsonReader parser;
            bool ok = parser.parse(rest);
            if (!ok) {
                InputThread::print("Unable to parse: " % rest);
                usage();
                return false;
            }
            QVariant arg = parser.result();
            if (gDebug)
                qDebug() << "Sending remove:" << arg;
            mRequests << mConnection->remove(arg, partition);
        }
    } else if (cmd == "create" || cmd == "update" || cmd == "find" ) {
        JsonReader parser;
        bool ok = parser.parse(rest);
        if (!ok) {
            InputThread::print("Unable to parse: " % rest);
            usage();
            return false;
        }
        QVariant arg = parser.result();
        if (gDebug)
            qDebug() << "Sending" << cmd << ":" << arg;
        int id = 0;
        QMetaObject::invokeMethod(mConnection, cmd.toLatin1(), Q_RETURN_ARG(int, id),
                                  Q_ARG(QVariant, arg), Q_ARG(QString, partition));
        mRequests << id;
    } else if (cmd == "changesSince") {
        int stateNumber = 0;
        QStringList types;
        QStringList args = rest.split(" ");

        if (args.isEmpty()) {
            InputThread::print("Must specify the state number");
            usage();
            return false;
        }

        stateNumber = args.takeFirst().trimmed().toInt();

        if (!args.isEmpty())
            types = args;

        if (gDebug)
            qDebug() << "Sending changesSince: " << stateNumber << "types: " << types;

        mRequests << mConnection->changesSince(stateNumber, types, partition);
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
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly)) {
        if (gDebug)
            qDebug() << "Couldn't load file" << fileName;
        return false;
    }
    JsonReader parser;
    bool ok = parser.parse(file.readAll());
    file.close();
    if (!ok) {
        std::cout << "Unable to parse the content of the file" << qPrintable(fileName) << ":"
                  << qPrintable(parser.errorString()) << std::endl;
        return false;
    }
    QVariant arg = parser.result();
    mRequests << mConnection->create(arg);
    return true;
}
