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

#ifndef JSONDB_VIEW_H
#define JSONDB_VIEW_H

#include <QObject>
#include <QString>

#include "jsondb-global.h"
#include "jsondbmapdefinition.h"
#include "jsondbreducedefinition.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class JsonDb;
class JsonDbOwner;
class JsonDbPartition;
class JsonDbObjectTable;

class JsonDbView : public QObject
{
    Q_OBJECT
public:
    JsonDbView(JsonDb *mJsonDb, JsonDbPartition *partition, const QString &viewType,
               QObject *parent = 0);
    ~JsonDbView();
    JsonDbPartition *partition() const { return mPartition; }
    JsonDbObjectTable *objectTable() const { return mViewObjectTable; }
    QStringList sourceTypes() const { return mSourceTypes; }

    void open();
    void close();

    static void initViews(JsonDbPartition *partition, const QString &partitionName);
    static void createDefinition(JsonDbPartition *partition, QJsonObject definition);
    static void removeDefinition(JsonDbPartition *partition, QJsonObject definition);

    void updateView();
    void reduceMemoryUsage();

private:
    void createMapDefinition(QJsonObject mapDefinition);
    void removeMapDefinition(QJsonObject mapDefinition);
    void createReduceDefinition(QJsonObject reduceDefinition);
    void removeReduceDefinition(QJsonObject reduceDefinition);
    bool processUpdatedDefinitions(const QString &viewType, quint32 targetStateNumber,
                                   QSet<QString> &processedDefinitions);
    void updateSourceTypesList();
private:
    JsonDb        *mJsonDb;
    JsonDbPartition *mPartition;
    JsonDbObjectTable   *mViewObjectTable;     // view object table
    JsonDbObjectTable   *mMainObjectTable; // partition's main object table
    QString        mViewType;
    QStringList    mSourceTypes;
    typedef QMap<JsonDbObjectTable*,QSet<QString> > ObjectTableSourceTypeMap;
    ObjectTableSourceTypeMap              mObjectTableSourceTypeMap;
    QMap<QString,JsonDbMapDefinition*>    mMapDefinitions;         // maps uuid to view definition
    QMap<QString,JsonDbMapDefinition*>    mMapDefinitionsBySource; // maps map source type to view definition
    QMap<QString,JsonDbReduceDefinition*> mReduceDefinitions;      // maps uuid to view definition
    QMap<QString,JsonDbReduceDefinition*> mReduceDefinitionsBySource; // maps reduce source type to view definition
    bool mUpdating;

    friend class JsonDbMapDefinition;
    friend class JsonDbReduceDefinition;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_VIEW_H
