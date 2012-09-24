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

#ifndef JSONDB_ERRORS_H
#define JSONDB_ERRORS_H

#include "jsondbpartitionglobal.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class Q_JSONDB_PARTITION_EXPORT JsonDbError {
public:
    enum ErrorCode {
        // common errors
        NoError = 0,
        InvalidRequest = 1,
        OperationNotPermitted = 2,
        InvalidPartition = 3,
        DatabaseConnectionError = 4,
        PartitionUnavailable = 5,

        // read / notify errors
        MissingQuery = 6,
        InvalidMessage= 7,
        InvalidLimit = 8,
        InvalidOffset = 9,
        InvalidStateNumber = 10,

        // write errors
        MissingObject = 11,
        DatabaseError = 12,
        MissingUUID = 13,
        MissingType = 14,
        UpdatingStaleVersion = 15,
        FailedSchemaValidation = 16,
        InvalidMap = 17,
        InvalidReduce = 18,
        InvalidSchemaOperation = 19,
        InvalidIndexOperation = 20,
        InvalidType = 21,
        FlushFailed = 22,
        StorageProblem = 23,
        OutOfSpace = 24
    };
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_ERRORS_H
