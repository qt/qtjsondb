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

#include <QDebug>
#include <QElapsedTimer>
#include <QJSValue>
#include <QJSValueIterator>
#include <QStringList>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "jsondbpartition.h"
#include "jsondbpartition_p.h"
#include "jsondbstrings.h"
#include "jsondberrors.h"

#include "jsondbproxy.h"
#include "jsondbsettings.h"
#include "jsondbobjecttable.h"
#include "jsondbreducedefinition.h"
#include "jsondbscriptengine.h"
#include "jsondbview.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

JsonDbReduceDefinition::JsonDbReduceDefinition(const JsonDbOwner *owner, JsonDbPartition *partition,
                                               QJsonObject definition, QObject *parent) :
    QObject(parent)
    , mOwner(owner)
    , mPartition(partition)
    , mDefinition(definition)
    , mScriptEngine(0)
    , mUuid(mDefinition.value(JsonDbString::kUuidStr).toString())
    , mTargetType(mDefinition.value(QStringLiteral("targetType")).toString())
    , mTargetTable(mPartition->findObjectTable(mTargetType))
    , mSourceType(mDefinition.value(QStringLiteral("sourceType")).toString())
    , mJoinProxy(0)
{
    if (mDefinition.contains(QStringLiteral("targetKeyName")))
        mTargetKeyName = mDefinition.value(QStringLiteral("targetKeyName")).toString();
    else
        mTargetKeyName = QLatin1String("key");
    if (mDefinition.contains(QStringLiteral("sourceKeyName")))
        mSourceKeyName = mDefinition.value(QStringLiteral("sourceKeyName")).toString();
    mSourceKeyNameList = mSourceKeyName.split(QStringLiteral("."));
    if (mDefinition.contains(QStringLiteral("targetValueName"))) {
        if (mDefinition.value(QStringLiteral("targetValueName")).isString())
            mTargetValueName = mDefinition.value(QStringLiteral("targetValueName")).toString();
    } else
        mTargetValueName = QLatin1String("value");

}

void JsonDbReduceDefinition::definitionCreated()
{
    initScriptEngine();
    initIndexes();

    GetObjectsResult getObjectResponse = mPartition->d_func()->getObjects(JsonDbString::kTypeStr, mSourceType);
    if (!getObjectResponse.error.isNull()) {
        if (jsondbSettings->verbose())
            qDebug() << "createReduceDefinition" << mTargetType << getObjectResponse.error.toString();
        setError(getObjectResponse.error.toString());
    }
    JsonDbObjectList objects = getObjectResponse.data;
    for (int i = 0; i < objects.size(); i++)
        updateObject(QJsonObject(), objects.at(i));
}

void JsonDbReduceDefinition::definitionRemoved(JsonDbPartition *partition, JsonDbObjectTable *table, const QString targetType, const QString &definitionUuid)
{
    if (jsondbSettings->verbose())
        qDebug() << "Removing Reduce view objects" << targetType;
    // remove the output objects
    GetObjectsResult getObjectResponse = table->getObjects(QLatin1String("_reduceUuid"), definitionUuid, targetType);
    JsonDbObjectList objects = getObjectResponse.data;
    for (int i = 0; i < objects.size(); i++) {
        JsonDbObject o = objects[i];
        o.markDeleted();
        partition->updateObject(partition->defaultOwner(), o, JsonDbPartition::ViewObject);
    }
}

void JsonDbReduceDefinition::initScriptEngine()
{
    if (mScriptEngine)
        return;

    mScriptEngine = JsonDbScriptEngine::scriptEngine();
    mJoinProxy = new JsonDbJoinProxy(mOwner, mPartition, this);

    QString message;
    bool status = compileFunctions(mScriptEngine, mDefinition, mJoinProxy, mFunctions, message);
    if (!status)
        setError(message);

    Q_ASSERT(!mDefinition.value(QStringLiteral("add")).toString().isEmpty());
    Q_ASSERT(!mDefinition.value(QStringLiteral("subtract")).toString().isEmpty());
}

void JsonDbReduceDefinition::releaseScriptEngine()
{
    mFunctions.clear();
    if (mJoinProxy) {
        delete mJoinProxy;
        mJoinProxy = 0;
    }
    mScriptEngine = 0;
}

void JsonDbReduceDefinition::initIndexes()
{
    // TODO: this index should not be automatic
    if (!mSourceKeyName.isEmpty()) {
        JsonDbObjectTable *sourceTable = mPartition->findObjectTable(mSourceType);
        sourceTable->addIndexOnProperty(mSourceKeyName, QLatin1String("string"));
    }
    // TODO: this index should not be automatic
    mTargetTable->addIndexOnProperty(mTargetKeyName, QLatin1String("string"), mTargetType);
    mTargetTable->addIndexOnProperty(QLatin1String("_reduceUuid"), QLatin1String("string"), mTargetType);
}

void JsonDbReduceDefinition::updateObject(JsonDbObject before, JsonDbObject after, JsonDbUpdateList *changeList)
{
    initScriptEngine();

    QJsonValue beforeKeyValue = sourceKeyValue(before);
    QJsonValue afterKeyValue = sourceKeyValue(after);

    if (jsondbSettings->debug())
        qDebug() << "JsonDbReduceDefinition::updateObject"
                 << "beforeKeyValue" << beforeKeyValue
                 << "afterKeyValue" << afterKeyValue;
    if (!after.isEmpty() && !after.isDeleted() && !before.isEmpty() && (beforeKeyValue != afterKeyValue)) {
        // do a subtract only on the before key
        if (!beforeKeyValue.isUndefined())
          updateObject(before, QJsonObject(), changeList);

        // and then continue here with the add with the after key
        before = QJsonObject();
    }

    const QJsonValue keyValue((after.isEmpty() || after.isDeleted()) ? beforeKeyValue : afterKeyValue);
    if (keyValue.isUndefined())
        return;

    GetObjectsResult getObjectResponse = mTargetTable->getObjects(mTargetKeyName, keyValue, mTargetType);
    if (!getObjectResponse.error.isNull())
        setError(getObjectResponse.error.toString());

    JsonDbObject previousObject;
    QJsonValue previousValue(QJsonValue::Undefined);

    JsonDbObjectList previousResults = getObjectResponse.data;
    for (int k = 0; k < previousResults.size(); ++k) {
        JsonDbObject previous = previousResults.at(k);
        if (previous.value(QStringLiteral("_reduceUuid")).toString() == mUuid) {
            previousObject = previous;
            previousValue = previousObject;
            break;
        }
    }

    QJsonValue value = previousValue;
    if (!before.isEmpty())
        value = addObject(JsonDbReduceDefinition::Subtract, keyValue, value, before);
    if (!after.isEmpty() && !after.isDeleted())
        value = addObject(JsonDbReduceDefinition::Add, keyValue, value, after);

    JsonDbObjectList objectsToUpdate;
    // if we had a previous object to reduce
    if (previousObject.contains(JsonDbString::kUuidStr)) {
        // and now the value is undefined
        if (value.isUndefined()) {
            // then remove it
            previousObject.markDeleted();
            objectsToUpdate.append(previousObject);
        } else {
            //otherwise update it
            JsonDbObject reduced(value.toObject());
            reduced.insert(JsonDbString::kTypeStr, mTargetType);
            reduced.insert(JsonDbString::kUuidStr,
                         previousObject.value(JsonDbString::kUuidStr));
            reduced.insert(JsonDbString::kVersionStr,
                         previousObject.value(JsonDbString::kVersionStr));
            reduced.insert(mTargetKeyName, keyValue);
            reduced.insert(QStringLiteral("_reduceUuid"), mUuid);
            objectsToUpdate.append(reduced);
        }
    } else if (!value.isUndefined()) {
        // otherwise create the new object
        JsonDbObject reduced(value.toObject());
        reduced.insert(JsonDbString::kTypeStr, mTargetType);
        reduced.insert(mTargetKeyName, keyValue);
        reduced.insert(QStringLiteral("_reduceUuid"), mUuid);

        objectsToUpdate.append(reduced);
    }

    JsonDbWriteResult res = mPartition->updateObjects(mOwner, objectsToUpdate, JsonDbPartition::ViewObject, changeList);
    if (res.code != JsonDbError::NoError)
        setError(QString::fromLatin1("Error executing add function: %1").arg(res.message));
}

