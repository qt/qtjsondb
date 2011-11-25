/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef LISTMODEL_BENCHMARK_H
#define LISTMODEL_BENCHMARK_H

#include <QCoreApplication>
#include <QProcess>
#include <QTest>
#include <QDebug>

#include <QEventLoop>
#include <QLocalSocket>

#include <jsondb-client.h>
#include <jsondb-error.h>

#include "clientwrapper.h"
#include "jsondb-listmodel.h"

Q_USE_JSONDB_NAMESPACE

class QDeclarativeEngine;
class QDeclarativeComponent;
class JsonDbListModel;

class ModelData {
public:
    ModelData();
    ~ModelData();
    QDeclarativeEngine *engine;
    QDeclarativeComponent *component;
    QObject *model;
};

class TestListModel: public ClientWrapper
{
    Q_OBJECT
public:
    TestListModel();
    ~TestListModel();

    void deleteDbFiles();
    void connectListModel(JsonDbListModel *model);

public slots:
    void notified(const QString& notifyUuid, const QVariant& object, const QString& action);
    void response(int id, const QVariant& data);
    void error(int id, int code, const QString& message);

    void dataChanged(QModelIndex,QModelIndex);
    void modelReset();
    void layoutChanged();
    void rowsInserted(QModelIndex, int, int);
    void rowsRemoved(QModelIndex, int, int);
    void rowsMoved(QModelIndex, int, int, QModelIndex, int);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void createListModelHundredItems();
    void createListModelThousandItems();
    void createListModelGroupedQuery();
    void createListModelSortedQuery();
    void changeOneItemClient();
    void changeOneItemSet();
    void changeOneItemSetProperty();
    void getOneItemInCache();
    void getOneItemNotInCache();
    void getOneItemNotInCacheThousandItems();
    void scrollThousandItems();
private:
    JsonDbListModel *createModel();
    void deleteModel(JsonDbListModel *model);
private:
    QProcess *mProcess;
    QVariant mLastResponseData;
    QList<ModelData*> mModels;
    QString mPluginPath;
};

#endif
