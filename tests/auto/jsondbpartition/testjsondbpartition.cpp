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

#include <QtTest/QtTest>
#include <QJSEngine>
#include <QJSValueIterator>
#include "testjsondbpartition.h"
#include "../../shared/util.h"

static const char dbfile[] = "dbFile-jsondb-partition";

const QString qmlProgram = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.Partition { "
                "signal callbackSignal(variant error, variant response);"
                "id: sharedPartition;"
                "name: \"com.nokia.shared.1\";"
            "}");

TestJsonDbPartition::TestJsonDbPartition()
    : mTimedOut(false)
{
    qsrand(QTime::currentTime().msec());
}

TestJsonDbPartition::~TestJsonDbPartition()
{
    connection = 0;
}

void TestJsonDbPartition::timeout()
{
    RequestWrapper::timeout();
    mTimedOut = true;
    eventLoop1.quit();
}

void TestJsonDbPartition::deleteDbFiles()
{
    // remove all the test files.
    QDir currentDir;
    QStringList nameFilter;
    nameFilter << QString("*.db");
    nameFilter << "objectFile.bin" << "objectFile2.bin";
    QFileInfoList databaseFiles = currentDir.entryInfoList(nameFilter, QDir::Files);
    foreach (QFileInfo fileInfo, databaseFiles) {
        QFile file(fileInfo.fileName());
        file.remove();
    }
}

void TestJsonDbPartition::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << "-base-name" << dbfile, __FILE__);

    connection = QJsonDbConnection::defaultConnection();
    connection->connectToServer();

    mPluginPath = findQMLPluginPath("QtJsonDb");
    if (mPluginPath.isEmpty())
        qDebug() << "Couldn't find the plugin path for the plugin QtJsonDb";
}

ComponentData *TestJsonDbPartition::createComponent()
{
    ComponentData *componentData = new ComponentData();
    componentData->engine = new QQmlEngine();
    QString error;
    if (!componentData->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &error)) {
        qDebug()<<"Unable to load the plugin :"<<error;
        delete componentData->engine;
        return 0;
    }
    componentData->component = new QQmlComponent(componentData->engine);
    componentData->component->setData(qmlProgram.toLocal8Bit(), QUrl());
    componentData->qmlElement = componentData->component->create();
    if (componentData->component->isError())
        qDebug() << componentData->component->errors();
    QObject::connect(componentData->qmlElement, SIGNAL(callbackSignal(QVariant, QVariant)),
                         this, SLOT(callbackSlot(QVariant, QVariant)));

    mComponents.append(componentData);
    return componentData;
}

void TestJsonDbPartition::deleteComponent(ComponentData *componentData)
{
    mComponents.removeOne(componentData);
    if (componentData)
        delete componentData;
}

void TestJsonDbPartition::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

void TestJsonDbPartition::callbackSlot(QVariant error, QVariant response)
{
    callbackError = error.isValid();
    callbackMeta = response;
    callbackResponse = response.toMap().value("items").toList();
    eventLoop1.quit();
}

// JsonDb.Partition.create()
void TestJsonDbPartition::create()
{
    const QString createString = QString("create(%1, function (error, response) {callbackSignal(error, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QString expression;

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));

    QQmlExpression *expr;
    int id = 0;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);
    delete expr;

    deleteComponent(partition);
}

// JsonDb.Partition.update()
void TestJsonDbPartition::update()
{
    const QString createString = QString("create(%1, function (error, response) {callbackSignal(error, response);});");
    const QString updateString = QString("update(%1, function (error, response) {callbackSignal(error, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QVariantMap objMap;
    QVariantList objList;
    QString expression;

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));

    QQmlExpression *expr;
    int id = 0;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //Modify and update this object
    objMap = updateObject(obj, callbackResponse.toList()).toMap();
    objMap.insert("alphabet", objMap.value("alphabet").toString()+QString("**"));
    obj = objMap;
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //Modify all  update this object
    objList = updateObjectList(obj.toList(), callbackResponse.toList()).toList();
    for (int i = 0; i<5; i++) {
        objMap = objList[i].toMap();
        objMap.insert("alphabet", objMap.value("alphabet").toString()+QString("**"));
        objList[i] = objMap;
    }
    obj = objList;
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    delete expr;
    deleteComponent(partition);
}

// JsonDb.Partition.update()
void TestJsonDbPartition::update_RejectStale()
{
    const QString createString = QString("create(%1, function (error, response) {callbackSignal(error, response);});");
    const QString updateString = QString("update(%1, { mode : 0 }, function (error, response) {callbackSignal(error, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QVariantMap objMap;
    QVariantList objList;
    QString expression;

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));

    QQmlExpression *expr;
    int id = 0;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //Modify and update this object
    objMap = updateObject(obj, callbackResponse.toList()).toMap();
    objMap.insert("alphabet", objMap.value("alphabet").toString()+QString("**"));
    obj = objMap;
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //This should be rejected since _version wont match
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, true);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 0);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //Modify all  update this object
    objList = updateObjectList(obj.toList(), callbackResponse.toList()).toList();
    for (int i = 0; i<5; i++) {
        objMap = objList[i].toMap();
        objMap.insert("alphabet", objMap.value("alphabet").toString()+QString("**"));
        objList[i] = objMap;
    }
    obj = objList;
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //This should fail for _version mismatch
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, true);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 0);

    delete expr;
    deleteComponent(partition);
}

