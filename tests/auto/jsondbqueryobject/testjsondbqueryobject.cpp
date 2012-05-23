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
#include "testjsondbqueryobject.h"
#include "../../shared/util.h"

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

const QString qmlProgramForQueryWithoutPartition = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.Query { "
                "id:queryObject;"
            "}");

TestJsonDbQueryObject::TestJsonDbQueryObject()
    : mTimedOut(false)
{
    qsrand(QTime::currentTime().msec());
}

TestJsonDbQueryObject::~TestJsonDbQueryObject()
{
    connection = 0;
}

void TestJsonDbQueryObject::timeout()
{
    RequestWrapper::timeout();
    mTimedOut = true;
    eventLoop1.quit();
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
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList(), __FILE__);

    connection = QJsonDbConnection::defaultConnection();
    connection->connectToServer();

    mPluginPath = findQMLPluginPath("QtJsonDb");
    if (mPluginPath.isEmpty())
        qDebug() << "Couldn't find the plugin path for the plugin QtJsonDb";
}

ComponentData *TestJsonDbQueryObject::createComponent(const QString &qml)
{
    QString tmp = qml;
    if (tmp.isEmpty())
        tmp = qmlProgram;
    ComponentData *componentData = new ComponentData();
    componentData->engine = new QQmlEngine();
    QList<QQmlError> errors;
    if (!componentData->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &errors)) {
        qDebug()<<"Unable to load the plugin :"<<errors;
        delete componentData->engine;
        return 0;
    }
    componentData->component = new QQmlComponent(componentData->engine);
    componentData->component->setData(tmp.toLocal8Bit(), QUrl());
    componentData->qmlElement = componentData->component->create();
    if (componentData->component->isError())
        qDebug() << componentData->component->errors();
    QObject::connect(componentData->qmlElement, SIGNAL(finished()),
                     this, SLOT(finishedSlot()));
    QObject::connect(componentData->qmlElement, SIGNAL(errorChanged(QVariantMap)),
                     this, SLOT(errorSlot(QVariantMap)));
    mComponents.append(componentData);
    return componentData;
}

ComponentData *TestJsonDbQueryObject::createPartitionComponent()
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

void TestJsonDbQueryObject::errorSlot(const QVariantMap &newError)
{
    int code = newError.value("code").toInt();
    QString message = newError.value("message").toString();
    callbackError = true;
    callbackErrorCode = code;
    callbackErrorMessage = message;
    eventLoop1.quit();
}

namespace {
static QVariant qjsvalue_to_qvariant(const QJSValue &value)
{
    if (value.isQObject()) {
        // We need the QVariantMap & not the QObject wrapper
        return qjsvalue_cast<QVariantMap>(value);
    } else {
        // Converts to either a QVariantList or a QVariantMap
        return value.toVariant();
    }
}
}

void TestJsonDbQueryObject::finishedSlot()
{
    QJSValue cbData;
    QMetaObject::invokeMethod(currentQmlElement, "takeResults", Qt::DirectConnection,
                              Q_RETURN_ARG(QJSValue, cbData));
    callbackData = qjsvalue_to_qvariant(cbData).toList();
    eventLoop1.quit();
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
    create(item,  "com.nokia.shared");
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(queryObject->engine->rootContext(), queryObject->qmlElement, expression);
    expr->evaluate();
    callbackData.clear();
    waitForCallback1();
    QCOMPARE(callbackData.size(), 1);
    QCOMPARE(callbackError, false);
    delete expr;
    deleteComponent(queryObject);
}

void TestJsonDbQueryObject::multipleObjects()
{
    ComponentData *queryObject = createComponent();
    if (!queryObject || !queryObject->qmlElement) return;

    // define index on pos property
    QVariantMap index;
    index.insert("_type", "Index");
    index.insert("propertyName", "pos");
    index.insert("propertyType", "number");
    int id = create(index, "com.nokia.shared");
    waitForResponse1(id);

    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"][/pos]");
    queryObject->qmlElement->setProperty("query", queryString);
    currentQmlElement = queryObject->qmlElement;

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 10).toList();
    create(items,  "com.nokia.shared");
    qSort(items.begin(), items.end(), posLessThan);
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(queryObject->engine->rootContext(), queryObject->qmlElement, expression);
    expr->evaluate();
    callbackData.clear();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackData.size(), 10);
    for (int i = 0; i<10; i++) {
        QVariantMap item = items[i].toMap();
        QVariantMap obj = callbackData[i].toMap();
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
    }

    delete expr;
    deleteComponent(queryObject);
}

