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
#include "jsondbsortinglistmodel-bench.h"

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


JsonDbSortingListModelBench::JsonDbSortingListModelBench()
    : mWaitingForNotification(false), mWaitingForDataChange(false), mWaitingForRowsRemoved(false)
{
}

JsonDbSortingListModelBench::~JsonDbSortingListModelBench()
{
}

void JsonDbSortingListModelBench::deleteDbFiles()
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

void JsonDbSortingListModelBench::connectListModel(QAbstractListModel *model)
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

void JsonDbSortingListModelBench::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << "-base-name" << dbfile);

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

QAbstractListModel *JsonDbSortingListModelBench::createModel()
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
    newModel->component->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n"
                                 "JsonDb.JsonDbCachingListModel {signal callbackSignal(variant index, variant response); id: contactsModel;}",
                                 QUrl());
    newModel->model = newModel->component->create();
    if (newModel->component->isError())
        qDebug() << newModel->component->errors();

    QObject::connect(newModel->model, SIGNAL(callbackSignal(QVariant, QVariant)),
                         this, SLOT(callbackSlot(QVariant, QVariant)));

    newModel->partitionComponent1 = new QQmlComponent(newModel->engine);
    newModel->partitionComponent1->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n"
                                           "JsonDb.Partition {name: \"com.nokia.shared.1\"}",
                                           QUrl());
    newModel->partition1 = newModel->partitionComponent1->create();
    if (newModel->partitionComponent1->isError())
        qDebug() << newModel->partitionComponent1->errors();


    newModel->partitionComponent2 = new QQmlComponent(newModel->engine);
    newModel->partitionComponent2->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n"
                                           "JsonDb.Partition {name: \"com.nokia.shared.2\"}",
                                           QUrl());
    newModel->partition2 = newModel->partitionComponent2->create();
    if (newModel->partitionComponent2->isError())
        qDebug() << newModel->partitionComponent2->errors();

    QQmlListReference partitions(newModel->model, "partitions", newModel->engine);
    partitions.append(newModel->partition1);
    partitions.append(newModel->partition2);

    mModels.append(newModel);
    return (QAbstractListModel*)(newModel->model);
}

void JsonDbSortingListModelBench::deleteModel(QAbstractListModel *model)
{
    for (int i = 0; i < mModels.count(); i++) {
        if (mModels[i]->model == model) {
            ModelData *modelData = mModels.takeAt(i);
            delete modelData;
            return;
        }
    }
}

// Delete all the items of this type from JsonDb
void JsonDbSortingListModelBench::deleteItems(const QString &type, const QString &partition)
{
    int id = mClient->query(QString("[?_type=\"%1\"]").arg(type), 0, -1, partition);
    waitForResponse1(id);
    id = mClient->remove(mData.toMap().value("data"), partition);
    waitForResponse1(id);
}

void JsonDbSortingListModelBench::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

void JsonDbSortingListModelBench::callbackSlot(QVariant error, QVariant response)
{
    mCallbackReceived = true;
    callbackError = error.isValid();
    callbackMeta = response;
    callbackResponse = response.toMap().value("object");
    mEventLoop.quit();
}

void JsonDbSortingListModelBench::getIndex(int index)
{
    mCallbackReceived = false;

    const QString createString = QString("get(%1, function (error, response) {callbackSignal(error, response);});");
    const QString getString = QString(createString).arg(index);
    QQmlExpression expr(mModels.last()->engine->rootContext(), mModels.last()->model, getString);
    expr.evaluate().toInt();

    if (!mCallbackReceived)
        waitForCallback();
}

void JsonDbSortingListModelBench::createIndex(const QString &property, const QString &propertyType)
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


// Populate model of 300 items.
void JsonDbSortingListModelBench::ModelStartup()
{
    QVariantMap item;

    for (int i=0; i < 300; i++) {
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
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    QBENCHMARK_ONCE {
        waitForStateOrTimeout();
    }

    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}

// Populate model of 300 items two partitions.
void JsonDbSortingListModelBench::ModelStartupTwoPartitions()
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

    QBENCHMARK_ONCE {
        waitForStateOrTimeout();
    }

    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteItems(__FUNCTION__, "com.nokia.shared.2");
    deleteModel(listModel);
}


// Populate model of 300 items sorted.
void JsonDbSortingListModelBench::ModelStartupSorted()
{
    QVariantMap item;

    for (int i=0; i < 300; i++) {
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

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    QBENCHMARK_ONCE {
        waitForStateOrTimeout();
    }

    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}


void JsonDbSortingListModelBench::getItemNotInCache()
{
    QVariantMap item;
    for (int i=0; i < 300; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("sortOrder", "[/name]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 300);

    // Now get some items so we know that index 20 is not in the cache
    getIndex(100);
    getIndex(151);
    getIndex(202);
    getIndex(255);

    QBENCHMARK_ONCE {
        getIndex(20);
    }

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}

void JsonDbSortingListModelBench::deleteItem()
{
    QVariantMap item;
    for (int i=0; i < 300; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("sortOrder", "[/name]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 300);

    QVariantMap itemToRemove;
    // get the item that we shall remove
    getIndex(20);
    itemToRemove.insert("_uuid", callbackResponse.toMap().value("_uuid").toString());
    itemToRemove.insert("_version", callbackResponse.toMap().value("_version").toString());

    // Now get some items so we know that index 20 is not in the cache
    getIndex(100);
    getIndex(151);
    getIndex(202);
    getIndex(255);

    // Delete the item
    mClient->remove(itemToRemove, "com.nokia.shared.1");

    QBENCHMARK_ONCE {
        waitForItemChanged(true);
    }

    QCOMPARE(listModel->rowCount(), 299);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}


void JsonDbSortingListModelBench::scrollThousandItems()
{
    QVariantMap item;
    for (int i=0; i < 1000; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("sortOrder", "[/name]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 1000);

    int rowCount = listModel->rowCount();

    QBENCHMARK {
        for (int i=0 ; i < rowCount; i++)
            for (int j=0; j < 3; j++) // roles
                listModel->data(listModel->index(i,0), j);
    }

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}

void JsonDbSortingListModelBench::modelReset()
{
    mWaitingForReset = false;
    mEventLoop2.exit(0);
}

void JsonDbSortingListModelBench::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    mWaitingForDataChange = false;
}

void JsonDbSortingListModelBench::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsCreated++;
    mEventLoop2.exit(0);
}

void JsonDbSortingListModelBench::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mWaitingForRowsRemoved = false;
}

void JsonDbSortingListModelBench::rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row )
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);
}

void JsonDbSortingListModelBench::stateChanged()
{
    // only exit on ready state.
    QAbstractListModel *model = qobject_cast<QAbstractListModel *>(sender());
    if (model->property("state") == 2) {
        mWaitingForStateChanged = false;
        mEventLoop2.exit(0);
    }
}

void JsonDbSortingListModelBench::waitForItemsCreated(int items)
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

void JsonDbSortingListModelBench::waitForExitOrTimeout()
{
    mTimeoutCalled = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop2, SLOT(quit()));
    timer.start(mClientTimeout);
    mElapsedTimer.start();
    mEventLoop2.exec(QEventLoop::AllEvents);
}

void JsonDbSortingListModelBench::waitForStateOrTimeout()
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

void JsonDbSortingListModelBench::timeout()
{
    ClientWrapper::timeout();
    mTimeoutCalled = true;
    mTimedOut = true;
}

void JsonDbSortingListModelBench::waitForItemChanged(bool waitForRemove)
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
QTEST_MAIN(JsonDbSortingListModelBench)

