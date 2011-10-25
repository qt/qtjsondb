/*
 * Copyright (C) 2010 Nokia Corporation
 */

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
