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

#include <QtCore>
#include <QAbstractSocket>
#include <iostream>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "signals.h"
#include "dbserver.h"

QString progname;

QT_ADDON_JSONDB_BEGIN_NAMESPACE

extern bool gUseJsonInDb;

QT_ADDON_JSONDB_END_NAMESPACE

QT_ADDON_JSONDB_USE_NAMESPACE

/***************************************************************************/

void daemonize()
{
    if (::getppid() == 1)
        return;  // Already a daemon if owned by init

    int i = fork();
    if (i < 0) exit(1); // Fork error
    if (i > 0) exit(0); // Parent exits

    ::setsid();  // Create a new process group

    /*
    for (i = ::getdtablesize() ; i > 0 ; i-- )
        ::close(i);   // Close all file descriptors
    i = ::open("/dev/null", O_RDWR); // Stdin
    ::dup(i);  // stdout
    ::dup(i);  // stderr
    */
    ::close(0);
    ::open("/dev/null", O_RDONLY);  // Stdin

//    ::umask(027);
    // ::chdir("/foo");

    QString pidfile = QString("/var/run/%1.pid").arg(QFileInfo(progname).fileName());
    int lfp = ::open(qPrintable(pidfile), O_RDWR|O_CREAT, 0640);
    if (lfp<0)
        qFatal("Cannot open pidfile %s\n", qPrintable(pidfile));
    if (lockf(lfp, F_TLOCK, 0)<0)
        qFatal("Can't get a lock on %s - another instance may be running\n", qPrintable(pidfile));
    QByteArray ba = QByteArray::number(::getpid());
    ::write(lfp, ba.constData(), ba.size());
    ::close(lfp);

    ::signal(SIGCHLD,SIG_IGN);
    ::signal(SIGTSTP,SIG_IGN);
    ::signal(SIGTTOU,SIG_IGN);
    ::signal(SIGTTIN,SIG_IGN);
}

/***************************************************************************/

using namespace std;

static void usage()
{
    cout << "Usage: " << qPrintable(progname) << " [OPTIONS] [FILENAME]" << endl
         << endl
#ifdef Q_OS_LINUX
         << "     -daemon             Run as a daemon process" << endl
#endif
         << "     -tcpPort port       Specify a TCP port to listen on" << endl
#ifndef QT_NO_DEBUG_OUTPUT
         << "     -debug" << endl
         << "     -debug-recovery" << endl
         << "     -performance-log    Print timings of database operations" << endl
#endif
         << "     -verbose" << endl
         << "     -clear              Clear the database on startup" << endl
         << "     -pid pidfilename" << endl
         << "     -load file.json     Load objects from a json file" << endl
         << "     -json" << endl
         << "     -terminate          Terminate after loading files" << endl
         << "     -reject-stale-updates" << endl
         << "     -validate-schemas   Validate schemas of objects on create and update" << endl
         << "     -enforce-access-control " << endl
         << "     -limit megabytes" << endl
         << endl
         << "Arguments:" << endl
         << "  FILENAME      If FILENAME not specified, use default in database.db in the current directory" << endl
         << endl;
    exit(0);
}

static FILE *logstream = 0;
void logMessageOutput(QtMsgType type, const char *msg)
{
    if (!logstream)
        return;
    switch (type) {
    case QtDebugMsg:
        fprintf(logstream, "Debug: %s\n", msg);
        break;
    case QtWarningMsg:
        fprintf(logstream, "Warning: %s\n", msg);
        break;
    case QtCriticalMsg:
        fprintf(logstream, "Critical: %s\n", msg);
        break;
    case QtFatalMsg:
        fprintf(logstream, "Fatal: %s\n", msg);
        abort();
    }
}

