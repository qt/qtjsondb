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

#ifndef JSONDB_STRINGS_H
#define JSONDB_STRINGS_H

#include <QString>
#include "jsondb-global.h"

QT_ADDON_JSONDB_BEGIN_NAMESPACE

class Q_ADDON_JSONDB_EXPORT JsonDbString {
public:
    static const QString kActionStr;
    static const QString kActionsStr;
    static const QString kActiveStr;
    static const QString kAddIndexStr;
    static const QString kCodeStr;
    static const QString kConnectStr;
    static const QString kCountStr;
    static const QString kCreateStr;
    static const QString kDropStr;
    static const QString kCurrentStr;
    static const QString kDataStr;
    static const QString kDeletedStr;
    static const QString kDisconnectStr;
    static const QString kDomainStr;
    static const QString kErrorStr;
    static const QString kExplanationStr;
    static const QString kFieldNameStr;
    static const QString kFindStr;
    static const QString kNameStr;
    static const QString kIdStr;
    static const QString kLengthStr;
    static const QString kLimitStr;
    static const QString kMapTypeStr;
    static const QString kMessageStr;
    static const QString kNotifyStr;
    static const QString kNotificationTypeStr;
    static const QString kObjectStr;
    static const QString kParentStr;
    static const QString kOffsetStr;
    static const QString kOwnerStr;
    static const QString kQueryStr;
    static const QString kReduceTypeStr;
    static const QString kRemoveStr;
    static const QString kResultStr;
    static const QString kSchemaStr;
    static const QString kSchemaTypeStr;
    static const QString kTypeStr;
    static const QString kTypesStr;
    static const QString kUpdateStr;
    static const QString kUuidStr;
    static const QString kVersionStr;
    static const QString kViewTypeStr;
    static const QString kTokenStr;
    static const QString kSettingsStr;
    static const QString kChangesSinceStr;
    static const QString kStateNumberStr;
    static const QString kCollapsedStr;
    static const QString kCurrentStateNumberStr;
    static const QString kStartingStateNumberStr;
    static const QString kTombstoneStr;
    static const QString kPartitionTypeStr;
    static const QString kPartitionStr;

    static const QString kSystemPartitionName;
    static const QString kEphemeralPartitionName;
};

QT_ADDON_JSONDB_END_NAMESPACE

#endif /* JSONDB-STRINGS_H */
