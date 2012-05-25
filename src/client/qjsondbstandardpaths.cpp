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

#include "private/qjsondbstandardpaths_p.h"

#include <QtCore/qdir.h>

#include <pwd.h>

QT_BEGIN_NAMESPACE_JSONDB

static bool qtjsondb_autotestMode = false;

QString QJsonDbStandardPaths::homePath(const QString &user)
{
    if (qtjsondb_autotestMode) {
        if (user == QLatin1String("userthatdoesnotexist"))
            return QString();
        return QDir::homePath() + QLatin1String("/.qttest/qtjsondb/") + user;
    }

    struct passwd *pwd = getpwnam(user.toLatin1().constData()); // not thread-safe!
    if (!pwd)
        return QString();

    return QString::fromUtf8(pwd->pw_dir);
}

QString QJsonDbStandardPaths::currentUser()
{
    if (qtjsondb_autotestMode)
        return QStringLiteral("fakeroot");
    return QString::fromLatin1(qgetenv("USER"));
}

void QJsonDbStandardPaths::setAutotestMode(bool value)
{
    qtjsondb_autotestMode = value;
}


QT_END_NAMESPACE_JSONDB
