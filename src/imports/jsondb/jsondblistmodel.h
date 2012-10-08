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

#ifndef JSONDBLISTMODEL_QML_H
#define JSONDBLISTMODEL_QML_H

#include <QQmlParserStatus>
#include <QQmlListProperty>
#include <QJSValue>

#include "jsondbpartition.h"
#include <private/qjsondbquerymodel_p.h>

QT_BEGIN_NAMESPACE_JSONDB


class JsonDbListModel : public QJsonDbQueryModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

public:

    JsonDbListModel(QObject *parent = 0);
    virtual ~JsonDbListModel();

    Q_PROPERTY(QQmlListProperty<QT_PREPEND_NAMESPACE_JSONDB(JsonDbPartition)> partitions READ partitions)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int limit READ limit WRITE setLimit)
    Q_PROPERTY(int chunkSize READ chunkSize WRITE setChunkSize)
    Q_PROPERTY(int lowWaterMark READ lowWaterMark WRITE setLowWaterMark)
      Q_PROPERTY(QT_PREPEND_NAMESPACE_JSONDB(JsonDbPartition)* partition READ partition WRITE setPartition)

    virtual void classBegin();
    virtual void componentComplete();

    QQmlListProperty<JsonDbPartition> partitions();

    Q_INVOKABLE void get(int index, const QJSValue &callback);
    Q_INVOKABLE QT_PREPEND_NAMESPACE_JSONDB(JsonDbPartition)* getPartition(int index);
    Q_INVOKABLE int indexOf(const QString &uuid) const;
    static void partitions_append(QQmlListProperty<JsonDbPartition> *p, JsonDbPartition *v);
    static int partitions_count(QQmlListProperty<JsonDbPartition> *p);
    static JsonDbPartition* partitions_at(QQmlListProperty<JsonDbPartition> *p, int idx);
    static void partitions_clear(QQmlListProperty<JsonDbPartition> *p);

    // Offered for backwards compatibility with JsonDbListModel only
    // Deprecated
    QtJsonDb::JsonDbPartition* partition();
    int count() const;
    void setPartition(QtJsonDb::JsonDbPartition *newPartition);
    int limit() const;
    void setLimit(int newCacheSize);
    int chunkSize() const;
    void setChunkSize(int newChunkSize);
    int lowWaterMark() const;
    void setLowWaterMark(int newLowWaterMark);

Q_SIGNALS:
    // Offered for backwards compatibility with JsonDbListModel only
    // Deprecated
    void countChanged() const;

private:
    Q_DISABLE_COPY(JsonDbListModel)

    QMap <int, QJSValue> getCallbacks;
    QMap <QString, JsonDbPartition *> nameToJsonDbPartitionMap;
private slots:
    void onObjectAvailable(int index, QJsonObject availableObject, QString partitionName);
};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBLISTMODEL_QML_H
