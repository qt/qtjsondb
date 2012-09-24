/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore>
#include <QCoreApplication>
#include <iostream>
#include "client.h"

using namespace std;

static void usage(const QString &name, int exitCode = 0)
{
    cout << "Usage: " << qPrintable(name) << " [OPTIONS] [command]" << endl
         << endl
         << "    -debug" << endl
         << "    -load FILE               Load the specified .json or .qml file" << endl
         << "    -partition PARTITION     Set the specified partition as the default" << endl
         << "    -terminate               Terminate after processing any -load parameters" << endl
         << endl
         << " where command is valid JsonDb command object" << endl;
    exit(exitCode);
}


int main(int argc, char * argv[])
{
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setOrganizationDomain("qt.nokia.com");
    QCoreApplication::setApplicationName("jclient");
    QCoreApplication::setApplicationVersion("1.0");
    QCoreApplication app(argc, argv);

    QStringList args = QCoreApplication::arguments();
    qDebug() << "args: " << args;
    QString progname = args.takeFirst();
    QStringList command;
    QStringList filesToLoad;
    QString partition;
    bool terminate = false;
    bool debug = false;

    while (args.size()) {
        QString arg = args.takeFirst();
        if (!arg.startsWith("-")) {
            command << arg;
            continue;
        }

        if (arg == QLatin1String("-help")) {
            usage(progname);
        } else if (arg == QLatin1String("-debug")) {
            debug = true;
        } else if (arg == QLatin1String("-load")) {
            if (args.isEmpty()) {
                cout << "Must specify a file to load" << endl;
                usage(progname, 1);
            }
            filesToLoad << args.takeFirst();
        } else if (arg == QLatin1String("-partition")) {
            if (args.isEmpty()) {
                cout << "Must specify a partition" << endl;
                usage(progname, 1);
            }
            partition = args.takeFirst();
        } else if (arg == QLatin1String("-terminate")) {
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

    if (!partition.isEmpty())
        client.setDefaultPartition(partition);

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
