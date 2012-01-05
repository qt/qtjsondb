/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef TestJsonDbListModel_H
#define TestJsonDbListModel_H

#include <QCoreApplication>
#include <QList>
#include <QTest>
#include <QFile>
#include <QProcess>
#include <QEventLoop>
#include <QDebug>
#include <QLocalSocket>
#include <QTimer>

#include <jsondb-client.h>
#include <jsondb-error.h>

#include <QAbstractListModel>
#include "clientwrapper.h"

QT_BEGIN_NAMESPACE
class QDeclarativeEngine;
class QDeclarativeComponent;
QT_END_NAMESPACE

QT_USE_NAMESPACE_JSONDB

class JsonDbListModel;

class ModelData {
public:
    ModelData();
    ~ModelData();
    QDeclarativeEngine *engine;
    QDeclarativeComponent *component;
    QDeclarativeComponent *partitionComponent1;
    QDeclarativeComponent *partitionComponent2;
    QObject *model;
    QObject *parttion1;
    QObject *parttion2;
};

class TestJsonDbSortingListModel: public ClientWrapper
{
    Q_OBJECT
public:
    TestJsonDbSortingListModel();
    ~TestJsonDbSortingListModel();

    void deleteDbFiles();
    void connectListModel(QAbstractListModel *model);

public slots:
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsRemoved(const QModelIndex &parent, int first, int last);
    void rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row );
    void modelReset();
    void stateChanged();
    void timeout();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void createItem();
    void updateItemClient();
    void deleteItem();
    void sortedQuery();
    void ordering();
    void checkRemoveNotification();
    void checkUpdateNotification();
    void totalRowCount();
    void listProperty();

private:
    void waitForExitOrTimeout();
    void waitForItemsCreated(int items);
    void waitForStateOrTimeout();
    void waitForItemChanged(bool waitForRemove = false);
    QStringList getOrderValues(QAbstractListModel *listModel);
    QAbstractListModel *createModel();
    void deleteModel(QAbstractListModel *model);
    QVariant readJsonFile(const QString &filename);

private:
    QProcess *mProcess;
    QStringList mNotificationsReceived;
    QList<ModelData*> mModels;
    QString mPluginPath;
    QEventLoop mEventLoop2; // for all listmodel slots

    // Response values
    int mItemsCreated;
    bool mWaitingForNotification;
    bool mWaitingForDataChange;
    bool mWaitingForRowsRemoved;
    bool mTimeoutCalled;
    bool mWaitingForReset;
    bool mWaitingForStateChanged;

};

#endif
