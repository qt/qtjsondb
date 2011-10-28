/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDB_INDEX_H
#define JSONDB_INDEX_H

#include <QObject>
#include <QPointer>

#include <QtJsonDbQson/private/qson_p.h>

#include "jsondb-global.h"
#include "aodb.h"
#include "objectkey.h"

QT_BEGIN_HEADER

class Bdb;
class AoDb;

namespace QtAddOn { namespace JsonDb {

class JsonDbBtreeStorage;

class JsonDbIndex : public QObject
{
    Q_OBJECT
public:
    JsonDbIndex(const QString &fileName, const QString &fieldName, QObject *parent = 0);
    ~JsonDbIndex();

    QString fieldName() const { return mFieldName; }
    QStringList fieldPath() const { return mPath; }

    AoDb *bdb();

    void indexObject(const ObjectKey &objectKey, QsonObject &object, quint32 stateNumber, bool inTransaction=false);
    void deindexObject(const ObjectKey &objectKey, QsonObject &object, quint32 stateNumber, bool inTransaction=false);

    quint32 stateNumber() const;

    bool begin();
    bool commit(quint32);
    bool abort();
    bool clear();

    void checkIndex();
//    bool checkValidity(const QMap<QString, QsonObject> &objects,
//                       const QMap<quint32, QString> &keyUuids,
//                       const QMap<QString, quint32> &uuidKeys,
//                       JsonDbBtreeStorage *storage);
    bool open();
    void close();

private:
    QString mFileName;
    QString mFieldName;
    QStringList mPath;
    quint32 mStateNumber;
    QScopedPointer<AoDb> mBdb;
};

class JsonDbIndexCursor
{
public:
    JsonDbIndexCursor(JsonDbIndex *index);

    bool seek(const QVariant &value);
    bool seekRange(const QVariant &value);

    bool first();
    bool current(QVariant &key, ObjectKey &value);
    bool currentKey(QVariant &key);
    bool currentValue(ObjectKey &value);
    bool next();
    bool prev();

private:
    AoDbCursor mCursor;

    JsonDbIndexCursor(const JsonDbIndexCursor&);
};

class IndexSpec {
public:
    QString fieldName;
    QStringList path;
    QString fieldType;
    QString objectType;
    bool    lazy;
    QPointer<JsonDbIndex> index;
};

} } // end namespace QtAddOn::JsonDb

QT_END_HEADER

#endif
