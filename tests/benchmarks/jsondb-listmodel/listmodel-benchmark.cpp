/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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
#include "listmodel-benchmark.h"

#include "../../shared/util.h"
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDeclarativeContext>

static const char dbfile[] = "dbFile-test-jsondb";
ModelData::ModelData(): engine(0), component(0), model(0)
{
}

ModelData::~ModelData()
{
    if (model)
        delete model;
    if (component)
        delete component;
    if (engine)
        delete engine;
}

TestListModel::TestListModel()
    : mProcess(0)
{
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

TestListModel::~TestListModel()
{
}

void TestListModel::connectListModel(JsonDbListModel *model)
{
    connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this , SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));
    connect(model, SIGNAL(layoutChanged()), this, SLOT(layoutChanged()));
    connect(model, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(rowsInserted(QModelIndex, int, int)));
    connect(model, SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(rowsRemoved(QModelIndex, int, int)));
    connect(model, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)), this, SLOT(rowsMoved(QModelIndex, int, int, QModelIndex, int)));
}

void TestListModel::deleteDbFiles()
{
    // remove all the test files.
    QDir currentDir;
    QStringList nameFilter;
    nameFilter << QString("*.db");
    QFileInfoList databaseFiles = currentDir.entryInfoList(nameFilter, QDir::Files);
    foreach (QFileInfo fileInfo, databaseFiles) {
        //qDebug() << "Deleted : " << fileInfo.fileName();
        QFile file(fileInfo.fileName());
        file.remove();
    }
}

void TestListModel::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << dbfile);

    mClient = new JsonDbClient(this);
    QVERIFY(mClient!= 0);
    connectToServer();

    // Create alot of items in the database
    QVariantList friendsList;
    for (int i=0; i<1000; i++) {
        QVariantMap item;
        item.insert("_type", "Friends");
        item.insert("name", QString("Name-%1").arg(i));
        item.insert("phone",QString("%1").arg(qrand()));
        friendsList << item;
    }
    mId = mClient->create(friendsList);

    QVariantList ImageList;
    for (int i=0; i<1000; i++) {
        QVariantMap item;
        item.insert("_type", "Image");
        item.insert("name", QString("Name-%1.jpg").arg(i));
        item.insert("location",QString("/home/qt/Pictures/Myfolder-%1").arg(i));
        ImageList << item;
    }
    mId = mClient->create(ImageList);

    QVariantList numberList;
    for (int i=0; i<1000; i++) {
        QVariantMap item;
        item.insert("_type", "RandNumber");
        item.insert("number", qrand()%100);
        numberList << item;
    }
    mId = mClient->create(numberList);

    QVariantList trollList;
    for (int i=0; i<100; i++) {
        QVariantMap item;
        item.insert("_type", "Troll");
        item.insert("age", i);
        item.insert("name", QString("Troll-%1").arg(i));
        trollList << item;
    }
    mId = mClient->create(trollList);

    mEventLoop.exec(QEventLoop::AllEvents);
}

JsonDbListModel *TestListModel::createModel()
{
    ModelData *newModel = new ModelData();
    newModel->engine = new QDeclarativeEngine();
    QString error;
    if (!newModel->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &error)) {
        qDebug()<<"Unable to load the plugin :"<<error;
        delete newModel->engine;
        return 0;
    }
    newModel->component = new QDeclarativeComponent(newModel->engine);
    newModel->component->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n JsonDb.JsonDbListModel {id: contactsModel}", QUrl());
    newModel->model = newModel->component->create();
    mModels.append(newModel);
    return (JsonDbListModel*)(newModel->model);
}

void TestListModel::deleteModel(JsonDbListModel *model)
{
    for (int i = 0; i < mModels.count(); i++) {
        if (mModels[i]->model == model) {
            ModelData *modelData = mModels.takeAt(i);
            delete modelData;
            return;
        }
    }
}

void TestListModel::cleanupTestCase()
{
    if (mClient) {
        delete mClient;
        mClient = NULL;
    }

    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
        mProcess = NULL;
    }
    deleteDbFiles();
}

void TestListModel::notified(const QString& notifyUuid, const QVariant& object, const QString& action)
{
    Q_UNUSED(notifyUuid);
    Q_UNUSED(object);
    Q_UNUSED(action);
    //qDebug() << Q_FUNC_INFO << "notifyUuid=" << notifyUuid << "action=" << action << "object=" << object;
    //mEventLoop.exit(0);
}

void TestListModel::response(int id, const QVariant& data)
{
    //qDebug() << Q_FUNC_INFO << "id: " << id << data;
    QMap<QString,QVariant> map = data.toMap();
    mLastUuid = map.value("_uuid").toString();
    mLastResponseData = data;
    if (mId == id)
        mEventLoop.exit(0);
}

