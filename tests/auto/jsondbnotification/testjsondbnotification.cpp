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
#include "testjsondbnotification.h"
#include "../../shared/util.h"
#include <QJSValueIterator>
#include "json.h"

static const char dbfile[] = "dbFile-jsondb-partition";

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


TestJsonDbNotification::TestJsonDbNotification()
    : mTimedOut(false)
{
    qsrand(QTime::currentTime().msec());
}

TestJsonDbNotification::~TestJsonDbNotification()
{
}

void TestJsonDbNotification::timeout()
{
    ClientWrapper::timeout();
    mTimedOut = true;
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
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << dbfile);

    mClient = new JsonDbClient(this);
    connect(mClient, SIGNAL(notified(QString,QtAddOn::JsonDb::JsonDbNotification)),
            this, SLOT(notified(QString,QtAddOn::JsonDb::JsonDbNotification)));
    connect( mClient, SIGNAL(response(int, const QVariant&)),
             this, SLOT(response(int, const QVariant&)));
    connect( mClient, SIGNAL(error(int, int, const QString&)),
             this, SLOT(error(int, int, const QString&)));

    mPluginPath = findQMLPluginPath("QtJsonDb");

    // Create the shared Partitions
    QVariantMap item;
    item.insert("_type", "Partition");
    item.insert("name", "com.nokia.shared");
    int id = mClient->create(item);
    waitForResponse1(id);

}

ComponentData *TestJsonDbNotification::createComponent()
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
    QObject::connect(componentData->qmlElement, SIGNAL(notificationSignal(QVariant, int, int)),
                     this, SLOT(notificationSlot(QVariant, int, int)));
    mComponents.append(componentData);
    return componentData;
}

ComponentData *TestJsonDbNotification::createPartitionComponent()
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
    cbData.append(data);
    mEventLoop2.quit();
}

void TestJsonDbNotification::errorSlot(int code, const QString &message)
{
    callbackError = true;
    callbackErrorCode = code;
    callbackErrorMessage = message;
    mEventLoop2.quit();
}

void TestJsonDbNotification::notificationSlot2(QJSValue result, Actions action, int stateNumber)
{
    CallbackData data;
    data.result = result.toVariant().toMap();
    data.action = action;
    data.stateNumber = stateNumber;
    cbData.append(data);
    mEventLoop2.quit();
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

    CallbackData data;
    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    mClient->create(item,  "com.nokia.shared");
    waitForCallback2();
    QCOMPARE(cbData.size(), 1);
    data = cbData.takeAt(0);
    QCOMPARE(data.action, 1);
    QCOMPARE(data.result.value("alphabet"), item.value("alphabet"));
    QString uuid = data.result.value("_uuid").toString();
    //update the object
    QString newAlphabet = data.result.value("alphabet").toString()+QString("**");
    data.result.insert("alphabet", newAlphabet);
    mClient->update(data.result,  "com.nokia.shared");
    waitForCallback2();
    QCOMPARE(cbData.size(), 1);
    data = cbData.takeAt(0);
    QCOMPARE(data.action, 2);
    QCOMPARE(data.result.value("alphabet").toString(), newAlphabet);
    //Remove the object
    mClient->remove(data.result,  "com.nokia.shared");
    waitForCallback2();
    QCOMPARE(cbData.size(), 1);
    data = cbData.takeAt(0);
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

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 10).toList();
    mClient->create(QVariant(items),  "com.nokia.shared");
    for (int i = 0; i<10; i++) {
        waitForCallback2();
        if (cbData.size() >= 10)
            break;
    }
    QCOMPARE(cbData.size(), 10);
    QVariantList objList;
    for (int i = 0; i<10; i++) {
        QCOMPARE(cbData[i].action, 1);
        QVariantMap item = items[i].toMap();
        QVariantMap obj = cbData[i].result;
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
        QString newAlphabet = cbData[i].result.value("alphabet").toString()+QString("**");
        obj.insert("alphabet", newAlphabet);
        objList.append(obj);
    }
    cbData.clear();

    //update the object
    mClient->update(QVariant(objList),  "com.nokia.shared");
    for (int i = 0; i<10; i++) {
        waitForCallback2();
        if (cbData.size() >= 10)
            break;
    }
    QCOMPARE(cbData.size(), 10);
    QVariantList lst = objList;
    objList.clear();
    for (int i = 0; i<10; i++) {
        QCOMPARE(cbData[i].action, 2);
        QVariantMap item = lst[i].toMap();
        QVariantMap obj = cbData[i].result;
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
        objList.append(obj);
    }
    cbData.clear();

    //Remove the object
    mClient->remove(objList,  "com.nokia.shared");
    for (int i = 0; i<10; i++) {
        waitForCallback2();
        if (cbData.size() >= 10)
            break;
    }
    QCOMPARE(cbData.size(), 10);
    for (int i = 0; i<10; i++) {
        QCOMPARE(cbData[i].action, 4);
        QVariantMap item = objList[i].toMap();
        QVariantMap obj = cbData[i].result;
        QCOMPARE(obj.value("_uuid"), item.value("_uuid"));
    }
    cbData.clear();
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
    QObject::connect(notification, SIGNAL(notification(QJSValue, Actions, int)),
                     this, SLOT(notificationSlot2(QJSValue, Actions, int)));
    CallbackData data;
    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    mClient->create(item,  "com.nokia.shared");
    waitForCallback2();
    QCOMPARE(cbData.size(), 1);
    data = cbData.takeAt(0);
    QCOMPARE(data.action, 1);
    QCOMPARE(data.result.value("alphabet"), item.value("alphabet"));

    delete expr;
    deleteComponent(partition);
}

QTEST_MAIN(TestJsonDbNotification)