QJsonValue JsonDbReduceDefinition::addObject(JsonDbReduceDefinition::FunctionNumber functionNumber,
                                             const QJsonValue &keyValue, QJsonValue previousValue, JsonDbObject object)
{
    initScriptEngine();
    QJSValue svKeyValue = mScriptEngine->toScriptValue(keyValue);
    if (!mTargetValueName.isEmpty())
        previousValue = previousValue.toObject().value(mTargetValueName);
    QJSValue svPreviousValue = mScriptEngine->toScriptValue(previousValue);
    QJSValue svObject = mScriptEngine->toScriptValue(static_cast<QJsonObject>(object));

    QJSValueList reduceArgs;
    reduceArgs << svKeyValue << svPreviousValue << svObject;
    QJSValue reduced = mFunctions[functionNumber].call(reduceArgs);

    if (!reduced.isUndefined() && !reduced.isError()) {
        QJsonValue jsonReduced = mScriptEngine->fromScriptValue<QJsonValue>(reduced);
        QJsonObject jsonReducedObject;
        if (!mTargetValueName.isEmpty())
            jsonReducedObject.insert(mTargetValueName, jsonReduced);
        else
            jsonReducedObject = jsonReduced.toObject();
        return jsonReducedObject;
    } else {

        if (reduced.isError())
            setError(QString::fromLatin1("Error executing add function: %1").arg(reduced.toString()));

        return QJsonValue(QJsonValue::Undefined);
    }
}

bool JsonDbReduceDefinition::isActive() const
{
    return !mDefinition.contains(JsonDbString::kActiveStr) || mDefinition.value(JsonDbString::kActiveStr).toBool();
}

void JsonDbReduceDefinition::setError(const QString &errorMsg)
{
    mDefinition.insert(JsonDbString::kActiveStr, false);
    mDefinition.insert(JsonDbString::kErrorStr, errorMsg);
    if (jsondbSettings->verboseErrors())
        qDebug() << "Error in Reduce definition" << mTargetType << errorMsg;
    if (mPartition)
        mPartition->updateObject(mOwner, mDefinition, JsonDbPartition::Replace);
}