void TestJsonDbQueryObject::createQuery()
{
    const QString createString = QString("createQuery('[?_type=\"%1\"]', -1, {});");
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
    QObject::connect(currentQmlElement, SIGNAL(errorChanged(QVariantMap)),
                     this, SLOT(errorSlot(QVariantMap)));
    //Create an object
    QVariantMap item = createObject(__FUNCTION__).toMap();
    int id = create(item,  "com.nokia.shared");
    waitForResponse1(id);

    QMetaObject::invokeMethod(currentQmlElement, "start", Qt::DirectConnection);
    callbackData.clear();
    waitForCallback1();
    QCOMPARE(callbackData.size(), 1);
    QCOMPARE(callbackError, false);
    delete expr;
    deleteComponent(partition);
}

void TestJsonDbQueryObject::queryWithoutPartition()
{
    ComponentData *queryObject = createComponent(qmlProgramForQueryWithoutPartition);
    if (!queryObject || !queryObject->qmlElement) return;

    // define index on pos property
    QVariantMap index;
    index.insert("_type", "Index");
    index.insert("propertyName", "pos");
    index.insert("propertyType", "number");
    int id = create(index);
    waitForResponse1(id);

    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"][/pos]");
    queryObject->qmlElement->setProperty("query", queryString);
    currentQmlElement = queryObject->qmlElement;

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 10).toList();
    create(items);
    qSort(items.begin(), items.end(), posLessThan);
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(queryObject->engine->rootContext(), queryObject->qmlElement, expression);
    expr->evaluate();
    callbackData.clear();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackData.size(), 10);
    for (int i = 0; i<10; i++) {
        QVariantMap item = items[i].toMap();
        QVariantMap obj = callbackData[i].toMap();
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
    }

    delete expr;
    deleteComponent(queryObject);
}

void TestJsonDbQueryObject::queryBinding()
{
    ComponentData *queryObject = createComponent();
    if (!queryObject || !queryObject->qmlElement) return;

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 1).toList();

    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"][?pos=%posValue]");
    queryObject->qmlElement->setProperty("query", queryString);
    QVariantMap bindingMap;
    bindingMap.insert("posValue", items[0].toMap().value("pos"));
    queryObject->qmlElement->setProperty("bindings", bindingMap);
    currentQmlElement = queryObject->qmlElement;

    create(items,  "com.nokia.shared");
    qSort(items.begin(), items.end(), posLessThan);
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(queryObject->engine->rootContext(), queryObject->qmlElement, expression);
    expr->evaluate();
    callbackData.clear();
    waitForCallback1();

    QCOMPARE(callbackError, false);
    QCOMPARE(callbackData.size(), 1);

    QVariantMap item = items[0].toMap();
    QVariantMap obj = callbackData[0].toMap();
    QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
    delete expr;
    deleteComponent(queryObject);
}

void TestJsonDbQueryObject::queryError()
{
    ComponentData *queryObject = createComponent();
    if (!queryObject || !queryObject->qmlElement) return;

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 1).toList();
    //Query contains an invalid palaceholder marker
    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"][?pos=1%posValue]");
    queryObject->qmlElement->setProperty("query", queryString);
    QVariantMap bindingMap;
    bindingMap.insert("posValue", items[0].toMap().value("pos"));
    queryObject->qmlElement->setProperty("bindings", bindingMap);
    currentQmlElement = queryObject->qmlElement;

    create(items,  "com.nokia.shared");
    qSort(items.begin(), items.end(), posLessThan);
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(queryObject->engine->rootContext(), queryObject->qmlElement, expression);
    expr->evaluate();
    callbackData.clear();
    waitForCallback1();

    QCOMPARE(callbackError, true);
    delete expr;
    deleteComponent(queryObject);
}

void TestJsonDbQueryObject::queryLimit()
{
    ComponentData *queryObject = createComponent();
    if (!queryObject || !queryObject->qmlElement) return;

    // define index on pos property
    QVariantMap index;
    index.insert("_type", "Index");
    index.insert("propertyName", "pos");
    index.insert("propertyType", "number");
    int id = create(index, "com.nokia.shared");
    waitForResponse1(id);

    const QString queryString = QString("[?_type = \""+QString( __FUNCTION__ )+"\"][/pos]");
    queryObject->qmlElement->setProperty("query", queryString);
    queryObject->qmlElement->setProperty("limit", 5);
    currentQmlElement = queryObject->qmlElement;

    //Create objects
    QVariantList items = createObjectList(__FUNCTION__, 10).toList();
    create(items,  "com.nokia.shared");
    qSort(items.begin(), items.end(), posLessThan);
    const QString expression("start();");
    QQmlExpression *expr;
    expr = new QQmlExpression(queryObject->engine->rootContext(), queryObject->qmlElement, expression);
    expr->evaluate();
    callbackData.clear();
    waitForCallback1();
    QCOMPARE(callbackError, false);
    QCOMPARE(callbackData.size(), 5);
    for (int i = 0; i<5; i++) {
        QVariantMap item = items[i].toMap();
        QVariantMap obj = callbackData[i].toMap();
        QCOMPARE(obj.value("alphabet"), item.value("alphabet"));
    }

    delete expr;
    deleteComponent(queryObject);
}

QTEST_MAIN(TestJsonDbQueryObject)

