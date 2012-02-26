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
    JsonDbObjectTable *objectTable() const { return mObjectTable; }
    QStringList sourceTypes() const { return mSourceTypes; }

    void open();
    void close();

    static void initViews(JsonDbPartition *partition, const QString &partitionName);
    void createJsonDbMapDefinition(QJsonObject mapDefinition, bool firstTime);
    void removeJsonDbMapDefinition(QJsonObject mapDefinition);
    void createJsonDbReduceDefinition(QJsonObject reduceDefinition, bool firstTime);
    void removeJsonDbReduceDefinition(QJsonObject reduceDefinition);
    quint32 findUpdatedMapJsonDbReduceDefinitions(const QString &definitionType,
                                            const QString &viewType, quint32 targetStateNumber,
                                            QMap<QString,QJsonObject> &addedDefinitions,
                                            QMap<QString,QJsonObject> &removedDefinitions) const;

    void updateView();
    void updateMap();
    void updateReduce();
    void reduceMemoryUsage();

private:
    void updateSourceTypesList();
private:
    JsonDb        *mJsonDb;
    JsonDbPartition *mPartition;
    JsonDbObjectTable   *mObjectTable;
    QString        mViewType;
    QStringList    mSourceTypes;
    QSet<JsonDbMapDefinition*> mJsonDbMapDefinitions;
    QMultiMap<QString,JsonDbMapDefinition*> mJsonDbMapDefinitionsBySource; // maps map source type to view definition
    QSet<JsonDbReduceDefinition*> mJsonDbReduceDefinitions;
    QMultiMap<QString,JsonDbReduceDefinition*> mJsonDbReduceDefinitionsBySource; // maps reduce source type to view definition
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_VIEW_H
