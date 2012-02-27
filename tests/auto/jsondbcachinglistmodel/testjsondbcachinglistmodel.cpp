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
#include <QDeclarativeListReference>
#include "json.h"

static const char dbfile[] = "dbFile-jsondb-cached-listmodel";
ModelData::ModelData(): engine(0), component(0), model(0)
{
}

ModelData::~ModelData()
{
    if (model)
        delete model;
    if (partition1)
        delete partition1;
    if (partition2)
        delete partition2;

    if (component)
        delete component;
    if (partitionComponent1)
        delete partitionComponent1;
    if (partitionComponent2)
        delete partitionComponent2;

    if (engine)
        delete engine;
}

const QString qmlProgram = QLatin1String(
            "import QtQuick 2.0 \n"
            "import QtJsonDb 1.0 as JsonDb \n"
            "signal callbackSignal(variant index, variant response)"
            "function updateItemCallback(index, response) { callbackSignal( index, response); }");


TestJsonDbCachingListModel::TestJsonDbCachingListModel()
    : mWaitingForNotification(false), mWaitingForDataChange(false), mWaitingForRowsRemoved(false)
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
        //qDebug() << "Deleted : " << fileInfo.fileName();
        QFile file(fileInfo.fileName());
        file.remove();
    }
}

QVariant TestJsonDbCachingListModel::readJsonFile(const QString& filename)
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

    item.clear();
    item.insert("_type", "Partition");
    item.insert("name", "com.nokia.shared.2");
    id = mClient->create(item);
    waitForResponse1(id);

}

QAbstractListModel *TestJsonDbCachingListModel::createModel()
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
    newModel->component->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n"
                                 "JsonDb.JsonDbCachingListModel {signal callbackSignal(variant index, variant response); id: contactsModel; cacheSize: 200;}",
                                 QUrl());
    newModel->model = newModel->component->create();
    if (newModel->component->isError())
        qDebug() << newModel->component->errors();

    QObject::connect(newModel->model, SIGNAL(callbackSignal(QVariant, QVariant)),
                         this, SLOT(callbackSlot(QVariant, QVariant)));

    newModel->partitionComponent1 = new QDeclarativeComponent(newModel->engine);
    newModel->partitionComponent1->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n"
                                           "JsonDb.Partition {name: \"com.nokia.shared.1\"}",
                                           QUrl());
    newModel->partition1 = newModel->partitionComponent1->create();
    if (newModel->partitionComponent1->isError())
        qDebug() << newModel->partitionComponent1->errors();


    newModel->partitionComponent2 = new QDeclarativeComponent(newModel->engine);
    newModel->partitionComponent2->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n"
                                           "JsonDb.Partition {name: \"com.nokia.shared.2\"}",
                                           QUrl());
    newModel->partition2 = newModel->partitionComponent2->create();
    if (newModel->partitionComponent2->isError())
        qDebug() << newModel->partitionComponent2->errors();

    QDeclarativeListReference partitions(newModel->model, "partitions", newModel->engine);
    partitions.append(newModel->partition1);
    partitions.append(newModel->partition2);

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

void TestJsonDbCachingListModel::callbackSlot(QVariant error, QVariant response)
{
    mCallbackReceived = true;
    callbackError = error.isValid();
    callbackMeta = response;
    callbackResponse = response.toMap().value("object");
    mEventLoop.quit();
}

void TestJsonDbCachingListModel::getIndex(int index)
{
    mCallbackReceived = false;

    const QString createString = QString("get(%1, function (error, response) {callbackSignal(error, response);});");
    const QString getString = QString(createString).arg(index);
    QDeclarativeExpression *expr;
    expr = new QDeclarativeExpression(mModels.last()->engine->rootContext(), mModels.last()->model, getString);
    expr->evaluate().toInt();

    if (!mCallbackReceived)
        waitForCallback();
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

    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    id = mClient->create(item, "com.nokia.shared.2");
    waitForResponse1(id);
}


