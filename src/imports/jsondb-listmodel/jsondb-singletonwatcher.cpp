/****************************************************************************
**
** Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "jsondb-singletonwatcher.h"
#include "jsondb-watcher.h"

#include <QTimer>


JsonDbSingletonWatcher::JsonDbSingletonWatcher(QObject *parent) :
    QObject(parent),
    mComponentComplete(false)
{
    mWatcher = new JsonDbWatcher(this);
    connect(mWatcher, SIGNAL(response(QVariant)),
            this, SLOT(handleResponse(QVariant)));
    connect(mWatcher, SIGNAL(disconnected()),
            this, SIGNAL(disconnected()));
}

JsonDbSingletonWatcher::~JsonDbSingletonWatcher()
{
}

QVariant JsonDbSingletonWatcher::value() const
{
    return mValue;
}

void JsonDbSingletonWatcher::setQuery(const QString &q)
{
    mQuery = q;
    emit queryChanged();

    if (mComponentComplete && !mQuery.isEmpty()) {
        mWatcher->start(mQuery);
    }
}

void JsonDbSingletonWatcher::setDefaultValue(const QVariant &val)
{
    mDefaultValue = val;
    mWatcher->setDefault(val);
    emit defaultValueChanged();
}

void JsonDbSingletonWatcher::classBegin()
{
}

void JsonDbSingletonWatcher::componentComplete()
{
    mComponentComplete = true;

    if (!mQuery.isEmpty()) {
        // XXX shouldn't be necessary, but something inside of
        // QDeclarativeComponent crashes if we don't wait.
        QTimer::singleShot(0, this, SLOT(start()));
    }
}

void JsonDbSingletonWatcher::start()
{
    if (!mQuery.isEmpty())
        mWatcher->start(mQuery);
}

void JsonDbSingletonWatcher::handleResponse( const QVariant& object )
{
    mValue = object;
    emit valueChanged();
}
