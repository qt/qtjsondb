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
#include "testjsondbsortinglistmodel.h"

#include "../../shared/util.h"
#include <QDeclarativeListReference>
#include "json.h"

static const char dbfile[] = "dbFile-jsondb-listmodel";
ModelData::ModelData(): engine(0), component(0), model(0)
{
}

ModelData::~ModelData()
{
    if (model)
        delete model;
    if (parttion1)
        delete parttion1;
    if (parttion2)
        delete parttion2;

    if (component)
        delete component;
    if (partitionComponent1)
        delete partitionComponent1;
    if (partitionComponent2)
        delete partitionComponent2;

    if (engine)
        delete engine;
}

QVariant get(QObject* object, int index, QString propertyName)
{
    QVariant retVal;
    QMetaObject::invokeMethod(object, "get", Qt::DirectConnection,
                              Q_RETURN_ARG(QVariant, retVal),
                              Q_ARG(int, index),
                              Q_ARG(QString, propertyName));
    return retVal;
}

QJSValue get(QObject* object, int index)
{
    QJSValue retVal;
    QMetaObject::invokeMethod(object, "get", Qt::DirectConnection,
                              Q_RETURN_ARG(QJSValue, retVal),
                              Q_ARG(int, index));
    return retVal;
}

int indexOf(QObject* object, const QString &uuid)
{
    int retVal;
    QMetaObject::invokeMethod(object, "indexOf", Qt::DirectConnection,
                              Q_RETURN_ARG(int, retVal),
                              Q_ARG(QString, uuid));
    return retVal;
}

TestJsonDbSortingListModel::TestJsonDbSortingListModel()
    : mWaitingForNotification(false), mWaitingForDataChange(false), mWaitingForRowsRemoved(false)
{
}

TestJsonDbSortingListModel::~TestJsonDbSortingListModel()
{
}

void TestJsonDbSortingListModel::deleteDbFiles()
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

QVariant TestJsonDbSortingListModel::readJsonFile(const QString& filename)
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

void TestJsonDbSortingListModel::connectListModel(QAbstractListModel *model)
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

void TestJsonDbSortingListModel::initTestCase()
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

QAbstractListModel *TestJsonDbSortingListModel::createModel()
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
                                 "JsonDb.JsonDbSortingListModel {id: contactsModel}",
                                 QUrl());
    newModel->model = newModel->component->create();
    if (newModel->component->isError())
        qDebug() << newModel->component->errors();

    newModel->partitionComponent1 = new QDeclarativeComponent(newModel->engine);
    newModel->partitionComponent1->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n"
                                           "JsonDb.Partition {name: \"com.nokia.shared.1\"}",
                                           QUrl());
    newModel->parttion1 = newModel->partitionComponent1->create();
    if (newModel->partitionComponent1->isError())
        qDebug() << newModel->partitionComponent1->errors();


    newModel->partitionComponent2 = new QDeclarativeComponent(newModel->engine);
    newModel->partitionComponent2->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n"
                                           "JsonDb.Partition {name: \"com.nokia.shared.2\"}",
                                           QUrl());
    newModel->parttion2 = newModel->partitionComponent2->create();
    if (newModel->partitionComponent2->isError())
        qDebug() << newModel->partitionComponent2->errors();

    QDeclarativeListReference partitions(newModel->model, "partitions", newModel->engine);
    partitions.append(newModel->parttion1);
    partitions.append(newModel->parttion2);

    mModels.append(newModel);
    return (QAbstractListModel*)(newModel->model);
}

void TestJsonDbSortingListModel::deleteModel(QAbstractListModel *model)
{
    for (int i = 0; i < mModels.count(); i++) {
        if (mModels[i]->model == model) {
            ModelData *modelData = mModels.takeAt(i);
            delete modelData;
            return;
        }
    }
}

void TestJsonDbSortingListModel::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

// Create items in the model.
void TestJsonDbSortingListModel::createItem()
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
    QCOMPARE(listModel->rowCount(), 0);

    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(listModel->property("state").toInt(), 2);

    item.clear();
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Baker");
    id = mClient->create(item, "com.nokia.shared.1");
    waitForItemChanged();

    QCOMPARE(listModel->rowCount(), 2);
    deleteModel(listModel);
}

