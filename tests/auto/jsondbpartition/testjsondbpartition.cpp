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
#include "testjsondbpartition.h"

#include "../../shared/util.h"
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDeclarativeContext>
#include <QJSValueIterator>
#include <QDir>
#include "json.h"
#include <QDeclarativeExpression>
#include <QDeclarativeError>

static const char dbfile[] = "dbFile-jsondb-partition";

#define waitForCallback() \
{ \
    QTimer timer; \
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout())); \
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop, SLOT(quit())); \
    timer.start(mClientTimeout);                                       \
    mElapsedTimer.start(); \
    mTimedOut = false;\
    mEventLoop.exec(QEventLoop::AllEvents); \
    QCOMPARE(false, mTimedOut); \
}

const QString qmlProgram = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.Partition { "
                "signal callbackSignal(bool error, variant meta, variant response);"
                "id: sharedPartition;"
                "name: \"com.nokia.shared.1\";"
            "}");

ComponentData::ComponentData(): engine(0), component(0), qmlElement(0)
{
}

ComponentData::~ComponentData()
{
    if (qmlElement)
        delete qmlElement;
    if (component)
        delete component;
    if (engine)
        delete engine;
}

QVariant createObject(const QString& functionName)
{
    static QStringList greekAlphabets;
    static int size = 0;
    if (greekAlphabets.isEmpty()) {
        greekAlphabets  << "alpha" << "beta" << "gamma" << "epsilon" << "zeta"
                        << "eta" << "theta"<< "iota" << "kappa"  << "lambda";
        size = greekAlphabets.size();
    }

    QVariantMap obj;
    obj.insert("_type", functionName);
    int position = qrand()%size;
    obj.insert("alphabet", greekAlphabets[position]);
    obj.insert("pos", position);
    return obj;
}

QVariant createObjectList(const QString& functionName, int size)
{
    QVariantList list;
    for (int i = 0; i<size; i++) {
        list.append(createObject(functionName));
    }
    return list;
}

QVariant updateObjectList(QVariantList objects, QVariantList extra)
{
    QVariantList list;
    for (int i = 0; i<objects.size(); i++) {
        QVariantMap objMap = objects[i].toMap();
        QVariantMap extraMap = extra[i].toMap();
        QVariantMap::Iterator j = extraMap.begin();
        while (j != extraMap.end()) {
            objMap.insert(j.key(), j.value());
            ++j;
        }
        list.append(objMap);
    }
    return list;
}

QVariant updateObject(QVariant object, QVariantList extra)
{
    QVariantMap objMap = object.toMap();
    QVariantMap extraMap = extra[0].toMap();
    QVariantMap::Iterator j = extraMap.begin();
    while (j != extraMap.end()) {
        objMap.insert(j.key(), j.value());
        ++j;
    }
    return objMap;
}


QString objectString(QString key, QVariant value)
{
    QString fullObject;
    if (!key.isEmpty()) {
        fullObject = QString("%1:").arg(key);
    }
    if (value.type() == QVariant::Map) {
        fullObject += "{";
        QVariantMap map = value.toMap();
        QVariantMap::Iterator i = map.begin();
        int count = 0;
        while (i != map.end()) {
            if (count)
                fullObject += ",";
            fullObject += objectString(i.key(),i.value());
            ++i;
            ++count;
        }
        fullObject += "}";
    } else if (value.type() == QVariant::List) {
        fullObject += "[";
        QVariantList list = value.toList();
        QVariantList::Iterator i = list.begin();
        int count = 0;
        while (i != list.end()) {
            if (count)
                fullObject += ",";
            fullObject += objectString(QString(),*i);
            ++i;
            ++count;
        }
        fullObject += "]";

    } else {
        fullObject += QString("'%1'").arg(value.toString());
    }
    return fullObject;
}


TestJsonDbPartition::TestJsonDbPartition()
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
    QString filepath = findFile(SRCDIR, filename);
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

    // Create the shared Partitions
    QVariantMap item;
    item.insert("_type", "Partition");
    item.insert("name", "com.nokia.shared.1");
    int id = mClient->create(item, QString("com.nokia.qtjsondb.System"));
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
    QObject::connect(componentData->qmlElement, SIGNAL(callbackSignal(bool, QVariant, QVariant)),
                         this, SLOT(callbackSlot(bool, QVariant, QVariant)));

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

void TestJsonDbPartition::callbackSlot(bool error, QVariant meta, QVariant response)
{
    callbackError = error;
    callbackMeta = meta;
    callbackResponse = response;
    mEventLoop.quit();
}

// JsonDb.Partition.create()
void TestJsonDbPartition::create()
{
    const QString createString = QString("create(%1, function (error, meta, response) {callbackSignal(error, meta, response);});");
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
    const QString createString = QString("create(%1, function (error, meta, response) {callbackSignal(error, meta, response);});");
    const QString updateString = QString("update(%1, function (error, meta, response) {callbackSignal(error, meta, response);});");
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
    const QString createString = QString("create(%1, function (error, meta, response) {callbackSignal(error, meta, response);});");
    const QString removeString = QString("remove(%1, function (error, meta, response) {callbackSignal(error, meta, response);});");
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
    const QString createString = QString("create(%1, function (error, meta, response) {callbackSignal(error, meta, response);});");
    const QString findString = QString("find('[?_type = \""+QString( __FUNCTION__ )+"\"]', function (error, meta, response) {callbackSignal(error, meta, response);});");
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

// JsonDb.Partition.changesSince()
void TestJsonDbPartition::changesSince()
{
    const QString createString = QString("create(%1, function (error, meta, response) {callbackSignal(error, meta, response);});");
    const QString findString = QString("find('[?_type = \""+QString( __FUNCTION__ )+"\"]', function (error, meta, response) {callbackSignal(error, meta, response);});");
    const QString changesString = QString("changesSince(%1, '{types : \""+QString( __FUNCTION__ )+"\"}', function (error, meta, response) {callbackSignal(error, meta, response);});");
    ComponentData *partition = createComponent();
    if (!partition || !partition->qmlElement) return;
    QVariant obj;
    QString expression;
    int currentStateNumber = 0;

    //Retrieve the currentStateNumber
    expression = QString(changesString).arg(currentStateNumber);
    QDeclarativeExpression *expr;
    int id = 0;
    expr = new QDeclarativeExpression(partition->engine->rootContext(), partition->qmlElement, expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    currentStateNumber = callbackMeta.toMap()["currentStateNumber"].toInt();

    obj = createObject(__FUNCTION__);
    expression = QString(createString).arg(objectString(QString(), obj));
    expr->setExpression(expression);
    id = expr->evaluate().toInt();
    waitForCallback();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackMeta.toMap()["id"].toInt(), id);
    QCOMPARE(callbackResponse.toList().size(), 1);

    expression = QString(changesString).arg(currentStateNumber);
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

    expression = QString(changesString).arg(currentStateNumber);
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

