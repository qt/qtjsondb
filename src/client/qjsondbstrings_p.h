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

#ifndef JSONDB_STRINGS_P_H
#define JSONDB_STRINGS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtJsonDb API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtJsonDb/QtJsonDbGlobal>

QT_BEGIN_NAMESPACE_JSONDB

namespace JsonDbStrings {

class Protocol
{
public:
    static inline const QString action() { return QStringLiteral("action"); }
    static inline const QString create() { return QStringLiteral("create"); }
    static inline const QString update() { return QStringLiteral("update"); }
    static inline const QString remove() { return QStringLiteral("remove"); }
    static inline const QString query() { return QStringLiteral("find"); }
    static inline const QString token() { return QStringLiteral("token"); }
    static inline const QString changesSince() { return QStringLiteral("changesSince"); }
    static inline const QString object() { return QStringLiteral("object"); }
    static inline const QString partition() { return QStringLiteral("partition"); }
    static inline const QString requestId() { return QStringLiteral("id"); }
    static inline const QString result() { return QStringLiteral("result"); }
    static inline const QString data() { return QStringLiteral("data"); }
    static inline const QString changes() { return QStringLiteral("changes"); }
    static inline const QString count() { return QStringLiteral("count"); }
    static inline const QString stateNumber() { return QStringLiteral("stateNumber"); }
    static inline const QString error() { return QStringLiteral("error"); }
    static inline const QString errorCode() { return QStringLiteral("code"); }
    static inline const QString errorMessage() { return QStringLiteral("message"); }
    static inline const QString types() { return QStringLiteral("types"); }
};

class Property
{
public:
    static inline const QString type() { return QStringLiteral("_type"); }
    static inline const QString uuid() { return QStringLiteral("_uuid"); }
    static inline const QString version() { return QStringLiteral("_version"); }
    static inline const QString deleted() { return QStringLiteral("_deleted"); }
    static inline const QString notify() { return QStringLiteral("notify"); }
    static inline const QString query() { return QStringLiteral("query"); }
    static inline const QString queryLimit() { return QStringLiteral("limit"); }
    static inline const QString queryOffset() { return QStringLiteral("offset"); }
    static inline const QString actions() { return QStringLiteral("actions"); }
    static inline const QString bindings() { return QStringLiteral("bindings"); }
    static inline const QString state() { return QStringLiteral("state"); }
    static inline const QString sortKey() { return QStringLiteral("sortKey"); }
    static inline const QString startingStateNumber() { return QStringLiteral("startingStateNumber"); }
    static inline const QString currentStateNumber() { return QStringLiteral("currentStateNumber"); }
};

class Types
{
public:
    static inline const QString notification() { return QStringLiteral("notification"); }
};

class Notification
{
public:
    static inline const QString actionCreate() { return QStringLiteral("create"); }
    static inline const QString actionUpdate() { return QStringLiteral("update"); }
    static inline const QString actionRemove() { return QStringLiteral("remove"); }
};

} // namespace JsonDbStrings
QT_END_NAMESPACE_JSONDB

#endif // JSONDB_STRINGS_P_H
