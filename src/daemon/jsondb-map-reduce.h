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

#ifndef JSONDB_MAP_REDUCE_H
#define JSONDB_MAP_REDUCE_H

#include <QJSEngine>
#include <QStringList>

#include "jsondb-global.h"
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include <jsondbobject.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class JsonDb;
class JsonDbOwner;
class JsonDbJoinProxy;
class JsonDbMapProxy;
class ObjectTable;

class JsonDbMapDefinition : public QObject
{
    Q_OBJECT
public:
    JsonDbMapDefinition(JsonDb *mJsonDb, JsonDbOwner *mOwner, const QString &partition, QJsonObject mapDefinition, QObject *parent = 0);
    QString uuid() const { return mUuid; }
    QString targetType() const { return mTargetType; }
    const QStringList &sourceTypes() const { return mSourceTypes; }
    QString partition() const { return mPartition; }
    bool isActive() const;
    QJsonObject definition() const { return mDefinition; }
    QJSValue mapFunction(const QString &sourceType) const;
    ObjectTable *sourceTable(const QString &sourceType) const { return mSourceTables.value(sourceType); }

    void mapObject(JsonDbObject object);
    void unmapObject(const JsonDbObject &object);
    void setError(const QString &errorMsg);

public slots:
    void viewObjectEmitted(const QJSValue &value);
    void lookupRequested(const QJSValue &spec, const QJSValue &context);

private:
    JsonDb        *mJsonDb;
    QString        mPartition;
    JsonDbOwner   *mOwner;
    QJsonObject     mDefinition;
    QJSEngine     *mScriptEngine;
    JsonDbMapProxy *mMapProxy; // to be removed when old map/lookup converted to join/lookup
    JsonDbJoinProxy *mJoinProxy;
    QMap<QString,QJSValue> mMapFunctions;
    QString        mUuid;
    QString        mTargetType;
    QStringList    mSourceTypes;
    ObjectTable   *mTargetTable;
    QMap<QString,ObjectTable*> mSourceTables;
    QList<QString> mSourceUuids;
};

class JsonDbReduceDefinition : public QObject
{
    Q_OBJECT
public:
    JsonDbReduceDefinition(JsonDb *mJsonDb, JsonDbOwner *mOwner, const QString &partition, QJsonObject reduceDefinition, QObject *parent = 0);
    QString uuid() const { return mUuid; }
    QString targetType() const { return mTargetType; }
    QString sourceType() const { return mSourceType; }
    QString partition() const { return mPartition; }
    QString sourceKeyName() const { return mSourceKeyName; }
    QString targetKeyName() const { return mTargetKeyName; }
    QString targetValueName() const { return mTargetValueName; }
    // sourceKeyName split on .
    QStringList sourceKeyNameList() const { return mSourceKeyNameList; }
    bool isActive() const;
    QJsonObject definition() const { return mDefinition; }
    const QJSValue &addFunction() const { return mAddFunction; }
    const QJSValue &subtractFunction() const { return mSubtractFunction; }

    void updateObject(JsonDbObject before, JsonDbObject after);
    QJsonValue addObject(const QJsonValue &keyValue, const QJsonValue &previousResult, JsonDbObject object);
    QJsonValue subtractObject(const QJsonValue &keyValue, const QJsonValue &previousResult, JsonDbObject object);

    void setError(const QString &errorMsg);

private:
    JsonDb        *mJsonDb;
    JsonDbOwner   *mOwner;
    QString        mPartition;
    QJsonObject     mDefinition;
    QJSEngine *mScriptEngine;
    QJSValue   mAddFunction;
    QJSValue   mSubtractFunction;
    QString        mUuid;
    QString        mTargetType;
    QString        mSourceType;
    QString        mTargetKeyName;
    QString        mTargetValueName;
    QString        mSourceKeyName;
    // mSourceKeyName split on .
    QStringList    mSourceKeyNameList;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif
