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

#ifndef JSONDB_INDEX_H
#define JSONDB_INDEX_H

#include <QObject>
#include <QJSEngine>
#include <QPointer>
#include <QStringList>

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "jsondbobject.h"

#include "jsondbpartitionglobal.h"
#include "jsondbobjectkey.h"
#include "jsondbbtree.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbIndexSpec
{
public:
    QString name;
    QString propertyName;
    QString propertyFunction;
    QString propertyType;
    QString locale;
    QString collation;
    QString casePreference;
    Qt::CaseSensitivity caseSensitivity;
    QStringList objectTypes;

    inline JsonDbIndexSpec()
        : caseSensitivity(Qt::CaseSensitive)
    { }
    inline bool hasPropertyFunction() const { return !propertyFunction.isEmpty(); }
    static JsonDbIndexSpec fromIndexObject(const QJsonObject &indexObject);
};

class JsonDbPartition;
class JsonDbObjectTable;
class JsonDbIndexPrivate;
class Q_JSONDB_PARTITION_EXPORT JsonDbIndex : public QObject
{
    Q_OBJECT
public:
    JsonDbIndex(const QString &fileName, JsonDbObjectTable *objectTable);
    ~JsonDbIndex();

    void setIndexSpec(const JsonDbIndexSpec &);
    const JsonDbIndexSpec &indexSpec() const;

    JsonDbBtree *bdb();

    bool indexObject(JsonDbObject &object, quint32 stateNumber);
    bool deindexObject(JsonDbObject &object, quint32 stateNumber);
    QList<QJsonValue> indexValues(JsonDbObject &object);

    quint32 stateNumber() const;

    JsonDbBtree::Transaction *begin();
    bool commit(quint32);
    bool abort();
    bool clearData();

    void setCacheSize(quint32 cacheSize);
    bool open();
    void close();
    bool isOpen() const;

    static bool validateIndex(const JsonDbObject &newIndex, const JsonDbObject &oldIndex, QString &message);
    static QString determineName(const JsonDbObject &index);

    QString fileName() const;

private:
    Q_DECLARE_PRIVATE(JsonDbIndex)
    Q_DISABLE_COPY(JsonDbIndex)
    QScopedPointer<JsonDbIndexPrivate> d_ptr;

    Q_PRIVATE_SLOT(d_func(), void _q_propertyValueEmitted(QJSValue))
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_INDEX_H
