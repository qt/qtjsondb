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
#include <QQmlListReference>
#include "testjsondbsortinglistmodel.h"
#include "../../shared/util.h"

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
{
}

TestJsonDbSortingListModel::~TestJsonDbSortingListModel()
{
    connection = 0;
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
        QFile file(fileInfo.fileName());
        file.remove();
    }
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
            this, SLOT(stateChanged(State)));
}

void TestJsonDbSortingListModel::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList(), __FILE__);

    connection = QJsonDbConnection::defaultConnection();
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

QAbstractListModel *TestJsonDbSortingListModel::createModel()
{
    ModelData *newModel = new ModelData();
    newModel->engine = new QQmlEngine();
    QList<QQmlError> errors;
    Q_ASSERT(!mPluginPath.isEmpty());
    if (!newModel->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &errors)) {
        qDebug()<<"Unable to load the plugin :"<<errors;
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
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);
    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(listModel->property("state").toInt(), 2);

    item.clear();
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Baker");
    mItemsCreated = 0;
    id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);
    while (!mItemsCreated) {
        mWaitingForRowsInserted = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRowsInserted, false);

    QCOMPARE(listModel->rowCount(), 2);
    deleteModel(listModel);
}

// Create an item and then update it.
void TestJsonDbSortingListModel::updateItemClient()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item,"com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);
    QCOMPARE(listModel->rowCount(), 1);

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
    id = update(item, "com.nokia.shared.1");
    waitForResponse1(id);
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(get(listModel, 0, "_uuid").toString(), lastUuid);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel,0, "name").toString(), QLatin1String("Baker"));
    deleteModel(listModel);
}

void TestJsonDbSortingListModel::deleteItem()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);
    QCOMPARE(listModel->rowCount(), 1);

    item.insert("name", "Baker");
    mItemsCreated = 0;
    id = create(item, "com.nokia.shared.2");
    waitForResponse1(id);
    while (!mItemsCreated) {
        mWaitingForRowsInserted = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRowsInserted, false);
    QCOMPARE(listModel->rowCount(), 2);

    QString lastUuid,lastVersion;
    QVariantMap lastItem;
    if (lastResult.count()) {
        lastItem = lastResult[0].toMap();
        lastUuid = lastItem.value("_uuid").toString();
        lastVersion = lastItem.value("_version").toString();
    }
    item.insert("_uuid", lastUuid);
    item.insert("_version", lastVersion);
    id = remove(item, "com.nokia.shared.2");
    waitForResponse1(id);
    while (!mItemsRemoved) {
        mWaitingForRemoved = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRemoved, false);

    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Charlie"));
    deleteModel(listModel);
}

