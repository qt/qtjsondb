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

#ifndef OBJECT_TABLE_H
#define OBJECT_TABLE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPair>
#include <QtEndian>

#include <QtJsonDbQson/private/qson_p.h>

#include "aodb.h"

#include "objectkey.h"

QT_BEGIN_HEADER

class AoDb;

namespace QtAddOn { namespace JsonDb {

class IndexSpec;
class JsonDbBtreeStorage;
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
    QsonMap oldObject;

    inline ObjectChange(const ObjectKey &obj, Action act, const QsonMap &old = QsonMap())
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


class ObjectTable : public QObject
{
    Q_OBJECT
public:
    ObjectTable(JsonDbBtreeStorage *parent=0);
    ~ObjectTable();

    QString filename() const { return mFilename; }
    bool open(const QString &filename, AoDb::DbFlags flags);
    void close();
    AoDb *bdb() const { return mBdb; }
    bool begin();
    bool commit(quint32);
    bool abort();
    bool compact();

    quint32 stateNumber() const { return mStateNumber; }
    quint32 storeStateChange(const ObjectKey &key1, ObjectChange::Action action, const QsonMap &old = QsonMap());
    quint32 storeStateChange(const QList<ObjectChange> &stateChange);
    void changesSince(quint32 stateNumber, QMap<quint32, QList<ObjectChange> > *changes);
    QsonMap changesSince(quint32 stateNumber, const QSet<QString> &limitTypes = QSet<QString>());

    IndexSpec *indexSpec(const QString &propertyName);
    bool addIndex(const QString &propertyName,
                  const QString &propertyType = QString("string"),
                  const QString &objectType = QString(),
                  const QString &propertyFunction = QString());
    bool removeIndex(const QString &propertyName);
    void reindexObjects(const QString &propertyName, const QStringList &path, quint32 stateNumber, bool inTransaction = false);
    void indexObject(const ObjectKey & objectKey, QsonMap object, quint32 stateNumber);
    void deindexObject(const ObjectKey &objectKey, QsonMap object, quint32 stateNumber);
    void updateIndex(JsonDbIndex *index);    

    bool get(const ObjectKey &objectKey, QsonMap &object, bool includeDeleted=false);
    bool put(const ObjectKey &objectKey, QsonObject &object);
    bool remove(const ObjectKey &objectKey);

    QString errorMessage() const;

    QsonMap getObjects(const QString &keyName, const QVariant &keyValue, const QString &objectType);


private:
    JsonDbBtreeStorage *mStorage;
    QString             mFilename;
    AoDb               *mBdb;
    QHash<QString,IndexSpec> mIndexes; // indexed by full path, e.g., _type or _name.first
    QVector<AoDb *> mBdbTransactions;
    bool                mInTransaction;

    quint32 mStateNumber;

    // intermediate state changes until the commit is called
    QByteArray mStateChanges;
    QList<QsonMap> mStateObjectChanges;
};

void makeStateKey(QByteArray &baStateKey, quint32 stateNumber);
bool isStateKey(const QByteArray &baStateKey);


} } // end namespace QtAddOn::JsonDb

QT_END_HEADER

#endif
