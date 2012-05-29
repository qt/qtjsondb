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

#include "qjsondbprivatepartition_p.h"
#include "qjsondbconnection_p.h"
#include "qjsondbrequest_p.h"
#include "qjsondbstrings_p.h"
#include "qjsondbstandardpaths_p.h"

#include "jsondbowner.h"
#include "jsondbpartition.h"
#include "jsondbquery.h"
#include "jsondbqueryparser.h"

#include <qdir.h>

QT_BEGIN_NAMESPACE_JSONDB

QJsonDbPrivatePartition::QJsonDbPrivatePartition(QObject *parent)
    : QObject(parent), partitionOwner(0), privatePartition(0)
{
}

QJsonDbPrivatePartition::~QJsonDbPrivatePartition()
{
}

void QJsonDbPrivatePartition::handleRequest(const QJsonObject &request)
{
    QString partitionName = request.value(JsonDbStrings::Protocol::partition()).toString();
    int requestId = static_cast<int>(request.value(JsonDbStrings::Protocol::requestId()).toDouble());

    QString errorMessage;
    QJsonDbRequest::ErrorCode errorCode = ensurePartition(partitionName, errorMessage);

    if (errorCode == QJsonDbRequest::NoError) {
        QList<QJsonObject> results;

        if (request.value(JsonDbStrings::Protocol::action()).toString() == JsonDbStrings::Protocol::update()) {
            Partition::JsonDbObjectList objects;
            QJsonArray objectArray = request.value(JsonDbStrings::Protocol::object()).toArray();
            if (objectArray.isEmpty())
                objectArray.append(request.value(JsonDbStrings::Protocol::object()).toObject());

            foreach (const QJsonValue &val, objectArray)
                objects.append(val.toObject());

            Partition::JsonDbPartition::ConflictResolutionMode writeMode =
                    request.value(JsonDbStrings::Protocol::conflictResolutionMode()).toString() == JsonDbStrings::Protocol::replace() ?
                        Partition::JsonDbPartition::Replace : Partition::JsonDbPartition::RejectStale;
            Partition::JsonDbWriteResult writeResults = privatePartition->updateObjects(privatePartition->defaultOwner(), objects, writeMode);
            if (writeResults.code == Partition::JsonDbError::NoError) {
                emit writeRequestStarted(requestId, writeResults.state);

                results.reserve(writeResults.objectsWritten.count());
                foreach (const Partition::JsonDbObject &object, writeResults.objectsWritten)
                    results.append(object);

                emit resultsAvailable(requestId, results);
            } else {
                errorCode = static_cast<QJsonDbRequest::ErrorCode>(writeResults.code);
                errorMessage = writeResults.message;
            }
        } else {
            Q_ASSERT(request.value(JsonDbStrings::Protocol::action()).toString() == JsonDbStrings::Protocol::query());
            QJsonObject object = request.value(JsonDbStrings::Protocol::object()).toObject();
            QMap<QString, QJsonValue> bindings;
            QJsonObject jsonbindings = object.value(JsonDbStrings::Property::bindings()).toObject();
            for (QJsonObject::const_iterator it = jsonbindings.constBegin(), e = jsonbindings.constEnd(); it != e; ++it)
                bindings.insert(it.key(), it.value());

            int limit = -1;
            int offset = 0;
            if (request.contains(JsonDbStrings::Property::queryLimit()))
                limit = request.value(JsonDbStrings::Property::queryLimit()).toDouble();
            if (request.contains(JsonDbStrings::Property::queryOffset()))
                offset = request.value(JsonDbStrings::Property::queryOffset()).toDouble();

            Partition::JsonDbQueryParser parser;
            parser.setQuery(object.value(JsonDbStrings::Property::query()).toString());
            parser.setBindings(bindings);
            parser.parse(); // parse errors can be handled in queryObjects
            Partition::JsonDbQueryResult queryResult = privatePartition->queryObjects(privatePartition->defaultOwner(),
                                                                                      parser.result(), limit, offset);
            if (queryResult.code == Partition::JsonDbError::NoError) {
                emit readRequestStarted(requestId, queryResult.state, queryResult.sortKeys.at(0));

                results.reserve(queryResult.data.count());
                foreach (const Partition::JsonDbObject &val, queryResult.data)
                    results.append(val);
                emit resultsAvailable(requestId, results);
            } else {
                errorCode = static_cast<QJsonDbRequest::ErrorCode>(queryResult.code);
                errorMessage = queryResult.message;
            }
        }
    }

    if (errorCode == QJsonDbRequest::NoError) {
        emit finished(requestId);
    } else {
        emit error(requestId, errorCode, errorMessage);
    }
}

QtJsonDb::QJsonDbRequest::ErrorCode QJsonDbPrivatePartition::ensurePartition(const QString &partitionName, QString &message)
{
    Q_ASSERT(!partitionName.isEmpty());
    QString user;
    QString fullPartitionName;
    if (partitionName == JsonDbStrings::Partition::privatePartition()) {
        user = QJsonDbStandardPaths::currentUser();
        fullPartitionName = user + JsonDbStrings::Partition::dotPrivatePartition();
    } else {
        Q_ASSERT(partitionName.endsWith(JsonDbStrings::Partition::dotPrivatePartition()));
        fullPartitionName = partitionName;
        user = partitionName.mid(0, partitionName.size() - JsonDbStrings::Partition::dotPrivatePartition().size());
    }

    // only keep a single private partition open at a time. This will cut down
    // on file contention and also keep the memory usage of the client under control
    if (privatePartition && privatePartition->partitionSpec().name != fullPartitionName) {
        delete privatePartition;
        privatePartition = 0;
    }

    if (!privatePartition) {
        QString home = QJsonDbStandardPaths::homePath(user);
        if (home.isEmpty()) {
            message = QStringLiteral("Private partition not found");
            return QJsonDbRequest::InvalidPartition;
        }

        QDir homeDir(home);
        homeDir.mkpath(QStringLiteral(".jsondb"));
        homeDir.cd(QStringLiteral(".jsondb"));

        if (!partitionOwner) {
            partitionOwner = new Partition::JsonDbOwner(this);
            partitionOwner->setAllowAll(true);
        }

        Partition::JsonDbPartitionSpec spec;
        spec.name = fullPartitionName;
        spec.path = homeDir.absolutePath();

        privatePartition = new Partition::JsonDbPartition(this);
        privatePartition->setPartitionSpec(spec);
        privatePartition->setDefaultOwner(partitionOwner);
        privatePartition->setObjectName(QStringLiteral("private"));

        if (!privatePartition->open()) {
            delete privatePartition;
            privatePartition = 0;
            message = QStringLiteral("Unable to open private partition");
            return QJsonDbRequest::InvalidPartition;
        }
    }

    return QJsonDbRequest::NoError;
}

#include "moc_qjsondbprivatepartition_p.cpp"

QT_END_NAMESPACE_JSONDB