void TestJsonDbSortingListModel::bindings()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);
    QString lastUuid,lastVersion;
    QVariantMap lastItem;
    if (lastResult.count()) {
        lastItem = lastResult[0].toMap();
        lastUuid = lastItem.value("_uuid").toString();
        lastVersion = lastItem.value("_version").toString();
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QVariantMap bindingsMap;
    bindingsMap.insert("firstName", "Charlie");
    listModel->setProperty("bindings", bindingsMap);
    listModel->setProperty("query", QString("[?_type=\"%1\"][?name=%firstName]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);
    QCOMPARE(listModel->rowCount(), 1);

    item.insert("name", "Baker");
    mItemsCreated = 0;
    id = create(item, "com.nokia.shared.2");
    waitForResponse1(id);
    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Charlie"));

    item.insert("_uuid", lastUuid);
    item.insert("_version", lastVersion);
    id = remove(item, "com.nokia.shared.1");
    waitForResponse1(id);
    while (!mItemsRemoved) {
        mWaitingForRemoved = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForRemoved, false);

    QCOMPARE(listModel->rowCount(), 0);
    deleteModel(listModel);
}

void TestJsonDbSortingListModel::sortedQuery()
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

    connectListModel(listModel);

    QStringList rolenames;
    rolenames << "_uuid" << "_type" << "number";
    listModel->setProperty("roleNames", rolenames);
    listModel->setProperty("sortOrder", "[/number]");
    listModel->setProperty("query", "[?_type=\"RandNumber\"]");
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

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
    resetWaitFlags();
    for (int i = 9; i >= 1; --i) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", QString::number(i));
        int id = create(item, QString("com.nokia.shared.%1").arg(i%2+1).toAscii());
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    listModel->setProperty("sortOrder", "[/order]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "_version" << "name" << "order");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->rowCount(), 0);
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);
    QStringList expectedOrder = QStringList() << "1" << "2" << "3" << "4" <<
                                                 "5" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    {
        QVariant uuid = get(listModel, 4, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());
        QVariant version = get(listModel, 4, "_version");
        QVERIFY(!version.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_version", version);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "99");  // move it to the end
        update(item, "com.nokia.shared.2");
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
        QVariant uuid = get(listModel, 8, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());
        QVariant version = get(listModel, 8, "_version");
        QVERIFY(!version.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_version", version);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "22");    // move it after "2"
        update(item, "com.nokia.shared.2");
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
        QVariant uuid = get(listModel, 5, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());
        QVariant version = get(listModel, 5, "_version");
        QVERIFY(!version.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_version", version);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "0");    // move it to the beginning
        update(item, "com.nokia.shared.1");
    }
    while (!mItemsUpdated) {
        mWaitingForChanged = true;
        waitForExitOrTimeout();
    }
    QCOMPARE(mWaitingForChanged, false);

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
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 5);
    QCOMPARE(getOrderValues(listModel), QStringList(expectedOrder.mid(0, 5)));

    listModel->setProperty("sortOrder", "[\\order]");
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 5);
    QCOMPARE(getOrderValues(listModel), QStringList(reverseOrder.mid(0, 5)));

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::checkRemoveNotification()
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
        connectListModel(listModel);
        listModel->setProperty("queryLimit", 10);
        listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
        listModel->setProperty("sortOrder", "[/order]");
        QStringList roleNames = (QStringList() << "_type" << "_uuid" << "_version"<< "name" << "order");
        listModel->setProperty("roleNames", roleNames);

        mWaitingForStateChanged = true;
        waitForStateOrTimeout();
        QCOMPARE(mWaitingForStateChanged, false);
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
        id = remove(item, "com.nokia.shared.2");
        waitForResponse1(id);
        waitForStateChanged(listModel);

        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 10);

        //Remove item at 9
        item.insert("_uuid", get(listModel, 9, "_uuid"));
        item.insert("_version", get(listModel, 9, "_version"));
        id = remove(item, "com.nokia.shared.2");
        waitForResponse1(id);
        waitForStateChanged(listModel);

        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 11);

        //Remove item at 4
        item.insert("_uuid", get(listModel, 4, "_uuid"));
        item.insert("_version", get(listModel, 4, "_version"));
        id = remove(item, "com.nokia.shared.2");
        waitForResponse1(id);
        waitForStateChanged(listModel);

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
    int id = create(itemList, "com.nokia.shared.1");
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

        mWaitingForStateChanged = true;
        waitForStateOrTimeout();
        QCOMPARE(mWaitingForStateChanged, false);
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
        item.insert("_version", get(listModel, 0, "_version"));
        item.insert("_type", get(listModel, 0, "_type"));
        item.insert("name", get(listModel, 0, "name"));
        item.insert("order", 1);
        id = update(item, "com.nokia.shared.1");

        waitForItemChanged();
        QCOMPARE(mWaitingForChanged, false);

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
        item.insert("_version", get(listModel, 9, "_version"));
        item.insert("_type", get(listModel, 9, "_type"));
        item.insert("name", get(listModel, 9, "name"));
        item.insert("order", 19);
        id = update(item,"com.nokia.shared.1");

        waitForItemChanged();
        QCOMPARE(mWaitingForStateChanged, false);
        waitForStateChanged(listModel);

        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 0, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 19);

        //Update item at 9
        item.insert("_uuid", get(listModel, 9, "_uuid"));
        item.insert("_version", get(listModel, 9, "_version"));
        item.insert("_type", get(listModel, 9, "_type"));
        item.insert("name", get(listModel, 9, "name"));
        item.insert("order", 59);
        id = update(item, "com.nokia.shared.1");

        waitForItemChanged();
        QCOMPARE(mWaitingForStateChanged, false);
        waitForStateChanged(listModel);

        QCOMPARE(listModel->rowCount(), 10);
        result = get(listModel, 0, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 1);
        result = get(listModel, 9, "order");
        QVERIFY(result.isValid());
        QCOMPARE(result.toInt(), 20);

        //Update item at 8
        item.insert("_uuid", get(listModel, 8, "_uuid"));
        item.insert("_version", get(listModel, 8, "_version"));
        item.insert("_type", get(listModel, 8, "_type"));
        item.insert("name", get(listModel, 8, "name"));
        item.insert("order", 17);
        id = update(item, "com.nokia.shared.1");

        waitForItemChanged();
        QCOMPARE(mWaitingForStateChanged, false);
        waitForStateChanged(listModel);

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

    listModel->setProperty("queryLimit", 100);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "order");
    listModel->setProperty("roleNames", roleNames);
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 10);

    mItemsCreated = 0;
    for (int i = 10; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        id = create(item, "com.nokia.shared.2");
        waitForResponse1(id);
    }
    waitForItemsCreated(40); // will set mWaitingForRowsInserted = true for you
    if (id != lastRequestId)
        waitForResponse1(id);
    QCOMPARE(mWaitingForRowsInserted, false);
    QCOMPARE(mItemsCreated, 40);
    QCOMPARE(listModel->rowCount(), 50);

    // Change sort order
    mWaitingForReadyState = true;
    listModel->setProperty("sortOrder", "[\\order]");
    QVariant state = listModel->property("state");
    if (state.toInt() != 2) {
        waitForReadyStateOrTimeout();
        QCOMPARE(mWaitingForReadyState, false);
    }
    else
        mWaitingForReadyState = false;

    QCOMPARE(listModel->rowCount(), 50);

    // Delete the first 10 items
    foreach (QVariant item, insertedItems) {
        mItemsRemoved = 0;
        id = remove(item.toMap(), "com.nokia.shared.1");
        waitForResponse1(id);
        waitForItemsRemoved(1);
    }

    QCOMPARE(listModel->rowCount(), 40);

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::listProperty()
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
    connectListModel(listModel);
    QString type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("queryLimit", 10);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(type));
    listModel->setProperty("sortOrder", "[/features.0.properties.0.description]");
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "features.0.properties.0.description"<< "features.0.feature");
    listModel->setProperty("roleNames", roleNames);
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), itemList.count());

    QCOMPARE(get(listModel, 0, "_type").toString(), type);
    QCOMPARE(get(listModel, 0, "features.0.properties.0.description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(get(listModel, 0, "features.0.feature").toString(), QLatin1String("provide Facebook"));
    //Liang: todo or not?
    //QCOMPARE(get(listModel, 1, "_uuid").toString(), mLastUuid);
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
    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

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
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 300);
    QCOMPARE(get(listModel, 0, "name").toString(), QString("Arnie_0"));
    QCOMPARE(get(listModel, 1, "name").toString(), QString("Arnie_1"));
    QCOMPARE(get(listModel, 2, "name").toString(), QString("Arnie_10"));
    QCOMPARE(get(listModel, 3, "name").toString(), QString("Arnie_100"));

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::changeQuery()
{
    resetWaitFlags();
    QVariantMap item;

    for (int i=0; i < 10; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    listModel->setProperty("query", QString(""));

    QCOMPARE(listModel->rowCount(), 0);
    QCOMPARE(listModel->property("query").toString(), QString(""));

    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(listModel->property("query").toString(), QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::getQJSValue()
{
    resetWaitFlags();
    QVariantMap item;

    for (int i=0; i < 10; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 10);
    QCOMPARE(get(listModel, 0).property("object").property("name").toString(), QString("Arnie_0"));
    QCOMPARE(get(listModel, 1).property("object").property("name").toString(), QString("Arnie_1"));

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::indexOfUuid()
{
    resetWaitFlags();
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie_0"));
    int id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractListModel *listModel = createModel();
    if (!listModel) return;

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    listModel->setProperty("sortOrder", "[/name]");
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    connectListModel(listModel);

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 1);
    QCOMPARE(get(listModel, 0, "name").toString(), QString("Arnie_0"));
    QCOMPARE(indexOf(listModel, get(listModel, 0, "_uuid").toString()), 0);

    item.insert("_type", __FUNCTION__);
    item.insert("name", QString("Arnie_1"));
    id = create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    waitForItemsCreated(1); // will set mWaitingForRowsInserted = true for you
    if (id != lastRequestId)
        waitForResponse1(id);
    QCOMPARE(mWaitingForRowsInserted, false);
    QCOMPARE(mItemsCreated, 1);

    QCOMPARE(listModel->rowCount(), 2);
    QCOMPARE(get(listModel, 1, "name").toString(), QString("Arnie_1"));
    QCOMPARE(indexOf(listModel, get(listModel, 1, "_uuid").toString()), 1);
    QCOMPARE(indexOf(listModel, "notValid"), -1);

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::queryLimit()
{
    resetWaitFlags();
    QVariantMap item;

    for (int i=0; i < 300; i++) {
        item.insert("_type", __FUNCTION__);
        item.insert("name", QString("Arnie_%1").arg(i));
        int id = create(item, "com.nokia.shared.1");
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

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 100);
    QCOMPARE(listModel->property("overflow").toBool(), true);

    listModel->setProperty("queryLimit", 100);
    QCOMPARE(listModel->rowCount(), 100);
    QCOMPARE(listModel->property("overflow").toBool(), true);

    listModel->setProperty("queryLimit", 500);

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

    QCOMPARE(listModel->rowCount(), 300);
    QCOMPARE(listModel->property("overflow").toBool(), false);

    deleteModel(listModel);
}

void TestJsonDbSortingListModel::roleNames()
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

    mWaitingForStateChanged = true;
    waitForStateOrTimeout();
    QCOMPARE(mWaitingForStateChanged, false);

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

QStringList TestJsonDbSortingListModel::getOrderValues(QAbstractListModel *listModel)
{
    QStringList vals;
    for (int i = 0; i < listModel->rowCount(); ++i)
        vals << get(listModel, i, "order").toString();
    return vals;
}

void TestJsonDbSortingListModel::modelReset()
{
    //qDebug() << "modelReset";
    if (mWaitingForReset) {
        mWaitingForReset = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbSortingListModel::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    mItemsUpdated++;
    //qDebug() << "mItemsUpdated++" << mItemsUpdated;
    if (mWaitingForChanged) {
        mWaitingForChanged = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbSortingListModel::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsCreated++;
    //qDebug() << "mItemsCreated++" << mItemsCreated;
    if (mWaitingForRowsInserted) {
        mWaitingForRowsInserted = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbSortingListModel::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsRemoved++;
    //qDebug() << "mItemsRemoved++" << mItemsRemoved;
    if (mWaitingForRemoved) {
        mWaitingForRemoved = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbSortingListModel::rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row )
{
    Q_UNUSED(parent);

    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);
}

void TestJsonDbSortingListModel::stateChanged(State state)
{
    if (mWaitingForStateChanged) {
        mWaitingForStateChanged = false;
        eventLoop1.exit(0);
    }
    else if (mWaitingForReadyState && state == Ready) {
        mWaitingForReadyState = false;
        eventLoop1.exit(0);
    }
}

void TestJsonDbSortingListModel::waitForItemsCreated(int items)
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
}

void TestJsonDbSortingListModel::waitForItemsRemoved(int items)
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

void TestJsonDbSortingListModel::waitForExitOrTimeout()
{
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();
    eventLoop1.exec(QEventLoop::AllEvents);
}

void TestJsonDbSortingListModel::waitForReadyStateOrTimeout()
{
    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    while (mWaitingForReadyState && !mTimedOut)
        eventLoop1.exec(QEventLoop::AllEvents);
}

void TestJsonDbSortingListModel::waitForStateOrTimeout()
{
    mTimedOut = false;
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer.start(clientTimeout);
    elapsedTimer.start();

    while (mWaitingForStateChanged && !mTimedOut)
        eventLoop1.exec(QEventLoop::AllEvents);
}

void TestJsonDbSortingListModel::timeout()
{
    RequestWrapper::timeout();
    mTimedOut = true;
    eventLoop1.quit();
}

void TestJsonDbSortingListModel::resetWaitFlags()
{
    mItemsCreated  = 0;
    mItemsUpdated = 0;
    mItemsRemoved = 0;
    mWaitingForRowsInserted = false;
    mWaitingForReset = false;
    mWaitingForChanged = false;
    mWaitingForRemoved = false;
    mWaitingForStateChanged = false;
}

void TestJsonDbSortingListModel::waitForStateChanged(QAbstractListModel *listModel)
{
    int currentState;
    currentState = listModel->property("state").toInt();
    //1: JsonDbSortingListModel::Querying
    if (currentState != 1) {
        mWaitingForStateChanged = true;
        waitForStateOrTimeout();
        QCOMPARE(mWaitingForStateChanged, false);
        currentState = listModel->property("state").toInt();
    }
    if (currentState == 1) {
        mWaitingForStateChanged = true;
        waitForStateOrTimeout();
        QCOMPARE(mWaitingForStateChanged, false);
        currentState = listModel->property("state").toInt();
    }
}

void TestJsonDbSortingListModel::waitForItemChanged(bool waitForRemove)
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

QTEST_MAIN(TestJsonDbSortingListModel)

