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

#include "plugin.h"

#include "jsondblistmodel.h"
#include "jsondbsortinglistmodel.h"
#include "jsondatabase.h"
#include "jsondbpartition.h"
#include "jsondbnotification.h"
#include "jsondbqueryobject.h"
#include "jsondbchangessinceobject.h"
#include "jsondbcachinglistmodel.h"

QT_USE_NAMESPACE_JSONDB

QQmlEngine *g_declEngine = 0;

static QObject *jsondb_new_module_api_provider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    JsonDatabase *database = new JsonDatabase();
    return database;
}

void JsonDbPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);
    g_declEngine = engine;
}

void JsonDbPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(uri == QLatin1String("QtJsonDb"));
    qmlRegisterModuleApi(uri, 1, 0, jsondb_new_module_api_provider);
    qmlRegisterType<JsonDbListModel>(uri, 1, 0, "JsonDbListModel");
    qmlRegisterType<JsonDbSortingListModel>(uri, 1, 0, "JsonDbSortingListModel");
    qmlRegisterType<JsonDbPartition>(uri, 1, 0, "Partition");
    qmlRegisterType<JsonDbNotify>(uri, 1, 0, "Notification");
    qmlRegisterType<JsonDbQueryObject>(uri, 1, 0, "Query");
    qmlRegisterType<JsonDbChangesSinceObject>(uri, 1, 0, "ChangesSince");
    qmlRegisterType<JsonDbCachingListModel>(uri, 1, 0, "JsonDbCachingListModel");
}
