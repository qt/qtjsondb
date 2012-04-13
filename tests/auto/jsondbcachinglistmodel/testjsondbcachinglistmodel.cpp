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
#include "testjsondbcachinglistmodel.h"

#include "../../shared/util.h"
#include <QQmlListReference>

static const char dbfile[] = "dbFile-jsondb-cached-listmodel";
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
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.JsonDbCachingListModel {"
                "id: contactsModel; cacheSize: 200;"
                "partitions: ["
                    "JsonDb.Partition {name: \"com.nokia.shared.1\"},"
                    "JsonDb.Partition {name: \"com.nokia.shared.2\"}"
                "]"
            "}");

TestJsonDbCachingListModel::TestJsonDbCachingListModel()
{
}

TestJsonDbCachingListModel::~TestJsonDbCachingListModel()
{
}

void TestJsonDbCachingListModel::deleteDbFiles()
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

void TestJsonDbCachingListModel::connectListModel(QAbstractListModel *model)
{
    connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(rowsInserted(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
            this, SLOT(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    connect(model, SIGNAL(stateChanged(State)),
            this, SLOT(stateChanged()));
}

void TestJsonDbCachingListModel::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << "-base-name" << dbfile, __FILE__);

    connection = new QJsonDbConnection();
    connection->connectToServer();

    mPluginPath = findQMLPluginPath("QtJsonDb");
    if (mPluginPath.isEmpty())
        qDebug() << "Couldn't find the plugin path for the plugin QtJsonDb";
}

QAbstractListModel *TestJsonDbCachingListModel::createModel()
{
    ModelData *newModel = new ModelData();
    newModel->engine = new QQmlEngine();
    QString error;
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
    return (QAbstractListModel*)(newModel->model);
}

void TestJsonDbCachingListModel::deleteModel(QAbstractListModel *model)
{
    for (int i = 0; i < mModels.count(); i++) {
        if (mModels[i]->model == model) {
            ModelData *modelData = mModels.takeAt(i);
            delete modelData;
            return;
        }
    }
}

void TestJsonDbCachingListModel::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

QVariant TestJsonDbCachingListModel::getIndex(QAbstractListModel *model, int index, int role)
{
    mWaitingForIndexChanged = true;
    mWaitingForChanged = false;
    mIndexWaited = index;
    QVariant val =  model->data(model->index(index), role);
    while (!val.isValid()) {
        waitForIndexChanged();
        val = model->data(model->index(index), role);
    }
    return val;
}

QVariant TestJsonDbCachingListModel::getProperty(QAbstractListModel *model, int index, const QByteArray &roleName)
{
    mWaitingForIndexChanged = true;
    mWaitingForChanged = false;
    mIndexWaited = index;
    QHash<int, QByteArray> roleNames;
    roleNames = model->roleNames();
    int roleIndex = roleNames.key(roleName);
    QVariant val =  model->data(model->index(index), roleIndex);
    while (!val.isValid()) {
        waitForIndexChanged();
        val = model->data(model->index(index), roleIndex);
    }
    return val;
}

int indexOf(QObject* object, const QString &uuid)
{
    int retVal;
    QMetaObject::invokeMethod(object, "indexOf", Qt::DirectConnection,
                              Q_RETURN_ARG(int, retVal),
                              Q_ARG(QString, uuid));
    return retVal;
}

void TestJsonDbCachingListModel::createIndex(const QString &property, const QString &propertyType)
{
    QVariantMap item;
    item.insert("_type", "Index");
    item.insert("name", property);
    item.insert("propertyName", property);
    item.insert("propertyType", propertyType);

    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    id = create(item, "com.nokia.shared.2");
    waitForResponse1(id);
}

// Create items in the model.
void TestJsonDbCachingListModel::createItem()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Arnie");
    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("cacheSize", 200);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(listModel->property("cacheSize").toInt(), 200);

    QVariant val = getIndex(listModel, 0, 0);
    QCOMPARE(val.toString(), QLatin1String(__FUNCTION__));
    val = getIndex(listModel, 0, 2);
    QCOMPARE(val.toString(), QLatin1String("Arnie"));

    item.clear();
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Barney");
    mItemsCreated = 0;
    id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);
    while (!mItemsCreated) {
        mWaitingForRowsInserted = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRowsInserted, false);

    QCOMPARE(listModel->rowCount(), 2);

    val = getIndex(listModel, 1, 0);
    QCOMPARE(val.toString(), QLatin1String(__FUNCTION__));
    val = getIndex(listModel, 1, 2);
    QCOMPARE(val.toString(), QLatin1String("Barney"));

    deleteModel(listModel);
}