// Create items in the model.
void TestJsonDbCachingListModel::createItem()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Arnie");
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 1);

    getIndex(0);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(callbackResponse.toMap().value("name").toString(), QLatin1String("Arnie"));

    item.clear();
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Barney");
    id = mClient->create(item, "com.nokia.shared.1");
    waitForItemChanged();

    QCOMPARE(listModel->rowCount(), 2);

    getIndex(1);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(callbackResponse.toMap().value("name").toString(), QLatin1String("Barney"));

    deleteModel(listModel);
}

// Populate model of 300 items two partitions.
void TestJsonDbCachingListModel::createModelTwoPartitions()
{
    QVariantMap item;

    for (int i=0; i < 300; i = i+2) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    for (int i=1; i < 300; i = i+2) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 300);

    deleteModel(listModel);
}



// Create an item and then update it.
void TestJsonDbCachingListModel::updateItemClient()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = mClient->create(item,"com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 1);

    getIndex(0);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(callbackResponse.toMap().value("name").toString(), QLatin1String("Charlie"));

    item.insert("_uuid", mLastUuid);
    item.insert("name", "Baker");

    mWaitingForDataChange = true;

    id = mClient->update(item, "com.nokia.shared.1");
    waitForItemChanged();

    QCOMPARE(listModel->rowCount(), 1);

    getIndex(0);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(callbackResponse.toMap().value("name").toString(), QLatin1String("Baker"));
    deleteModel(listModel);
}

void TestJsonDbCachingListModel::deleteItem()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 1);

    item.insert("name", "Baker");
    id = mClient->create(item, "com.nokia.shared.2");
    waitForItemChanged();

    QCOMPARE(listModel->rowCount(), 2);

    mWaitingForRowsRemoved = true;
    item.insert("_uuid", mLastUuid);
    id = mClient->remove(item, "com.nokia.shared.2");
    waitForItemChanged(true);


    QCOMPARE(listModel->rowCount(), 1);
    getIndex(0);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(callbackResponse.toMap().value("name").toString(), QLatin1String("Charlie"));
    deleteModel(listModel);
}

void TestJsonDbCachingListModel::sortedQuery()
{
    int id = 0;
    for (int i = 0; i < 1000; i++) {
        QVariantMap item;
        item.insert("_type", "RandNumber");
        item.insert("number", i);
        id = mClient->create(item,"com.nokia.shared.2");
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
    listModel->setProperty("query", "[?_type=\"RandNumber\"]");

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 1000);
    for (int i = 0; i < 1000; i++) {
        getIndex(i);
        QCOMPARE(callbackResponse.toMap().value("number").toInt(), i);
    }

    listModel->setProperty("sortOrder", "[\\number]");

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 1000);

    for (int i = 0; i < 1000; i++) {
        getIndex(i);
        QCOMPARE(callbackResponse.toMap().value("number").toInt(), 999-i);
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
    for (int i = 9; i >= 1; --i) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("ordering", QString::number(i));
        int id = mClient->create(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    createIndex("ordering", "string");

    listModel->setProperty("sortOrder", "[/ordering]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "ordering");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    waitForStateOrTimeout();

    QStringList expectedOrder = QStringList() << "1" << "2" << "3" << "4" <<
                                                 "5" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        getIndex(4);
        QVariant uuid = callbackResponse.toMap().value("_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("ordering", "99");  // move it to the end
        mClient->update(item, "com.nokia.shared.2");
    }
    waitForItemChanged();

    expectedOrder = QStringList() << "1" << "2" << "3" <<
                                     "4" << "6" << "7" << "8" << "9" << "99";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        getIndex(8);
        QVariant uuid =callbackResponse.toMap().value("_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("ordering", "22");    // move it after "2"
        mClient->update(item, "com.nokia.shared.2");
    }
    waitForItemChanged();

    expectedOrder = QStringList() << "1" << "2" << "22" << "3" <<
                                     "4" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        getIndex(5);
        QVariant uuid =callbackResponse.toMap().value("_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("ordering", "0");    // move it to the beginning
        mClient->update(item, "com.nokia.shared.2");
    }
    waitForItemChanged();

    expectedOrder = QStringList() << "0" << "1" << "2" << "22" << "3" <<
                                     "4" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);

    listModel->setProperty("sortOrder", "[\\ordering]");

    waitForStateOrTimeout();

    QStringList reverseOrder = expectedOrder;
    qSort(reverseOrder.begin(), reverseOrder.end(), greaterThan);

    QCOMPARE(getOrderValues(listModel), reverseOrder);
    listModel->setProperty("sortOrder", "[/ordering]");

    waitForStateOrTimeout();


    QCOMPARE(getOrderValues(listModel), expectedOrder);

    deleteModel(listModel);

}

void TestJsonDbCachingListModel::checkRemoveNotification()
{
    QVariantList itemList;
    for (int i = 0; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Number");
        item.insert("order", i);
        itemList << item;
    }
    int id = mClient->create(itemList,"com.nokia.shared.2");
    waitForResponse1(id);

    {
        QAbstractListModel *listModel = createModel();
        if (!listModel) return;

        createIndex("order", "number");

        connectListModel(listModel);
        listModel->setProperty("sortOrder", "[/order]");
        listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
        QStringList roleNames = (QStringList() << "_type" << "_uuid" << "_version"<< "name" << "order");
        listModel->setProperty("roleNames", roleNames);

        waitForStateOrTimeout();

        QCOMPARE(listModel->rowCount(), 50);
        getIndex(0);
        QVariant result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 0);
        getIndex(9);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 9);

        //Remove item at 0
        QVariantMap item;
        getIndex(0);
        item.insert("_uuid", callbackResponse.toMap().value("_uuid"));
        item.insert("_version", callbackResponse.toMap().value("_version"));
        id = mClient->remove(item, "com.nokia.shared.2");
        waitForItemChanged(true);
        QCOMPARE(listModel->rowCount(), 49);
        getIndex(9);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 10);

        //Remove item at 9
        getIndex(9);
        item.insert("_uuid", callbackResponse.toMap().value("_uuid"));
        item.insert("_version", callbackResponse.toMap().value("_version"));
        id = mClient->remove(item, "com.nokia.shared.2");
        waitForItemChanged(true);
        QCOMPARE(listModel->rowCount(), 48);
        getIndex(9);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 11);

        //Remove item at 4
        getIndex(4);
        item.insert("_uuid", callbackResponse.toMap().value("_uuid"));
        item.insert("_version", callbackResponse.toMap().value("_version"));
        id = mClient->remove(item, "com.nokia.shared.2");
        waitForItemChanged(true);
        QCOMPARE(listModel->rowCount(), 47);
        getIndex(4);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 6);

        deleteModel(listModel);
    }
}

