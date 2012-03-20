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
#include "testjsondblistmodel.h"
#include "../../shared/util.h"

static const char dbfile[] = "dbFile-jsondb-listmodel";
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

const QString qmlProgram = QLatin1String(
            "import QtQuick 2.0\n"
            "import QtJsonDb 1.0 as JsonDb\n"
            "JsonDb.JsonDbListModel {"
                "id: contactsModel;"
                "partition: JsonDb.Partition {name: \"com.example.shared.1\"}"
            "}");

QVariant get(QObject* object, int index, QString propertyName)
{
    QVariant retVal;
    QMetaObject::invokeMethod(object, "get", Qt::DirectConnection,
                              Q_RETURN_ARG(QVariant, retVal),
                              Q_ARG(int, index),
                              Q_ARG(QString, propertyName));
    return retVal;
}

void set(QObject* object, int index, QJSValue value)
{
    QMetaObject::invokeMethod(object, "set", Qt::DirectConnection,
                              Q_ARG(int, index),
                              Q_ARG(QJSValue, value));
}

void listSetProperty(QObject* object, int index, QString propertyName, QVariant value)
{
    QMetaObject::invokeMethod(object, "setProperty", Qt::DirectConnection,
                              Q_ARG(int, index),
                              Q_ARG(QString, propertyName),
                              Q_ARG(QVariant, value));
}

TestJsonDbListModel::TestJsonDbListModel()
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
        QFile file(fileInfo.fileName());
        file.remove();
    }
}

void TestJsonDbListModel::connectListModel(QAbstractItemModel *model)
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
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << "-base-name" << dbfile);

    connection = new QJsonDbConnection();
    connection->connectToServer();

    mPluginPath = findQMLPluginPath("QtJsonDb");
    if (mPluginPath.isEmpty())
        qDebug() << "Couldn't find the plugin path for the plugin QtJsonDb";

    // Create the shared Partitions
    QVariantMap item;
    item.insert("_type", "Partition");
    item.insert("name", "com.example.shared.1");
    int id = create(item);
    waitForResponse1(id);
}

QAbstractItemModel *TestJsonDbListModel::createModel()
{
    ModelData *newModel = new ModelData();
    newModel->engine = new QQmlEngine();
    QString error;
    Q_ASSERT(!mPluginPath.isEmpty());
    if (!newModel->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &error)) {
        qDebug()<<"Unable to load the plugin :"<<error;
        delete newModel->engine;
        return 0;
    }
    newModel->component = new QQmlComponent(newModel->engine);
    newModel->component->setData(qmlProgram.toLocal8Bit(), QUrl());
    newModel->model = newModel->component->create();
    if (newModel->component->isError())
        qDebug() << newModel->component->errors();
    mModels.append(newModel);
    return (QAbstractItemModel*)(newModel->model);
}

void TestJsonDbListModel::deleteModel(QAbstractItemModel *model)
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

void TestJsonDbListModel::timeout()
{
    RequestWrapper::timeout();
    mTimedOut = true;
    eventLoop1.quit();
}

void TestJsonDbListModel::waitForItemsCreated(int items)
{
    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    mItemsCreated = 0;
    while (!mTimedOut && mItemsCreated != items) {
        mWaitingForRowsInserted = true;
        eventLoop1.exec(QEventLoop::AllEvents);
    }
}

void TestJsonDbListModel::waitForExitOrTimeout()
{
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();
    eventLoop1.exec(QEventLoop::AllEvents);
}

void TestJsonDbListModel::resetWaitFlags()
{
    mItemsCreated  = 0;
    mItemsUpdated = 0;
    mItemsRemoved = 0;
    mWaitingForRowsInserted = false;
    mWaitingForReset = false;
    mWaitingForChanged = false;
    mWaitingForRemoved = false;
}

// Create items in the model.
void TestJsonDbListModel::createItem()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item,  "com.example.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1);

    item.insert("_type", __FUNCTION__);
    item.insert("name", "Baker");
    mItemsCreated = 0;
    id = create(item, "com.example.shared.1");
    waitForResponse1(id);
    while (!mItemsCreated) {
        mWaitingForRowsInserted = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRowsInserted, false);

    QCOMPARE(listModel->property("count").toInt(), 2);
    deleteModel(listModel);
}

