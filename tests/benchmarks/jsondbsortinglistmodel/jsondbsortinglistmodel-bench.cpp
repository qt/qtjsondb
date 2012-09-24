/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QJSEngine>
#include "jsondbsortinglistmodel-bench.h"

#include "util.h"
#include <QQmlListReference>

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
            "import QtJsonDb 1.0 as JsonDb \n"
            "JsonDb.JsonDbSortingListModel {"
                "id: contactsModel;"
                "partitions: [JsonDb.Partition {name: \"com.nokia.shared.1\"}, JsonDb.Partition {name: \"com.nokia.shared.2\"}]"
            "}");


JsonDbSortingListModelBench::JsonDbSortingListModelBench()
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
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList(), __FILE__);

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

QAbstractListModel *JsonDbSortingListModelBench::createModel()
{
    ModelData *newModel = new ModelData();
    newModel->engine = new QQmlEngine();
    QList<QQmlError> errors;
    if (!newModel->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &errors)) {
        qDebug() << "Unable to load the plugin :"<<errors;
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
    int id = query(QString("[?_type=\"%1\"]").arg(type), partition);
    waitForResponse1(id);
    id = remove(lastResult, partition);
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

void JsonDbSortingListModelBench::getIndex(int index)
{
    const QString createString = QString("get(%1);");
    const QString getString = QString(createString).arg(index);
    QQmlExpression expr(mModels.last()->engine->rootContext(), mModels.last()->model, getString);
    callbackResponse = expr.evaluate();
}

void JsonDbSortingListModelBench::createIndex(const QString &property, const QString &propertyType)
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
void JsonDbSortingListModelBench::ModelStartup()
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
        mWaitingForStateChanged = true;
        waitForStateOrTimeout();
    }

    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}

// Populate model of 300 items two partitions.
void JsonDbSortingListModelBench::ModelStartupTwoPartitions()
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
        mWaitingForStateChanged = true;
        waitForStateOrTimeout();
    }

    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteItems(__FUNCTION__, "com.nokia.shared.2");
    deleteModel(listModel);
}


// Populate model of 300 items sorted.
void JsonDbSortingListModelBench::ModelStartupSorted()
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
        mWaitingForStateChanged = true;
        waitForStateOrTimeout();
    }

    QCOMPARE(mWaitingForReset, false);
    QCOMPARE(listModel->rowCount(), 300);

    deleteItems(__FUNCTION__, "com.nokia.shared.1");
    deleteModel(listModel);
}


void JsonDbSortingListModelBench::getItems()
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
    listModel->setProperty("sortOrder", "[/name]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();

    QCOMPARE(mWaitingForReset, false);
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
    listModel->setProperty("sortOrder", "[/name]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    // now start it working
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(listModel->rowCount(), 300);

    QVariantMap itemToRemove;
    // get the item that we shall remove
    getIndex(20);
    QVariantMap retrievedItem = callbackResponse.toMap().value("object").toMap();
    itemToRemove.insert("_uuid", retrievedItem.value("_uuid").toString());
    itemToRemove.insert("_version", retrievedItem.value("_version").toString());

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


void JsonDbSortingListModelBench::scrollThousandItems()
{
    resetWaitFlags();
    QVariantMap item;
    for (int i=0; i < 1000; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.1");
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
    mWaitingForStateChanged = true;
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
    if (mWaitingForReset) {
        mWaitingForReset = false;
        eventLoop1.exit(0);
    }
}

void JsonDbSortingListModelBench::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    mItemsUpdated++;
    if (mWaitingForChanged) {
        mWaitingForChanged = false;
        eventLoop1.exit(0);
    }
}

void JsonDbSortingListModelBench::rowsInserted(const QModelIndex &parent, int first, int last)
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

void JsonDbSortingListModelBench::rowsRemoved(const QModelIndex &parent, int first, int last)
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
    if (model->property("state").toInt() == 2 && mWaitingForStateChanged) {
        mWaitingForStateChanged = false;
        eventLoop1.exit(0);
    }
}

void JsonDbSortingListModelBench::waitForItemsCreated(int items)
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

void JsonDbSortingListModelBench::waitForExitOrTimeout()
{
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();
    eventLoop1.exec(QEventLoop::AllEvents);
}

void JsonDbSortingListModelBench::waitForStateOrTimeout()
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

void JsonDbSortingListModelBench::timeout()
{
    qDebug () << "JsonDbSortingListModelBench::timeout()";
    RequestWrapper::timeout();
    mTimedOut = true;
    eventLoop1.quit();
}

void JsonDbSortingListModelBench::resetWaitFlags()
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

void JsonDbSortingListModelBench::waitForItemChanged(bool waitForRemove)
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
QTEST_MAIN(JsonDbSortingListModelBench)