void TestJsonDbCachingListModel::checkUpdateNotification()
{
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
    int id = mClient->create(itemList, "com.nokia.shared.1");
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

        waitForStateOrTimeout();

        QCOMPARE(listModel->rowCount(), 25);
        getIndex(0);
        QVariant result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 0);
        getIndex(9);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 18);

        //Update item at 0
        QVariantMap item;
        getIndex(0);
        item.insert("_uuid", callbackResponse.toMap().value("_uuid"));
        item.insert("_type", callbackResponse.toMap().value("_type"));
        item.insert("name", callbackResponse.toMap().value("name"));
        item.insert("order", 1);
        id = mClient->update(item, "com.nokia.shared.1");
        waitForItemChanged();

        QCOMPARE(listModel->rowCount(), 25);
        getIndex(0);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        getIndex(9);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 18);

        //Update item at 9
        item.clear();
        getIndex(9);
        item.insert("_uuid", callbackResponse.toMap().value("_uuid"));
        item.insert("_type", callbackResponse.toMap().value("_type"));
        item.insert("name", callbackResponse.toMap().value("name"));
        item.insert("order", 19);
        id = mClient->update(item,"com.nokia.shared.1");
        waitForItemChanged();

        item.clear();
        getIndex(9);
        item.insert("_uuid", callbackResponse.toMap().value("_uuid"));
        item.insert("_type", callbackResponse.toMap().value("_type"));
        item.insert("name", callbackResponse.toMap().value("name"));
        item.insert("order", 19);
        id = mClient->update(item,"com.nokia.shared.1");
        waitForItemChanged();

        QCOMPARE(listModel->rowCount(), 25);
        getIndex(0);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        getIndex(9);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 19);

        //Update item at 9
        getIndex(9);
        item.insert("_uuid", callbackResponse.toMap().value("_uuid"));
        item.insert("_type", callbackResponse.toMap().value("_type"));
        item.insert("name", callbackResponse.toMap().value("name"));
        item.insert("order", 59);
        id = mClient->update(item, "com.nokia.shared.1");
        waitForItemChanged();

        QCOMPARE(listModel->rowCount(), 25);
        getIndex(0);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        getIndex(9);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 20);

        //Update item at 8
        getIndex(8);
        item.insert("_uuid", callbackResponse.toMap().value("_uuid"));
        item.insert("_type", callbackResponse.toMap().value("_type"));
        item.insert("name", callbackResponse.toMap().value("name"));
        item.insert("order", 17);
        id = mClient->update(item, "com.nokia.shared.1");
        waitForItemChanged();

        QCOMPARE(listModel->rowCount(), 25);
        getIndex(8);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 17);
        getIndex(0);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        getIndex(9);
        result = callbackResponse.toMap().value("order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 20);

        deleteModel(listModel);
    }
}