bool JsonDbReduceDefinition::validateDefinition(const JsonDbObject &reduce, JsonDbPartition *partition, QString &message)
{
    message.clear();
    QString targetType = reduce.value(QStringLiteral("targetType")).toString();
    QString sourceType = reduce.value(QStringLiteral("sourceType")).toString();
    QString uuid = reduce.value(JsonDbString::kUuidStr).toString();
    JsonDbView *view = partition->findView(targetType);

    if (targetType.isEmpty())
        message = QLatin1Literal("targetType property for Reduce not specified");
    else if (!view)
        message = QLatin1Literal("targetType must be of a type that extends View");
    else if (sourceType.isEmpty())
        message = QLatin1Literal("sourceType property for Reduce not specified");
    else if (view->mReduceDefinitionsBySource.contains(sourceType)
             && view->mReduceDefinitionsBySource.value(sourceType)->uuid() != uuid)
        message = QString::fromLatin1("duplicate Reduce definition on source %1 and target %2")
            .arg(sourceType).arg(targetType);
    else if (reduce.value(QStringLiteral("sourceKeyName")).toString().isEmpty() && reduce.value(QStringLiteral("sourceKeyFunction")).toString().isEmpty())
        message = QLatin1Literal("sourceKeyName or sourceKeyFunction must be provided for Reduce");
    else if (!reduce.value(QStringLiteral("sourceKeyName")).toString().isEmpty() && !reduce.value(QStringLiteral("sourceKeyFunction")).toString().isEmpty())
        message = QLatin1Literal("Only one of sourceKeyName and sourceKeyFunction may be provided for Reduce");
    else if (reduce.value(QStringLiteral("add")).toString().isEmpty())
        message = QLatin1Literal("add function for Reduce not specified");
    else if (reduce.value(QStringLiteral("subtract")).toString().isEmpty())
        message = QLatin1Literal("subtract function for Reduce not specified");
    else if (reduce.contains(QStringLiteral("targetValueName"))
             && !(reduce.value(QStringLiteral("targetValueName")).isString() || reduce.value(QStringLiteral("targetValueName")).isNull()))
        message = QLatin1Literal("targetValueName for Reduce must be a string or null");
    else {
        QJSEngine *scriptEngine = JsonDbScriptEngine::scriptEngine();
        QVector<QJSValue> functions;
        // check for script errors
        compileFunctions(scriptEngine, reduce, 0, functions, message);
    }
    return message.isEmpty();
}

bool JsonDbReduceDefinition::compileFunctions(QJSEngine *scriptEngine, QJsonObject definition, JsonDbJoinProxy *proxy,
                                              QVector<QJSValue> &functions, QString &message)
{
    bool status = true;
    QStringList functionNames = (QStringList()
                                 << QLatin1String("add")
                                 << QLatin1String("subtract")
                                 << QLatin1String("sourceKeyFunction"));
    int i = 0;
    functions.resize(3);
    foreach (const QString &functionName, functionNames) {
        int functionNumber = i++;
        if (!definition.contains(functionName))
            continue;
        QString script = definition.value(functionName).toString();

        // first, package it as a function that takes a jsondb proxy and returns the add/subtract function
        QJSValue moduleFunction = scriptEngine->evaluate(QString::fromLatin1("(function (proxy) { jsondb = {createUuidFromString: proxy.createUuidFromString}; return (%1); })").arg(script));

        if (moduleFunction.isError() || !moduleFunction.isCallable()) {
            message = QString::fromLatin1("Unable to parse %1 function: %2").arg(functionName).arg(moduleFunction.toString());
            status = false;
            continue;
        }

        // now pass it the jsondb proxy to get the add/subtract function
        QJSValueList args;
        if (proxy)
            args << scriptEngine->newQObject(proxy);
        else
            args << QJSValue(QJSValue::UndefinedValue);
        QJSValue function = moduleFunction.call(args);
        if (moduleFunction.isError() || !moduleFunction.isCallable()) {
            message = QString::fromLatin1("Unable to evaluate %1 function: %2").arg(functionName).arg(moduleFunction.toString());
            status = false;
        }
        functions[functionNumber] = function;
    }
    return status;
}

QJsonValue JsonDbReduceDefinition::sourceKeyValue(const JsonDbObject &object)
{
    if (object.isEmpty() || object.isDeleted()) {
        return QJsonValue(QJsonValue::Undefined);
    } else if (mFunctions[JsonDbReduceDefinition::SourceKeyValue].isCallable()) {
        QJSValueList args;
        args << mScriptEngine->toScriptValue(static_cast<QJsonObject>(object));
        QJsonValue keyValue = mScriptEngine->fromScriptValue<QJsonValue>(mFunctions[JsonDbReduceDefinition::SourceKeyValue].call(args));
        return keyValue;
    } else
        return mSourceKeyName.contains('.') ? JsonDbObject(object).valueByPath(mSourceKeyNameList) : object.value(mSourceKeyName);

}

#include "moc_jsondbreducedefinition.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
