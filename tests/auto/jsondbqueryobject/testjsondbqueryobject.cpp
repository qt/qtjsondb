/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include <QtTest/QtTest>
#include <QJSEngine>
#include "testjsondbqueryobject.h"
#include "../../shared/util.h"
#include <QJSValueIterator>
#include <QDir>
#include "json.h"

static const char dbfile[] = "dbFile-jsondb-partition";

const QString qmlProgram = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.Query { "
                "id:queryObject;"
                "partition :JsonDb.Partition { "
                    "name: \"com.nokia.shared\";"
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


TestJsonDbQueryObject::TestJsonDbQueryObject()
    : mTimedOut(false)
{
    qsrand(QTime::currentTime().msec());
    QDeclarativeEngine *engine = new QDeclarativeEngine();
    QStringList pluginPaths = engine->importPathList();
    for (int i=0; (i<pluginPaths.count() && mPluginPath.isEmpty()); i++) {
        QDir dir(pluginPaths[i]+"/QtJsonDb");
        dir.setFilter(QDir::Files | QDir::NoSymLinks);
        QFileInfoList list = dir.entryInfoList();
        for (int i = 0; i < list.size(); ++i) {
            QString error;
            if (engine->importPlugin(list.at(i).absoluteFilePath(), QString("QtJsonDb"), &error)) {
                mPluginPath = list.at(i).absoluteFilePath();
                break;
            }
        }
    }
    delete engine;
}

TestJsonDbQueryObject::~TestJsonDbQueryObject()
{
}

void TestJsonDbQueryObject::timeout()
{
    ClientWrapper::timeout();
    mTimedOut = true;
}

void TestJsonDbQueryObject::deleteDbFiles()
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

void TestJsonDbQueryObject::initTestCase()
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

    // Create the shared Partitions
    QVariantMap item;
    item.insert("_type", "Partition");
    item.insert("name", "com.nokia.shared");
    int id = mClient->create(item, QString("com.nokia.qtjsondb.System"));
    waitForResponse1(id);

}

ComponentData *TestJsonDbQueryObject::createComponent()
{
    ComponentData *componentData = new ComponentData();
    componentData->engine = new QDeclarativeEngine();
    QString error;
    if (!componentData->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &error)) {
        qDebug()<<"Unable to load the plugin :"<<error;
        delete componentData->engine;
        return 0;
    }
    componentData->component = new QDeclarativeComponent(componentData->engine);
    componentData->component->setData(qmlProgram.toLocal8Bit(), QUrl());
    componentData->qmlElement = componentData->component->create();
    if (componentData->component->isError())
        qDebug() << componentData->component->errors();
    QObject::connect(componentData->qmlElement, SIGNAL(finished()),
                     this, SLOT(finishedSlot()));
    QObject::connect(componentData->qmlElement, SIGNAL(error(int, QString)),
                     this, SLOT(errorSlot(int, QString)));
    mComponents.append(componentData);
    return componentData;
}

ComponentData *TestJsonDbQueryObject::createPartitionComponent()
{
    ComponentData *componentData = new ComponentData();
    componentData->engine = new QDeclarativeEngine();
    QString error;
    if (!componentData->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &error)) {
        qDebug()<<"Unable to load the plugin :"<<error;
        delete componentData->engine;
        return 0;
    }
    componentData->component = new QDeclarativeComponent(componentData->engine);
    componentData->component->setData(qmlProgramForPartition.toLocal8Bit(), QUrl());
    componentData->qmlElement = componentData->component->create();
    if (componentData->component->isError())
        qDebug() << componentData->component->errors();
    mComponents.append(componentData);
    return componentData;
}

void TestJsonDbQueryObject::deleteComponent(ComponentData *componentData)
{
    mComponents.removeOne(componentData);
    if (componentData)
        delete componentData;
}

void TestJsonDbQueryObject::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

void TestJsonDbQueryObject::errorSlot(int code, const QString &message)
{
    callbackError = true;
    callbackErrorCode = code;
    callbackErrorMessage = message;
    mEventLoop2.quit();
}

void TestJsonDbQueryObject::finishedSlot()
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

void TestJsonDbQueryObject::singleObject()
{
    ComponentData *queryObject = createComponent();
    if (!queryObject || !queryObject->qmlElement) return;

    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"]");
    queryObject->qmlElement->setProperty("query", queryString);
    currentQmlElement = queryObject->qmlElement;

    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    mClient->create(item,  "com.nokia.shared");
    const QString expression("exec();");
    QDeclarativeExpression *expr;
    expr = new QDeclarativeExpression(queryObject->engine->rootContext(), queryObject->qmlElement, expression);
    expr->evaluate();
    waitForCallback2();
    QCOMPARE(cbData.size(), 1);
    QCOMPARE(callbackError, false);
    delete expr;
    deleteComponent(queryObject);
}

void TestJsonDbQueryObject::multipleObjects()
{
    ComponentData *queryObject = createComponent();
    if (!queryObject || !queryObject->qmlElement) return;

    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"][/pos]");
    queryObject->qmlElement->setProperty("query", queryString);
    currentQmlElement = queryObject->qmlElement;

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 10).toList();
    mClient->create(QVariant(items),  "com.nokia.shared");
    qSort(items.begin(), items.end(), posLessThan);
    const QString expression("exec();");
    QDeclarativeExpression *expr;
    expr = new QDeclarativeExpression(queryObject->engine->rootContext(), queryObject->qmlElement, expression);
    expr->evaluate();
    waitForCallback2();
    QCOMPARE(callbackError, false);
    QCOMPARE(cbData.size(), 10);
    for (int i = 0; i<10; i++) {
        QVariantMap item = items[i].toMap();
        QVariantMap obj = cbData[i].toMap();
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
    }

    delete expr;
    deleteComponent(queryObject);
}

void TestJsonDbQueryObject::createQuery()
{
    const QString createString = QString("createQuery('[?_type=\"%1\"]', 0, -1, null);");
    ComponentData *partition = createPartitionComponent();
    if (!partition || !partition->qmlElement) return;
    QString expression;

    expression = QString(createString).arg(__FUNCTION__);
    QDeclarativeExpression *expr;
    expr = new QDeclarativeExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    QPointer<QObject> queryObject = expr->evaluate().value<QObject*>();
    QVERIFY(!queryObject.isNull());
    queryObject->setParent(partition->qmlElement);
    currentQmlElement = queryObject;
    QObject::connect(currentQmlElement, SIGNAL(finished()),
                     this, SLOT(finishedSlot()));
    QObject::connect(currentQmlElement, SIGNAL(error(int, QString)),
                     this, SLOT(errorSlot(int, QString)));
    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    mClient->create(item,  "com.nokia.shared");
    QMetaObject::invokeMethod(currentQmlElement, "exec", Qt::DirectConnection);
    cbData.clear();
    waitForCallback2();
    QCOMPARE(cbData.size(), 1);
    QCOMPARE(callbackError, false);
    delete expr;
    deleteComponent(partition);
}

QTEST_MAIN(TestJsonDbQueryObject)

