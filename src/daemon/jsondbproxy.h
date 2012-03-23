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

#ifndef JSONDB_PROXY_H
#define JSONDB_PROXY_H

#include <QObject>
#include <QMultiMap>
#include <QJSValue>

#include "jsondb-global.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbOwner;
class JsonDbPartition;

class JsonDbMapProxy : public QObject {
    Q_OBJECT
public:
    JsonDbMapProxy(const JsonDbOwner *owner, JsonDbPartition *partition, QObject *parent=0);
    ~JsonDbMapProxy();

    Q_SCRIPTABLE void emitViewObject(const QString &key, const QJSValue &value );
    Q_SCRIPTABLE void lookup(const QString &key, const QJSValue &value, const QJSValue &context );
    Q_SCRIPTABLE void lookupWithType(const QString &key, const QJSValue &value, const QJSValue &objectType, const QJSValue &context);

    void setOwner(const JsonDbOwner *owner) { mOwner = owner; }

 signals:
    void viewObjectEmitted(const QJSValue &);
    // to be removed when all map/lookup are converted to join/lookup
    void lookupRequested(const QJSValue &, const QJSValue &);
private:
    const JsonDbOwner *mOwner;
    JsonDbPartition *mPartition;
};

class JsonDbJoinProxy : public QObject {
    Q_OBJECT
public:
    JsonDbJoinProxy( const JsonDbOwner *owner, JsonDbPartition *partition, QObject *parent=0 );
    ~JsonDbJoinProxy();

    Q_SCRIPTABLE void create(const QJSValue &value );
    Q_SCRIPTABLE void lookup(const QJSValue &spec, const QJSValue &context );
    Q_SCRIPTABLE QString createUuidFromString(const QString &id);

    void setOwner(const JsonDbOwner *owner) { mOwner = owner; }

 signals:
    void viewObjectEmitted(const QJSValue &);
    void lookupRequested(const QJSValue &, const QJSValue &);
private:
    const JsonDbOwner *mOwner;
    JsonDbPartition *mPartition;
};

class Console : public QObject {
    Q_OBJECT
public:
    Console();
    Q_SCRIPTABLE void log(const QString &string);
    Q_SCRIPTABLE void debug(const QString &string);
    Q_SCRIPTABLE void info(const QString &string);
    Q_SCRIPTABLE void warn(const QString &string);
    Q_SCRIPTABLE void error(const QString &string);
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_PROXY_H