void TestListModel::error(int id, int code, const QString& message)
{
    qDebug() << Q_FUNC_INFO << "id:" << id << "code:" << code << "message:" << message;
    if (mId == id)
        mEventLoop.exit(0);
}

void TestListModel::dataChanged(QModelIndex, QModelIndex)
{
    //qDebug() << "dataChanged(QModelIndex,QModelIndex)";
    mEventLoop.exit(0);
}

void TestListModel::modelReset()
{
    //qDebug() << "modelReset()";
    mEventLoop.exit(0);
}

void TestListModel::layoutChanged()
{
    //qDebug() << "layoutChanged()";
    mEventLoop.exit(0);
}

void TestListModel::rowsInserted(QModelIndex parent, int start , int end)
{
    Q_UNUSED(parent);
    qDebug() << QString("rowsInserted(QModelIndex, %1, %2)").arg(start).arg(end);
    mEventLoop.exit(0);
}

void TestListModel::rowsRemoved(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent);
    qDebug() << QString("rowsRemoved(QModelIndex, %1, %2)").arg(start).arg(end);
    mEventLoop.exit(0);
}

void TestListModel::rowsMoved(QModelIndex sourceParent, int sourceStart, int sourceEnd, QModelIndex destinationParent, int destinationRow)
{
    Q_UNUSED(sourceParent);
    Q_UNUSED(destinationParent);
    qDebug() << QString("rowsMoved(QModelIndex, %1, %2, QModelIndex, %3)").arg(sourceStart).arg(sourceEnd).arg(destinationRow);
    mEventLoop.exit(0);
}

void TestListModel::createListModelHundredItems()
{
    JsonDbListModel *listModel = createModel();
    if (!listModel) return;

    connectListModel(listModel);
    QStringList rolenames;
    rolenames << "age" << "name" << "_type";
    listModel->setScriptableRoleNames(rolenames);

    QBENCHMARK {
        listModel->setQuery("[?_type=\"Troll\"]");
        mEventLoop.exec(QEventLoop::AllEvents);
    }
    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}


