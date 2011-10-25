/****************************************************************************
**
** Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QThread>
#include <QStringList>
#include <QLocalSocket>
#include <QSocketNotifier>
#include <QFile>

#include <histedit.h>

#include "jsondb-client.h"



class InputThread : public QThread {
    Q_OBJECT
public:
    static InputThread *instance();
    ~InputThread();
    void run();
    void async_print(const QString &);
    static void print(const QString &);
    static char const* prompt(EditLine *e);
    static unsigned char console_tabkey(EditLine * el, int ch);
signals:
    void commandReceived(QString);
private:
    EditLine *el;
    History *hist;
    QString historyFile;
    InputThread(QObject *parent = 0) : QThread(parent), el(0), hist(0) {}
    static QString longestCommonPrefix(const QStringList &list);
    static InputThread *threadInstance;
    static const char *commands[];
};

class Client : public QObject
{
    Q_OBJECT
public:
    Client(QObject *parent = 0);
    ~Client();

    bool connectToServer();
    void interactiveMode();

public slots:
    bool processCommand(const QString &);  // true if we're waiting for a response
    bool loadJsonFile(const QString &fileName);

protected slots:
    void disconnected();

    void response(int, const QVariant &map);
    void error(int id, int code, const QString &message);
    void notified(const QString &notify_uuid, const QVariant &object, const QString &action);

signals:
    void requestsProcessed();

private:
    void usage();

    QSocketNotifier *mNotifier;
    QFile           *mInput;
    QtAddOn::JsonDb::JsonDbClient *mConnection;
    QSet<int> mRequests;
    InputThread *mInputThread;
};


#endif // CLIENT_H
