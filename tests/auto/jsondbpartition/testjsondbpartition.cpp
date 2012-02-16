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
#include "testjsondbpartition.h"
#include "../../shared/util.h"
#include <QJSValueIterator>
#include "json.h"

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
}

void TestJsonDbPartition::timeout()
{
    ClientWrapper::timeout();
    mTimedOut = true;
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

QVariant TestJsonDbPartition::readJsonFile(const QString& filename)
{
    QString filepath = findFile(filename);
    QFile jsonFile(filepath);
    jsonFile.open(QIODevice::ReadOnly);
    QByteArray json = jsonFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok) {
      qDebug() << filepath << parser.errorString();
    }
    return parser.result();
}

void TestJsonDbPartition::initTestCase()
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
    item.insert("name", "com.nokia.shared.1");
    int id = mClient->create(item);
    waitForResponse1(id);

}

ComponentData *TestJsonDbPartition::createComponent()
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
    mEventLoop.quit();
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

    QDeclarativeExpression *expr;
    int id = 0;
    expr = new QDeclarativeExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
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

    QDeclarativeExpression *expr;
    int id = 0;
    expr = new QDeclarativeExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback();
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
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
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
    waitForCallback();
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

    QDeclarativeExpression *expr;
    int id = 0;
    expr = new QDeclarativeExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //Remove this object
    obj = callbackResponse.toList();
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //Remove all objects
    obj = callbackResponse.toList();
    expression = QString(removeString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
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

    QDeclarativeExpression *expr;
    int id = 0;
    expr = new QDeclarativeExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    //find this object
    expression = findString;
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    obj = createObjectList(__FUNCTION__, 5);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 5);

    //Find all objects
    expression = findString;
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 6);

    delete expr;
    deleteComponent(partition);
}

QTEST_MAIN(TestJsonDbPartition)

