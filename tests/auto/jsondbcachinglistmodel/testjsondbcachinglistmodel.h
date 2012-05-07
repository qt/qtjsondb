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
#ifndef TestJsonDbCachingListModel_H
#define TestJsonDbCachingListModel_H

#include <QAbstractListModel>
#include "requestwrapper.h"
#include "qmltestutil.h"

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QQmlComponent;
QT_END_NAMESPACE

QT_USE_NAMESPACE_JSONDB

class JsonDbListModel;

class ModelData {
public:
    ModelData();
    ~ModelData();
    QQmlEngine *engine;
    QQmlComponent *component;
    QObject *model;
};

class TestJsonDbCachingListModel: public RequestWrapper
{
    Q_OBJECT
public:
    TestJsonDbCachingListModel();
    ~TestJsonDbCachingListModel();

    void deleteDbFiles();
    void connectListModel(QAbstractListModel *model);

public slots:
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsRemoved(const QModelIndex &parent, int first, int last);
    void rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row );
    void modelReset();
    void stateChanged();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void createItem();
    void createModelTwoPartitions();
    void updateItemClient();
    void deleteItem();
    void bindings();
    void sortedQuery();
    void ordering();
    void orderingCaseSensitive();
    void checkRemoveNotification();
    void checkUpdateNotification();
    void totalRowCount();
    void checkAddNotification();
    void listProperty();
    void changeQuery();
    void indexOfUuid();
    void roleNames();
    void getItemNotInCache();
public:
    void timeout();
private:
    void waitForExitOrTimeout();
    void waitForItemsCreated(int items);
    void waitForItemsRemoved(int items);
    void waitForStateOrTimeout();
    void waitForItemChanged(bool waitForRemove = false);
    void waitForIndexChanged();
    QStringList getOrderValues(QAbstractListModel *listModel);
    QVariant getIndex(QAbstractListModel *model, int index, int role);
    QVariant getProperty(QAbstractListModel *model, int index, const QByteArray &roleName);
    void createIndex(const QString &property, const QString &propertyType);
    void createIndexNoName(const QString &property, const QString &propertyType);
    void createIndexCaseSensitive(const QString &name, const QString &property, const QString &propertyType, bool caseSensitive);
    QAbstractListModel *createModel();
    void deleteModel(QAbstractListModel *model);
    void resetWaitFlags();

private:
    QProcess *mProcess;
    //QStringList mNotificationsReceived;
    QList<ModelData*> mModels;
    QString mPluginPath;

    // Response values
    bool mTimedOut;
    int mItemsCreated;
    int mItemsUpdated;
    int mItemsRemoved;
    bool mWaitingForStateChanged;
    bool mWaitingForRowsInserted;
    bool mWaitingForReset;
    bool mWaitingForChanged;
    bool mWaitingForIndexChanged;
    int mIndexWaited;
    bool mWaitingForRemoved;
};

#endif
