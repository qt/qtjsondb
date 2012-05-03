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

#ifndef JSONDB_UTIL_H
#define JSONDB_UTIL_H

#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>
#include <QLocalSocket>
#include <qtestsystem.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>

inline QString findFile(const QString &filename)
{
    QString file = ":/json/" + filename;
    if (QFile::exists(file))
    {
        return file;
    }

    file = QCoreApplication::applicationDirPath() + QDir::separator() + filename;
    if (QFile::exists(file))
        return file;

    file = QDir::currentPath() + QDir::separator() + filename;
    if (QFile::exists(file))
        return file;
    return "";
}

inline QString findFile(const char *filename)
{
    return findFile(QString::fromLocal8Bit(filename));
}

inline QJsonValue readJsonFile(const QString &filename, QJsonParseError *error = 0)
{
    QString filepath = filename;
    QFile jsonFile(filepath);
    if (!jsonFile.exists()) {
        if (error) {
            error->error = QJsonParseError::MissingObject;
            error->offset = 0;
        }
        return QJsonValue();
    }
    jsonFile.open(QIODevice::ReadOnly);
    QByteArray json = jsonFile.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(json, error));
    return doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
}

inline QProcess *launchJsonDbDaemon(const char *prefix, const QString &socketName, const QStringList &args, const char *sourceFile)
{
    static bool dontlaunch = qgetenv("AUTOTEST_DONT_LAUNCH_JSONDB").toInt() == 1;
    static bool useValgrind = qgetenv("AUTOTEST_VALGRIND_JSONDB").toInt() == 1;
    if (dontlaunch)
        return 0;

    QString configfile = QTest::qFindTestData("partitions.json", sourceFile);
    if (configfile.isEmpty()) {
        qDebug() << "Cannot find partitions.json configuration file for jsondb";
        return 0;
    }

    QString jsondb_app = QString::fromLocal8Bit(prefix) + QDir::separator() + "jsondb";
    if (!QFile::exists(jsondb_app))
        jsondb_app = QLatin1String("jsondb"); // rely on the PATH

    QProcess *process = new QProcess;
    process->setProcessChannelMode( QProcess::ForwardedChannels );

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("JSONDB_SOCKET", socketName);
    process->setProcessEnvironment(env);
    ::setenv("JSONDB_SOCKET", qPrintable(socketName), 1);

    QStringList argList(args);
    argList << QLatin1String("-reject-stale-updates");
    argList << QLatin1String("-config-path") << QFileInfo(configfile).absolutePath().toLocal8Bit();

    qDebug() << "Starting process" << jsondb_app << argList << "with socket" << socketName;

    if (useValgrind) {
        QStringList args1 = argList;
        args1.prepend(jsondb_app);
        process->start("valgrind", args1);
    } else {
        process->start(jsondb_app, argList);
    }
    if (!process->waitForStarted())
        qFatal("Unable to start jsondb database process");

    /* Wait until the jsondb is accepting connections */
    int tries = 0;
    bool connected = false;
    while (!connected && tries++ < 100) {
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

inline qint64 launchJsonDbDaemonDetached(const char *prefix, const QString &socketName, const QStringList &args, const char *sourceFile)
{
    static bool dontlaunch = qgetenv("AUTOTEST_DONT_LAUNCH_JSONDB").toInt() == 1;
    static bool useValgrind = qgetenv("AUTOTEST_VALGRIND_JSONDB").toInt() == 1;
    if (dontlaunch)
        return 0;

    QString configfile = QTest::qFindTestData("partitions.json", sourceFile);
    if (configfile.isEmpty()) {
        qDebug() << "Cannot find partitions.json configuration file for jsondb";
        return 0;
    }

    QString jsondb_app = QString::fromLocal8Bit(prefix) + QDir::separator() + "jsondb";
    if (!QFile::exists(jsondb_app))
        jsondb_app = QLatin1String("jsondb"); // rely on the PATH

    ::setenv("JSONDB_SOCKET", qPrintable(socketName), 1);
    QStringList argList(args);
    argList << QLatin1String("-reject-stale-updates");
    argList << QLatin1String("-config-path") << QFileInfo(configfile).absolutePath().toLocal8Bit();

    qDebug() << "Starting process" << jsondb_app << argList << "with socket" << socketName;
    qint64 pid;
    if (useValgrind) {
        QStringList args1 = argList;
        args1.prepend(jsondb_app);
        QProcess::startDetached ( jsondb_app, args1, QDir::currentPath(), &pid );
    } else {
        QProcess::startDetached ( jsondb_app, argList, QDir::currentPath(), &pid );
    }

    /* Wait until the jsondb is accepting connections */
    int tries = 0;
    bool connected = false;
    while (!connected && tries++ < 100) {
        QLocalSocket socket;
        socket.connectToServer(socketName);
        if (socket.waitForConnected())
            connected = true;
        QTest::qWait(250);
    }
    if (!connected)
        qFatal("Unable to connect to jsondb process");
    return pid;
}

#endif // JSONDB_UTIL_H
