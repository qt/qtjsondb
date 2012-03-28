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

#include "jsondbmodelutils.h"

QT_BEGIN_NAMESPACE_JSONDB

SortingKey::SortingKey(int partitionIndex, const QVariantMap &object, const QList<bool> &directions, const QList<QStringList> &paths, const SortIndexSpec &spec){
    QVariantList values;
    for (int i = 0; i < paths.size(); i++)
        values.append(lookupProperty(object, paths[i]));
    QByteArray uuid = QUuid(object[QLatin1String("_uuid")].toString()).toRfc4122();
    d = new SortingKeyPrivate(partitionIndex, uuid, directions, values, spec);

}

SortingKey::SortingKey(int partitionIndex, const QVariantList &object, const QList<bool> &directions, const SortIndexSpec &spec)
{
    QVariantList values = object.mid(1);
    QByteArray uuid = QUuid(object.at(0).toString()).toRfc4122();
    d = new SortingKeyPrivate(partitionIndex, uuid, directions, values, spec);
}

SortingKey::SortingKey(const SortingKey &other)
    :d(other.d)
{
}

int SortingKey::partitionIndex() const
{
    return d->partitionIndex;
}

static bool operator<(const QVariant& lhs, const QVariant& rhs)
{
    if ((lhs.type() == QVariant::Int) && (rhs.type() == QVariant::Int))
        return lhs.toInt() < rhs.toInt();
    else if ((lhs.type() == QVariant::LongLong) && (rhs.type() == QVariant::LongLong))
        return lhs.toLongLong() < rhs.toLongLong();
    else if ((lhs.type() == QVariant::Double) && (rhs.type() == QVariant::Double))
        return lhs.toFloat() < rhs.toFloat();
    return (QString::compare(lhs.toString(), rhs.toString(), Qt::CaseInsensitive ) < 0);
}

static int equalWithSpec(const QVariant& lhs, const QVariant& rhs, const SortIndexSpec &spec)
{
    // Supports only string and UUID
    if (spec.type == SortIndexSpec::String) {
        Qt::CaseSensitivity cs = spec.caseSensitive ? Qt::CaseSensitive :Qt::CaseInsensitive;
        return QString::compare(lhs.toString(), rhs.toString(), cs);
    } else if (spec.type == SortIndexSpec::UUID) {
        QByteArray lhsUuid = QUuid(lhs.toString()).toRfc4122();
        QByteArray rhsUuid = QUuid(lhs.toString()).toRfc4122();
        return memcmp(lhsUuid.constData(), rhsUuid.constData(), qMin(lhsUuid.size(), rhsUuid.size()));
    }
    return -1;
}

bool SortingKey::operator <(const SortingKey &rhs) const
{
    const SortingKeyPrivate *dLhs = d;
    const SortingKeyPrivate *dRhs = rhs.d;
    const int nKeys = dLhs->count;
    for (int i = 0; i < nKeys; i++) {
        const QVariant &lhsValue = dLhs->values[i];
        const QVariant &rhsValue = dRhs->values[i];
        int cmp = -1;
        // The index spec is only applied to the first item
        if (!i && (dLhs->indexSpec.type == SortIndexSpec::String || dLhs->indexSpec.type == SortIndexSpec::UUID)) {
            if ((cmp = equalWithSpec(lhsValue, rhsValue, dLhs->indexSpec))) {
                return (dLhs->directions[i] ? (cmp < 0) : (cmp > 0));
            }
        } else if (lhsValue != rhsValue) {
            bool result = lhsValue < rhsValue;
            return (dLhs->directions[i] ? result :!result);
        }
    }
    int cmp = memcmp(dLhs->uuid.constData(), dRhs->uuid.constData(), qMin(dLhs->uuid.size(), dRhs->uuid.size()));
    // In case of even score jsondb sorts according to _uuid in the same direction as the last sort item
    if (nKeys)
        return (dLhs->directions[nKeys-1] ? (cmp < 0) : (cmp > 0));
    return cmp;
}

bool SortingKey::operator ==(const SortingKey &rhs) const
{
    bool equal = true;
    const SortingKeyPrivate *dLhs = d;
    const SortingKeyPrivate *dRhs = rhs.d;
    const int nKeys = dLhs->count;
    for (int i = 0; i < nKeys; i++) {
        const QVariant &lhsValue = dLhs->values[i];
        const QVariant &rhsValue = dRhs->values[i];
        // The index spec is only applied to the first item
        if (!i && (dLhs->indexSpec.type == SortIndexSpec::String || dLhs->indexSpec.type == SortIndexSpec::UUID)) {
            if (equalWithSpec(lhsValue, rhsValue, dLhs->indexSpec)) {
                equal = false;
                break;
            }
        } else if (lhsValue != rhsValue) {
            equal = false;
            break;
        }
    }
    if (equal) {
        return (memcmp(dLhs->uuid.constData(), dRhs->uuid.constData(), qMin(dLhs->uuid.size(), dRhs->uuid.size())) == 0);
    }
    return false;
}

QVariant lookupProperty(QVariantMap object, const QStringList &path)
{
    if (!path.size()) {
        return QVariant();
    }
    QVariantMap emptyMap;
    QVariantList emptyList;
    QVariantList objectList;
    for (int i = 0; i < path.size() - 1; i++) {
        const QString &key = path.at(i);
        // this part of the property is a list
        if (!objectList.isEmpty()) {
            bool ok = false;
            int index = key.toInt(&ok);
            if (ok && (index >= 0) && (objectList.count() > index)) {
                if (objectList.at(index).type() == QVariant::List) {
                    objectList = objectList.at(index).toList();
                    object = emptyMap;
                } else  {
                    object = objectList.at(index).toMap();
                    objectList = emptyList;
                }
                continue;
            }
        }
        // this part is a map
        if (object.contains(key)) {
            if (object.value(key).type() == QVariant::List) {
                objectList = object.value(key).toList();
                object = emptyMap;
            } else  {
                object = object.value(key).toMap();
                objectList = emptyList;
            }
        } else {
            return QVariant();
        }
    }
    const QString &key = path.last();
    // get the last part from the list
    if (!objectList.isEmpty()) {
        bool ok = false;
        int index = key.toInt(&ok);
        if (ok && (index >= 0) && (objectList.count() > index)) {
            return objectList.at(index);
        }
    }
    // if the last part is in a map
    return object.value(key);
}

QString removeArrayOperator(QString propertyName)
{
    propertyName.replace(QLatin1String("["), QLatin1String("."));
    propertyName.remove(QLatin1Char(']'));
    return propertyName;
}

QT_END_NAMESPACE_JSONDB
