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
#include "testjsondbchangessinceobject.h"
#include "../../shared/util.h"
#include <QJSValueIterator>

static const char dbfile[] = "dbFile-jsondb-partition";

const QString qmlProgram = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.ChangesSince { "
                "id:changessinceObject;"
                "partition :JsonDb.Partition { "
                    "name: \"com.nokia.shared\";"
                "}"
            "}");

const QString qmlProgramForPartition = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.Partition { "
                "id: sharedPartition;"
                "name: \"com.nokia.shared\";"
            "}");

TestJsonDbChangesSinceObject::TestJsonDbChangesSinceObject()
    : mTimedOut(false)
{
    qsrand(QTime::currentTime().msec());
}

TestJsonDbChangesSinceObject::~TestJsonDbChangesSinceObject()
{
}

void TestJsonDbChangesSinceObject::timeout()
{
    ClientWrapper::timeout();
    mTimedOut = true;
}

void TestJsonDbChangesSinceObject::deleteDbFiles()
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

void TestJsonDbChangesSinceObject::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << "-base-name" << dbfile);

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

ComponentData *TestJsonDbChangesSinceObject::createComponent()
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
    componentData->component->setData(qmlProgram.toUtf8(), QUrl());
    componentData->qmlElement = componentData->component->create();
    if (componentData->component->isError())
        qDebug() << componentData->component->errors();
    QObject::connect(componentData->qmlElement, SIGNAL(finished()),
                     this, SLOT(finishedSlot()));
    QObject::connect(componentData->qmlElement, SIGNAL(errorChanged(const QVariantMap&)),
                     this, SLOT(errorSlot(const QVariantMap&)));
    mComponents.append(componentData);
    return componentData;
}

ComponentData *TestJsonDbChangesSinceObject::createPartitionComponent()
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
    componentData->component->setData(qmlProgramForPartition.toUtf8(), QUrl());
    componentData->qmlElement = componentData->component->create();
    if (componentData->component->isError())
        qDebug() << componentData->component->errors();
    mComponents.append(componentData);
    return componentData;
}

void TestJsonDbChangesSinceObject::deleteComponent(ComponentData *componentData)
{
    mComponents.removeOne(componentData);
    if (componentData)
        delete componentData;
}

void TestJsonDbChangesSinceObject::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

void TestJsonDbChangesSinceObject::errorSlot(const QVariantMap &newError)
{
    int code = newError.value("code").toInt();
    QString message = newError.value("message").toString();
    callbackError = true;
    callbackErrorCode = code;
    callbackErrorMessage = message;
    mEventLoop2.quit();
}

void TestJsonDbChangesSinceObject::finishedSlot()
{
    QMetaObject::invokeMethod(currentQmlElement, "takeResults", Qt::DirectConnection,
                              Q_RETURN_ARG(QVariantList, cbData));
    mEventLoop2.quit();
}

bool posLessThan(const QVariant &v1, const QVariant &v2)
{
    QVariantMap v1Map = v1.toMap();
    QVariantMap v2Map = v2.toMap();

    return v1Map.value("pos").toInt() < v2Map.value("pos").toInt();
}

void TestJsonDbChangesSinceObject::singleType()
{
    ComponentData *changesSinceObject = createComponent();
    if (!changesSinceObject || !changesSinceObject->qmlElement) return;

    const QString typeString( __FUNCTION__ );
    changesSinceObject->qmlElement->setProperty("types", QStringList(typeString));
    currentQmlElement = changesSinceObject->qmlElement;

    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    int id = mClient->create(item,  "com.nokia.shared");
    waitForResponse1(id);
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(changesSinceObject->engine->rootContext(), changesSinceObject->qmlElement, expression);
    expr->evaluate();
    waitForCallback2();
    QCOMPARE(cbData.size(), 1);
    QCOMPARE(callbackError, false);
    delete expr;
    deleteComponent(changesSinceObject);
}

void TestJsonDbChangesSinceObject::multipleObjects()
{
    ComponentData *changesSinceObject = createComponent();
    if (!changesSinceObject || !changesSinceObject->qmlElement) return;

    const QString typeString( __FUNCTION__ );
    changesSinceObject->qmlElement->setProperty("types", QStringList(typeString));
    currentQmlElement = changesSinceObject->qmlElement;

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 10).toList();
    int id = mClient->create(QVariant(items),  "com.nokia.shared");
    waitForResponse1(id);
    qSort(items.begin(), items.end(), posLessThan);
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(changesSinceObject->engine->rootContext(), changesSinceObject->qmlElement, expression);
    expr->evaluate();
    waitForCallback2();
    QCOMPARE(callbackError, false);
    QCOMPARE(cbData.size(), 10);
    QVariantList result;
    for (int i = 0; i<10; i++) {
        QVariantMap obj = cbData[i].toMap();
        result.append(obj.value("after"));
    }
    qSort(result.begin(), result.end(), posLessThan);
    for (int i = 0; i<10; i++) {
        QVariantMap item = items[i].toMap();
        QVariantMap obj = result[i].toMap();
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
    }

    delete expr;
    deleteComponent(changesSinceObject);
}

void TestJsonDbChangesSinceObject::multipleTypes()
{
    ComponentData *changesSinceObject = createComponent();
    if (!changesSinceObject || !changesSinceObject->qmlElement) return;

    QStringList types;
    types << __FUNCTION__
          <<QString(__FUNCTION__)+".2";
    changesSinceObject->qmlElement->setProperty("types", types);
    currentQmlElement = changesSinceObject->qmlElement;

    //Create an object
    QVariantMap item = createObject(types[0]).toMap();
    int id = mClient->create(item,  "com.nokia.shared");
    waitForResponse1(id);
    item = createObject(types[1]).toMap();
    id = mClient->create(item,  "com.nokia.shared");
    waitForResponse1(id);
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(changesSinceObject->engine->rootContext(), changesSinceObject->qmlElement, expression);
    expr->evaluate();
    waitForCallback2();
    QCOMPARE(cbData.size(), 2);
    QCOMPARE(callbackError, false);
    delete expr;
    deleteComponent(changesSinceObject);
}

void TestJsonDbChangesSinceObject::createChangesSince()
{
    const QString createString = QString("createChangesSince(0, [\"%1\"]);");
    ComponentData *partition = createPartitionComponent();
    if (!partition || !partition->qmlElement) return;
    QString expression;

    expression = QString(createString).arg(__FUNCTION__);
    QQmlExpression *expr;
    expr = new QQmlExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    QPointer<QObject> queryObject = expr->evaluate().value<QObject*>();
    QVERIFY(!queryObject.isNull());
    queryObject->setParent(partition->qmlElement);
    currentQmlElement = queryObject;
    QObject::connect(currentQmlElement, SIGNAL(finished()),
                     this, SLOT(finishedSlot()));
    QObject::connect(currentQmlElement, SIGNAL(errorChanged(const QVariantMap&)),
                     this, SLOT(errorSlot(const QVariantMap&)));
    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    mClient->create(item,  "com.nokia.shared");
    QMetaObject::invokeMethod(currentQmlElement, "start", Qt::DirectConnection);
    cbData.clear();
    waitForCallback2();
    QCOMPARE(cbData.size(), 1);
    QCOMPARE(callbackError, false);
    delete expr;
    deleteComponent(partition);
}

QTEST_MAIN(TestJsonDbChangesSinceObject)

