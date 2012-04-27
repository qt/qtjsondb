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

#ifndef CLIENT_H
#define CLIENT_H

#include <QtCore>
#include <QtJsonDb>

#include <histedit.h>

QT_BEGIN_NAMESPACE
class QQmlEngine;
QT_END_NAMESPACE

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
    void loadFiles(const QStringList &files);

    inline void setDefaultPartition(const QString &partition) { mDefaultPartition = partition; }
    inline void setTerminateOnCompleted(bool terminate) { mTerminate = terminate; }
    inline void setDebug(bool debug) { mDebug = debug; }

public slots:
    bool processCommand(const QString &);  // true if we're waiting for a response

protected slots:
    void error(QtJsonDb::QJsonDbConnection::ErrorCode, const QString &message);
    void statusChanged(QtJsonDb::QJsonDbConnection::Status);

    void onNotificationsAvailable(int);
    void onNotificationStatusChanged(QtJsonDb::QJsonDbWatcher::Status);
    void onNotificationError(QtJsonDb::QJsonDbWatcher::ErrorCode, const QString &);

    void onRequestFinished();
    void onRequestError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message);
    void aboutToRemove();

    void pushRequest(QtJsonDb::QJsonDbRequest *);
    void popRequest();

    void fileLoadError();
    void fileLoadSuccess();

Q_SIGNALS:
    void terminate();

private:
    void usage();

    void loadNextFile();
    void loadJsonFile(const QString &jsonFile);
    void loadQmlFile(const QString &qmlFile);
#ifndef JSONDB_NO_DEPRECATED
    void loadJavaScriptFile(const QString &jsFile);
#endif

    QSocketNotifier *mNotifier;
    QtJsonDb::QJsonDbConnection *mConnection;
    QList<QtJsonDb::QJsonDbRequest *> mRequests;
    InputThread *mInputThread;
    bool mTerminate;
    bool mDebug;
    QStringList mFilesToLoad;
    QString mDefaultPartition;
    QQmlEngine *mEngine;
};


#endif // CLIENT_H
