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

#ifndef JSONDB_INDEX_P_H
#define JSONDB_INDEX_P_H

#include <QObject>
#include <QJSEngine>
#include <QPointer>
#include <QStringList>

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "jsondbpartitionglobal.h"
#include "jsondbobject.h"
#include "jsondbbtree.h"
#include "jsondbcollator.h"
#include "jsondbindex.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbIndex;
class JsonDbObjectTable;

class Q_JSONDB_PARTITION_EXPORT JsonDbIndexPrivate
{
    Q_DECLARE_PUBLIC(JsonDbIndex)
public:
    JsonDbIndexPrivate(JsonDbIndex *q);
    ~JsonDbIndexPrivate();

    JsonDbIndex *q_ptr;
    JsonDbIndexSpec mSpec;
    JsonDbObjectTable *mObjectTable;
    QString mPath;
    QString mBaseName;
    QStringList mPropertyNamePath;
#ifndef NO_COLLATION_SUPPORT
    JsonDbCollator mCollator;
#endif
    JsonDbBtree mBdb;
    QPointer<QJSEngine> mScriptEngine;
    QJSValue   mPropertyFunction;
    QList<QJsonValue> mFieldValues;
    quint32 mCacheSize;

    QString fileName() const;
    bool initScriptEngine();
    QJsonValue indexValue(const QJsonValue &v);

    // private slots
    void _q_propertyValueEmitted(QJSValue value);

    static int indexCompareFunction(const QByteArray &ab, const QByteArray &bb);
    static QByteArray makeForwardKey(const QJsonValue &fieldValue, const ObjectKey &objectKey);
    static QByteArray makeForwardValue(const ObjectKey &objectKey);
    static void truncateFieldValue(QJsonValue *value, const QString &type);
    static QJsonValue makeFieldValue(const QJsonValue &value, const QString &type);
    static void forwardKeySplit(const QByteArray &forwardKey, QJsonValue &fieldValue);
    static void forwardKeySplit(const QByteArray &forwardKey, QJsonValue &fieldValue, ObjectKey &objectKey);
    static void forwardValueSplit(const QByteArray &forwardValue, ObjectKey &objectKey);
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_INDEX_P_H