void TestListModel::createListModelThousandItems()
{
    JsonDbListModel *listModel = createModel();

    connectListModel(listModel);
    QStringList rolenames;
    rolenames << "name" << "phone" << "_uuid" << "_type";
    listModel->setScriptableRoleNames(rolenames);

    QBENCHMARK {
        listModel->setQuery("[?_type=\"Friends\"]");
        mEventLoop.exec(QEventLoop::AllEvents);
    }
    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestListModel::createListModelGroupedQuery()
{
    JsonDbListModel *listModel = createModel();
    if (!listModel) return;

    connectListModel(listModel);
    QStringList rolenames;
    rolenames << "_uuid" << "_type" << "number";
    listModel->setScriptableRoleNames(rolenames);

    QBENCHMARK {
        listModel->setQuery("[?_type=\"RandNumber\"][?number=77]");
        mEventLoop.exec(QEventLoop::AllEvents);
    }
    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}


void TestListModel::createListModelSortedQuery()
{
    JsonDbListModel *listModel = createModel();
    if (!listModel) return;

    connectListModel(listModel);

    QStringList rolenames;
    rolenames << "_uuid" << "_type" << "number";
    listModel->setScriptableRoleNames(rolenames);

    QBENCHMARK {
        listModel->setQuery("[?_type=\"RandNumber\"][/number]");
        mEventLoop.exec(QEventLoop::AllEvents);
    }
    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestListModel::changeOneItemClient()
{

    QString queryString("[?_type=\"Friends\"][?name=\"Name-1\"]");
    mId = mClient->query(queryString);
    mEventLoop.exec(QEventLoop::AllEvents);

    QVariantMap mapResponse = mLastResponseData.toMap();
    QVariantMap item = mapResponse.value("data").toList().at(0).toMap();
    int i = mapResponse.value("length").toInt();
    QVERIFY(i == 1);
    item.insert("phone","111122223");

    // Make sure that we only exit the eventloop
    // on listmodel updates.
    disconnect(mClient, 0, this, 0);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    QStringList rolenames;
    rolenames << "name" << "phone" << "_uuid" << "_type";
    listModel->setScriptableRoleNames(rolenames);
    listModel->setQuery("[?_type=\"Friends\"][/name]");
    connectListModel(listModel);

    mEventLoop.exec(QEventLoop::AllEvents);

    QBENCHMARK {
        mId = mClient->update(item);
        mEventLoop.exec(QEventLoop::AllEvents);
    }

    connectToServer();

    mEventLoop.processEvents();
    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestListModel::changeOneItemSet()
{
    // Make sure that we only exit the eventloop
    // on listmodel updates.
    disconnect(mClient, 0, this, 0);
    mEventLoop.processEvents();

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    QStringList rolenames;
    rolenames << "name" << "phone" << "_uuid" << "_type";
    listModel->setScriptableRoleNames(rolenames);
    listModel->setQuery("[?_type=\"Friends\"][/name]");
    connectListModel(listModel);

    mEventLoop.exec(QEventLoop::AllEvents);

    QJSEngine engine;
    QJSValue value = engine.newObject();
    value.setProperty("phone", "987654321");

    QBENCHMARK {
        listModel->set(0,value);
        mEventLoop.exec(QEventLoop::AllEvents);
    }

    QVERIFY(listModel->get(0, "phone") == "987654321");
    deleteModel(listModel);
}

void TestListModel::changeOneItemSetProperty()
{
    // Make sure that we only exit the eventloop
    // on listmodel updates.
    disconnect(mClient, 0, this, 0);
    mEventLoop.processEvents();

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    QStringList rolenames;
    rolenames << "name" << "phone" << "_uuid" << "_type";
    listModel->setScriptableRoleNames(rolenames);
    listModel->setQuery("[?_type=\"Friends\"]");
    connectListModel(listModel);

    mEventLoop.exec(QEventLoop::AllEvents);

    QBENCHMARK {
        listModel->setProperty(1, "phone", "111122223");
        mEventLoop.exec(QEventLoop::AllEvents);
    }

    connectToServer();

    mEventLoop.processEvents();
    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestListModel::getOneItemInCache()
{
    disconnect(mClient, 0, this, 0);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    connectListModel(listModel);
    QStringList rolenames;
    rolenames << "name" << "phone" << "_uuid" << "_type" ;
    listModel->setScriptableRoleNames(rolenames);
    listModel->setQuery("[?_type=\"Friends\"]");
    mEventLoop.exec(QEventLoop::AllEvents);

    QBENCHMARK {
        QVariant res = listModel->data(listModel->index(10,0), listModel->roleFromString("name"));
        // Since it is in the cache the fetch value should be valid at this point.
        QVERIFY(res.isValid());
    }

    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestListModel::getOneItemNotInCache()
{
    disconnect(mClient, 0, this, 0);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    connectListModel(listModel);
    listModel->setLimit(80);
    QStringList rolenames;
    rolenames << "name" << "phone" << "_uuid" << "_type";
    listModel->setScriptableRoleNames(rolenames);
    listModel->setQuery("[?_type=\"Friends\"][/name]");
    mEventLoop.exec(QEventLoop::AllEvents);

    bool flip = true; // so we can run multiple benchmarks

    QBENCHMARK {
        QVariant res;
        if (flip)
            res = listModel->data(listModel->index(960,0), listModel->roleFromString("name"));
        else
            res = listModel->data(listModel->index(10,0), listModel->roleFromString("name"));

        flip = !flip;
    }

    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestListModel::getOneItemNotInCacheThousandItems()
{
    disconnect(mClient, 0, this, 0);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    connectListModel(listModel);
    listModel->setLimit(80);
    QStringList rolenames;
    rolenames << "name" << "location" << "_type";
    listModel->setScriptableRoleNames(rolenames);
    listModel->setQuery("[?_type=\"Image\"][/name]");
    mEventLoop.exec(QEventLoop::AllEvents);

    QVERIFY(listModel->count() == 1000);

    bool flip = true; // so we can run multiple benchmarks

    QBENCHMARK {
        QVariant res;
        if (flip)
            res = listModel->data(listModel->index(980,0), listModel->roleFromString("name"));
        else
            res = listModel->data(listModel->index(10,0), listModel->roleFromString("name"));

        flip = !flip;
    }

    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestListModel::scrollThousandItems()
{
    disconnect(mClient, 0, this, 0);
    mEventLoop.processEvents();

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    QStringList rolenames;
    rolenames << "name" << "phone" << "_uuid" << "_type";
    listModel->setScriptableRoleNames(rolenames);
    listModel->setQuery("[?_type=\"Friends\"][/name]");
    listModel->setLimit(80);
    connectListModel(listModel);

    mEventLoop.exec(QEventLoop::AllEvents);

    int rowCount = listModel->rowCount();

    QBENCHMARK {
        for( int i=0 ; i<rowCount; i++)
            foreach(QString role, rolenames)
                listModel->data(listModel->index(i,0), listModel->roleFromString(role));
    }
    deleteModel(listModel);
}


QTEST_MAIN(TestListModel)
