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

#include <QtCore>
#include <iostream>
#include "client.h"

QString progname;
bool gDebug;

/***************************************************************************/

using namespace std;

static void usage()
{
    cout << "Usage: " << qPrintable(progname) << " [OPTIONS] [command]" << endl
         << endl
         << "    -debug" << endl
         << "    -load file.json     Load objects from a json file" << endl
         << endl
         << " where command is valid JsonDb command object" << endl;
    exit(0);
}


int main(int argc, char * argv[])
{
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setOrganizationDomain("nrcc.noklab.com");
    QCoreApplication::setApplicationName("jclient");
    QCoreApplication::setApplicationVersion("1.0");

    QCoreApplication app(argc, argv);
    QStringList args = QCoreApplication::arguments();
    QString loadFile;

    progname = args.takeFirst();
    while (args.size()) {
        QString arg = args.at(0);
        if (!arg.startsWith("-"))
            break;
        args.removeFirst();
        if ( arg == "-help")
            usage();
        else if ( arg == "-debug")
            gDebug = true;
        else if (arg == "-load") {
            if (args.isEmpty()) {
                cout << "Invalid argument " << qPrintable(arg) << endl;
                usage();
                return 0;
            }
            loadFile = args.at(0);
            args.removeFirst();
        } else {
            cout << "Unknown argument " << qPrintable(arg) << endl;
            usage();
            return 0;
        }
    }

    Client client;
    if (!client.connectToServer())
        return 0;

    bool interactive = !args.size();
    if (!interactive)
        QObject::connect(&client, SIGNAL(requestsProcessed()), &app, SLOT(quit()));

    if (!loadFile.isEmpty()) {
        client.loadJsonFile(loadFile);
    } else if (!args.size()) {
        client.interactiveMode();
    } else {
        if (!client.processCommand(args.join(" ")))
            return 0;
    }
    return app.exec();
}
