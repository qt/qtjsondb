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

#include "aodb.h"
#include <iostream>
#include <QCoreApplication>
#include <QStringList>
#include <QDebug>

QString progname;
bool gCompact = false;

/***************************************************************************/

using namespace std;

static void usage()
{
    cout << "Usage: " << qPrintable(progname) << " [OPTIONS] [<.db filename>]" << endl
         << endl << "OPTIONS:" << endl
         << "-compact\tcompacts the db file before dumping." << endl << endl;
    exit(0);
}


int main(int argc, char * argv[])
{
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setOrganizationDomain("nrcc.noklab.com");
    QCoreApplication::setApplicationName("adb-dump");
    QCoreApplication::setApplicationVersion("1.0");

    QCoreApplication app(argc, argv);
    QStringList args = QCoreApplication::arguments();

    progname = args.takeFirst();
    while (args.size()) {
        QString arg = args.at(0);
        if (!arg.startsWith("-"))
            break;
        args.removeFirst();
        if ( arg == "-help")
            usage();
        else if ( arg == "-compact")
            gCompact = true;
        /*else if (arg == "-load") {
            if (args.isEmpty()) {
                cout << "Invalid argument " << qPrintable(arg) << endl;
                usage();
                return 0;
            }
            loadFile = args.at(0);
            args.removeFirst();
        }*/ else {
            cout << "Unknown argument " << qPrintable(arg) << endl;
            usage();
            return 0;
        }
    }

    AoDb aoDB;
    while (args.size()) {
        QString adbFileName = args.takeFirst();
        qDebug();
        if (gCompact) {
            qDebug() << "Compacting...";
            aoDB.open(adbFileName, AoDb::NoSync);
            aoDB.compact();
        }
        else
            aoDB.open(adbFileName, AoDb::ReadOnly|AoDb::NoSync);

        qDebug () << "Dumping file: " << adbFileName;
        qDebug () << "=============================================================";
        aoDB.dump();
        qDebug (); qDebug ();
        aoDB.close();
    }

    return 0;
}