void TestJsonDbPartition::update_Replace()
{
    const QString createString = QString("create(%1, function (error, response) {callbackSignal(error, response);});");
    const QString updateString = QString("update(%1, { mode : 1 }, function (error, response) {callbackSignal(error, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QVariantMap objMap;
    QVariantList objList;
    QString expression;

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));

    QQmlExpression *expr;
    int id = 0;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //Modify and update this object
    objMap = updateObject(obj, callbackResponse.toList()).toMap();
    objMap.insert("alphabet", objMap.value("alphabet").toString()+QString("**"));
    obj = objMap;
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //Should not be rejected
    objMap.remove("_version");
    obj = objMap;
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //Modify all  update this object
    objList = updateObjectList(obj.toList(), callbackResponse.toList()).toList();
    for (int i = 0; i<5; i++) {
        objMap = objList[i].toMap();
        objMap.insert("alphabet", objMap.value("alphabet").toString()+QString("**"));
        objList[i] = objMap;
    }
    obj = objList;
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //This should not fail, eventhough we have _version mismatch
    expression = QString(updateString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    delete expr;
    deleteComponent(partition);
}

// JsonDb.Partition.remove()
void TestJsonDbPartition::remove()
{
    const QString createString = QString("create(%1, function (error, response) {callbackSignal(error, response);});");
    const QString removeString = QString("remove(%1, function (error, response) {callbackSignal(error, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QVariantMap objMap;
    QVariantList objList;
    QString expression;

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));

    QQmlExpression *expr;
    int id = 0;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //Remove this object
    {
        QVariantList newList;
        QVariantList list = callbackResponse.toList();
        for (int i = 0; i<list.count(); i++) {
            QVariantMap objMap = list[i].toMap();
            objMap.insert("_type", __FUNCTION__);
            newList.append(objMap);
        }
        obj = newList;
    }
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //Remove all objects
    {
        QVariantList newList;
        QVariantList list = callbackResponse.toList();
        for (int i = 0; i<list.count(); i++) {
            QVariantMap objMap = list[i].toMap();
            objMap.insert("_type", __FUNCTION__);
            newList.append(objMap);
        }
        obj = newList;
    }
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    delete expr;
    deleteComponent(partition);
}
// JsonDb.Partition.remove()
void TestJsonDbPartition::remove_RejectStale()
{
    const QString createString = QString("create(%1, function (error, response) {callbackSignal(error, response);});");
    const QString removeString = QString("remove(%1, { mode : 0 }, function (error, response) {callbackSignal(error, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QVariantMap objMap;
    QVariantList objList;
    QString expression;

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));

    QQmlExpression *expr;
    int id = 0;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    QVariantList originalList = callbackResponse.toList();

    //Remove this object (should fail)
    {
        QVariantList newList;
        QVariantList list = originalList;
        for (int i = 0; i<list.count(); i++) {
            QVariantMap objMap = list[i].toMap();
            objMap.insert("_type", __FUNCTION__);
            objMap.remove("_version");
            newList.append(objMap);
        }
        obj = newList;
    }
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, true);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 0);

    //Remove this object
    {
        QVariantList newList;
        QVariantList list = originalList;
        for (int i = 0; i<list.count(); i++) {
            QVariantMap objMap = list[i].toMap();
            objMap.insert("_type", __FUNCTION__);
            newList.append(objMap);
        }
        obj = newList;
    }
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);
    originalList = callbackResponse.toList();

    //Remove all objects (should fail)
    {
        QVariantList newList;
        QVariantList list = originalList;
        for (int i = 0; i<list.count(); i++) {
            QVariantMap objMap = list[i].toMap();
            objMap.insert("_type", __FUNCTION__);
            objMap.remove("_version");
            newList.append(objMap);
        }
        obj = newList;
    }
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, true);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 0);

    //Remove all objects
    {
        QVariantList newList;
        QVariantList list = originalList;
        for (int i = 0; i<list.count(); i++) {
            QVariantMap objMap = list[i].toMap();
            objMap.insert("_type", __FUNCTION__);
            newList.append(objMap);
        }
        obj = newList;
    }
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    delete expr;
    deleteComponent(partition);
}

// JsonDb.Partition.remove()
void TestJsonDbPartition::remove_Replace()
{
    const QString createString = QString("create(%1, function (error, response) {callbackSignal(error, response);});");
    const QString removeString = QString("remove(%1, { mode : 1 }, function (error, response) {callbackSignal(error, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QVariantMap objMap;
    QVariantList objList;
    QString expression;

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));

    QQmlExpression *expr;
    int id = 0;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    QVariantList originalList = callbackResponse.toList();

    //Remove this object
    {
        QVariantList newList;
        QVariantList list = originalList;
        for (int i = 0; i<list.count(); i++) {
            QVariantMap objMap = list[i].toMap();
            objMap.insert("_type", __FUNCTION__);
            objMap.remove("_version");
            newList.append(objMap);
        }
        obj = newList;
    }
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);
    originalList = callbackResponse.toList();

    //Remove all objects (should fail)
    {
        QVariantList newList;
        QVariantList list = originalList;
        for (int i = 0; i<list.count(); i++) {
            QVariantMap objMap = list[i].toMap();
            objMap.insert("_type", __FUNCTION__);
            objMap.remove("_version");
            newList.append(objMap);
        }
        obj = newList;
    }
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    delete expr;
    deleteComponent(partition);
}

// JsonDb.Partition.find()
void TestJsonDbPartition::find()
{
    const QString createString = QString("create(%1, function (error, response) {callbackSignal(error, response);});");
    const QString findString = QString("find('[?_type = \""+QString( __FUNCTION__ )+"\"]', function (error, response) {callbackSignal(error, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QVariantMap objMap;
    QVariantList objList;
    QString expression;

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));

    QQmlExpression *expr;
    int id = 0;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //find this object
    expression = findString;
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //Find all objects
    expression = findString;
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 6);

    delete expr;
    deleteComponent(partition);
}

QTEST_MAIN(TestJsonDbPartition)