// Create an item and then update it.
void TestJsonDbListModel::updateItemClient()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item, "com.example.shared.1");
    waitForResponse1(id);
    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1);

    QString lastUuid,lastVersion;
    QVariantMap lastItem;
    if (lastResult.count()) {
        lastItem = lastResult[0].toMap();
        lastUuid = lastItem.value("_uuid").toString();
        lastVersion = lastItem.value("_version").toString();
    }
    item.insert("_uuid", lastUuid);
    item.insert("_version", lastVersion);
    item.insert("name", "Baker");
    id = update(item, "com.example.shared.1");
    waitForResponse1(id);
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    QCOMPARE(listModel->property("count").toInt(), 1);
    QCOMPARE(get(listModel, 0, "_uuid").toString(), lastUuid);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Baker"));
    deleteModel(listModel);
}

// Create an item and then update it.
void TestJsonDbListModel::updateItemSet()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = create(item, "com.example.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1);

    QJSEngine engine;
    QJSValue value = engine.newObject();

    value.setProperty("name", "Baker");
    value.setProperty("phone", "987654321");

    set(listModel, 0, value);
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    QCOMPARE(listModel->property("count").toInt(), 1);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Baker"));
    QCOMPARE(get(listModel, 0, "phone").toString(), QLatin1String("987654321"));
    deleteModel(listModel);
}

void TestJsonDbListModel::updateItemSetProperty()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = create(item, "com.example.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1);

    listSetProperty(listModel, 0, QString("phone"), QVariant("987654321"));
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    QCOMPARE(listModel->property("count").toInt(), 1);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Charlie"));
    QCOMPARE(get(listModel, 0, "phone").toString(), QLatin1String("987654321"));
    deleteModel(listModel);
}

void TestJsonDbListModel::deleteItem()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item, "com.example.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1);

    item.insert("name", "Baker");
    mItemsCreated = 0;
    id = create(item,"com.example.shared.1");
    waitForResponse1(id);
    while (!mItemsCreated) {
        mWaitingForRowsInserted = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRowsInserted, false);
    QCOMPARE(listModel->property("count").toInt(), 2);

    QString lastUuid,lastVersion;
    QVariantMap lastItem;
    if (lastResult.count()) {
        lastItem = lastResult[0].toMap();
        lastUuid = lastItem.value("_uuid").toString();
        lastVersion = lastItem.value("_version").toString();
    }
    item.insert("_uuid", lastUuid);
    item.insert("_version", lastVersion);
    id = remove(item, "com.example.shared.1");
    waitForResponse1(id);
    while (!mItemsRemoved) {
        mWaitingForRemoved = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRemoved, false);

    QCOMPARE(listModel->property("count").toInt(), 1);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Charlie"));
    deleteModel(listModel);
}

void TestJsonDbListModel::sortedQuery()
{
    resetWaitFlags();
    int id = 0;

    QVariantMap index;
    index.insert("_type", "Index");
    index.insert("name", "number");
    index.insert("propertyName", "number");
    index.insert("propertyType", "number");
    id = create(index, "com.example.shared.1");
    waitForResponse1(id);

    for (int i = 0; i < 1000; i++) {
        QVariantMap item;
        item.insert("_type", "RandNumber");
        item.insert("number", i);
        id = create(item, "com.example.shared.1");
        waitForResponse1(id);
    }

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;

    connectListModel(listModel);

    QStringList rolenames;
    rolenames << "_uuid" << "_type" << "number";
    listModel->setProperty("roleNames", rolenames);
    listModel->setProperty("query", "[?_type=\"RandNumber\"][/number]");

    QCOMPARE(listModel->property("count").toInt(), 0);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1000);

    for (int i = 0; i < 1000; i++)
        QCOMPARE(get(listModel, i,"number").toInt(), i);
    listModel->setProperty("query","[?_type=\"RandNumber\"][\\number]");
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1000);
    for (int i = 0; i < 1000; i++) {
        QCOMPARE(get(listModel, i,"number").toInt(), 999-i);
    }

    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

