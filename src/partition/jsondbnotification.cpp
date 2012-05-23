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

#include <QDebug>
#include <QList>
#include <QMap>
#include <QVariantList>
#include <QString>
#include <QStringList>

#include "jsondbindex.h"
#include "jsondbobjecttable.h"
#include "jsondbnotification.h"
#include "jsondbpartition.h"
#include "jsondbquery.h"
#include "jsondbstrings.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

JsonDbNotification::JsonDbNotification(const JsonDbOwner *owner, JsonDbQuery *query,
                                       QStringList actions, qint32 initialStateNumber)
    : mOwner(owner)
    , mCompiledQuery(query)
    , mActions(None)
    , mPartition(0)
    , mObjectTable(0)
    , mInitialStateNumber(initialStateNumber)
    , mLastStateNumber(0)
{
    foreach (QString s, actions) {
        if (s == JsonDbString::kCreateStr)
            mActions |= Create;
        else if (s == JsonDbString::kUpdateStr)
            mActions |= Update;
        else if (s == JsonDbString::kRemoveStr)
            mActions |= Remove;
    }
}
JsonDbNotification::~JsonDbNotification()
{
    if (mCompiledQuery) {
        delete mCompiledQuery;
        mCompiledQuery = 0;
    }
}

void JsonDbNotification::notifyIfMatches(JsonDbObjectTable *objectTable, const JsonDbObject &oldObject, const JsonDbObject &newObject,
                                         Action action, quint32 stateNumber)
{
    if (objectTable != mObjectTable)
        return;

    JsonDbNotification::Action effectiveAction = action;
    JsonDbObject r;

    bool oldMatches = mCompiledQuery->match(oldObject, 0 /* cache */, 0/*mStorage*/);
    bool newMatches = mCompiledQuery->match(newObject, 0 /* cache */, 0/*mStorage*/);

    if (oldMatches || newMatches)
        r = newObject;
    if (!oldMatches && newMatches) {
        effectiveAction = JsonDbNotification::Create;
    } else if (oldMatches && (!newMatches || newObject.isDeleted())) {
        r = oldObject;
        if (newObject.isDeleted())
            r.insert(JsonDbString::kDeletedStr, true);
        effectiveAction = JsonDbNotification::Remove;
    } else if (oldMatches && newMatches) {
        effectiveAction = JsonDbNotification::Update;
    }

    if (!r.isEmpty() &&
            (mActions & effectiveAction) &&
            mOwner->isAllowed(r, mPartition ? mPartition->name() : QString(), QStringLiteral("read"))) {

        // FIXME: looking up of _indexValue should be encapsulated in JsonDbPartition
        if (mPartition && !mCompiledQuery->orderTerms.isEmpty()) {
            const QString &indexName = mCompiledQuery->orderTerms[0].propertyName;
            QString objectType = r.type();
            JsonDbObjectTable *objectTable = mPartition->findObjectTable(objectType);
            JsonDbIndex *index = objectTable->index(indexName);
            if (index) {
                QList<QJsonValue> indexValues = index->indexValues(r);
                if (!indexValues.isEmpty())
                    r.insert(JsonDbString::kIndexValueStr, indexValues.at(0));
            }
        }

        emit notified(r, stateNumber, effectiveAction);
        mLastStateNumber = stateNumber;
    }
}

void JsonDbNotification::notifyStateChange()
{
    QJsonObject stateChange;
    stateChange.insert(QStringLiteral("_state"), static_cast<int>(mLastStateNumber));
    emit notified(stateChange, mLastStateNumber, StateChange);
}

#include "moc_jsondbnotification.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
