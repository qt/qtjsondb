/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "chatclient.h"

#include <QDateTime>
#include <QDebug>
#include <QtJsonDb/qjsondbobject.h>
#include <QtJsonDb/QJsonDbReadRequest>
#include <QtJsonDb/QJsonDbWatcher>
#include <QtJsonDb/QJsonDbWriteRequest>

ChatClient::ChatClient(const QString &username, const QString &realName, QObject *parent) :
    QObject(parent)
  , m_username(username)
  , m_name(realName)
  , m_conn(new QtJsonDb::QJsonDbConnection(this))
{
    connect(m_conn, SIGNAL(statusChanged(QtJsonDb::QJsonDbConnection::Status)),
            this, SLOT(statusChanged(QtJsonDb::QJsonDbConnection::Status)));

    m_conn->connectToServer();

    // create an index on time
    QtJsonDb::QJsonDbObject index;
    index.insert(QLatin1String("_type"), QLatin1String("Index"));
    index.insert(QLatin1String("objectType"), QLatin1String("ChatMessage"));
    index.insert(QLatin1String("propertyName"), QLatin1String("time"));

    QtJsonDb::QJsonDbWriteRequest *indexReq = new QtJsonDb::QJsonDbCreateRequest(index, this);
    connect(indexReq, SIGNAL(finished()), indexReq, SLOT(deleteLater()));
    connect(indexReq, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    m_conn->send(indexReq);

    // create a watcher to catch new messages sent to the user
    QtJsonDb::QJsonDbWatcher *messageWatcher = new QtJsonDb::QJsonDbWatcher(this);
    connect(messageWatcher, SIGNAL(notificationsAvailable(int)), this, SLOT(incomingMessage()));
    messageWatcher->setQuery(QString("[?_type=\"ChatMessage\"][?to=\"%1\"][\\time]").arg(m_username));
    m_conn->addWatcher(messageWatcher);

}

void ChatClient::statusChanged(QtJsonDb::QJsonDbConnection::Status newStatus)
{
    if (newStatus == QtJsonDb::QJsonDbConnection::Connected) {
        fprintf(stderr, "Logging in as %s %s\n", qPrintable(m_username),
                qPrintable(!m_name.isEmpty() ? QString("(%1)").arg(m_name) : QString()));
        setStatus();
    }
}

void ChatClient::echo(const QStringList &args)
{
    fprintf(stderr, "%s\n", qPrintable(args.join(QLatin1String(" "))));
}

void ChatClient::help(const QStringList &args)
{
    Q_UNUSED(args);

    QStringList commands;
    commands << "help\t\t\t\tdisplay this help text";
    commands << "quit\t\t\t\tquit the chat client";
    commands << "status status-message\t\tset your status to status-message";
    commands << "who [username]\t\t\tget info for username if provided, otherwise list everyone online";
    commands << "message username message-txt\tsend a message containing message-txt to the user identified by username";
    commands << "history username\t\tprint past messages sent between you and the user identified by username";
    fprintf(stderr, "%s\n", qPrintable(commands.join(QLatin1String("\n"))));
}

void ChatClient::status(const QStringList &args)
{
    if (m_conn->status() != QtJsonDb::QJsonDbConnection::Connected) {
        fprintf(stderr, "Not connected to JSONDB\n");
        return;
    }

    if (!args.isEmpty())
        setStatus(args.join(QLatin1String(" ")));
}

void ChatClient::who(const QStringList &args)
{
    if (m_conn->status() != QtJsonDb::QJsonDbConnection::Connected) {
        fprintf(stderr, "Not connected to JSONDB\n");
        return;
    }

    if (args.isEmpty()) {
        QtJsonDb::QJsonDbReadRequest *listRequest = new QtJsonDb::QJsonDbReadRequest(this);
        connect(listRequest, SIGNAL(finished()), this, SLOT(listWho()));
        connect(listRequest, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
                this, SLOT(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
        listRequest->setPartition(QLatin1String("Ephemeral"));
        listRequest->setQuery(QLatin1String("[?_type=\"ChatStatus\"]"));
        m_conn->send(listRequest);
        return;
    }

    QtJsonDb::QJsonDbReadObjectRequest *request = new QtJsonDb::QJsonDbReadObjectRequest(this);
    connect(request, SIGNAL(finished()), this, SLOT(singleWho()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    request->setPartition(QLatin1String("Ephemeral"));
    request->setUuid(QtJsonDb::QJsonDbObject::createUuidFromString(QString("status%1").arg(args.first())));
    m_conn->send(request);
}

void ChatClient::message(const QStringList &args)
{
    if (m_conn->status() != QtJsonDb::QJsonDbConnection::Connected) {
        fprintf(stderr, "Not connected to JSONDB\n");
        return;
    }

    QStringList messageArgs = args;
    QtJsonDb::QJsonDbObject message;
    message.insert(QLatin1String("_type"), QLatin1String("ChatMessage"));
    message.insert(QLatin1String("from"), m_username);
    message.insert(QLatin1String("to"), messageArgs.takeFirst());
    if (message.value(QLatin1String("from")) == message.value(QLatin1String("to"))) {
        fprintf(stderr, "Talking to oneself is not supported\n");
        return;
    }
    message.insert(QLatin1String("message"), messageArgs.join(QLatin1String(" ")));
    message.insert(QLatin1String("time"), QDateTime::currentDateTime().toString("ddMMyyyy hh:mm:ss"));

    QtJsonDb::QJsonDbWriteRequest *request = new QtJsonDb::QJsonDbCreateRequest(message, this);
    connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    m_conn->send(request);
}

void ChatClient::history(const QStringList &args)
{
    if (args.isEmpty())
        return;

    if (m_conn->status() != QtJsonDb::QJsonDbConnection::Connected) {
        fprintf(stderr, "Not connected to JSONDB\n");
        return;
    }

    QtJsonDb::QJsonDbReadRequest *request = new QtJsonDb::QJsonDbReadRequest(this);
    connect(request, SIGNAL(finished()), this, SLOT(listHistory()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    request->setQuery(QLatin1String("[?_type=\"ChatMessage\"][?to=%self | to=%friend][?from=%self | from=%friend][\\time]"));
    request->bindValue(QLatin1String("self"), m_username);
    request->bindValue(QLatin1String("friend"), args.first());

    m_conn->send(request);
}

void ChatClient::listWho()
{
    QtJsonDb::QJsonDbReadRequest *request = qobject_cast<QtJsonDb::QJsonDbReadRequest*>(sender());
    if (!request)
        return;

    QStringList results;
    QList<QJsonObject> whoResults = request->takeResults();
    request->deleteLater();

    foreach (const QJsonObject &who, whoResults)
        results << who.value(QLatin1String("username")).toString();

    fprintf(stderr, "Users online:\n%s\n", qPrintable(results.join(QLatin1String("\n"))));
}

void ChatClient::singleWho()
{
    QtJsonDb::QJsonDbReadRequest *request = qobject_cast<QtJsonDb::QJsonDbReadRequest*>(sender());
    if (!request)
        return;

    QList<QJsonObject> whoResults = request->takeResults();
    if (whoResults.isEmpty()) {
        fprintf(stderr, "Unknown user\n");
        return;
    }

    QJsonObject who = whoResults.takeFirst();
    request->deleteLater();

    QStringList results;
    results << QString("User: %1").arg(who.value(QLatin1String("username")).toString());
    results << QString("Logged in: %1").arg(who.value(QLatin1String("time")).toString());

    if (who.contains(QLatin1String("name")))
        results << QString("Real name: %1").arg(who.value(QLatin1String("name")).toString());
    if (who.contains(QLatin1String("status")))
        results << QString("Status: %1").arg(who.value(QLatin1String("status")).toString());

    fprintf(stderr, "%s\n", qPrintable(results.join("\n")));
}

void ChatClient::listHistory()
{
    QtJsonDb::QJsonDbReadRequest *request = qobject_cast<QtJsonDb::QJsonDbReadRequest*>(sender());
    if (!request)
        return;

    QList<QJsonObject> history = request->takeResults();
    request->deleteLater();

    if (history.isEmpty()) {
        fprintf(stderr, "No conversation history\n");
        return;
    }

    QStringList results;

    foreach (const QJsonObject &message, history)
        results << messageToString(message);

    fprintf(stderr, "%s\n", qPrintable(results.join("\n")));

}

void ChatClient::incomingMessage()
{
    QtJsonDb::QJsonDbWatcher *watcher = qobject_cast<QtJsonDb::QJsonDbWatcher*>(sender());
    if (!watcher)
        return;

    QList<QtJsonDb::QJsonDbNotification> messages = watcher->takeNotifications();

    QStringList results;
    foreach (const QtJsonDb::QJsonDbNotification &message, messages)
        results << messageToString(message.object());

    fprintf(stderr, "\n%s\njsondb-chat> ", qPrintable(results.join("\n")));
}

void ChatClient::error(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message)
{
    QtJsonDb::QJsonDbRequest *request = qobject_cast<QtJsonDb::QJsonDbRequest*>(sender());
    if (request)
        request->deleteLater();

    fprintf(stderr, "Error: (%d) %s\n", code, qPrintable(message));
}

void ChatClient::setStatus(const QString &statusMessage)
{
    QtJsonDb::QJsonDbWriteRequest *request = new QtJsonDb::QJsonDbWriteRequest(this);
    connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            this, SLOT(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    request->setPartition(QLatin1String("Ephemeral"));
    request->setConflictResolutionMode(QtJsonDb::QJsonDbWriteRequest::Replace);

    QtJsonDb::QJsonDbObject status;
    status.setUuid(QtJsonDb::QJsonDbObject::createUuidFromString(QString("status%1").arg(m_username)));
    status.insert(QLatin1String("_type"), QLatin1String("ChatStatus"));
    status.insert(QLatin1String("username"), m_username);
    status.insert(QLatin1String("time"), QDateTime::currentDateTime().toString("ddMMyyyy hh:mm:ss"));

    if (!m_name.isEmpty())
        status.insert(QLatin1String("name"), m_name);
    if (!statusMessage.isEmpty())
        status.insert(QLatin1String("status"), statusMessage);

    request->setObjects(QList<QJsonObject>() << status);
    m_conn->send(request);
}

QString ChatClient::messageToString(const QJsonObject &message)
{
    return QString("%1: %2 - %3")
            .arg(message.value(QLatin1String("time")).toString())
            .arg(message.value(QLatin1String("from")).toString())
            .arg(message.value(QLatin1String("message")).toString());
}
