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

#ifndef JSONDB_ERRORS_H
#define JSONDB_ERRORS_H

#include "jsondb-global.h"

QT_BEGIN_HEADER

namespace QtAddOn { namespace JsonDb {

class JsonDbError {
public:
    enum ErrorCode {
        NoError = 0,
        InvalidMessage,   // Unable to parse the query message
        InvalidRequest,   // Request object doesn't contain correct elements
        MissingObject,    // Invalid or missing "object" field
        DatabaseError,    // Error directly from the database
        MissingUUID,      // Missing id field
        MissingType,      // Missing _type field
        MissingQuery,     // Missing query field
        InvalidLimit,     // Invalid limit field
        InvalidOffset,    // Invalid offset field
        MismatchedNotifyId,   // Request to delete notify doesn't match existing notification
        InvalidActions,       // List of actions supplied to setNotification is invalid
        UpdatingStaleVersion, // Updating stale version of object
        OperationNotPermitted,
        QuotaExceeded,
        FailedSchemaValidation, // Invalid according to the schema
        InvalidMap,             // The Map definition is invalid
        InvalidReduce,          // The Reduce definition is invalid
        InvalidSchemaOperation,
        InvalidPartition,
        InvalidIndexOperation
    };
};

} } // namespace

QT_END_HEADER

#endif // JSONDB_ERRORS_H
