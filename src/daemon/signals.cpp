/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <QSocketNotifier>
#include <QDebug>
#include "signals.h"

int Signals::sSigFD[2];

Signals::Signals( QObject *parent )
    : QObject(parent)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sSigFD))
	qFatal("Unable to create signal socket pair");

    mNotifier = new QSocketNotifier(sSigFD[1], QSocketNotifier::Read, this);
    connect(mNotifier, SIGNAL(activated(int)), this, SLOT(handleSig()));
}

void Signals::start()
{
    struct sigaction action;

    if (receivers(SIGNAL(sigTerm())) > 0) {
	action.sa_handler = Signals::signalHandler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_flags |= SA_RESTART;

	if (::sigaction(SIGTERM, &action, 0) < 0)
	    qFatal("Unable to set sigaction on TERM");
    }

    if (receivers(SIGNAL(sigHUP())) > 0) {
	action.sa_handler = Signals::signalHandler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_flags |= SA_RESTART;

	if (::sigaction(SIGHUP, &action, 0) < 0)
	    qFatal("Unable to set sigaction on HUP");
    }

    if (receivers(SIGNAL(sigINT())) > 0) {
	action.sa_handler = Signals::signalHandler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_flags |= SA_RESTART;

	if (::sigaction(SIGINT, &action, 0) < 0)
	    qFatal("Unable to set sigaction on INT");
    }
}

void Signals::signalHandler(int number)
{
    int tmp = number;
    ::write(sSigFD[0], &tmp, sizeof(tmp));
}

void Signals::handleSig()
{
    mNotifier->setEnabled(false);
    int tmp;
    ::read(sSigFD[1], &tmp, sizeof(tmp));
    switch (tmp) {
    case SIGTERM:
	emit sigTerm();
	break;
    case SIGHUP:
	emit sigHUP();
	break;
    case SIGINT:
	emit sigINT();
	break;
    default:
	break;
    }
    mNotifier->setEnabled(true);
}


