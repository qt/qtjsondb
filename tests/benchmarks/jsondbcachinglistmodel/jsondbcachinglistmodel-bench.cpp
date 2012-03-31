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
#include <QQmlListReference>
#include "jsondbcachinglistmodel-bench.h"

#include "util.h"

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
                "signal callbackSignal(variant index, variant response);"
                "id: contactsModel; cacheSize: 75;"
                "partitions: ["
                    "JsonDb.Partition {name: \"com.nokia.shared.1\"},"
                    "JsonDb.Partition {name: \"com.nokia.shared.2\"}"
                "]"
            "}");


JsonDbCachingListModelBench::JsonDbCachingListModelBench()
{
}

JsonDbCachingListModelBench::~JsonDbCachingListModelBench()
{
}

void JsonDbCachingListModelBench::deleteDbFiles()
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

void JsonDbCachingListModelBench::connectListModel(QAbstractListModel *model)
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

void JsonDbCachingListModelBench::initTestCase()
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

    // Create the shared Partitions
    QVariantMap item;
    item.insert("_type", "Partition");
    item.insert("name", "com.nokia.shared.1");
    int id = create(item);
    waitForResponse1(id);

    item.clear();
    item.insert("_type", "Partition");
    item.insert("name", "com.nokia.shared.2");
    id = create(item);
    waitForResponse1(id);

}

QAbstractListModel *JsonDbCachingListModelBench::createModel()
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

    QObject::connect(newModel->model, SIGNAL(callbackSignal(QVariant, QVariant)),
                         this, SLOT(callbackSlot(QVariant, QVariant)));

    mModels.append(newModel);
    return (QAbstractListModel*)(newModel->model);
}

void JsonDbCachingListModelBench::deleteModel(QAbstractListModel *model)
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
void JsonDbCachingListModelBench::deleteItems(const QString &type, const QString &partition)
{
    int id = query(QString("[?_type=\"%1\"]").arg(type), partition);
    waitForResponse1(id);
    id = remove(lastResult, partition);
    waitForResponse1(id);
}

void JsonDbCachingListModelBench::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

void JsonDbCachingListModelBench::callbackSlot(QVariant error, QVariant response)
{
    mCallbackReceived = true;
    callbackError = error.isValid();
    callbackMeta = response;
    callbackResponse = response.toMap().value("object");
    eventLoop1.quit();
}

void JsonDbCachingListModelBench::getIndex(int index)
{
    mCallbackReceived = false;

    const QString createString = QString("get(%1, function (error, response) { callbackSignal(error, response);});");
    const QString getString = QString(createString).arg(index);
    QQmlExpression expr(mModels.last()->engine->rootContext(), mModels.last()->model, getString);
    expr.evaluate().toInt();

    if (!mCallbackReceived)
        waitForCallback1();
}

QVariant JsonDbCachingListModelBench::getIndexRaw(QAbstractListModel *model, int index, int role)
{
    QVariant val =  model->data(model->index(index), role);
    return val;
}

void JsonDbCachingListModelBench::createIndex(const QString &property, const QString &propertyType)
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


// Populate model of 300 items.
void JsonDbCachingListModelBench::ModelStartup()
{
    resetWaitFlags();
    QVariantMap item;

    for (int i=0; i < 300; i++) {
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
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    QBENCHMARK_ONCE {
        mWaitingForReset = true;
        waitForExitOrTimeout();
    }

    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}

// Populate model of 300 items two partitions.
void JsonDbCachingListModelBench::ModelStartupTwoPartitions()
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
        mWaitingForReset = true;
        waitForExitOrTimeout();
    }

    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteItems(__FUNCTION__, "com.nokia.shared.2");
    deleteModel(listModel);
}


// Populate model of 300 items sorted.
void JsonDbCachingListModelBench::ModelStartupSorted()
{
    resetWaitFlags();
    QVariantMap item;

    for (int i=0; i < 300; i++) {
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

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    QBENCHMARK_ONCE {
        mWaitingForReset = true;
        waitForExitOrTimeout();
    }

    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}


void JsonDbCachingListModelBench::getItemNotInCache()
{
    resetWaitFlags();
    QVariantMap item;
    for (int i=0; i < 300; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("cacheSize", 100);
    listModel->setProperty("sortOrder", "[/name]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);

    mWaitingForReset = true;
    waitForExitOrTimeout();
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

void JsonDbCachingListModelBench::deleteItem()
{
    resetWaitFlags();
    QVariantMap item;
    for (int i=0; i < 300; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("cacheSize", 100);
    listModel->setProperty("sortOrder", "[/name]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    mWaitingForReset = true;
    waitForExitOrTimeout();
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
    int id = remove(itemToRemove, "com.nokia.shared.1");

    QBENCHMARK_ONCE {
        waitForItemChanged(true);
    }
    while (lastRequestId < id)
        waitForResponse1(id);

    QCOMPARE(listModel->rowCount(), 299);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}

void JsonDbCachingListModelBench::flicking()
{
    resetWaitFlags();
    QVariantList items;
    QVariantMap item;
    for (int i=0; i < 300; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        items.append(item);
    }
    int id = create(items, "com.nokia.shared.1");
    waitForResponse1(id);

    items.clear();
    for (int i=0; i < 300; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Bertta_%1").arg(i));
        items.append(item);
    }
    id = create(items, "com.nokia.shared.2");
    waitForResponse1(id);

    createIndex("name", "string");

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("cacheSize", 75);
    listModel->setProperty("sortOrder", "[/name]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    mWaitingForReset = true;
    waitForExitOrTimeout();
    QCOMPARE(listModel->rowCount(), 600);

    int noOfCacheMisses = 0;
    mItemsUpdated = 0;
    // simulate flicking through lhe list
    for (int i = 0; i < 600; i++) {
        QVariant nameVariant = getIndexRaw (listModel, i, 2);
        if (nameVariant.isNull())
            noOfCacheMisses++;
        waitForMs(10, 6);
    }

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteItems(__FUNCTION__, "com.nokia.shared.2");
    deleteModel(listModel);
}


void JsonDbCachingListModelBench::modelReset()
{
    if (mWaitingForReset) {
        mWaitingForReset = false;
        eventLoop1.exit(0);
    }
}

void JsonDbCachingListModelBench::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    mItemsUpdated++;
    if (mWaitingForChanged) {
        mWaitingForChanged = false;
        eventLoop1.exit(0);
    }
}

void JsonDbCachingListModelBench::rowsInserted(const QModelIndex &parent, int first, int last)
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

void JsonDbCachingListModelBench::rowsRemoved(const QModelIndex &parent, int first, int last)
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

void JsonDbCachingListModelBench::rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row )
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);
}

void JsonDbCachingListModelBench::stateChanged()
{
    // only exit on ready state.
    QAbstractListModel *model = qobject_cast<QAbstractListModel *>(sender());
    if (model->property("state").toInt() == 2 && mWaitingForStateChanged) {
        mWaitingForStateChanged = false;
        eventLoop1.exit(0);
    }
}

void JsonDbCachingListModelBench::waitForItemsCreated(int items)
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

void JsonDbCachingListModelBench::waitForItemsUpdated(int items)
{
    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    while (!mTimedOut && mItemsUpdated != items) {
        mWaitingForChanged = true;
        eventLoop1.exec(QEventLoop::AllEvents);
    }
    if (mTimedOut)
        qDebug () << "waitForItemsUpdated Timed out";
}

void JsonDbCachingListModelBench::waitForExitOrTimeout()
{
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();
    eventLoop1.exec(QEventLoop::AllEvents);
}

void JsonDbCachingListModelBench::waitForMs(int ms, int warningThreshold)
{
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(silentTimeout()));
    timer.start(ms);
    qint64 elap;
    QElapsedTimer elt;
    elt.start();
    eventLoop1.exec(QEventLoop::AllEvents);
    if ((elap = elt.elapsed()) > ms+warningThreshold)
        qDebug() << "Some event took more than " << warningThreshold << "ms" << "(" << elap-ms << "ms )";
}

void JsonDbCachingListModelBench::waitForStateOrTimeout()
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

void JsonDbCachingListModelBench::timeout()
{
    qDebug () << "JsonDbCachingListModelBench::timeout()";
    RequestWrapper::timeout();
    mTimedOut = true;
    eventLoop1.quit();
}

void JsonDbCachingListModelBench::silentTimeout()
{
    eventLoop1.quit();
}

void JsonDbCachingListModelBench::resetWaitFlags()
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

void JsonDbCachingListModelBench::waitForItemChanged(bool waitForRemove)
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
QTEST_MAIN(JsonDbCachingListModelBench)

