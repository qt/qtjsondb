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

#ifndef JSONDBCHANGESSINCEOBJECT_H
#define JSONDBCHANGESSINCEOBJECT_H

#include <QObject>
#include <QVariant>
#include <QPointer>
#include <QJSValue>
#include <QDeclarativeParserStatus>
#include <QDeclarativeListProperty>
#include "jsondb-client.h"

class JsonDbPartition;
class JsonDbPartitionPrivate;

QT_USE_NAMESPACE_JSONDB

class JsonDbChangesSinceObject : public QObject, public QDeclarativeParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QDeclarativeParserStatus)

public:
    Q_PROPERTY(JsonDbPartition* partition READ partition WRITE setPartition)
    Q_PROPERTY(QStringList types READ types WRITE setTypes)
    Q_PROPERTY(quint32 stateNumber READ stateNumber WRITE setStateNumber)

    Q_PROPERTY(quint32 startingStateNumber READ startingStateNumber)
    Q_PROPERTY(quint32 currentStateNumber READ currentStateNumber)

    JsonDbChangesSinceObject(QObject *parent = 0);
    ~JsonDbChangesSinceObject();

    QStringList types() const;
    void setTypes(const QStringList &newTypes);

    JsonDbPartition* partition();
    void setPartition(JsonDbPartition* newPartition);


    quint32 stateNumber() const;
    void setStateNumber(quint32 stateNumber);

    quint32 startingStateNumber() const;
    quint32 currentStateNumber() const;

    void classBegin() {}
    void componentComplete();

    Q_INVOKABLE int exec();
    Q_INVOKABLE QVariantList takeResults();

Q_SIGNALS:
    void resultsReady(int resultsAvailable);
    void finished();
    void error(int code, const QString &message);
private Q_SLOTS:
    void emitError(QtAddOn::JsonDb::JsonDbError::ErrorCode code, const QString& message);

private:
    bool completed;
    quint32 startStateNumber;
    QStringList filterTypes;
    QPointer<JsonDbPartition> partitionObject;
    QPointer<JsonDbPartition> defaultPartitionObject;
    QPointer<JsonDbChangesSince> jsondbChangesSince;

    friend class JsonDbPartition;
    friend class JsonDbPartitionPrivate;
};

#endif //JSONDBCHANGESSINCEOBJECT_H
