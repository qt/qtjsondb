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

#ifndef UTIL_H
#define UTIL_H

#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>
#include <QLocalSocket>
#include <qtestsystem.h>

inline QString findFile(const char *srcdir, const QString &filename)
{
    QString file = QString::fromLocal8Bit(srcdir) + QDir::separator() + filename;
    if (QFile::exists(file))
        return file;
    file = QCoreApplication::arguments().at(0) + QDir::separator() + filename;
    if (QFile::exists(file))
        return file;
    return QDir::currentPath() + QDir::separator() + filename;
}

inline QString findFile(const char *srcdir, const char *filename)
{
    return findFile(srcdir, QString::fromLocal8Bit(filename));
}

inline QProcess *launchJsonDbDaemon(const char *prefix, const QString &socketName, const QStringList &args)
{
    static bool dontlaunch = qgetenv("AUTOTEST_DONT_LAUNCH_JSONDB").toInt() == 1;
    if (dontlaunch)
        return 0;
    QString jsondb_app = QString::fromLocal8Bit(prefix) + QDir::separator() + "jsondb";
    if (!QFile::exists(jsondb_app))
        jsondb_app = QLatin1String("jsondb"); // rely on the PATH

    QProcess *process = new QProcess;
    process->setProcessChannelMode( QProcess::ForwardedChannels );

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("JSONDB_SOCKET", socketName);
    process->setProcessEnvironment(env);
    ::setenv("JSONDB_SOCKET", qPrintable(socketName), 1);
    qDebug() << "Starting process" << jsondb_app << args << "with socket" << socketName;
    process->start(jsondb_app, args);

    if (!process->waitForStarted())
        qFatal("Unable to start jsondb database process");

    /* Wait until the jsondb is accepting connections */
    int tries = 0;
    bool connected = false;
    while (!connected && tries++ < 10) {
        QLocalSocket socket;
        socket.connectToServer(socketName);
        if (socket.waitForConnected())
            connected = true;
        QTest::qWait(250);
    }
    if (!connected)
        qFatal("Unable to connect to jsondb process");
    return process;
}

#endif // UTIL_H
