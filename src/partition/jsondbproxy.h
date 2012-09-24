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

#ifndef JSONDB_PROXY_H
#define JSONDB_PROXY_H

#include <QObject>
#include <QMultiMap>
#include <QJSValue>

#include "jsondbpartitionglobal.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbOwner;
class JsonDbPartition;

class Q_JSONDB_PARTITION_EXPORT JsonDbJoinProxy : public QObject {
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

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_PROXY_H
