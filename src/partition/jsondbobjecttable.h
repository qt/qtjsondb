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

#ifndef JSONDB_OBJECT_TABLE_H
#define JSONDB_OBJECT_TABLE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPair>
#include <QtEndian>

#include "jsondbobjectkey.h"
#include "jsondbbtree.h"

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include <jsondbobject.h>
#include "jsondbpartition.h"
#include "jsondbindex.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbBtree;

inline QDebug &operator<<(QDebug &qdb, const JsonDbUpdate &oc)
{
    qdb.nospace() << "JsonDbUpdate(";
    //qdb.nospace() << oc.objectKey;
    qdb.nospace() << "action = ";
    switch (oc.action) {
    case JsonDbNotification::None: qdb.nospace() << "None"; break;
    case JsonDbNotification::Create: qdb.nospace() << "Created"; break;
    case JsonDbNotification::Update: qdb.nospace() << "Updated"; break;
    case JsonDbNotification::Remove: qdb.nospace() << "Removed"; break;
    default: break;
    }
    if (oc.action != JsonDbNotification::Create)
        qdb.nospace() << ", oldObject = " << oc.oldObject;
    if (oc.action != JsonDbNotification::Remove)
        qdb.nospace() << ", newObject = " << oc.newObject;
    qdb.nospace() << ")";
    return qdb;
}


class Q_JSONDB_PARTITION_EXPORT JsonDbObjectTable : public QObject
{
    Q_OBJECT
public:
    enum SyncFlag {
        SyncObjectTable = 0x1,
        SyncIndexes = 0x2,
        SyncStateNumbers = 0x4
    };
    Q_DECLARE_FLAGS(SyncFlags, SyncFlag)

    JsonDbObjectTable(JsonDbPartition *parent=0);
    ~JsonDbObjectTable();

    QString filename() const { return mFilename; }
    bool open(const QString &filename);
    void close();
    JsonDbPartition *partition() const { return mPartition; }
    JsonDbBtree *bdb() const { return mBdb; }
    bool begin();
    void begin(JsonDbIndex *btree);
    bool commit(quint32);
    bool abort();
    bool compact();
    bool sync(SyncFlags flags);

    JsonDbStat stat() const;
    void flushCaches();
    void closeIndexes();

    quint32 stateNumber() const { return mStateNumber; }
    quint32 storeStateChange(const ObjectKey &key, const JsonDbUpdate &stateChange);
    enum TypeChangeMode {
        SplitTypeChanges, KeepTypeChanges
    };
    quint32 changesSince(quint32 stateNumber, const QSet<QString> &limitTypes, QList<JsonDbUpdate> *updateList, TypeChangeMode mode=KeepTypeChanges);

    JsonDbIndex *index(const QString &indexName);
    QList<JsonDbIndex *> indexes() const;

    bool addIndex(const JsonDbIndexSpec &indexSpec);
    bool addIndexOnProperty(const QString &propertyName,
                            const QString &propertyType = QLatin1String("string"),
                            const QString &objectType = QString());
    bool removeIndex(const QString &indexName);
    void reindexObjects(const QString &indexName, quint32 stateNumber);
    void indexObject(JsonDbObject object, quint32 stateNumber);
    void deindexObject(JsonDbObject object, quint32 stateNumber);
    void updateIndex(JsonDbIndex *index);    

    bool get(const ObjectKey &objectKey, QJsonObject *object, bool includeDeleted=false);
    bool put(const ObjectKey &objectKey, const JsonDbObject &object);
    bool remove(const ObjectKey &objectKey);

    QString errorMessage() const;

    GetObjectsResult getObjects(const QString &keyName, const QJsonValue &keyValue, const QString &objectType);

private:
    quint32 changesSince(quint32 stateNumber, QMap<ObjectKey,JsonDbUpdate> *changes);

private:
    JsonDbPartition *mPartition;
    QString             mFilename;
    JsonDbBtree      *mBdb;
    QHash<QString, JsonDbIndex *> mIndexes; // indexed by full path, e.g., _type or _name.first
    QVector<JsonDbBtree::Transaction *> mBdbTransactions;

    quint32 mStateNumber;

    QMultiMap<quint32,JsonDbUpdate> mChangeCache;

    // intermediate state changes until the commit is called
    QByteArray mStateChanges;
    QList<JsonDbUpdate> mStateObjectChanges;

    Q_DISABLE_COPY(JsonDbObjectTable)
};

void makeStateKey(QByteArray &baStateKey, quint32 stateNumber);
bool isStateKey(const QByteArray &baStateKey);

Q_DECLARE_OPERATORS_FOR_FLAGS(JsonDbObjectTable::SyncFlags)

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_OBJECT_TABLE_H
