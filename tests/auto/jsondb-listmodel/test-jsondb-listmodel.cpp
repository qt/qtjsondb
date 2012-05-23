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
#include "test-jsondb-listmodel.h"

#include "../../shared/util.h"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDir>

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

TestJsonDbListModel::TestJsonDbListModel()
    : mWaitingForNotification(false), mWaitingForDataChange(false), mWaitingForRowsRemoved(false)
{
}

TestJsonDbListModel::~TestJsonDbListModel()
{
}

void TestJsonDbListModel::deleteDbFiles()
{
    // remove all the test files.
    QDir currentDir;
    QStringList nameFilter;
    nameFilter << QString("*.db");
    nameFilter << "objectFile.bin" << "objectFile2.bin";
    QFileInfoList databaseFiles = currentDir.entryInfoList(nameFilter, QDir::Files);
    foreach (QFileInfo fileInfo, databaseFiles) {
        //qDebug() << "Deleted : " << fileInfo.fileName();
        QFile file(fileInfo.fileName());
        file.remove();
    }
}

void TestJsonDbListModel::connectListModel(JsonDbListModel *model)
{
    connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(rowsInserted(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
            this, SLOT(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
}

void TestJsonDbListModel::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList(), __FILE__);

    mClient = new JsonDbClient(this);
    connect(mClient, SIGNAL(notified(QString,QtAddOn::JsonDb::JsonDbNotification)),
            this, SLOT(notified(QString,QtAddOn::JsonDb::JsonDbNotification)));
    connect( mClient, SIGNAL(response(int, const QVariant&)),
             this, SLOT(response(int, const QVariant&)));
    connect( mClient, SIGNAL(error(int, int, const QString&)),
             this, SLOT(error(int, int, const QString&)));

    QQmlEngine *engine = new QQmlEngine();
    QStringList pluginPaths = engine->importPathList();
    for (int i=0; (i<pluginPaths.count() && mPluginPath.isEmpty()); i++) {
        QDir dir(pluginPaths[i]+"/QtAddOn/JsonDb");
        dir.setFilter(QDir::Files | QDir::NoSymLinks);
        QFileInfoList list = dir.entryInfoList();
        for (int i = 0; i < list.size(); ++i) {
          QList<QQmlError> errors;
            if (engine->importPlugin(list.at(i).absoluteFilePath(), QString("QtAddOn.JsonDb"), &errors)) {
                mPluginPath = list.at(i).absoluteFilePath();
                break;
            }
        }
    }
    delete engine;

}

JsonDbListModel *TestJsonDbListModel::createModel()
{
    ModelData *newModel = new ModelData();
    newModel->engine = new QQmlEngine();
    QList<QQmlError> errors;
    if (!newModel->engine->importPlugin(mPluginPath, QString("QtAddOn.JsonDb"), &errors)) {
        qDebug()<<"Unable to load the plugin :"<<errors;
        delete newModel->engine;
        return 0;
    }
    newModel->component = new QQmlComponent(newModel->engine);
    newModel->component->setData("import QtQuick 2.0\nimport QtAddOn.JsonDb 1.0 \n JsonDbListModel {id: contactsModel}", QUrl());
    newModel->model = newModel->component->create();
    if (newModel->component->isError())
        qDebug() << newModel->component->errors();
    mModels.append(newModel);
    return (JsonDbListModel*)(newModel->model);
}

void TestJsonDbListModel::deleteModel(JsonDbListModel *model)
{
    for (int i = 0; i < mModels.count(); i++) {
        if (mModels[i]->model == model) {
            ModelData *modelData = mModels.takeAt(i);
            delete modelData;
            return;
        }
    }
}

void TestJsonDbListModel::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

void TestJsonDbListModel::waitForItemsCreated(int items)
{
    mItemsCreated = 0;
    while(mItemsCreated != items)
        mEventLoop.processEvents(QEventLoop::AllEvents, mClientTimeout);
}

void TestJsonDbListModel::waitForExitOrTimeout()
{
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop, SLOT(quit()));
    timer.start(mClientTimeout);
    mElapsedTimer.start();
    mEventLoop.exec(QEventLoop::AllEvents);
}

// Create items in the model.
void TestJsonDbListModel::createItem()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = mClient->create(item);

    waitForResponse1(id);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setQuery(QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setScriptableRoleNames(roleNames);
    connectListModel(listModel);

    // now start it working
    listModel->componentComplete();
    QCOMPARE(listModel->count(), 0);

    waitForExitOrTimeout();
    QCOMPARE(listModel->count(), 1);

    mItemsCreated = 0;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Baker");
    id = mClient->create(item);
    waitForResponse1(id);
    if (!mItemsCreated)
        waitForExitOrTimeout();;
    QCOMPARE(listModel->count(), 2);
    deleteModel(listModel);
}