// Create an item and then update it.
void TestJsonDbSortingListModel::updateItemClient()
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

    item.insert("_uuid", mLastUuid);
    item.insert("name", "Baker");

    mWaitingForDataChange = true;

    id = mClient->update(item, "com.nokia.shared.1");
    waitForItemChanged();

    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(get(listModel, 0, "_uuid").toString(), mLastUuid);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel,0, "name").toString(), QLatin1String("Baker"));
    deleteModel(listModel);
}

void TestJsonDbSortingListModel::deleteItem()
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
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Charlie"));
    deleteModel(listModel);
}

void TestJsonDbSortingListModel::sortedQuery()
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

    connectListModel(listModel);

    QStringList rolenames;
    rolenames << "_uuid" << "_type" << "number";
    listModel->setProperty("roleNames", rolenames);
    listModel->setProperty("sortOrder", "[/number]");
    listModel->setProperty("query", "[?_type=\"RandNumber\"]");
    waitForStateOrTimeout();

    QCOMPARE(listModel->property("sortOrder").toString(), QString("[/number]"));

    QCOMPARE(listModel->rowCount(), 1000);
    for (int i = 0; i < 1000; i++)
        QCOMPARE(get(listModel, i,"number").toInt(), i);

    listModel->setProperty("sortOrder", "[\\number]");
    for (int i = 0; i < 1000; i++)
        QCOMPARE(get(listModel, i,"number").toInt(), 999-i);

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

void TestJsonDbSortingListModel::ordering()
{
    for (int i = 9; i >= 1; --i) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", QString::number(i));
        int id = mClient->create(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    listModel->setProperty("sortOrder", "[/order]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "order");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);
    waitForStateOrTimeout();
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
        mClient->update(item, "com.nokia.shared.2");
    }
    waitForItemChanged();

    expectedOrder = QStringList() << "1" << "2" << "3" <<
                                     "4" << "6" << "7" << "8" << "9" << "99";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        QVariant uuid = get(listModel, 8, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "22");    // move it after "2"
        mClient->update(item, "com.nokia.shared.2");
    }
    waitForItemChanged();

    expectedOrder = QStringList() << "1" << "2" << "22" << "3" <<
                                     "4" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        QVariant uuid = get(listModel, 5, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "0");    // move it to the beginning
        mClient->update(item, "com.nokia.shared.2");
    }
    waitForItemChanged();

    // Check for order and togther with queryLimit
    expectedOrder = QStringList() << "0" << "1" << "2" << "22" << "3" <<
                                     "4" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    listModel->setProperty("sortOrder", "[\\order]");

    QStringList reverseOrder = expectedOrder;
    qSort(reverseOrder.begin(), reverseOrder.end(), greaterThan);
    QCOMPARE(getOrderValues(listModel), reverseOrder);
    listModel->setProperty("sortOrder", "[/order]");
    QCOMPARE(getOrderValues(listModel), expectedOrder);

    listModel->setProperty("queryLimit", 5);
    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 5);
    QCOMPARE(getOrderValues(listModel), QStringList(expectedOrder.mid(0, 5)));

    listModel->setProperty("sortOrder", "[\\order]");
    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 5);
    QCOMPARE(getOrderValues(listModel), QStringList(reverseOrder.mid(0, 5)));

    deleteModel(listModel);

}

void TestJsonDbSortingListModel::checkRemoveNotification()
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
        connectListModel(listModel);
        listModel->setProperty("queryLimit", 10);
        listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
        listModel->setProperty("sortOrder", "[/order]");
        QStringList roleNames = (QStringList() << "_type" << "_uuid" << "_version"<< "name" << "order");
        listModel->setProperty("roleNames", roleNames);
        waitForStateOrTimeout();

        QCOMPARE(listModel->rowCount(), 10);
        QVariant result = get(listModel, 0, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 0);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 9);

        //Remove item at 0
        QVariantMap item;
        item.insert("_uuid", get(listModel, 0, "_uuid"));
        item.insert("_version", get(listModel, 0, "_version"));
        id = mClient->remove(item, "com.nokia.shared.2");
        waitForItemChanged();
        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 10);

        //Remove item at 9
        item.insert("_uuid", get(listModel, 9, "_uuid"));
        item.insert("_version", get(listModel, 9, "_version"));
        id = mClient->remove(item, "com.nokia.shared.2");
        waitForItemChanged();
        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 11);

        //Remove item at 4
        item.insert("_uuid", get(listModel, 4, "_uuid"));
        item.insert("_version", get(listModel, 4, "_version"));
        id = mClient->remove(item, "com.nokia.shared.2");
        waitForItemChanged();
        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 4, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 6);

        deleteModel(listModel);
    }
}