void TestJsonDbCachingListModel::totalRowCount()
{
    int id = 0;
    QVariantList insertedItems;
    for (int i = 0; i < 10; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
        insertedItems << mData;
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel)
        return;

    connectListModel(listModel);

    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "order");
    listModel->setProperty("roleNames", roleNames);

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 10);

    for (int i = 10; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        mClient->create(item, "com.nokia.shared.2");
    }
    waitForItemsCreated(40);
    QCOMPARE(listModel->rowCount(), 50);


    createIndex("order", "number");

    // Change sort order
    listModel->setProperty("sortOrder", "[\\order]");
    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 50);

    // Delete the first 10 items
    foreach (QVariant item, insertedItems) {
        mWaitingForRowsRemoved = true;
        id = mClient->remove(item.toMap(), "com.nokia.shared.1");
        waitForItemChanged(true);
    }

    QCOMPARE(listModel->rowCount(), 40);

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::listProperty()
{
    QVariant jsonData = readJsonFile("list-objects.json");
    QVariantList itemList = jsonData.toList();
    int id = 0;
    for (int i = 0; i < itemList.count()/2; i++) {
        id = mClient->create(itemList[i].toMap(), "com.nokia.shared.1");
        waitForResponse1(id);
    }
    for (int i = itemList.count()/2; i < itemList.count(); i++) {
        id = mClient->create(itemList[i].toMap(), "com.nokia.shared.2");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel)
        return;

    createIndex("features.0.properties.0.description", "string");

    connectListModel(listModel);
    QString type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "features.0.properties.0.description"<< "features.0.feature");
    listModel->setProperty("roleNames", roleNames);

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), itemList.count());
    getIndex(0);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), type);
    QCOMPARE(callbackResponse.toMap().value("features").toList().at(0).toMap().value("properties").toList().at(0).toMap().value("description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(callbackResponse.toMap().value("features").toList().at(0).toMap().value("feature").toString(), QLatin1String("provide Facebook"));
    getIndex(1);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), type);
    QCOMPARE(callbackResponse.toMap().value("features").toList().at(0).toMap().value("properties").toList().at(0).toMap().value("description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(callbackResponse.toMap().value("features").toList().at(0).toMap().value("feature").toString(), QLatin1String("provide Gmail"));

    deleteModel(listModel);

    listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));
    roleNames.clear();
    roleNames = (QStringList() << "_type" << "_uuid" << "features[0].properties[0].description"<< "features[0].supported[0]");
    listModel->setProperty("roleNames", roleNames);
    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), itemList.count());
    getIndex(0);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), type);
    QCOMPARE(callbackResponse.toMap().value("features").toList().at(0).toMap().value("properties").toList().at(0).toMap().value("description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(callbackResponse.toMap().value("features").toList().at(0).toMap().value("supported").toList().at(0).toString(), QLatin1String("share"));
    getIndex(1);
    QCOMPARE(callbackResponse.toMap().value("_type").toString(), type);
    QCOMPARE(callbackResponse.toMap().value("features").toList().at(0).toMap().value("properties").toList().at(0).toMap().value("description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(callbackResponse.toMap().value("features").toList().at(0).toMap().value("supported").toList().at(0).toString(), QLatin1String("share"));

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::changeQuery()
{
    QVariantMap item;

    for (int i=0; i < 10; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.1");
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

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    listModel->setProperty("query", QString(""));

    QCOMPARE(listModel->rowCount(), 0);
    QCOMPARE(listModel->property("query").toString(), QString(""));

    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::indexOfUuid()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie_0"));
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 1);
    getIndex(0);
    QCOMPARE(callbackResponse.toMap().value("name").toString(), QString("Arnie_0"));
    QCOMPARE(indexOf(listModel, callbackResponse.toMap().value("_uuid").toString()), 0);

    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie_1"));
    id = mClient->create(item, "com.nokia.shared.1");

    waitForItemsCreated(1);

    QCOMPARE(listModel->rowCount(), 2);
    getIndex(1);
    QCOMPARE(callbackResponse.toMap().value("name").toString(), QString("Arnie_1"));
    QCOMPARE(indexOf(listModel, callbackResponse.toMap().value("_uuid").toString()), 1);
    QCOMPARE(indexOf(listModel, "notValid"), -1);

    deleteModel(listModel);
}

void TestJsonDbCachingListModel::roleNames()
{
    QVariantMap item;

    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie"));
    item.insert("friend", QString("Bert"));
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);


    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "friend");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    waitForStateOrTimeout();

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

QStringList TestJsonDbCachingListModel::getOrderValues(QAbstractListModel *listModel)
{
    QStringList vals;
    for (int i = 0; i < listModel->rowCount(); ++i) {
        getIndex(i);
        vals << callbackResponse.toMap().value("ordering").toString();
    }
    return vals;
}

void TestJsonDbCachingListModel::modelReset()
{
    mWaitingForReset = false;
    mEventLoop2.exit(0);
}

void TestJsonDbCachingListModel::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    mWaitingForDataChange = false;
}

void TestJsonDbCachingListModel::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsCreated++;
    mEventLoop2.exit(0);
}

void TestJsonDbCachingListModel::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mWaitingForRowsRemoved = false;
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
    if (model->property("state") == 2) {
        mWaitingForStateChanged = false;
        mEventLoop2.exit(0);
    }
}