// Create an item and then update it.
void TestJsonDbListModel::updateItemClient()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = mClient->create(item);
    waitForResponse1(id);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setQuery(QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setScriptableRoleNames(roleNames);
    connectListModel(listModel);

    // now start it working
    listModel->componentComplete();
    QCOMPARE(listModel->rowCount(), 0);

    waitForExitOrTimeout();
    QCOMPARE(listModel->rowCount(), 1);

    item.insert("_uuid", mLastUuid);
    item.insert("_version", mLastVersion);
    item.insert("name", "Baker");

    mWaitingForDataChange = true;

    id = mClient->update(item);
    waitForResponse1(id);

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    QCOMPARE(listModel->count(), 1);
    QCOMPARE(listModel->get(0, "_uuid").toString(), mLastUuid);
    QCOMPARE(listModel->get(0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(listModel->get(0, "name").toString(), QLatin1String("Baker"));
    deleteModel(listModel);
}

// Create an item and then update it.
void TestJsonDbListModel::updateItemSet()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = mClient->create(item);
    waitForResponse1(id);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setQuery(QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setScriptableRoleNames(roleNames);
    connectListModel(listModel);

    // now start it working
    listModel->componentComplete();
    QCOMPARE(listModel->rowCount(), 0);

    waitForExitOrTimeout();
    QCOMPARE(listModel->rowCount(), 1);

    QJSEngine engine;
    QJSValue value = engine.newObject();

    value.setProperty("name", "Baker");
    value.setProperty("phone", "987654321");

    mWaitingForDataChange = true;

    listModel->set(0,value);

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    QCOMPARE(listModel->count(), 1);
    QCOMPARE(listModel->get(0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(listModel->get(0, "name").toString(), QLatin1String("Baker"));
    QCOMPARE(listModel->get(0, "phone").toString(), QLatin1String("987654321"));
    deleteModel(listModel);
}

void TestJsonDbListModel::updateItemSetProperty()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = mClient->create(item);
    waitForResponse1(id);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setQuery(QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setScriptableRoleNames(roleNames);
    connectListModel(listModel);

    // now start it working
    listModel->componentComplete();
    QCOMPARE(listModel->rowCount(), 0);

    waitForExitOrTimeout();
    QCOMPARE(listModel->rowCount(), 1);

    mWaitingForDataChange = true;

    listModel->setProperty(0,"phone","987654321");

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    QCOMPARE(listModel->count(), 1);
    QCOMPARE(listModel->get(0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(listModel->get(0, "name").toString(), QLatin1String("Charlie"));
    QCOMPARE(listModel->get(0, "phone").toString(), QLatin1String("987654321"));
    deleteModel(listModel);
}

void TestJsonDbListModel::deleteItem()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = mClient->create(item);
    waitForResponse1(id);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setQuery(QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setScriptableRoleNames(roleNames);
    connectListModel(listModel);

    // now start it working
    listModel->componentComplete();
    waitForExitOrTimeout();
    QCOMPARE(listModel->rowCount(), 1);

    mItemsCreated = 0;
    item.insert("name", "Baker");
    id = mClient->create(item);
    waitForResponse1(id);
    if (!mItemsCreated)
        waitForExitOrTimeout();;
    QCOMPARE(listModel->rowCount(), 2);

    mWaitingForRowsRemoved = true;
    item.insert("_uuid", mLastUuid);
    item.insert("_version", mLastVersion);
    id = mClient->remove(item);
    waitForResponse1(id);
    while(mWaitingForRowsRemoved)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(listModel->get(0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(listModel->get(0, "name").toString(), QLatin1String("Charlie"));
    deleteModel(listModel);
}

void TestJsonDbListModel::sortedQuery()
{
    int id = 0;

    QVariantMap index;
    index.insert("_type", "Index");
    index.insert("name", "number");
    index.insert("propertyName", "number");
    index.insert("propertyType", "number");
    id = mClient->create(index);
    waitForResponse1(id);

    for (int i = 0; i < 1000; i++) {
        QVariantMap item;
        item.insert("_type", "RandNumber");
        item.insert("number", i);
        id = mClient->create(item);
        waitForResponse1(id);
    }

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;

    connectListModel(listModel);

    QStringList rolenames;
    rolenames << "_uuid" << "_type" << "number";
    listModel->setScriptableRoleNames(rolenames);

    listModel->setQuery("[?_type=\"RandNumber\"][/number]");

    waitForExitOrTimeout();
    QCOMPARE(listModel->count(), 1000);
    for (int i = 0; i < 1000; i++)
        QCOMPARE(listModel->get(i,"number").toInt(), i);

    listModel->setQuery("[?_type=\"RandNumber\"][\\number]");
    waitForExitOrTimeout();
    for (int i = 0; i < 1000; i++)
        QCOMPARE(listModel->get(i,"number").toInt(), 999-i);

    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestJsonDbListModel::ordering()
{
    for (int i = 9; i >= 1; --i) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", QString::number(i));
        int id = mClient->create(item);
        waitForResponse1(id);
    }

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setQuery(QString("[?_type=\"%1\"][/order]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "_version" << "name" << "order");
    listModel->setScriptableRoleNames(roleNames);
    connectListModel(listModel);

    // now start it working
    listModel->componentComplete();
    QCOMPARE(listModel->rowCount(), 0);

    waitForExitOrTimeout();

    QStringList expectedOrder = QStringList() << "1" << "2" << "3" << "4" <<
        "5" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    mWaitingForDataChange = true;
    {
        QVariant uuid = listModel->get(4, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());
        QVariant version = listModel->get(4, "_version");
        QVERIFY(!version.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_version", version);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "99");  // move it to the end
        int id = mClient->update(item);
        waitForResponse1(id);
    }

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    expectedOrder = QStringList() << "1" << "2" << "3" <<
        "4" << "6" << "7" << "8" << "9" << "99";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    mWaitingForDataChange = true;
    {
        QVariant uuid = listModel->get(8, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());
        QVariant version = listModel->get(8, "_version");
        QVERIFY(!version.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_version", version);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "22");    // move it after "2"
        int id = mClient->update(item);
        waitForResponse1(id);
    }

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    expectedOrder = QStringList() << "1" << "2" << "22" << "3" <<
        "4" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    mWaitingForDataChange = true;
    {
        QVariant uuid = listModel->get(5, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());
        QVariant version = listModel->get(5, "_version");
        QVERIFY(!version.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_version", version);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "0");    // move it to the beginning
        int id = mClient->update(item);
        waitForResponse1(id);
    }

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    expectedOrder = QStringList() << "0" << "1" << "2" << "22" << "3" <<
        "4" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    deleteModel(listModel);

}

void TestJsonDbListModel::itemNotInCache()
{
    QVariantMap index;
    index.insert("_type", "Index");
    index.insert("name", "order");
    index.insert("propertyName", "order");
    index.insert("propertyType", "number");
    int indexId = mClient->create(index);
    waitForResponse1(indexId);

    QVariantList itemList;
    for (int i = 0; i < 1000; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Number");
        item.insert("order", i);
        itemList << item;
    }
    int id = mClient->create(itemList);
    waitForResponse1(id);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    connectListModel(listModel);
    listModel->setLimit(80);
    listModel->setQuery(QString("[?_type=\"%1\"][/order]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "order");
    listModel->setScriptableRoleNames(roleNames);
    listModel->componentComplete();
    waitForExitOrTimeout();

    QCOMPARE(listModel->rowCount(), 1000);

    // Make sure that the first items in the list is in the cache.
    QVariant result = listModel->data(listModel->index(10,0), listModel->roleFromString("order"));
    QVERIFY(result.isValid());
    QCOMPARE(result.toInt(), 10);
    // This item should not be in the cache now.
    QVariant res = listModel->data(listModel->index(960,0), listModel->roleFromString("order"));
    QCOMPARE(res.toInt(), 960);
    deleteModel(listModel);
}

void TestJsonDbListModel::roles()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = mClient->create(item);

    waitForResponse1(id);

    JsonDbListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setQuery(QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setScriptableRoleNames(roleNames);
    connectListModel(listModel);

    // now start it working
    listModel->componentComplete();
    waitForExitOrTimeout();
    QCOMPARE(listModel->count(), 1);

    QVariantMap roles = listModel->scriptableRoleNames().toMap();
    QCOMPARE(roles.size(), 4);
    QVERIFY(roles.contains("name")) ;
    QVERIFY(roles.contains("phone"));
    QCOMPARE(listModel->roleFromString("_type"), 0);
    QCOMPARE(listModel->roleFromString("_uuid"), 1);
    QCOMPARE(listModel->roleFromString("name"), 2);
    QCOMPARE(listModel->roleFromString("phone"), 3);
    QCOMPARE(listModel->toString(0), QLatin1String("_type"));
    QCOMPARE(listModel->toString(1), QLatin1String("_uuid"));
    QCOMPARE(listModel->toString(2), QLatin1String("name"));
    QCOMPARE(listModel->toString(3), QLatin1String("phone"));
    QCOMPARE(listModel->toString(4), QLatin1String(""));
    deleteModel(listModel);
}

void TestJsonDbListModel::totalRowCount()
{
    int id = 0;
    QVariantList insertedItems;
    for (int i = 0; i < 10; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        id = mClient->create(item);
        waitForResponse1(id);
        insertedItems << mData;
    }

    JsonDbListModel *listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);

    listModel->setLimit(10);
    listModel->setQuery(QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "order");
    listModel->setScriptableRoleNames(roleNames);
    listModel->componentComplete();
    waitForExitOrTimeout();

    QCOMPARE(listModel->rowCount(), 10);

    for (int i = 10; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        mClient->create(item);
    }

    waitForItemsCreated(40);
    QCOMPARE(listModel->rowCount(), 50);

    // Change query
    listModel->setQuery(QString("[?_type=\"%1\"][\\order]").arg(__FUNCTION__));
    waitForExitOrTimeout();

    QCOMPARE(listModel->rowCount(), 50);

    // Delete the first 10 items
    foreach (QVariant item, insertedItems) {
        mWaitingForRowsRemoved = true;
        id = mClient->remove(item.toMap());
        while(mWaitingForRowsRemoved)
            mEventLoop.processEvents(QEventLoop::AllEvents);
    }

    QCOMPARE(listModel->rowCount(), 40);

    deleteModel(listModel);
}

void TestJsonDbListModel::listProperty()
{
    QVariant jsonData = readJsonFile(findFile("list-objects.json")).toVariant();
    QVariantList itemList = jsonData.toList();
    int id = 0;
    for (int i = 0; i < itemList.count(); i++) {
        id = mClient->create(itemList[i].toMap());
        waitForResponse1(id);
    }

    JsonDbListModel *listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    QString type = itemList[0].toMap()["_type"].toString();
    listModel->setLimit(10);
    listModel->setQuery(QString("[?_type=\"%1\"][/features.0.properties.0.description]").arg(type));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "features.0.properties.0.description"<< "features.0.feature");
    listModel->setScriptableRoleNames(roleNames);
    listModel->componentComplete();
    waitForExitOrTimeout();

    QCOMPARE(listModel->count(), itemList.count());
    QCOMPARE(listModel->get(0, "_type").toString(), type);
    QCOMPARE(listModel->get(0, "features.0.properties.0.description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(listModel->get(0, "features.0.feature").toString(), QLatin1String("provide Facebook"));
    QCOMPARE(listModel->get(1, "_uuid").toString(), mLastUuid);
    QCOMPARE(listModel->get(1, "_type").toString(), type);
    QCOMPARE(listModel->get(1, "features.0.properties.0.description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(listModel->get(1, "features.0.feature").toString(), QLatin1String("provide Gmail"));

    deleteModel(listModel);

    listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    type = itemList[0].toMap()["_type"].toString();
    listModel->setLimit(10);
    listModel->setQuery(QString("[?_type=\"%1\"][/features.0.properties.0.description]").arg(type));
    roleNames.clear();
    roleNames = (QStringList() << "_type" << "_uuid" << "features[0].properties[0].description"<< "features[0].supported[0]");
    listModel->setScriptableRoleNames(roleNames);
    listModel->componentComplete();
    waitForExitOrTimeout();

    QCOMPARE(listModel->count(), itemList.count());
    QCOMPARE(listModel->get(0, "_type").toString(), type);
    QCOMPARE(listModel->get(0, "features[0].properties[0].description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(listModel->get(0, "features[0].supported[0]").toString(), QLatin1String("share"));
    QCOMPARE(listModel->get(1, "_type").toString(), type);
    QCOMPARE(listModel->get(1, "features[0].properties[0].description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(listModel->get(1, "features[0].supported[0]").toString(), QLatin1String("share"));

    deleteModel(listModel);
}


QStringList TestJsonDbListModel::getOrderValues(const JsonDbListModel *listModel)
{
    QStringList vals;
    for (int i = 0; i < listModel->count(); ++i)
        vals << listModel->get(i, "order").toString();

    return vals;
}

void TestJsonDbListModel::modelReset()
{
    //qDebug() << "TestJsonDbListModel::modelReset";
    mEventLoop.exit(0);
}
void TestJsonDbListModel::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    //qDebug() << "TestJsonDbListModel::dataChanged";
    mWaitingForDataChange = false;
}
void TestJsonDbListModel::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsCreated++;
    //qDebug() << "TestJsonDbListModel::rowsInserted";
    mEventLoop.exit(0);
}
void TestJsonDbListModel::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mWaitingForRowsRemoved = false;
    //qDebug() << "TestJsonDbListModel::rowsRemoved";
}
void TestJsonDbListModel::rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row )
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);
    //qDebug() << "TestJsonDbListModel::rowsMoved";
}

QTEST_MAIN(TestJsonDbListModel)

