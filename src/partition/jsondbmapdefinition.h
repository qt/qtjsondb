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

#ifndef JSONDB_MAP_DEFINITION_H
#define JSONDB_MAP_DEFINITION_H

#include <QJSEngine>
#include <QStringList>

#include "jsondbpartition.h"
#include "jsondbpartitionglobal.h"

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include <jsondbobject.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbOwner;
class JsonDbJoinProxy;
class JsonDbMapProxy;
class JsonDbObjectTable;

class JsonDbMapDefinition : public QObject
{
    Q_OBJECT
public:
    JsonDbMapDefinition(const JsonDbOwner *mOwner, JsonDbPartition *partition, QJsonObject mapDefinition, QObject *parent = 0);
    QString uuid() const { return mUuid; }
    QString targetType() const { return mTargetType; }
    const QStringList &sourceTypes() const { return mSourceTypes; }
    QString partitionName() const { return mPartition->partitionSpec().name; }
    bool isActive() const;
    QJsonObject definition() const { return mDefinition; }
    QJSValue mapFunction(const QString &sourceType) const;
    JsonDbObjectTable *sourceTable(const QString &sourceType) const { return mSourceTables.value(sourceType); }
    const JsonDbOwner *owner() const { return mOwner; }

    static void definitionRemoved(JsonDbPartition *partition, JsonDbObjectTable *table, const QString targetType, const QString &definitionUuid);
    void definitionCreated();

    void initIndexes();

    void setError(const QString &errorMsg);
    void updateObject(const JsonDbObject &before, const JsonDbObject &after, JsonDbUpdateList *changeList = 0);
    static bool validateDefinition(const JsonDbObject &map, JsonDbPartition *partition, QString &message);
    static bool compileMapFunctions(QJSEngine *scriptEngine, QJsonObject definition, JsonDbJoinProxy *joinProxy, QMap<QString,QJSValue> &mapFunctions, QString &message);

public slots:
    void viewObjectEmitted(const QJSValue &value);
    void lookupRequested(const QJSValue &spec, const QJSValue &context);

private:
    void mapObject(JsonDbObject object);
    void unmapObject(const JsonDbObject &object);

public slots:
    void initScriptEngine();
    void releaseScriptEngine();

private:
    JsonDbPartition *mPartition;
    const JsonDbOwner *mOwner;
    QJsonObject     mDefinition;
    QString         mTargetKeyName;
    QPointer<QJSEngine> mScriptEngine;
    QMap<QString,QJSValue> mMapFunctions;
    QString        mUuid;
    QString        mMapId; // uuid with special characters converted to '$'
    QString        mTargetType;
    QStringList    mSourceTypes;
    JsonDbObjectTable   *mTargetTable;
    QMap<QString,JsonDbObjectTable *> mSourceTables;
    QStringList    mSourceUuids; // a set of uuids with sorted elements
    QHash<QString,JsonDbObject> mEmittedObjects;
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_MAP_DEFINITION_H
