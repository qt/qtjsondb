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
#include "testjsondbnotification.h"

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

#define waitForCallbackGeneric(eventloop) \
{ \
    QTimer timer; \
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout())); \
    QObject::connect(&timer, SIGNAL(timeout()), &eventloop, SLOT(quit())); \
    timer.start(mClientTimeout);                                       \
    mElapsedTimer.start(); \
    mTimedOut = false;\
    callbackError = false; \
    eventloop.exec(QEventLoop::AllEvents); \
    QCOMPARE(false, mTimedOut); \
}

#define waitForCallback() waitForCallbackGeneric(mEventLoop2)

const QString qmlProgram = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.Notification { "
                "signal notificationSignal(variant result, int action, int stateNumber);"
                "partition :JsonDb.Partition { "
                    "name: \"com.nokia.shared\";"
                "}"
                "onNotification: {"
                    "notificationSignal(result, action, stateNumber);"
                "}"
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


TestJsonDbNotification::TestJsonDbNotification()
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

QVariant TestJsonDbNotification::readJsonFile(const QString& filename)
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

    // Create the shared Partitions
    QVariantMap item;
    item.insert("_type", "Partition");
    item.insert("name", "com.nokia.shared");
    int id = mClient->create(item, QString("com.nokia.qtjsondb.System"));
    waitForResponse1(id);

}

ComponentData *TestJsonDbNotification::createComponent()
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
    QObject::connect(componentData->qmlElement, SIGNAL(notificationSignal(QVariant, int, int)),
                         this, SLOT(notificationSlot(QVariant, int, int)));
    QObject::connect(componentData->qmlElement, SIGNAL(error(int, QString)),
                         this, SLOT(errorSlot(int, QString)));
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
    callbackResult = result.toMap();
    callbackAction = action;
    callbackStateNumber = stateNumber;
    mEventLoop2.quit();
}

void TestJsonDbNotification::errorSlot(int code, const QString &message)
{
    callbackError = true;
    callbackErrorCode = code;
    callbackErrorMessage = message;
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

    //Create an object
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    mClient->create(item,  "com.nokia.shared");
    waitForCallback();
    QCOMPARE(callbackAction, 1);
    QCOMPARE(callbackResult.value("name"), item.value("name"));
    QString uuid = callbackResult.value("_uuid").toString();
    //update the object
    QString newName = callbackResult.value("name").toString()+QString("**");
    callbackResult.insert("name", newName);
    mClient->update(callbackResult,  "com.nokia.shared");
    waitForCallback();
    QCOMPARE(callbackAction, 2);
    QCOMPARE(callbackResult.value("name").toString(), newName);
    //Remove the object
    mClient->remove(callbackResult,  "com.nokia.shared");
    waitForCallback();
    QCOMPARE(callbackAction, 4);
    QCOMPARE(callbackResult.value("_uuid").toString(), uuid);
    deleteComponent(notification);
}

QTEST_MAIN(TestJsonDbNotification)

