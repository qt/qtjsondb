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

#include "jsondb-global.h"
#include "jsondbobjectkey.h"
#include "jsondbbtree.h"
#include "jsondbbtree.h"
#include "jsondbcollator.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbPartition;
class JsonDbObjectTable;

class JsonDbIndex : public QObject
{
    Q_OBJECT
public:
    JsonDbIndex(const QString &fileName, const QString &indexName, const QString &propertyName,
                const QString &propertyType, const QString &locale, const QString &collation,
                const QString &casePreference, Qt::CaseSensitivity caseSensitivity,
                JsonDbObjectTable *objectTable);
    ~JsonDbIndex();

    QString propertyName() const { return mPropertyName; }
    QStringList fieldPath() const { return mPath; }
    QString propertyType() const { return mPropertyType; }

    QtAddOn::JsonDb::JsonDbBtree *bdb();

    bool setPropertyFunction(const QString &propertyFunction);
    void indexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 stateNumber);
    void deindexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 stateNumber);

    quint32 stateNumber() const;

    JsonDbBtree::Transaction *begin();
    bool commit(quint32);
    bool abort();
    bool clearData();

    void checkIndex();
    void setCacheSize(quint32 cacheSize);
    bool open();
    void close();
    bool exists() const;

    static bool validateIndex(const JsonDbObject &newIndex, const JsonDbObject &oldIndex, QString &message);
    static QString determineName(const JsonDbObject &index);

private:
    QList<QJsonValue> indexValues(JsonDbObject &object);
    QJsonValue indexValue(const QJsonValue &v);

private slots:
    void propertyValueEmitted(QJSValue);

private:
    JsonDbObjectTable *mObjectTable;
    QString mFileName;
    QString mPropertyName;
    QStringList mPath;
    QString mPropertyType;
    QString mLocale;
    QString mCollation;
    QString mCasePreference;
    Qt::CaseSensitivity mCaseSensitivity;
#ifndef NO_COLLATION_SUPPORT
    JsonDbCollator mCollator;
#endif
    quint32 mStateNumber;
    QScopedPointer<JsonDbBtree> mBdb;
    QJSEngine *mScriptEngine;
    QJSValue   mPropertyFunction;
    QList<QJsonValue> mFieldValues;
    quint32 mCacheSize;
};

class JsonDbIndexCursor
{
public:
    JsonDbIndexCursor(JsonDbIndex *index);
    ~JsonDbIndexCursor();

    bool seek(const QJsonValue &value);
    bool seekRange(const QJsonValue &value);

    bool first();
    bool current(QJsonValue &key, ObjectKey &value);
    bool currentKey(QJsonValue &key);
    bool currentValue(ObjectKey &value);
    bool next();
    bool prev();

private:
    bool isOwnTransaction;
    JsonDbBtree::Transaction *mTxn;
    JsonDbBtree::Cursor mCursor;
    JsonDbIndex *mIndex;

    JsonDbIndexCursor(const JsonDbIndexCursor&);
};

class IndexSpec {
public:
    QString name;
    QString propertyName;
    QStringList path;
    QString propertyType;
    QString locale;
    QString collation;
    QString casePreference;
    Qt::CaseSensitivity caseSensitivity;
    QString objectType;
    bool    lazy;
    QPointer<JsonDbIndex> index;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_INDEX_H