void TestJsonDbSortingListModel::checkUpdateNotification()
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
        connectListModel(listModel);
        listModel->setProperty("queryLimit", 10);
        listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
        listModel->setProperty("sortOrder", "[/order]");
        QStringList roleNames = (QStringList() << "_type" << "_uuid" << "_version"<< "name" << "order");
        listModel->setProperty("roleNames", roleNames);
        waitForStateOrTimeout();

        QCOMPARE(listModel->rowCount(), 10);
        QVariant result = get(listModel, 0, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 0);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 18);

        //Update item at 0
        QVariantMap item;
        item.insert("_uuid", get(listModel, 0, "_uuid"));
        item.insert("_type", get(listModel, 0, "_type"));
        item.insert("name", get(listModel, 0, "name"));
        item.insert("order", 1);
        id = mClient->update(item, "com.nokia.shared.1");
        waitForItemChanged();

        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 0, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 18);

        //Update item at 9
        item.clear();
        item.insert("_uuid", get(listModel, 9, "_uuid"));
        item.insert("_type", get(listModel, 9, "_type"));
        item.insert("name", get(listModel, 9, "name"));
        item.insert("order", 19);
        id = mClient->update(item,"com.nokia.shared.1");
        waitForItemChanged();

        item.clear();
        item.insert("_uuid", get(listModel, 9, "_uuid"));
        item.insert("_type", get(listModel, 9, "_type"));
        item.insert("name", get(listModel, 9, "name"));
        item.insert("order", 19);
        id = mClient->update(item,"com.nokia.shared.1");
        waitForItemChanged();

        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 0, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 19);

        //Update item at 9
        item.insert("_uuid", get(listModel, 9, "_uuid"));
        item.insert("_type", get(listModel, 9, "_type"));
        item.insert("name", get(listModel, 9, "name"));
        item.insert("order", 59);
        id = mClient->update(item, "com.nokia.shared.1");
        waitForItemChanged();

        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 0, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 20);

        //Update item at 8
        item.insert("_uuid", get(listModel, 8, "_uuid"));
        item.insert("_type", get(listModel, 8, "_type"));
        item.insert("name", get(listModel, 8, "name"));
        item.insert("order", 17);
        id = mClient->update(item, "com.nokia.shared.1");
        waitForItemChanged();

        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 8, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 17);
        result = get(listModel, 0, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 20);

        deleteModel(listModel);
    }
}

void TestJsonDbSortingListModel::totalRowCount()
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

    listModel->setProperty("queryLimit", 50);
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