void TestJsonDbListModel::ordering()
{
    resetWaitFlags();
    for (int i = 9; i >= 1; --i) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", QString::number(i));
        int id = create(item, "com.example.shared.1");
        waitForResponse1(id);
    }

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"][/order]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "order");
    listModel->setProperty("roleNames",roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QStringList expectedOrder = QStringList() << "1" << "2" << "3" << "4" <<
        "5" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        QVariant uuid = get(listModel, 4, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "99");  // move it to the end
        int id = update(item, "com.example.shared.1");
        waitForResponse1(id);
    }
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    expectedOrder = QStringList() << "1" << "2" << "3" <<
        "4" << "6" << "7" << "8" << "9" << "99";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    mItemsUpdated = 0;
    {
        QVariant uuid =  get(listModel, 8, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "22");    // move it after "2"
        int id = update(item, "com.example.shared.1");
        waitForResponse1(id);
    }
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    expectedOrder = QStringList() << "1" << "2" << "22" << "3" <<
        "4" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    mItemsUpdated = 0;
    {
        QVariant uuid =  get(listModel, 5, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "0");    // move it to the beginning
        int id = update(item, "com.example.shared.1");
        waitForResponse1(id);
    }
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    expectedOrder = QStringList() << "0" << "1" << "2" << "22" << "3" <<
        "4" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    deleteModel(listModel);
}

void TestJsonDbListModel::itemNotInCache()
{
    resetWaitFlags();
    QVariantMap index;
    index.insert("_type", "Index");
    index.insert("name", "order");
    index.insert("propertyName", "order");
    index.insert("propertyType", "number");
    int indexId = create(index, "com.example.shared.1");
    waitForResponse1(indexId);

    QVariantList itemList;
    for (int i = 0; i < 1000; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Number");
        item.insert("order", i);
        itemList << item;
    }
    int id = create(itemList, "com.example.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    connectListModel(listModel);
    listModel->setProperty("limit", 80);
    listModel->setProperty("query", QString("[?_type=\"%1\"][/order]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "order");
    listModel->setProperty("roleNames",roleNames);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1000);

    // Make sure that the first items in the list is in the cache.
    QVariant result = listModel->data(listModel->index(10,0), 3);
    QVERIFY(result.isValid());
    QCOMPARE(result.toInt(), 10);

    // This item should not be in the cache now.
    QVariant res = listModel->data(listModel->index(960,0), 3);
    QCOMPARE(res.toInt(), 960);
    deleteModel(listModel);
}

void TestJsonDbListModel::roles()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = create(item, "com.example.shared.1");

    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setProperty("roleNames",roleNames);
    connectListModel(listModel);

    // now start it working
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 1);

    QVariantMap roles = listModel->property("roleNames").toMap();
    QCOMPARE(roles.size(), 4);
    QVERIFY(roles.contains("name")) ;
    QVERIFY(roles.contains("phone"));
    deleteModel(listModel);
}

void TestJsonDbListModel::totalRowCount()
{
    resetWaitFlags();
    int id = 0;
    QVariantList insertedItems;
    for (int i = 0; i < 10; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        id = create(item, "com.example.shared.1");
        waitForResponse1(id);
        insertedItems << lastResult;
    }
    QAbstractItemModel *listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);

    listModel->setProperty("limit",10);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "order");
    listModel->setProperty("roleNames",roleNames);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 10);

    for (int i = 10; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        create(item,  "com.example.shared.1");
    }

    waitForItemsCreated(40);
    QCOMPARE(listModel->property("count").toInt(), 50);

    // Change query
    listModel->setProperty("query", QString("[?_type=\"%1\"][\\order]").arg(__FUNCTION__));
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->property("count").toInt(), 50);

    // Delete the first 10 items
    foreach (QVariant item, insertedItems) {
        mItemsRemoved = 0;
        id = remove(item.toMap(), "com.example.shared.1");
        waitForResponse1(id);

        while (!mTimedOut && !mItemsRemoved) {
            mWaitingForRemoved = true;
            waitForExitOrTimeout();
        }
        QCOMPARE(mWaitingForRemoved, false);
    }

    QCOMPARE(listModel->property("count").toInt(), 40);

    deleteModel(listModel);
}

