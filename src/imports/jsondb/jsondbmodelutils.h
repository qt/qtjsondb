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


#ifndef JSONDBMODELUTILS_H
#define JSONDBMODELUTILS_H
#include <QSharedData>
#include <QStringList>
#include <QUuid>
#include <QJSValue>
#include <QVariant>
#include "jsondb-client.h"

QT_USE_NAMESPACE_JSONDB

struct CallbackInfo {
    int index;
    QJSValue successCallback;
    QJSValue errorCallback;
};

struct NotifyItem {
    QString  notifyUuid;
    QVariant item;
    JsonDbClient::NotifyType action;
};

struct SortIndexSpec
{
    QString propertyName;
    QString propertyType; //### TODO remove
    QString name;
    bool caseSensitive;
    enum Type { None, String, Number, UUID };
    Type type;

    SortIndexSpec() : caseSensitive(false), type(SortIndexSpec::None) {}
    SortIndexSpec(const SortIndexSpec &other)
        : propertyName(other.propertyName),
          propertyType(other.propertyType),
          name(other.name),
          caseSensitive(other.caseSensitive),
          type(other.type)
    {}

};

struct IndexInfo
{
    SortIndexSpec spec;
    bool valid;
    int requestId;
    IndexInfo () {clear();}
    void clear() {
        requestId = -1;
        valid = false;
        spec.type = SortIndexSpec::None;
    }
};

class SortingKeyPrivate;

class SortingKey {
public:
    SortingKey(int partitionIndex, const QVariantMap &object, const QList<bool> &directions, const QList<QStringList> &paths, const SortIndexSpec &spec = SortIndexSpec());
    SortingKey(int partitionIndex, const QVariantList &object, const QList<bool> &directions, const SortIndexSpec &spec = SortIndexSpec());
    SortingKey(const SortingKey&);
    SortingKey() {}
    int partitionIndex() const;
    bool operator <(const SortingKey &rhs) const;
    bool operator ==(const SortingKey &rhs) const;
private:
    QSharedDataPointer<SortingKeyPrivate> d;
};

struct RequestInfo
{
    int requestId;
    int lastOffset;
    QString notifyUuid;
    int lastSize;
    int requestCount;

    RequestInfo() { clear();}
    void clear()
    {
        requestId = -1;
        lastOffset = 0;
        lastSize = -1;
        requestCount = 0;
        notifyUuid.clear();
    }
};

class SortingKeyPrivate : public QSharedData {
public:
    SortingKeyPrivate(int index, const QByteArray &objectUuid, QList<bool> objectDirections, QVariantList objectKeys, const SortIndexSpec &spec)
        :uuid(objectUuid), directions(objectDirections), values(objectKeys), partitionIndex(index), indexSpec(spec){}
    QByteArray uuid;
    QList<bool>  directions;
    QVariantList values;
    int partitionIndex;
    SortIndexSpec indexSpec;
};

template <typename T> int iterator_position(T &begin, T &end, T &value)
{
    int i = 0;
    for (T itr = begin;(itr != end && itr != value); i++, itr++) {}
    return i;
}

QVariant lookupProperty(QVariantMap object, const QStringList &path);
QString removeArrayOperator(QString propertyName);

#endif // JSONDBMODELUTILS_H