void TestJsonDbCachingListModel::waitForItemsCreated(int items)
{
    mTimeoutCalled = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop2, SLOT(quit()));
    timer.start(mClientTimeout);
    mElapsedTimer.start();

    mItemsCreated = 0;
    while (mItemsCreated != items && !mTimeoutCalled)
        mEventLoop2.processEvents(QEventLoop::AllEvents, mClientTimeout);
}

void TestJsonDbCachingListModel::waitForExitOrTimeout()
{
    mTimeoutCalled = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop2, SLOT(quit()));
    timer.start(mClientTimeout);
    mElapsedTimer.start();
    mEventLoop2.exec(QEventLoop::AllEvents);
}

void TestJsonDbCachingListModel::waitForStateOrTimeout()
{
    mTimeoutCalled = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop2, SLOT(quit()));
    timer.start(mClientTimeout);
    mElapsedTimer.start();

    mWaitingForStateChanged = true;
    while (mWaitingForStateChanged && !mTimeoutCalled)
        mEventLoop2.processEvents(QEventLoop::AllEvents, mClientTimeout);
}

void TestJsonDbCachingListModel::timeout()
{
    ClientWrapper::timeout();
    mTimeoutCalled = true;
    mTimedOut = true;
}

void TestJsonDbCachingListModel::waitForItemChanged(bool waitForRemove)
{
    mTimeoutCalled = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop2, SLOT(quit()));
    timer.start(mClientTimeout);
    mElapsedTimer.start();

    mWaitingForRowsRemoved = true;
    mWaitingForDataChange = true;
    mItemsCreated = 0;
    mWaitingForReset = true;

    bool waitMore = true;
    while (waitMore && !mTimeoutCalled) {
        if (!mWaitingForDataChange)
            break;
        if (mItemsCreated)
            break;
        if (!mWaitingForReset)
            break;
        if (waitForRemove && !mWaitingForRowsRemoved)
            break;
        mEventLoop2.processEvents(QEventLoop::AllEvents);
    }
}
QTEST_MAIN(TestJsonDbCachingListModel)

