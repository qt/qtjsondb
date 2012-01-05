/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "jsondb-global.h"
#include "jsondb-error.h"

QT_BEGIN_NAMESPACE_JSONDB

/*!
    \class JsonDbError
    \brief The JsonDbError class lists possible error codes.
    \sa JsonDbError::ErrorCode
 */

/*!
     \enum JsonDbError::ErrorCode
     \omitvalue NoError
     \value InvalidMessage
         Unable to parse the query message.
     \value InvalidRequest
         Request object doesn't contain correct elements.
     \value MissingObject
         Invalid or missing "object" field.
     \value DatabaseError
         Error directly from the database.
     \value MissingUUID
         Missing id field.
     \value MissingType
         Missing _type field.
     \value MissingQuery
         Missing query field.
     \value InvalidLimit
         Invalid limit field.
     \value InvalidOffset
         Invalid offset field.
     \value MismatchedNotifyId
         Request to delete notify doesn't match existing notification.
     \value InvalidActions
         List of actions supplied to setNotification is invalid.
     \value UpdatingStaleVersion
         Updating stale version of object.
     \value OperationNotPermitted
         Operation prohibited by access control policy.
     \value QuotaExceeded
         Operation would exceed quota.
     \value FailedSchemaValidation
         Object to be created/updated was invalid according to the schema.
     \value InvalidMap
         The Map definition is invalid.
     \value InvalidReduce
         The Reduce definition is invalid.
     \value InvalidSchemaOperation
         Attempted to create a schema that already exists or to remove a schema when there are still objects belonging to the schema's type.
     \value InvalidPartition
         Invalid partition.
     \value InvalidIndexOperation
         An error when creating an index object
 */

QT_END_NAMESPACE_JSONDB