// Populate model of 300 items two partitions.
void TestJsonDbCachingListModel::createModelTwoPartitions()
{
    resetWaitFlags();
    QVariantMap item;

    for (int i=0; i < 300; i = i+2) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    for (int i=1; i < 300; i = i+2) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("cacheSize", 50);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 300);

    deleteModel(listModel);
}

// Create an item and then update it.
void TestJsonDbCachingListModel::updateItemClient()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item,"com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 1);

    QVariant val = getIndex(listModel, 0, 0);
    QCOMPARE(val.toString(), QLatin1String(__FUNCTION__));
    val = getIndex(listModel, 0, 2);
    QCOMPARE(val.toString(), QLatin1String("Charlie"));

    QString lastUuid,lastVersion;
    QVariantMap lastItem;
    if (lastResult.count()) {
        lastItem = lastResult[0].toMap();
        lastUuid = lastItem.value("_uuid").toString();
        lastVersion = lastItem.value("_version").toString();
    }
    item.insert("_uuid", lastUuid);
    item.insert("name", "Baker");

    mItemsUpdated = 0;
    id = update(item, "com.nokia.shared.1");
    waitForResponse1(id);
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }

    QCOMPARE(listModel->rowCount(), 1);

    val = getIndex(listModel, 0, 0);
    QCOMPARE(val.toString(), QLatin1String(__FUNCTION__));
    val = getIndex(listModel, 0, 2);
    QCOMPARE(val.toString(), QLatin1String("Baker"));
    deleteModel(listModel);
}

void TestJsonDbCachingListModel::deleteItem()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    QVariantMap roleNamesMap;
    roleNamesMap.insert("n", "name");
    roleNamesMap.insert("u", "_uuid");
    roleNamesMap.insert("t", "_type");
    listModel->setProperty("roleNames", roleNamesMap);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 1);

    item.insert("name", "Baker");
    mItemsCreated = 0;
    id = create(item, "com.nokia.shared.2");
    waitForResponse1(id);
    waitForItemsCreated(1);

    QCOMPARE(listModel->rowCount(), 2);

    QString lastUuid,lastVersion;
    QVariantMap lastItem;
    if (lastResult.count()) {
        lastItem = lastResult[0].toMap();
        lastUuid = lastItem.value("_uuid").toString();
        lastVersion = lastItem.value("_version").toString();
    }
    item.insert("_uuid", lastUuid);
    mItemsRemoved = 0;
    id = remove(item, "com.nokia.shared.2");
    waitForResponse1(id);

    while (!mItemsRemoved) {
        mWaitingForRemoved = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRemoved, false);

    QCOMPARE(listModel->rowCount(), 1);
    QVariant val = getProperty(listModel, 0, "t");
    QCOMPARE(val.toString(), QLatin1String(__FUNCTION__));
    val = getProperty(listModel, 0, "n");
    QCOMPARE(val.toString(), QLatin1String("Charlie"));
    deleteModel(listModel);
}

