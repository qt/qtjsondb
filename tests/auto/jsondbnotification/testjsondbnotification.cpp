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

#include <QtTest/QtTest>
#include <QJSEngine>
#include <QJSValueIterator>
#include "testjsondbnotification.h"
#include "../../shared/util.h"

const QString qmlProgram = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.Notification { "
                "id:notificationObject;"
                "signal notificationSignal(variant result, int action, int stateNumber);"
                "partition :JsonDb.Partition { "
                    "name: \"com.nokia.shared\";"
                "}"
                "onNotification: {"
                    "notificationSignal(result, action, stateNumber);"
                "}"
            "}");

const QString qmlProgramForPartition = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.Partition { "
                "signal callbackSignal(bool error, variant meta, variant response);"
                "id: sharedPartition;"
                "name: \"com.nokia.shared\";"
            "}");

#define waitForReadyStatus(obj) \
    { \
        int status = 0; \
        while ((status = obj->property("status").toInt()) < 2) { \
            waitForCallback1(); \
        } \
    }

#define waitForCallbackNId(id) \
    { \
        waitForCallback1(); \
        if (id != lastRequestId) \
            waitForResponse1(id); \
    }

TestJsonDbNotification::TestJsonDbNotification()
    : mTimedOut(false)
{
    qsrand(QTime::currentTime().msec());
}

TestJsonDbNotification::~TestJsonDbNotification()
{
    connection = 0;
}

void TestJsonDbNotification::timeout()
{
    RequestWrapper::timeout();
    mTimedOut = true;
    eventLoop1.quit();
}

void TestJsonDbNotification::deleteDbFiles()
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

void TestJsonDbNotification::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList(), __FILE__);

    connection = QJsonDbConnection::defaultConnection();
    connection->connectToServer();

    mPluginPath = findQMLPluginPath("QtJsonDb");
}

ComponentData *TestJsonDbNotification::createComponent()
{
    ComponentData *componentData = new ComponentData();
    componentData->engine = new QQmlEngine();
    QList<QQmlError> errors;
    if (!componentData->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &errors)) {
        qDebug()<<"Unable to load the plugin :"<<errors;
        delete componentData->engine;
        return 0;
    }
    componentData->component = new QQmlComponent(componentData->engine);
    componentData->component->setData(qmlProgram.toLocal8Bit(), QUrl());
    componentData->qmlElement = componentData->component->create();
    if (componentData->component->isError())
        qDebug() << componentData->component->errors();
    QObject::connect(componentData->qmlElement, SIGNAL(notificationSignal(QVariant, int, int)),
                     this, SLOT(notificationSlot(QVariant, int, int)));
    QObject::connect(componentData->qmlElement, SIGNAL(statusChanged(JsonDbNotify::Status)),
                     this, SLOT(statusChangedSlot2()));
    mComponents.append(componentData);
    return componentData;
}

ComponentData *TestJsonDbNotification::createPartitionComponent()
{
    ComponentData *componentData = new ComponentData();
    componentData->engine = new QQmlEngine();
    QList<QQmlError> errors;
    if (!componentData->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &errors)) {
        qDebug()<<"Unable to load the plugin :"<<errors;
        delete componentData->engine;
        return 0;
    }
    componentData->component = new QQmlComponent(componentData->engine);
    componentData->component->setData(qmlProgramForPartition.toLocal8Bit(), QUrl());
    componentData->qmlElement = componentData->component->create();
    if (componentData->component->isError())
        qDebug() << componentData->component->errors();
    mComponents.append(componentData);
    return componentData;
}

void TestJsonDbNotification::deleteComponent(ComponentData *componentData)
{
    mComponents.removeOne(componentData);
    if (componentData)
        delete componentData;
}

void TestJsonDbNotification::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

void TestJsonDbNotification::notificationSlot(QVariant result, int action, int stateNumber)
{
    CallbackData data;
    data.result = result.toMap();
    data.action = action;
    data.stateNumber = stateNumber;
    callbackData.append(data);
    eventLoop1.quit();
}

void TestJsonDbNotification::errorSlot(int code, const QString &message)
{
    callbackError = true;
    callbackErrorCode = code;
    callbackErrorMessage = message;
    eventLoop1.quit();
}

void TestJsonDbNotification::notificationSlot2(QJSValue result, Actions action, int stateNumber)
{
    CallbackData data;
    data.result = result.toVariant().toMap();
    data.action = action;
    data.stateNumber = stateNumber;
    callbackData.append(data);
    eventLoop1.quit();
}

void TestJsonDbNotification::statusChangedSlot2()
{
    eventLoop1.quit();
}

