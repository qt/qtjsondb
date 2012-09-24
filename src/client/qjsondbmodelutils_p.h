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


#ifndef JSONDBMODELUTILS_P_H
#define JSONDBMODELUTILS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtJsonDb API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//


#include <QSharedData>
#include <QStringList>
#include <QUuid>
#include <QVariant>
#include <QPointer>
#include <QJsonDbWatcher>
#include <QJsonDbReadRequest>

QT_BEGIN_NAMESPACE_JSONDB

struct NotificationItem {
    int partitionIndex;
    QJsonObject item;
    QJsonDbWatcher::Action action;
};

struct NotifyItem {
    int partitionIndex;
    QVariantMap item;
    QJsonDbWatcher::Action action;
};

struct SortIndexSpec
{
    QString propertyName;
    QString name;
    bool caseSensitive;
    enum Type { None, String, Number, UUID };
    Type type;

    SortIndexSpec() : caseSensitive(true), type(SortIndexSpec::None) {}
    SortIndexSpec(const SortIndexSpec &other)
        : propertyName(other.propertyName),
          name(other.name),
          caseSensitive(other.caseSensitive),
          type(other.type)
    {}

};

class Q_JSONDB_EXPORT ModelRequest : public QObject
{
    Q_OBJECT
public:

    ModelRequest(QObject *parent = 0);
    ~ModelRequest();

    QJsonDbReadRequest* newRequest(int newIndex);
    void resetRequest();
Q_SIGNALS:
    void finished(int index, const QList<QJsonObject> &items, const QString &sortKey);
    void error(QtJsonDb::QJsonDbRequest::ErrorCode code, const QString &message);

private Q_SLOTS:
    void onQueryFinished();

private:
    QPointer<QJsonDbReadRequest> request;
    int index;
};

struct IndexInfo
{
    SortIndexSpec spec;
    bool valid;
    IndexInfo () {clear();}
    void clear() {
        valid = false;
        spec.type = SortIndexSpec::None;
    }
};

class SortingKeyPrivate;

class Q_JSONDB_EXPORT SortingKey {
public:
    SortingKey(int partitionIndex, const QVariantMap &object, const QList<bool> &directions, const QList<QStringList> &paths, const SortIndexSpec &spec = SortIndexSpec());
    SortingKey(int partitionIndex, const QVariantList &object, const QList<bool> &directions, const SortIndexSpec &spec = SortIndexSpec());
    SortingKey(int partitionIndex, const QByteArray &uuid, const QVariantList &object, const QList<bool> &directions, const SortIndexSpec &spec = SortIndexSpec());
    SortingKey(int partitionIndex, const QByteArray &uuid, const QVariant &value, bool direction, const SortIndexSpec &spec);
    SortingKey(const SortingKey&);
    SortingKey() {}
    int partitionIndex() const;
    QVariant value() const;
    bool operator <(const SortingKey &rhs) const;
    bool operator ==(const SortingKey &rhs) const;
private:
    QSharedDataPointer<SortingKeyPrivate> d;
};

struct RequestInfo
{
    int lastOffset;
    int lastSize;
    int requestCount;
    QPointer<QJsonDbWatcher> watcher;

    RequestInfo() { clear();}
    void clear()
    {
        lastOffset = 0;
        lastSize = -1;
        requestCount = 0;
        if (watcher) {
            delete watcher;
            watcher = 0;
        }
    }
};

class SortingKeyPrivate : public QSharedData {
public:
    SortingKeyPrivate(int index, const QByteArray &objectUuid, QList<bool> objectDirections, QVariantList objectKeys, const SortIndexSpec &spec)
        :uuid(objectUuid), directions(0), values(0), count(0), partitionIndex(index), indexSpec(spec)
    {
        count = objectDirections.count();
        directions = new bool[count];
        for (int i=0; i<count; i++) {
            directions[i] = objectDirections[i];
        }
        count = objectKeys.count();
        values = new QVariant[count];
        for (int i=0; i<count; i++) {
            values[i] = objectKeys[i];
        }
        if (spec.type == SortIndexSpec::UUID)
            values[0] = QUuid(objectKeys[0].toString()).toRfc4122();
    }
    SortingKeyPrivate(int index, const QByteArray &objectUuid, bool direction, const QVariant &objectKey, const SortIndexSpec &spec)
        :uuid(objectUuid), directions(0), values(0), count(0), partitionIndex(index), indexSpec(spec)
    {
        count = 1;
        directions = new bool[1];
        directions[0] = direction;
        values = new QVariant[1];
        // Perf improvement
        // Change uuid to RFC4122/ByteArray already here
        // To speed up comparisons
        if (spec.type == SortIndexSpec::UUID)
            values[0] = QUuid(objectKey.toString()).toRfc4122();
        else values[0] = objectKey;
    }
    ~SortingKeyPrivate()
    {
        delete [] directions;
        delete [] values;
    }
    QByteArray uuid;
    bool *directions;
    QVariant *values;
    int count;
    int partitionIndex;
    SortIndexSpec indexSpec;
};

template <typename T> int iterator_position(T &begin, T &end, T &value)
{
    int i = 0;
    for (T itr = begin;(itr != end && itr != value); i++, itr++) {}
    return i;
}

Q_JSONDB_EXPORT QVariant lookupProperty(QVariantMap object, const QStringList &path);
Q_JSONDB_EXPORT QJsonValue lookupJsonProperty(QJsonObject object, const QStringList &path);
Q_JSONDB_EXPORT QString removeArrayOperator(QString propertyName);
Q_JSONDB_EXPORT QList<QJsonObject> qvariantlist_to_qjsonobject_list(const QVariantList &list);
Q_JSONDB_EXPORT QVariantList qjsonobject_list_to_qvariantlist(const QList<QJsonObject> &list);

QT_END_NAMESPACE_JSONDB

#endif // JSONDBMODELUTILS_P_H