int main(int argc, char * argv[])
{
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setOrganizationDomain("nrcc.noklab.com");
    QCoreApplication::setApplicationName("jsondb");
    QCoreApplication::setApplicationVersion("1.0");
    QString arguments;
    QStringList jsonFiles;
    QString pidFileName;
    quint16 port = 0;
    bool clear = false;
    rlim_t limit = 0;
    QString logFileName;
    QCoreApplication app(argc, argv);
    QStringList args = QCoreApplication::arguments();
    bool terminate = false;

    progname = args.takeFirst();
    while (args.size()) {
        QString arg = args.at(0);
        if (!arg.startsWith("-"))
            break;
        args.removeFirst();
        if (arg == "-help") {
            usage();
        } else if (arg == "-tcpPort") {
            if (!args.size())
                usage();
            port = args.takeFirst().toInt();
        } else if (arg == "-limit") {
            if (!args.size())
                usage();
            limit = args.takeFirst().toInt() * 1024*1024;
        } else if (arg == "-pid") {
            if (!args.size())
                usage();
            pidFileName = args.takeFirst();
        } else if (arg == "-load") {
            if (!args.size())
                usage();
            jsonFiles.append(args.takeFirst());
        } else if (arg == "-terminate") {
            terminate = true;
#ifndef QT_NO_DEBUG_OUTPUT
        } else if (arg == "-debug") {
            gDebug = true;
        } else if (arg == "-debug-recovery") {
            gDebugRecovery = true;
#endif
#ifdef Q_OS_LINUX
        } else if ( arg == "-daemon" ) {
            daemonize();
#endif
        } else if (arg == "-validate-schemas") {
            gValidateSchemas = true;
        } else if (arg == "-no-validate-schemas") {
            gValidateSchemas = false;
        } else if (arg == "-reject-stale-updates") {
            gRejectStaleUpdates = true;
        } else if (arg == "-no-reject-stale-updates") {
            gRejectStaleUpdates = false;
        } else if (arg == "-enforce-access-control") {
            gEnforceAccessControlPolicies = true;
        } else if (arg == "-verbose") {
            gVerbose = true;
        } else if (arg == "-clear") {
            clear = true;
#ifndef QT_NO_DEBUG_OUTPUT
        } else if (arg == "-performance-log") {
            gPerformanceLog = true;
#endif
        } else if (arg == "-log-file") {
            logFileName = args.takeFirst();
        } else {
            cout << "Unknown argument " << qPrintable(arg) << endl << endl;
            usage();
        }
    }

    if (!pidFileName.isEmpty()) {
        QFile pidFile(qPrintable(pidFileName));
        pidFile.open(QIODevice::ReadWrite|QIODevice::Truncate);
        QByteArray ba = QByteArray::number(::getpid());
        pidFile.write(ba);
        pidFile.close();
    }

    if (args.size() == 1)
        arguments = args.takeFirst();
    else if (!args.isEmpty())
        usage();

    if (!logFileName.isEmpty()) {
        logstream = fopen(logFileName.toLocal8Bit().data(), "w");
        if (!logstream) {
            qCritical() << QString::fromLocal8Bit("Could not open '%1'").arg(logFileName);
            return errno;
        }
        ::setbuf(logstream, 0);
        qInstallMsgHandler(logMessageOutput);
    }

    DBServer server(arguments);
    if (port)
        server.setTcpServerPort(port);
    Signals handler;
    QObject::connect(&handler, SIGNAL(sigTerm()), &server, SLOT(sigTerm()));
    QObject::connect(&handler, SIGNAL(sigHUP()), &server, SLOT(sigHUP()));
    QObject::connect(&handler, SIGNAL(sigINT()), &server, SLOT(sigINT()));
    handler.start();

    if (limit) {
        struct rlimit rlim;
        getrlimit(RLIMIT_AS, &rlim);
        qDebug() << "getrlimit" << "RLIMIT_DATA" << rlim.rlim_cur;
        rlim.rlim_cur = limit;
        rlim.rlim_max = limit;
        int rc = setrlimit(RLIMIT_AS, &rlim);
        if (rc)
            qWarning() << "Failed to setrlimit" << errno;
    }

    if (clear)
        server.clear();

    if (!server.socket())
        return -1;

    cout << "Ready" << endl << flush;

    if (!server.start())
        return -2;

    if (jsonFiles.size()) {
        foreach (QString jsonFile, jsonFiles) {
            bool ok = server.load(jsonFile);
            if (!ok) {
                qCritical() << "Failed to load" << jsonFile;
                return -1;
            }
        }
    }
    if (terminate)
        return 0;
    return app.exec();
}