void TestJsonDbNotification::singleObjectNotifications()
{
    ComponentData *notification = createComponent();
    if (!notification || !notification->qmlElement) return;

    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"]");
    notification->qmlElement->setProperty("query", queryString);
    QVariantList actionsList;
    actionsList.append(1);
    actionsList.append(2);
    actionsList.append(4);
    notification->qmlElement->setProperty("actions", actionsList);
    waitForReadyStatus(notification->qmlElement);

    CallbackData data;
    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    int id = create(item,  "com.nokia.shared");
    waitForCallbackNId(id);

    QCOMPARE(callbackData.size(), 1);
    data = callbackData.takeAt(0);
    QCOMPARE(data.action, 1);
    QCOMPARE(data.result.value("alphabet"), item.value("alphabet"));
    QString uuid = data.result.value("_uuid").toString();
    //update the object
    QString newAlphabet = data.result.value("alphabet").toString()+QString("**");
    data.result.insert("alphabet", newAlphabet);
    id = update(data.result,  "com.nokia.shared");
    waitForCallbackNId(id);

    QCOMPARE(callbackData.size(), 1);
    data = callbackData.takeAt(0);
    QCOMPARE(data.action, 2);
    QCOMPARE(data.result.value("alphabet").toString(), newAlphabet);
    //Remove the object
    id = remove(data.result,  "com.nokia.shared");
    waitForCallbackNId(id);

    QCOMPARE(callbackData.size(), 1);
    data = callbackData.takeAt(0);
    QCOMPARE(data.action, 4);
    QCOMPARE(data.result.value("_uuid").toString(), uuid);
    deleteComponent(notification);
}

void TestJsonDbNotification::multipleObjectNotifications()
{
    ComponentData *notification = createComponent();
    if (!notification || !notification->qmlElement) return;

    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"]");
    notification->qmlElement->setProperty("query", queryString);
    QVariantList actionsList;
    actionsList.append(1);
    actionsList.append(2);
    actionsList.append(4);
    notification->qmlElement->setProperty("actions", actionsList);
    waitForReadyStatus(notification->qmlElement);

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 10).toList();
    int id = create(items,  "com.nokia.shared");
    for (int i = 0; i<10; i++) {
        waitForCallback1();
        if (callbackData.size() >= 10)
            break;
    }
    if (id != lastRequestId)
        waitForResponse1(id);

    QCOMPARE(callbackData.size(), 10);
    QVariantList objList;
    for (int i = 0; i<10; i++) {
        QCOMPARE(callbackData[i].action, 1);
        QVariantMap item = items[i].toMap();
        QVariantMap obj = callbackData[i].result;
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
        QString newAlphabet = callbackData[i].result.value("alphabet").toString()+QString("**");
        obj.insert("alphabet", newAlphabet);
        objList.append(obj);
    }
    callbackData.clear();

    //update the object
    id = update(objList,  "com.nokia.shared");
    for (int i = 0; i<10; i++) {
        waitForCallback1();
        if (callbackData.size() >= 10)
            break;
    }
    if (id != lastRequestId)
        waitForResponse1(id);

    QCOMPARE(callbackData.size(), 10);
    QVariantList lst = objList;
    objList.clear();
    for (int i = 0; i<10; i++) {
        QCOMPARE(callbackData[i].action, 2);
        QVariantMap item = lst[i].toMap();
        QVariantMap obj = callbackData[i].result;
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
        objList.append(obj);
    }
    callbackData.clear();

    //Remove the object
    id = remove(objList,  "com.nokia.shared");
    for (int i = 0; i<10; i++) {
        waitForCallback1();
        if (callbackData.size() >= 10)
            break;
    }
    if (id != lastRequestId)
        waitForResponse1(id);

    QCOMPARE(callbackData.size(), 10);
    for (int i = 0; i<10; i++) {
        QCOMPARE(callbackData[i].action, 4);
        QVariantMap item = objList[i].toMap();
        QVariantMap obj = callbackData[i].result;
        QCOMPARE(obj.value("_uuid"), item.value("_uuid"));
    }
    callbackData.clear();
    deleteComponent(notification);
}

void TestJsonDbNotification::createNotification()
{
    const QString createString = QString("createNotification('[?_type=\"%1\"]');");
    ComponentData *partition = createPartitionComponent();
    if (!partition || !partition->qmlElement) return;
    QString expression;

    expression = QString(createString).arg(__FUNCTION__);
    QQmlExpression *expr;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    QPointer<QObject> notification = expr->evaluate().value<QObject*>();
    QVERIFY(!notification.isNull());
    notification->setParent(partition->qmlElement);
    QObject::connect(notification, SIGNAL(notification(QJSValue,Actions,int)),
                     this, SLOT(notificationSlot2(QJSValue,Actions,int)));
    QObject::connect(notification, SIGNAL(statusChanged(JsonDbNotify::Status)),
                     this, SLOT(statusChangedSlot2()));
    waitForReadyStatus(notification);


    CallbackData data;
    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    int id = create(item,  "com.nokia.shared");
    waitForCallbackNId(id);

    QCOMPARE(callbackData.size(), 1);
    data = callbackData.takeAt(0);
    QCOMPARE(data.action, 1);
    QCOMPARE(data.result.value("alphabet"), item.value("alphabet"));

    delete expr;
    deleteComponent(partition);
}

QTEST_MAIN(TestJsonDbNotification)

