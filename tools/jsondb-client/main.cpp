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

#include <QtCore>
#include <QGuiApplication>
#include <iostream>
#include "client.h"

using namespace std;

static void usage(const QString &name, int exitCode = 0)
{
    cout << "Usage: " << qPrintable(name) << " [OPTIONS] [command]" << endl
         << endl
         << "    -debug" << endl
         << "    -load FILE     Load the specified .json or .qml file" << endl
         << "    -terminate     Terminate after processing any -load parameters" << endl
         << endl
         << " where command is valid JsonDb command object" << endl;
    exit(exitCode);
}


int main(int argc, char * argv[])
{
    // Hack to avoid making people specify a platform plugin.
    // We only need QGuiApplication so that we can use QQmlEngine.
    bool platformSpecified = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-platform") == 0)
            platformSpecified = true;
    }

    if (!platformSpecified)
        setenv("QT_QPA_PLATFORM", "minimal", false);

    QGuiApplication::setOrganizationName("Nokia");
    QGuiApplication::setOrganizationDomain("qt.nokia.com");
    QGuiApplication::setApplicationName("jclient");
    QGuiApplication::setApplicationVersion("1.0");
    QGuiApplication app(argc, argv);

    QStringList args = QCoreApplication::arguments();
    qDebug() << "args: " << args;
    QString progname = args.takeFirst();
    QStringList command;
    QStringList filesToLoad;
    bool terminate = false;
    bool debug = false;


    while (args.size()) {
        QString arg = args.takeFirst();
        if (!arg.startsWith("-")) {
            command << arg;
            continue;
        }

        if (arg == QLatin1Literal("-help")) {
            usage(progname);
        } else if (arg == QLatin1Literal("-debug")) {
            debug = true;
        } else if (arg == QLatin1Literal("-load")) {
            if (args.isEmpty()) {
                cout << "Must specify a file to load" << endl;
                usage(progname, 1);
            }
            filesToLoad << args.takeFirst();
        } else if (arg == QLatin1Literal("-terminate")) {
            terminate = true;
        } else {
            cout << "Unknown argument " << qPrintable(arg) << endl;
            usage(progname, 1);
        }
    }

    Client client;
    QObject::connect(&client, SIGNAL(terminate()), &app, SLOT(quit()), Qt::QueuedConnection);
    client.setTerminateOnCompleted(terminate);
    client.setDebug(debug);

    if (!client.connectToServer())
        return 1;

    if (!filesToLoad.isEmpty()) {
        client.loadFiles(filesToLoad);
    } else if (command.size()) {
        client.setTerminateOnCompleted(true);
        if (!client.processCommand(command.join(" ")))
            return 1;
    } else {
        client.interactiveMode();
    }

    return app.exec();
}
