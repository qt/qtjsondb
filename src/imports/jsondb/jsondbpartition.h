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

#ifndef JSONDBPARTITION_H1
#define JSONDBPARTITION_H1

#include <QObject>
#include <QScopedPointer>
#include <QMap>
#include <QPointer>
#include <QJSValue>
#include <QJSEngine>
#include <QQmlListProperty>
#include <QJsonDbConnection>
#include <QJsonDbWriteRequest>
#include <QJsonDbWatcher>

QT_BEGIN_NAMESPACE_JSONDB
class JsonDbNotify;
class JsonDbPartitionPrivate;
class JsonDbQueryObject;

class JsonDbPartition: public QObject
{
    Q_OBJECT
public:

    enum ConflictResolutionMode {
        RejectStale = QJsonDbWriteRequest::RejectStale,
        Replace = QJsonDbWriteRequest::Replace
    };
    enum State { None, Online, Offline, Error };

    JsonDbPartition(const QString &partitionName=QString(), QObject *parent=0);
    ~JsonDbPartition();

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

    Q_INVOKABLE int create(const QJSValue &object,
                           const QJSValue &options = QJSValue(QJSValue::UndefinedValue),
                           const QJSValue &callback = QJSValue(QJSValue::UndefinedValue));

    Q_INVOKABLE int update(const QJSValue &object,
                           const QJSValue &options = QJSValue(QJSValue::UndefinedValue),
                           const QJSValue &callback = QJSValue(QJSValue::UndefinedValue));

    Q_INVOKABLE int remove(const QJSValue &object,
                           const QJSValue &options = QJSValue(QJSValue::UndefinedValue),
                           const QJSValue &callback = QJSValue(QJSValue::UndefinedValue));

    Q_INVOKABLE int find(const QString &query,
                         const QJSValue &options = QJSValue(QJSValue::UndefinedValue),
                         const QJSValue &callback = QJSValue(QJSValue::UndefinedValue));

    Q_INVOKABLE JsonDbNotify* createNotification(const QString &query);

    Q_INVOKABLE JsonDbQueryObject* createQuery(const QString &query, int limit, QVariantMap bindings);

    QString name() const;
    void setName(const QString &partitionName);

    State state() const { return _state; }

    Q_PROPERTY(QQmlListProperty<QObject> childElements READ childElements)
    Q_CLASSINFO("DefaultProperty", "childElements")
    QQmlListProperty<QObject> childElements();
    Q_ENUMS(ConflictResolutionMode)
Q_SIGNALS:
    void nameChanged(const QString &partitionName);
    void stateChanged(JsonDbPartition::State state);

private:
    QString _name;
    QString _file;
    QString _uuid;
    State _state;

    QJsonDbWatcher *partitionWatcher;
    QList<QPointer<QJsonDbWatcher> > watchers;
    QList<QObject*> childQMLElements;
    QMap<JsonDbQueryObject*, QJSValue> findCallbacks;
    QMap<JsonDbQueryObject*, int> findIds;

    QMap<QJsonDbWriteRequest*, QJSValue> writeCallbacks;
    void updateNotification(JsonDbNotify *notify);
    void removeNotification(JsonDbNotify *notify);

    void init();
    void call(QMap<QJsonDbWriteRequest*, QJSValue> &callbacks, QJsonDbWriteRequest *request);
    void callErrorCallback(QMap<QJsonDbWriteRequest*, QJSValue> &callbacks, QJsonDbWriteRequest *request,
                           QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message);

private Q_SLOTS:
    void queryFinished();
    void partitionQueryFinished();
    void partitionQueryError();
    void queryStatusChanged();
    void requestFinished();
    void requestError(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message);
    void notificationsAvailable();
    void notificationError(QtJsonDb::QJsonDbWatcher::ErrorCode code, const QString &message);

    friend class JsonDbNotify;
    friend class JsonDbQueryObject;
};

QJSValue qjsonobject_list_to_qjsvalue(const QList<QJsonObject> &list);

QT_END_NAMESPACE_JSONDB

#endif