void TestJsonDbCachingListModel::sortedQuery()
{
    resetWaitFlags();
    int id = 0;
    for (int i = 0; i < 1000; i++) {
        QVariantMap item;
        item.insert("_type", "RandNumber");
        item.insert("number", i);
        id = create(item,"com.nokia.shared.2");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    createIndex("number", "number");

    connectListModel(listModel);

    QStringList rolenames;
    rolenames << "_uuid" << "_type" << "number";
    listModel->setProperty("roleNames", rolenames);
    listModel->setProperty("sortOrder", "[/number]");
    listModel->setProperty("cacheSize", 100);
    listModel->setProperty("query", "[?_type=\"RandNumber\"]");

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 1000);

    for (int i = 0; i < 1000; i++) {
        QVariant num = getIndex(listModel, i, 2);
        QCOMPARE(num.toInt(), i);
    }

    listModel->setProperty("sortOrder", "[\\number]");

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->property("sortOrder").toString(), QString("[\\number]"));
    QCOMPARE(listModel->rowCount(), 1000);

    for (int i = 0; i < 1000; i++) {
        QVariant num = getIndex(listModel, i, 2);
        QCOMPARE(num.toInt(), 999-i);
    }

    listModel->setProperty("query", QString());
    QCOMPARE(listModel->rowCount(), 0);

    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}

bool lessThan(const QString &s1, const QString &s2)
{
    return s1 < s2;
}

bool greaterThan(const QString &s1, const QString &s2)
{
    return s1 > s2;
}

