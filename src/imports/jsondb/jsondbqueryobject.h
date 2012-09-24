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

#ifndef JSONDBQUERYOBJECT_H
#define JSONDBQUERYOBJECT_H

#include <QObject>
#include <QVariant>
#include <QPointer>
#include <QJSValue>
#include <QQmlParserStatus>
#include <QQmlListProperty>
#include <QJsonDbReadRequest>

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbPartition;
class JsonDbPartitionPrivate;

class JsonDbQueryObject : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

public:
    Q_ENUMS(Status)
    enum Status { Null, Loading, Ready, Error };

    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(JsonDbPartition* partition READ partition WRITE setPartition)
    Q_PROPERTY(quint32 stateNumber READ stateNumber)

    Q_PROPERTY(int limit READ limit WRITE setLimit)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap bindings READ bindings WRITE setBindings)

    JsonDbQueryObject(QObject *parent = 0);
    ~JsonDbQueryObject();

    QString query();
    void setQuery(const QString &newQuery);

    JsonDbPartition* partition();
    void setPartition(JsonDbPartition* newPartition);

    quint32 stateNumber() const;

    int limit();
    void setLimit(int newLimit);

    JsonDbQueryObject::Status status() const;
    QVariantMap error() const;

    QVariantMap bindings() const;
    void setBindings(const QVariantMap &newBindings);

    void classBegin() {}
    void componentComplete();

    Q_INVOKABLE int start();
    Q_INVOKABLE QJSValue takeResults();

Q_SIGNALS:
    void resultsReady(int resultsAvailable);
    void finished();
    void statusChanged(JsonDbQueryObject::Status newStatus);
    void errorChanged(const QVariantMap &newError);

private Q_SLOTS:
    void setError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString & message);
    void setReadyStatus();

private:
    bool completed;
    int queryLimit;
    QString queryString;
    QVariantList results;
    QPointer<JsonDbPartition> partitionObject;
    QPointer<JsonDbPartition> defaultPartitionObject;
    int errorCode;
    QString errorString;
    Status objectStatus;
    QVariantMap queryBindings;
    QPointer<QJsonDbReadRequest> readRequest;

    void clearError();
    inline bool parametersReady();
    void checkForReadyStatus();
    friend class JsonDbPartition;
    friend class JsonDbPartitionPrivate;
};

QT_END_NAMESPACE_JSONDB

#endif //JSONDBQUERYOBJECT_H
