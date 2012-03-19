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

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class IndexSpec;
class JsonDbBtree;
class JsonDbPartition;
class JsonDbIndex;

struct ObjectChange
{
    ObjectKey objectKey;
    enum Action {
        Created,
        Updated,
        Deleted,
        LastAction = Deleted
    } action;
    QJsonObject oldObject;

    // needed for QMap<ObjectKey,ObjectChange>
    inline ObjectChange() {}
    inline ObjectChange(const ObjectKey &obj, Action act, const QJsonObject &old = QJsonObject())
        : objectKey(obj), action(act), oldObject(old)
    {
    }
};

inline QDebug &operator<<(QDebug &qdb, const ObjectChange &oc)
{
    qdb.nospace() << "ObjectChange(";
    qdb.nospace() << oc.objectKey;
    qdb.nospace() << ", action = ";
    switch (oc.action) {
    case ObjectChange::Created: qdb.nospace() << "Created"; break;
    case ObjectChange::Updated: qdb.nospace() << "Updated"; break;
    case ObjectChange::Deleted: qdb.nospace() << "Deleted"; break;
    }
    if (oc.action != ObjectChange::Created)
        qdb.nospace() << ", oldObject = " << oc.oldObject;
    qdb.nospace() << ")";
    return qdb;
}


class JsonDbObjectTable : public QObject
{
    Q_OBJECT
public:
    enum SyncFlag {
        SyncObjectTable = 0x1,
        SyncIndexes = 0x2
    };
    Q_DECLARE_FLAGS(SyncFlags, SyncFlag)

    JsonDbObjectTable(JsonDbPartition *parent=0);
    ~JsonDbObjectTable();

    QString filename() const { return mFilename; }
    bool open(const QString &filename, JsonDbBtree::OpenFlags flags);
    void close();
    JsonDbPartition *partition() const { return mPartition; }
    JsonDbBtree *bdb() const { return mBdb; }
    bool begin();
    void begin(JsonDbIndex *btree);
    bool commit(quint32);
    bool abort();
    bool compact();
    void sync(SyncFlags flags);

    JsonDbStat stat() const;
    void flushCaches();

    quint32 stateNumber() const { return mStateNumber; }
    quint32 storeStateChange(const ObjectKey &key1, ObjectChange::Action action, const JsonDbObject &old = JsonDbObject());
    quint32 storeStateChange(const QList<ObjectChange> &stateChange);
    QJsonObject changesSince(quint32 stateNumber, const QSet<QString> &limitTypes = QSet<QString>());

    IndexSpec *indexSpec(const QString &indexName);
    QHash<QString, IndexSpec> indexSpecs() const;

    bool addIndex(const QString &indexName,
                  const QString &propertyName = QString(),
                  const QString &propertyType = QString("string"),
                  const QString &objectType = QString(),
                  const QString &propertyFunction = QString(),
                  const QString &locale = QString(),
                  const QString &collation = QString(),
                  Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive);
    bool addIndexOnProperty(const QString &propertyName,
                            const QString &propertyType = QString("string"),
                            const QString &objectType = QString())
    { return addIndex(propertyName, propertyName, propertyType, objectType); }
    bool removeIndex(const QString &indexName);
    void reindexObjects(const QString &indexName, const QStringList &path, quint32 stateNumber);
    void indexObject(const ObjectKey &objectKey, JsonDbObject object, quint32 stateNumber);
    void deindexObject(const ObjectKey &objectKey, JsonDbObject object, quint32 stateNumber);
    void updateIndex(JsonDbIndex *index);    

    bool get(const ObjectKey &objectKey, QJsonObject *object, bool includeDeleted=false);
    bool put(const ObjectKey &objectKey, const JsonDbObject &object);
    bool remove(const ObjectKey &objectKey);

    QString errorMessage() const;

    GetObjectsResult getObjects(const QString &keyName, const QJsonValue &keyValue, const QString &objectType);

private:
    void changesSince(quint32 stateNumber, QMap<ObjectKey,ObjectChange> *changes);

private:
    JsonDbPartition *mPartition;
    QString             mFilename;
    JsonDbBtree      *mBdb;
    QHash<QString,IndexSpec> mIndexes; // indexed by full path, e.g., _type or _name.first
    QVector<JsonDbBtree::Transaction *> mBdbTransactions;

    quint32 mStateNumber;

    // intermediate state changes until the commit is called
    QByteArray mStateChanges;
    QList<JsonDbObject> mStateObjectChanges;
};

void makeStateKey(QByteArray &baStateKey, quint32 stateNumber);
bool isStateKey(const QByteArray &baStateKey);

Q_DECLARE_OPERATORS_FOR_FLAGS(JsonDbObjectTable::SyncFlags)

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_OBJECT_TABLE_H
