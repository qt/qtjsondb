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

#ifndef JSONDBCACHINGLISTMODEL_QML_H
#define JSONDBCACHINGLISTMODEL_QML_H

#include <QQmlParserStatus>
#include <QQmlListProperty>
#include <QJSValue>

#include "jsondbpartition.h"
#include <private/qjsondbquerymodel_p.h>

QT_BEGIN_NAMESPACE_JSONDB


class JsonDbCachingListModel : public QJsonDbQueryModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

public:

    JsonDbCachingListModel(QObject *parent = 0);
    virtual ~JsonDbCachingListModel();

    Q_PROPERTY(QQmlListProperty<JsonDbPartition> partitions READ partitions)

    virtual void classBegin();
    virtual void componentComplete();

    QQmlListProperty<JsonDbPartition> partitions();

    Q_INVOKABLE void get(int index, const QJSValue &callback);
    Q_INVOKABLE JsonDbPartition* getPartition(int index);
    Q_INVOKABLE int indexOf(const QString &uuid) const;
    static void partitions_append(QQmlListProperty<JsonDbPartition> *p, JsonDbPartition *v);
    static int partitions_count(QQmlListProperty<JsonDbPartition> *p);
    static JsonDbPartition* partitions_at(QQmlListProperty<JsonDbPartition> *p, int idx);
    static void partitions_clear(QQmlListProperty<JsonDbPartition> *p);

private:
    Q_DISABLE_COPY(JsonDbCachingListModel)

    QMap <int, QJSValue> getCallbacks;
    QMap <QString, JsonDbPartition *> nameToJsonDbPartitionMap;
private slots:
    void onObjectAvailable(int index, QJsonObject availableObject, QString partitionName);
};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBCACHINGLISTMODEL_QML_H
