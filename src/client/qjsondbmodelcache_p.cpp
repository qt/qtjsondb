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
//#define JSONDB_LISTMODEL_DEBUG

#include "qjsondbmodelcache_p.h"
#include <QMap>
#include <QDebug>

QT_BEGIN_NAMESPACE_JSONDB

ModelPage::ModelPage()
    : index(-1)
    , count(0)
    , counter(0)
{
}

ModelPage::~ModelPage()
{
}

bool ModelPage::hasIndex(int pos)
{
    return (pos >= index && pos < (index+count));
}

QJsonObject ModelPage::value(const QString &key)
{
    counter = ++ModelCache::currentCounter;
    return objects.value(key);
}

bool ModelPage::hasValue(const QString &key)
{
    counter = ++ModelCache::currentCounter;
    return objects.contains(key);
}

bool ModelPage::insert(int pos, const QString &key, const QJsonObject &value)
{
    // allow adding a consecutive item
    if (pos >= index && pos <= (index+count)) {
        count++;
        counter = ++ModelCache::currentCounter;
        objects.insert(key, value);
        return true;
    }
    return false;
}

bool ModelPage::update(const QString &key, const QJsonObject &value)
{
    bool ret = (objects.remove(key) >= 1);
    if (ret) {
        objects.insert(key, value);
        counter = ++ModelCache::currentCounter;
    }
    return ret;
}

bool ModelPage::remove(int pos, const QString &key)
{
    if (pos >= index && pos < (index+count)) {
        count--;
        counter = ++ModelCache::currentCounter;
        objects.remove(key);
        return true;
    }
    return false;
}

void ModelPage::dumpPageDetails()
{
    JsonDbModelObjectType::const_iterator i;
    for (i = objects.constBegin(); i != objects.constEnd(); ++i) {
        qDebug() << i.key();
    }
}

qulonglong ModelCache::currentCounter = 1;
int ModelCache::pageSize = 50;
int ModelCache::maxPages = 5;

ModelCache::~ModelCache()
{
    clear();
}

void ModelCache::clear()
{
    while (!pages.isEmpty())
        delete pages.takeFirst();
}

int ModelCache::findPage(int pos)
{
    for (int i = 0; i < pages.count(); i++) {
        if (pages[i]->hasIndex(pos))
            return i;
    }
    return -1;
}

QJsonObject ModelCache::valueAtPage(int page, const QString &key)
{
    Q_ASSERT(page >= 0 && page < pages.count());
    QJsonObject o = pages[page]->value(key);
    return pages[page]->value(key);
}

bool ModelCache::hasValueAtPage(int page, const QString &key)
{
    Q_ASSERT(page >= 0 && page < pages.count());
    return pages[page]->hasValue(key);
}

void ModelCache::splitPage(int page, const JsonDbModelIndexType &objectUuids)
{
    // split the page into 2
    int count = pages[page]->count;
    ModelPage *halfPage = new ModelPage();
    halfPage->index = pages[page]->index+pages[page]->count/2;
    halfPage->count = count-(halfPage->index-pages[page]->index);
    pages[page]->count = count-halfPage->count;
    JsonDbModelIndexType::const_iterator itr = objectUuids.constBegin() + halfPage->index;
    for (int i = halfPage->index; i < halfPage->index+halfPage->count; i++, itr++) {
        // transfer the items to the new page
        const QString &key = itr.value();
        const QJsonObject &value = pages[page]->value(key);
        halfPage->objects.insert(key, value);
        pages[page]->objects.remove(key);
    }
    halfPage->counter = pages[page]->counter;
    pages.append(halfPage);
}

bool ModelCache::insert(int pos, const QString &key, const QJsonObject &value,
                        const JsonDbModelIndexType &objectUuids)
{
    if (!pages.count()) {
        ModelPage *newPage = new ModelPage();
        newPage->index = pos;
        newPage->count = 0;
        newPage->counter = ++ModelCache::currentCounter;
        pages.append(newPage);
    }
    int pagePos = -1;
    for (int i = 0; i < pages.count(); i++) {
        if (pages[i]->insert(pos, key, value)) {
            pagePos = i;
            break;
        }
    }
    for (int i = 0; i < pages.count(); i++) {
        // update starting index of all pages which
        // come after this position
        if (pages[i]->index > pos)
            pages[i]->index++;
    }
    if (pagePos != -1 && pages[pagePos]->count > pageSize) {
        // there is an overflow in this page
        // split this page into 2.
        splitPage(pagePos, objectUuids);
        if (count() > maxItems()) {
            // ### Make sure we have more than one page
#ifdef JSONDB_LISTMODEL_DEBUG
            dumpCacheDetails();
#endif
            dropLRUPages(1);
        }

    }
    return (pagePos != -1);
}

int ModelCache::count()
{
    int items = 0;
    for (int i = 0; i < pages.count(); i++)
        items += pages[i]->count;
    return items;
}

bool ModelCache::update(const QString &key, const QJsonObject &value)
{
    for (int i = 0; i < pages.count(); i++) {
        if (pages[i]->update(key, value))
            return true;
    }
    return false;
}


bool ModelCache::remove(int pos, const QString &key)
{
    bool ret = false;
    for (int i = 0; i < pages.count(); i++) {
        if (pages[i]->remove(pos, key)) {
            ret = true;
            break;
        }
    }
    for (int i = 0; i < pages.count(); i++) {
        // update starting index of all pages which
        // come after this position
        if (pages[i]->index > pos)
            pages[i]->index--;
    }
    return ret;
}

void ModelCache::dropPage(int page, int &index, int &count)
{
    Q_ASSERT(page < pages.count());
    ModelPage *changedPage = pages.takeAt(page);
    index = changedPage->index;
    count = changedPage->count;
    delete changedPage;
}

void ModelCache::dropLRUPages(int count)
{
    // Drop the LRU pages
    QMap<int, ModelPage*> lru;
    for (int i = 0; i < pages.count(); i++) {
        lru.insert(pages[i]->counter, pages[i]);
    }

    int i = 0;
    QMapIterator<int, ModelPage*> itr(lru);
    while (itr.hasNext()) {
        itr.next();
        if (i < count) {
            pages.removeOne(itr.value());
            // delete the page
            delete itr.value();
        } else {
            break;
        }
        i++;
    }
}

void ModelCache::addObjects(int index, const JsonDbModelIndexType &objectUuids,
                            const JsonDbModelObjectType &objects)
{
    int count = objects.count();
    int pagesToAdd = count/pageSize + ((count % pageSize) ? 1 : 0);

    if (pagesToAdd + pages.count() > maxPages) {
        // Make space for new pages
        dropLRUPages(pagesToAdd + pages.count() - maxPages);
    }
    JsonDbModelIndexType::const_iterator begin = objectUuids.constBegin();
    for (int i = 0; i < pagesToAdd; i++) {
        int j = index +  (i * pageSize);
        int maxIndexForPage = j + pageSize;
        ModelPage *newPage = new ModelPage();
        newPage->index = j;
        JsonDbModelIndexType::const_iterator itr = begin+j;
        for (; j < index+count && j < maxIndexForPage; j++, itr++) {
            const QString &key = itr.value();
            const QJsonObject& value = objects.value(key);
#ifdef JSONDB_LISTMODEL_DEBUG
            if (value.isEmpty()) // Could be an assert instead?
                qDebug() << "##################Empty value###############" << key;
#endif
            newPage->objects.insert(key, value);
        }
        newPage->count = newPage->objects.count();
        newPage->counter = ++ModelCache::currentCounter;
        pages.append(newPage);
    }

}


bool ModelCache::checkFor(int pos, int &pageIndex)
{
    if (pos < 0)
        return false;
    if (findPage(pos) == -1) {
        pageIndex = (pos/pageSize) * pageSize;
        if (findPage(pageIndex) == -1)
            return true;
        pageIndex = -1;
    }
    return false;
}

int ModelCache::findPrefetchIndex(int pos, int lowWaterMark)
{
    int page = findPage(pos);
    if (page != -1) {
        // check if the position is in the upper part
        if ((pos -pages[page]->index) <= lowWaterMark) {
            // get the page before this item
            int pageIndex = -1;
            if (checkFor(pages[page]->index -1, pageIndex)) {
                return pageIndex;
            }
        } else if ((pages[page]->index + pages[page]->count - pos) <= lowWaterMark) {
            // get the page after this item
            int pageIndex = -1;
            if (checkFor(pages[page]->index + pages[page]->count, pageIndex)) {
                return pageIndex;
            }
        }
    }
    return -1;
}

void ModelCache::setPageSize(int maxItems)
{
    //Make sure a pagesize between 25-100 and
    //there is more than one page
    pageSize = qMax(qMin(100, maxItems/4), 25);
    maxPages = qMax(maxItems/pageSize + ((maxItems % pageSize) ? 1 : 0), 2);
}

int ModelCache::maxItems()
{
    return pageSize * maxPages;
}

int ModelCache::chunkSize()
{
    // 2 pages or maximum 100 items
    return qMin(pageSize*2 , 100);
}

int ModelCache::findChunkSize(int pos)
{
    int nextPage = -1;
    int lastPos = -1;
    // find the closest page which comes after this page
    for (int i = 0; i< pages.count(); i++) {
        if (pos < pages[i]->index && (lastPos == -1 || lastPos > pages[i]->index)) {
            nextPage = i;
            lastPos = pages[i]->index;
        }
    }
    if (nextPage != -1 &&  pos + chunkSize() > lastPos ) {
        return lastPos - pos;
    }
    return chunkSize();
}

int ModelCache::findIndexNSize(int pos, int &size)
{
    Q_ASSERT(findPage(pos) == -1);
    size = chunkSize();
    int startIndex = (pos/pageSize)*pageSize;
    int page = findPage(pos);
    if (page != -1) {
        startIndex = pages[page]->index + pages[page]->count;
    }
    //size = findChunkSize(startIndex);
    //if (startIndex+size < pos)
    size = chunkSize();
    return startIndex;
}

void ModelCache::dumpCacheDetails()
{
    qDebug()<<"############ Cache Details ################";
    qDebug()<<"Page Size:"<<pageSize<<"Max Pages = "<<maxPages;
    qDebug()<<"Current Pages #"<<pages.count();
    for (int i = 0; i< pages.count(); i++) {
        qDebug()<<"Page["<<i<<"] Index :"<<pages[i]->index<<" Count :"<<pages[i]->count;
        qDebug()<<"=============== PAGE Details ################";
        //pages[i]->dumpPageDetails();
        qDebug()<<"**************  PAGE Details ################";
    }
    qDebug()<<"************** Cache Details ################";

}

QT_END_NAMESPACE_JSONDB