void TestJsonDbCachingListModel::ordering()
{
    resetWaitFlags();
    for (int i = 9; i >= 1; --i) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("ordering", QString::number(i));
        int id = create(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    createIndex("ordering", "string");

    listModel->setProperty("sortOrder", "[/ordering]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "ordering");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    mItemsUpdated = 0;
    QStringList expectedOrder = QStringList() << "1" << "2" << "3" << "4" <<
                                                 "5" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        QVariant uuid = getIndex(listModel, 4, 1);
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("ordering", "99");  // move it to the end
        int id = update(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }

    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    mItemsUpdated = 0;
    expectedOrder = QStringList() << "1" << "2" << "3" <<
                                     "4" << "6" << "7" << "8" << "9" << "99";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        QVariant uuid = getIndex(listModel, 8, 1);
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("ordering", "22");    // move it after "2"
        int id = update(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    mItemsUpdated = 0;
    expectedOrder = QStringList() << "1" << "2" << "22" << "3" <<
                                     "4" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        QVariant uuid = getIndex(listModel, 5, 1);
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("ordering", "0");    // move it to the beginning
        int id = update(item, "com.nokia.shared.2");
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

    listModel->setProperty("sortOrder", "[\\ordering]");

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QStringList reverseOrder = expectedOrder;
    qSort(reverseOrder.begin(), reverseOrder.end(), greaterThan);

    QCOMPARE(getOrderValues(listModel), reverseOrder);
    listModel->setProperty("sortOrder", "[/ordering]");

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(getOrderValues(listModel), expectedOrder);

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::checkRemoveNotification()
{
    resetWaitFlags();
    QVariantList itemList;
    for (int i = 0; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Number");
        item.insert("order", i);
        itemList << item;
    }
    int id = create(itemList,"com.nokia.shared.2");
    waitForResponse1(id);

    {
        QAbstractListModel *listModel = createModel();
        if (!listModel) return;

        createIndex("order", "number");

        connectListModel(listModel);
        listModel->setProperty("sortOrder", "[/order]");
        QStringList roleNames = (QStringList() << "_type" << "_uuid" << "_version"<< "name" << "order");
        listModel->setProperty("roleNames", roleNames);
        listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

        mWaitingForReset = true;
        waitForExitOrTimeout();
        QCOMPARE(mWaitingForReset, false);

        QCOMPARE(listModel->rowCount(), 50);
        QVariant result = getIndex(listModel, 0, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 0);
        result = getIndex(listModel, 9, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 9);

        //Remove item at 0
        QVariantMap item;
        QVariant uuid = getIndex(listModel, 0, 1);
        item.insert("_uuid", uuid);
        QVariant version = getIndex(listModel, 0, 2);
        item.insert("_version", version);
        mItemsRemoved = 0;
        id = remove(item, "com.nokia.shared.2");
        waitForResponse1(id);
        waitForItemsRemoved(1);
        QCOMPARE(listModel->rowCount(), 49);

        result = getIndex(listModel, 9, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 10);

        //Remove item at 9
        uuid = getIndex(listModel, 9, 1);
        item.insert("_uuid", uuid);
        version = getIndex(listModel, 9, 2);
        item.insert("_version", version);
        mItemsRemoved = 0;
        id = remove(item, "com.nokia.shared.2");
        waitForResponse1(id);
        waitForItemsRemoved(1);
        QCOMPARE(listModel->rowCount(), 48);
        result = getIndex(listModel, 9, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 11);

        //Remove item at 4
        uuid = getIndex(listModel, 4, 1);
        item.insert("_uuid", uuid);
        version = getIndex(listModel, 9, 2);
        item.insert("_version", version);
        mItemsRemoved = 0;
        id = remove(item, "com.nokia.shared.2");
        waitForResponse1(id);
        waitForItemsRemoved(1);
        QCOMPARE(listModel->rowCount(), 47);
        result = getIndex(listModel, 4, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 6);

        deleteModel(listModel);
    }
}

void TestJsonDbCachingListModel::checkUpdateNotification()
{
    resetWaitFlags();
    QVariantList itemList;
    for (int i = 0; i < 50; i++) {
        if (i%2)
            continue;
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Number");
        item.insert("order", i);
        itemList << item;
    }
    int id = create(itemList, "com.nokia.shared.1");
    waitForResponse1(id);

    {
        QAbstractListModel *listModel = createModel();
        if (!listModel) return;

        createIndex("order", "number");

        connectListModel(listModel);
        listModel->setProperty("sortOrder", "[/order]");
        QStringList roleNames = (QStringList() << "_type" << "_uuid" << "_version"<< "name" << "order");
        listModel->setProperty("roleNames", roleNames);
        listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

        mWaitingForReset = true;
        waitForExitOrTimeout();
        QCOMPARE(mWaitingForReset, false);

        QCOMPARE(listModel->rowCount(), 25);
        QVariant result = getIndex(listModel, 0, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 0);
        result = getIndex(listModel, 9, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 18);

        //Update item at 0
        QVariantMap item;
        QVariant uuid = getIndex(listModel, 0, 1);
        item.insert("_uuid", uuid);
        QVariant _type = getIndex(listModel, 0, 0);
        item.insert("_type", _type);
        QVariant _version = getIndex(listModel, 0, 2);
        item.insert("_version", _version);
        QVariant name = getIndex(listModel, 0, 3);
        item.insert("name", name);
        item.insert("order", 1);
        mItemsUpdated = 0;
        id = update(item, "com.nokia.shared.1");
        waitForResponse1(id);
        while (!mItemsUpdated) {
            mWaitingForChanged = true;
            waitForExitOrTimeout();
        }

        QCOMPARE(listModel->rowCount(), 25);
        result = getIndex(listModel, 0, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = getIndex(listModel, 9, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 18);

        //Update item at 9
        item.clear();
        uuid = getIndex(listModel, 9, 1);
        item.insert("_uuid", uuid);
        _type = getIndex(listModel, 9, 0);
        item.insert("_type", _type);
        _version = getIndex(listModel, 9, 2);
        item.insert("_version", _version);
        name = getIndex(listModel, 9, 3);
        item.insert("name", name);
        item.insert("order", 19);
        mItemsUpdated = 0;
        id = update(item,"com.nokia.shared.1");
        waitForResponse1(id);
        while (!mItemsUpdated) {
            mWaitingForChanged = true;
            waitForExitOrTimeout();
        }

        item.clear();
        uuid = getIndex(listModel, 9, 1);
        item.insert("_uuid", uuid);
        _type = getIndex(listModel, 9, 0);
        item.insert("_type", _type);
        name = getIndex(listModel, 9, 3);
        item.insert("name", name);
        _version = getIndex(listModel, 9, 2);
        item.insert("_version", _version);
        item.insert("order", 19);
        mItemsUpdated = 0;
        id = update(item,"com.nokia.shared.1");
        waitForResponse1(id);
        while (!mItemsUpdated) {
            mWaitingForChanged = true;
            waitForExitOrTimeout();
        }

        QCOMPARE(listModel->rowCount(), 25);
        result = getIndex(listModel, 0, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = getIndex(listModel, 9, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 19);

        //Update item at 9
        item.clear();
        uuid = getIndex(listModel, 9, 1);
        item.insert("_uuid", uuid);
        _type = getIndex(listModel, 9, 0);
        item.insert("_type", _type);
        name = getIndex(listModel, 9, 3);
        item.insert("name", name);
        item.insert("order", 59);
        mItemsUpdated = 0;
        id = update(item, "com.nokia.shared.1");
        waitForResponse1(id);
        while (!mItemsUpdated) {
            mWaitingForChanged = true;
            waitForExitOrTimeout();
        }
        QCOMPARE(listModel->rowCount(), 25);
        result = getIndex(listModel, 0, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = getIndex(listModel, 9, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 20);

        //Update item at 8
        item.clear();
        uuid = getIndex(listModel, 8, 1);
        item.insert("_uuid", uuid);
        _type = getIndex(listModel, 8, 0);
        item.insert("_type", _type);
        name = getIndex(listModel, 8, 3);
        item.insert("name", name);
        item.insert("order", 17);
        mItemsUpdated = 0;
        id = update(item, "com.nokia.shared.1");
        waitForResponse1(id);
        while (!mItemsUpdated) {
            mWaitingForChanged = true;
            waitForExitOrTimeout();
        }
        QCOMPARE(listModel->rowCount(), 25);
        result = getIndex(listModel, 8, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 17);
        result = getIndex(listModel, 0, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = getIndex(listModel, 9, 4);
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 20);

        deleteModel(listModel);
    }
}

void TestJsonDbCachingListModel::totalRowCount()
{
    resetWaitFlags();
    int id = 0;
    QVariantList insertedItems;
    for (int i = 0; i < 10; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
        insertedItems << lastResult;
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel)
        return;

    connectListModel(listModel);

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "order");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/order]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 10);

    mItemsCreated = 0;
    for (int i = 10; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        id = create(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }
    waitForItemsCreated(40);
    //wait for the last one
    if (id != lastRequestId)
        waitForResponse1(id);
    QCOMPARE(listModel->rowCount(), 50);

    // Change sort order
    listModel->setProperty("sortOrder", "[\\order]");

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 50);

    // Delete the first 10 items
    mItemsRemoved = 0;
    foreach (QVariant item, insertedItems) {
        id = remove(item.toMap(), "com.nokia.shared.1");
        waitForResponse1(id);
    }
    waitForItemsRemoved(10);

    QCOMPARE(listModel->rowCount(), 40);

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::checkAddNotification()
{
    resetWaitFlags();
    int id = 0;
    QVariantList insertedItems;
    for (int i = 0; i < 10; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel)
        return;

    connectListModel(listModel);

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "order");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/order]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 10);

    mItemsCreated = 0;
    QVariantList items;
    for (int i = 0; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        items << item;
    }
    id = create(items, "com.nokia.shared.2");
    items.clear();
    waitForResponse1(id);
    for (int i = 0; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        items << item;
    }
    id = create(items, "com.nokia.shared.1");
    waitForResponse1(id);
    waitForItemsCreated(100);
    QCOMPARE(listModel->rowCount(), 110);

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::listProperty()
{
    resetWaitFlags();
    QVariant jsonData = readJsonFile(findFile("list-objects.json")).toVariant();
    QVariantList itemList = jsonData.toList();
    int id = 0;
    for (int i = 0; i < itemList.count()/2; i++) {
        id = create(itemList[i].toMap(), "com.nokia.shared.1");
        waitForResponse1(id);
    }
    for (int i = itemList.count()/2; i < itemList.count(); i++) {
        id = create(itemList[i].toMap(), "com.nokia.shared.2");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel)
        return;

    createIndex("features.0.properties.0.description", "string");

    connectListModel(listModel);
    QString type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "features.0.properties.0.description"<< "features.0.feature");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), itemList.count());
    QVariant _type = getIndex(listModel, 0, 0);
    QCOMPARE(_type.toString(), type);
    QVariant featuresDesc = getIndex(listModel, 0, 2);
    QCOMPARE(featuresDesc.toString(), QLatin1String("Facebook account provider"));
    QVariant features = getIndex(listModel, 0, 3);
    QCOMPARE(features.toString(), QLatin1String("provide Facebook"));

    _type = getIndex(listModel, 1, 0);
    QCOMPARE(_type.toString(), type);
    featuresDesc = getIndex(listModel, 1, 2);
    QCOMPARE(featuresDesc.toString(), QLatin1String("Gmail account provider"));
    features = getIndex(listModel, 1, 3);
    QCOMPARE(features.toString(), QLatin1String("provide Gmail"));

    deleteModel(listModel);

    listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    roleNames.clear();
    roleNames = (QStringList() << "_type" << "_uuid" << "features[0].properties[0].description"<< "features[0].supported[0]");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), itemList.count());
    _type = getIndex(listModel, 0, 0);
    QCOMPARE(_type.toString(), type);
    featuresDesc = getIndex(listModel, 0, 2);
    QCOMPARE(featuresDesc.toString(), QLatin1String("Facebook account provider"));
    features = getIndex(listModel, 0, 3);
    QCOMPARE(features.toString(), QLatin1String("share"));

    _type = getIndex(listModel, 1, 0);
    QCOMPARE(_type.toString(), type);
    featuresDesc = getIndex(listModel, 1, 2);
    QCOMPARE(featuresDesc.toString(), QLatin1String("Gmail account provider"));
    features = getIndex(listModel, 1, 3);
    QCOMPARE(features.toString(), QLatin1String("share"));

    deleteModel(listModel);

    // Check that list properties also work with role maps
    listModel = createModel();
    if (!listModel)
        return;

    createIndex("features.0.properties.0.description", "string");

    connectListModel(listModel);
    type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    QVariantMap roleNamesMap;
    roleNamesMap.insert("f2", "features.0.feature");
    roleNamesMap.insert("f1", "features.0.properties.0.description");
    roleNamesMap.insert("u", "_uuid");
    roleNamesMap.insert("t", "_type");
    listModel->setProperty("roleNames", roleNamesMap);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), itemList.count());
    _type = getProperty(listModel, 0, "t");
    QCOMPARE(_type.toString(), type);
    featuresDesc = getProperty(listModel, 0, "f1");
    QCOMPARE(featuresDesc.toString(), QLatin1String("Facebook account provider"));
    features = getProperty(listModel, 0, "f2");
    QCOMPARE(features.toString(), QLatin1String("provide Facebook"));

    _type = getProperty(listModel, 1, "t");
    QCOMPARE(_type.toString(), type);
    featuresDesc = getProperty(listModel, 1, "f1");
    QCOMPARE(featuresDesc.toString(), QLatin1String("Gmail account provider"));
    features = getProperty(listModel, 1, "f2");
    QCOMPARE(features.toString(), QLatin1String("provide Gmail"));

    deleteModel(listModel);

    listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    roleNamesMap.clear();
    roleNamesMap.insert("f2", "features[0].supported[0]");
    roleNamesMap.insert("f1", "features[0].properties[0].description");
    roleNamesMap.insert("u", "_uuid");
    roleNamesMap.insert("t", "_type");
    listModel->setProperty("roleNames", roleNamesMap);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), itemList.count());
    _type = getProperty(listModel, 0, "t");
    QCOMPARE(_type.toString(), type);
    featuresDesc = getProperty(listModel, 0, "f1");
    QCOMPARE(featuresDesc.toString(), QLatin1String("Facebook account provider"));
    features = getProperty(listModel, 0, "f2");
    QCOMPARE(features.toString(), QLatin1String("share"));

    _type = getProperty(listModel, 1, "t");
    QCOMPARE(_type.toString(), type);
    featuresDesc = getProperty(listModel, 1, "f1");
    QCOMPARE(featuresDesc.toString(), QLatin1String("Gmail account provider"));
    features = getProperty(listModel, 1, "f2");
    QCOMPARE(features.toString(), QLatin1String("share"));

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::changeQuery()
{
    resetWaitFlags();
    QVariantMap item;

    for (int i=0; i < 10; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    listModel->setProperty("query", QString(""));

    QCOMPARE(listModel->rowCount(), 0);
    QCOMPARE(listModel->property("query").toString(), QString(""));

    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::indexOfUuid()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie_0"));
    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 1);
    QVariant name = getIndex(listModel, 0, 2);
    QCOMPARE(name.toString(), QString("Arnie_0"));
    QVariant uuid = getIndex(listModel, 0, 1);
    QCOMPARE(indexOf(listModel, uuid.toString()), 0);

    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie_1"));
    mItemsCreated = 0;
    id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);
    waitForItemsCreated(1);

    QCOMPARE(listModel->rowCount(), 2);
    name = getIndex(listModel, 1, 2);
    QCOMPARE(name.toString(), QString("Arnie_1"));
    uuid = getIndex(listModel, 1, 1);
    QCOMPARE(indexOf(listModel, uuid.toString()), 1);
    QCOMPARE(indexOf(listModel, "notValid"), -1);

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::roleNames()
{
    resetWaitFlags();
    QVariantMap item;

    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie"));
    item.insert("friend", QString("Bert"));
    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "friend");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 1);

    QVariant names = listModel->property("roleNames");
    QVariantMap roles = names.toMap();
    for (QVariantMap::const_iterator it = roles.begin(); it != roles.end(); ++it) {
        QCOMPARE(roleNames.contains(it.key()), true);
    }

    // insert again this time usa the map to insert
    listModel->setProperty("roleNames", names);

    names = listModel->property("roleNames");
    roles = names.toMap();
    for (QVariantMap::const_iterator it = roles.begin(); it != roles.end(); ++it) {
        QCOMPARE(roleNames.contains(it.key()), true);
    }

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::getItemNotInCache()
{
    resetWaitFlags();
    QVariantMap item;
    for (int i=0; i < 3000; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("number", i);
        int id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    createIndex("number", "number");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("cacheSize", 50);
    listModel->setProperty("sortOrder", "[/number]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "number");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(mWaitingForReset, false);

    QCOMPARE(listModel->rowCount(), 3000);


    QVariant number = getIndex(listModel, 2967, 2);
    QCOMPARE(number.toInt(), 2967);
    number = getIndex(listModel, 100, 2);
    QCOMPARE(number.toInt(), 100);
    number = getIndex(listModel, 1701, 2);
    QCOMPARE(number.toInt(), 1701);
    number = getIndex(listModel, 20, 2);
    QCOMPARE(number.toInt(), 20);

    deleteModel(listModel);
}

QStringList TestJsonDbCachingListModel::getOrderValues(QAbstractListModel *listModel)
{
    QStringList vals;
    for (int i = 0; i < listModel->rowCount(); ++i) {
        vals << getIndex(listModel, i, 3).toString();
    }
    return vals;
}

void TestJsonDbCachingListModel::modelReset()
{
    if (mWaitingForReset) {
        mWaitingForReset = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbCachingListModel::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    mItemsUpdated++;
    if (mWaitingForChanged) {
        mWaitingForChanged = false;
        eventLoop1.exit(0);
    } else if (mWaitingForIndexChanged && mIndexWaited >= topLeft.row() && mIndexWaited <= bottomRight.row()) {
        mWaitingForIndexChanged = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbCachingListModel::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsCreated += last-first+1;
    if (mWaitingForRowsInserted) {
        mWaitingForRowsInserted = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbCachingListModel::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsRemoved += last-first+1;
    if (mWaitingForRemoved) {
        mWaitingForRemoved = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbCachingListModel::rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row )
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);
}

void TestJsonDbCachingListModel::stateChanged()
{
    // only exit on ready state.
    QAbstractListModel *model = qobject_cast<QAbstractListModel *>(sender());
    if (model->property("state").toInt() == 2 && mWaitingForStateChanged) {
        mWaitingForStateChanged = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbCachingListModel::waitForItemsCreated(int items)
{
    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    while (!mTimedOut && mItemsCreated != items) {
        mWaitingForRowsInserted = true;
        eventLoop1.exec(QEventLoop::AllEvents);
    }
    if (mTimedOut)
        qDebug () << "waitForItemsCreated Timed out";
}

void TestJsonDbCachingListModel::waitForIndexChanged()
{

    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    while (!mTimedOut && mWaitingForIndexChanged) {
        eventLoop1.exec(QEventLoop::AllEvents);
    }
    if (mTimedOut)
        qDebug () << "waitForIndexChanged Timed out";
}

void TestJsonDbCachingListModel::waitForItemsRemoved(int items)
{
    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    while (!mTimedOut && mItemsRemoved != items) {
        mWaitingForRemoved = true;
        eventLoop1.exec(QEventLoop::AllEvents);
    }
    if (mTimedOut)
        qDebug () << "waitForItemsRemoved Timed out";
}

void TestJsonDbCachingListModel::waitForExitOrTimeout()
{
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();
    eventLoop1.exec(QEventLoop::AllEvents);
}

void TestJsonDbCachingListModel::waitForStateOrTimeout()
{
    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    while (mWaitingForStateChanged && !mTimedOut) {
        eventLoop1.exec(QEventLoop::AllEvents);
    }
    if (mTimedOut)
        qDebug () << "waitForStateOrTimeout Timed out";
}

void TestJsonDbCachingListModel::timeout()
{
    qDebug () << "TestJsonDbCachingListModel::timeout()";
    RequestWrapper::timeout();
    mTimedOut = true;
    eventLoop1.quit();
}

void TestJsonDbCachingListModel::resetWaitFlags()
{
    mItemsCreated  = 0;
    mItemsUpdated = 0;
    mItemsRemoved = 0;
    mWaitingForStateChanged = false;
    mWaitingForRowsInserted = false;
    mWaitingForReset = false;
    mWaitingForChanged = false;
    mWaitingForRemoved = false;
}

void TestJsonDbCachingListModel::waitForItemChanged(bool waitForRemove)
{
    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    mWaitingForRemoved = true;
    mWaitingForChanged = true;
    mItemsCreated = 0;
    mWaitingForReset = true;
    mWaitingForStateChanged = true;

    bool waitMore = true;
    while (waitMore && !mTimedOut) {
        if (!mWaitingForChanged) {
            //qDebug() << "waitForItemChanged: mWaitingForChanged";
            break;
        }
        if (!mWaitingForStateChanged) {
            //qDebug() << "waitForItemChanged: mWaitingForStateChanged";
            break;
        }
        if (mItemsCreated){
            //qDebug() << "waitForItemChanged: mItemsCreated";
            break;
        }
        if (!mWaitingForReset){
            //qDebug() << "waitForItemChanged: mWaitingForReset";
            break;
        }
        if (waitForRemove && !mWaitingForRemoved){
            //qDebug() << "waitForItemChanged: mWaitingForRemoved";
            break;
        }
        eventLoop1.exec(QEventLoop::AllEvents);
    }
}
QTEST_MAIN(TestJsonDbCachingListModel)

