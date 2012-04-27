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


#ifndef JSONDBMODELCACHE_P_H
#define JSONDBMODELCACHE_P_H

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


#include <QHash>
#include <QObject>
#include <QList>
#include <QJsonObject>
#include "qjsondbmodelutils_p.h"

QT_BEGIN_NAMESPACE_JSONDB

typedef QMap<SortingKey, QString> JsonDbModelIndexType;
typedef QHash<QString, QJsonObject> JsonDbModelObjectType;

class Q_JSONDB_EXPORT ModelPage
{
public:
    int index;
    int count;
    qulonglong counter;
    JsonDbModelObjectType objects;

public:
    ModelPage();
    ~ModelPage();
    bool hasIndex(int pos);
    QJsonObject value(const QString & key);
    bool hasValue(const QString &key);

    bool insert(int pos, const QString &key, const QJsonObject &vlaue);
    bool update(const QString &key, const QJsonObject &value);
    bool remove(int pos, const QString &key);
    void dumpPageDetails();
};

class Q_JSONDB_EXPORT ModelCache
{
public:
    static qulonglong currentCounter;
    static int pageSize;
    static int maxPages;
    QList<ModelPage *> pages;

public:
    ~ModelCache();
    void clear();

    int findPage(int pos);
    QJsonObject valueAtPage(int page, const QString & key);
    bool hasValueAtPage(int page, const QString &key);

    bool insert(int pos, const QString &key, const QJsonObject &vlaue,
                const JsonDbModelIndexType &objectUuids);
    bool update(const QString &key, const QJsonObject &value);
    bool remove(int pos, const QString &key);

    void splitPage(int pageno, const JsonDbModelIndexType &objectUuids);
    void addObjects(int index, const JsonDbModelIndexType &objectUuids,
                    const JsonDbModelObjectType &objects);

    bool checkFor(int pos, int &pageIndex);
    int findPrefetchIndex(int pos, int lowWaterMark);
    void dropPage(int page, int &index, int &count);
    void dropLRUPages(int count);

    void setPageSize(int maxItems);
    int maxItems();
    int chunkSize();
    int findChunkSize(int pos);
    int findIndexNSize(int pos, int &size);
    int count();
    void dumpCacheDetails();
};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBMODELCACHE_P_H
