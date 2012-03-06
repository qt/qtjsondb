/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
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
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
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

#ifndef JSONDB_CHATCLIENT_H
#define JSONDB_CHATCLIENT_H

#include <QObject>
#include <QStringList>
#include <QtJsonDb/QJsonDbConnection>
#include <QtJsonDb/QJsonDbRequest>

class ChatClient : public QObject
{
    Q_OBJECT
public:
    ChatClient(const QString &username, const QString &realName, QObject *parent = 0);

public Q_SLOTS:
    void echo(const QStringList &args);
    void help(const QStringList &args);
    void status(const QStringList &args);
    void who(const QStringList &args);
    void message(const QStringList &args);
    void history(const QStringList &args);

private Q_SLOTS:
    void statusChanged(QtJsonDb::QJsonDbConnection::Status newStatus);
    void error(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message);
    void listWho();
    void singleWho();
    void listHistory();
    void incomingMessage();

private:
    void setStatus(const QString &statusMessage = QString());
    QString messageToString(const QJsonObject &message);

    QString m_username;
    QString m_name;
    QtJsonDb::QJsonDbConnection *m_conn;
};

#endif // JSONDB_CHATCLIENT_H