void TestJsonDbListModel::listProperty()
{
    resetWaitFlags();
    QVariant jsonData = readJsonFile(findFile("list-objects.json")).toVariant();
    QVariantList itemList = jsonData.toList();

    int id = 0;
    for (int i = 0; i < itemList.count(); i++) {
        id = create(itemList[i].toMap(), "com.example.shared.1");
        waitForResponse1(id);
    }

    QAbstractItemModel *listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    QString type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("limit",10);
    listModel->setProperty("query", QString("[?_type=\"%1\"][/features.0.properties.0.description]").arg(type));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "features.0.properties.0.description"<< "features.0.feature");
    listModel->setProperty("roleNames",roleNames);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QString lastUuid,lastVersion;
    QVariantMap lastItem;
    if (lastResult.count()) {
        lastItem = lastResult[0].toMap();
        lastUuid = lastItem.value("_uuid").toString();
        lastVersion = lastItem.value("_version").toString();
    }

    QCOMPARE(listModel->property("count").toInt(), itemList.count());
    QCOMPARE(get(listModel, 0, "_type").toString(), type);
    QCOMPARE(get(listModel, 0, "features.0.properties.0.description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(get(listModel, 0, "features.0.feature").toString(), QLatin1String("provide Facebook"));
    QCOMPARE(get(listModel, 1, "_uuid").toString(), lastUuid);
    QCOMPARE(get(listModel, 1, "_type").toString(), type);
    QCOMPARE(get(listModel, 1, "features.0.properties.0.description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(get(listModel, 1, "features.0.feature").toString(), QLatin1String("provide Gmail"));

    deleteModel(listModel);

    listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("limit",10);
    listModel->setProperty("query", QString("[?_type=\"%1\"][/features.0.properties.0.description]").arg(type));
    roleNames.clear();
    roleNames = (QStringList() << "_type" << "_uuid" << "features[0].properties[0].description"<< "features[0].supported[0]");
    listModel->setProperty("roleNames",roleNames);
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->property("count").toInt(), itemList.count());
    QCOMPARE(get(listModel, 0, "_type").toString(), type);
    QCOMPARE(get(listModel, 0, "features[0].properties[0].description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(get(listModel, 0, "features[0].supported[0]").toString(), QLatin1String("share"));
    QCOMPARE(get(listModel, 1, "_type").toString(), type);
    QCOMPARE(get(listModel, 1, "features[0].properties[0].description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(get(listModel, 1, "features[0].supported[0]").toString(), QLatin1String("share"));

    deleteModel(listModel);
}

QStringList TestJsonDbListModel::getOrderValues(QAbstractItemModel *listModel)
{
    QStringList vals;
    for (int i = 0; i < listModel->property("count").toInt(); ++i)
        vals << get(listModel, i, "order").toString();
    return vals;
}

void TestJsonDbListModel::modelReset()
{
    if (mWaitingForReset) {
        mWaitingForReset = false;
        eventLoop1.exit(0);
    }
}
void TestJsonDbListModel::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    mItemsUpdated++;
    if (mWaitingForChanged) {
        mWaitingForChanged = false;
        eventLoop1.exit(0);
    }
}
void TestJsonDbListModel::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsCreated++;
    if (mWaitingForRowsInserted) {
        mWaitingForRowsInserted = false;
        eventLoop1.exit(0);
    }
}
void TestJsonDbListModel::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsRemoved++;
    if (mWaitingForRemoved) {
        mWaitingForRemoved = false;
        eventLoop1.exit(0);
    }
}
void TestJsonDbListModel::rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row )
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);
}

QTEST_MAIN(TestJsonDbListModel)