void TestJsonDbSortingListModel::listProperty()
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
    connectListModel(listModel);
    QString type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("queryLimit", 10);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "features.0.properties.0.description"<< "features.0.feature");
    listModel->setProperty("roleNames", roleNames);
    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), itemList.count());
    QCOMPARE(get(listModel, 0, "_type").toString(), type);
    QCOMPARE(get(listModel, 0, "features.0.properties.0.description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(get(listModel, 0, "features.0.feature").toString(), QLatin1String("provide Facebook"));
    QCOMPARE(get(listModel, 1, "_uuid").toString(), mLastUuid);
    QCOMPARE(get(listModel, 1, "_type").toString(), type);
    QCOMPARE(get(listModel, 1, "features.0.properties.0.description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(get(listModel, 1, "features.0.feature").toString(), QLatin1String("provide Gmail"));

    deleteModel(listModel);

    listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("queryLimit", 10);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    roleNames.clear();
    roleNames = (QStringList() << "_type" << "_uuid" << "features[0].properties[0].description"<< "features[0].supported[0]");
    listModel->setProperty("roleNames", roleNames);
    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), itemList.count());
    QCOMPARE(get(listModel, 0, "_type").toString(), type);
    QCOMPARE(get(listModel, 0, "features[0].properties[0].description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(get(listModel, 0, "features[0].supported[0]").toString(), QLatin1String("share"));
    QCOMPARE(get(listModel, 1, "_type").toString(), type);
    QCOMPARE(get(listModel, 1, "features[0].properties[0].description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(get(listModel, 1, "features[0].supported[0]").toString(), QLatin1String("share"));

    deleteModel(listModel);
}


// Populate model of 300 items two partitions.
void TestJsonDbSortingListModel::twoPartitions()
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

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 300);
    QCOMPARE(get(listModel, 0, "name").toString(), QString("Arnie_0"));
    QCOMPARE(get(listModel, 1, "name").toString(), QString("Arnie_1"));
    QCOMPARE(get(listModel, 2, "name").toString(), QString("Arnie_10"));
    QCOMPARE(get(listModel, 3, "name").toString(), QString("Arnie_100"));

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::changeQuery()
{
    QVariantMap item;

    for (int i=0; i < 10; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

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

void TestJsonDbSortingListModel::getQJSValue()
{
    QVariantMap item;

    for (int i=0; i < 10; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(get(listModel, 0).property("object").property("name").toString(), QString("Arnie_0"));
    QCOMPARE(get(listModel, 1).property("object").property("name").toString(), QString("Arnie_1"));

    deleteModel(listModel);
}


void TestJsonDbSortingListModel::indexOfUuid()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie_0"));
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(get(listModel, 0, "name").toString(), QString("Arnie_0"));
    QCOMPARE(indexOf(listModel, get(listModel, 0, "_uuid").toString()), 0);

    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie_1"));
    id = mClient->create(item, "com.nokia.shared.1");

    waitForItemsCreated(1);

    QCOMPARE(listModel->rowCount(), 2);
    QCOMPARE(get(listModel, 1, "name").toString(), QString("Arnie_1"));
    QCOMPARE(indexOf(listModel, get(listModel, 1, "_uuid").toString()), 1);
    QCOMPARE(indexOf(listModel, "notValid"), -1);

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::queryLimit()
{
    QVariantMap item;

    for (int i=0; i < 300; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("queryLimit", 100);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    waitForStateOrTimeout();

    QCOMPARE(listModel->rowCount(), 100);
    QCOMPARE(listModel->property("overflow").toBool(), true);

    listModel->setProperty("queryLimit", 500);

    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 300);
    QCOMPARE(listModel->property("overflow").toBool(), false);

    deleteModel(listModel);
}


QStringList TestJsonDbSortingListModel::getOrderValues(QAbstractListModel *listModel)
{
    QStringList vals;
    for (int i = 0; i < listModel->rowCount(); ++i)
        vals << get(listModel, i, "order").toString();
    return vals;
}

void TestJsonDbSortingListModel::modelReset()
{
    mWaitingForReset = false;
    mEventLoop2.exit(0);
}

void TestJsonDbSortingListModel::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    mWaitingForDataChange = false;
}

void TestJsonDbSortingListModel::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsCreated++;
    mEventLoop2.exit(0);
}

void TestJsonDbSortingListModel::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mWaitingForRowsRemoved = false;
}

void TestJsonDbSortingListModel::rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row )
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);
}

void TestJsonDbSortingListModel::stateChanged()
{
    mWaitingForStateChanged = false;
    mEventLoop2.exit(0);
}

void TestJsonDbSortingListModel::waitForItemsCreated(int items)
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

void TestJsonDbSortingListModel::waitForExitOrTimeout()
{
    mTimeoutCalled = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop2, SLOT(quit()));
    timer.start(mClientTimeout);
    mElapsedTimer.start();
    mEventLoop2.exec(QEventLoop::AllEvents);
}

void TestJsonDbSortingListModel::waitForStateOrTimeout()
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

void TestJsonDbSortingListModel::timeout()
{
    ClientWrapper::timeout();
    mTimeoutCalled = true;
}

void TestJsonDbSortingListModel::waitForItemChanged(bool waitForRemove)
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
QTEST_MAIN(TestJsonDbSortingListModel)

