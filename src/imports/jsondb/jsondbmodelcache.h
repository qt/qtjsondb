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


#ifndef JSONDBMODELCACHE_H
#define JSONDBMODELCACHE_H

#include <QHash>
#include <QObject>
#include <QList>
#include <QVariantMap>
#include "jsondbmodelutils.h"

typedef QMap<SortingKey, QString> JsonDbModelIndexType;
typedef QHash<QString, QVariantMap> JsonDbModelObjectType;

class ModelPage
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
    QVariantMap value(const QString & key);
    bool hasValue(const QString &key);

    bool insert(int pos, const QString &key, const QVariantMap &vlaue);
    bool update(const QString &key, const QVariantMap &value);
    bool remove(int pos, const QString &key);
    void dumpPageDetails();
};

class ModelCache
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
    QVariantMap valueAtPage(int page, const QString & key);
    bool hasValueAtPage(int page, const QString &key);

    bool insert(int pos, const QString &key, const QVariantMap &vlaue,
                const JsonDbModelIndexType &objectUuids);
    bool update(const QString &key, const QVariantMap &value);
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

#endif // JSONDBMODELCACHE_H
