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

#ifndef JSONDB_OBJECT_H
#define JSONDB_OBJECT_H

#include <QJSEngine>
#include <QUuid>
#include <QDebug>
#include <QVariant>

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "jsondbpartitionglobal.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class Q_JSONDB_PARTITION_EXPORT JsonDbObject : public QJsonObject
{
public:
    JsonDbObject();
    JsonDbObject(const QJsonObject &object);
    ~JsonDbObject();

    QByteArray toBinaryData() const;

    QUuid uuid() const;
    QString version() const;
    QString type() const;
    bool isDeleted() const;
    void markDeleted();

    void generateUuid();
    static QUuid createUuidFromString(const QString &id);
    QString computeVersion();

    bool updateVersionOptimistic(const JsonDbObject &other, QString *versionWritten);
    bool updateVersionReplicating(const JsonDbObject & other);

    bool operator <(const JsonDbObject &other) const;
    bool isAncestorOf(const JsonDbObject &other) const;

    QJsonValue propertyLookup(const QString &path) const;
    QJsonValue propertyLookup(const QStringList &path) const;

private:
    bool populateMerge(QMap<JsonDbObject, bool> *documents, const QUuid &id, const JsonDbObject &source, bool validateSource = false, bool recurse = true) const;
    void populateHistory(QJsonArray *history, const JsonDbObject &doc, bool includeCurrent) const;
    QString tokenizeVersion(const QString &version, int *updateCount) const;
    QString versionAsString(const int updateCount, const QString &hash) const;

    bool computeVersion(const int oldUpdateCount, const QString& oldHash, int *newUpdateCount, QString *newHash) const;

    bool isAncestorOf(const QJsonArray &history, const int updateCount, const QString &hash) const;
    void addAncestor(QJsonArray *history, const int updateCount, const QString &hash) const;
};


typedef QList<JsonDbObject> JsonDbObjectList;

struct GetObjectsResult
{
    JsonDbObjectList data;
    QJsonValue error;

};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_OBJECT_H
