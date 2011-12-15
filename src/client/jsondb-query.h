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

#ifndef JSONDB_QUERY_H
#define JSONDB_QUERY_H

#include <QObject>
#include <QVariantMap>
#include <QList>
#include <QStringList>
#include <QScopedPointer>

#include "jsondb-global.h"

QT_BEGIN_HEADER

namespace QtAddOn { namespace JsonDb {

class JsonDbClient;
class JsonDbResultBasePrivate;
class JsonDbQueryPrivate;
class JsonDbChangesSincePrivate;

class Q_ADDON_JSONDB_EXPORT JsonDbResultBase : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int requestId READ requestId)
    Q_PROPERTY(bool isFinished READ isFinished)
    Q_PROPERTY(int resultsAvailable READ resultsAvailable)
    Q_PROPERTY(QString partition READ partition WRITE setPartition)

public:
    ~JsonDbResultBase();

    int requestId() const;
    bool isFinished() const;

    QVariantMap header() const;

    int resultsAvailable() const;

    QString partition() const;
    void setPartition(const QString &);

public Q_SLOTS:
    virtual void start();
    QList<QVariantMap> takeResults();

Q_SIGNALS:
    void started();
    void resultsReady(int resultsAvailable);
    void finished();
    void error(int code, const QString &message);

protected:
    JsonDbResultBase(JsonDbResultBasePrivate *d, QObject *parent = 0);

    Q_DISABLE_COPY(JsonDbResultBase)
    Q_DECLARE_PRIVATE(JsonDbResultBase)
    QScopedPointer<JsonDbResultBasePrivate> d_ptr;
};

class Q_ADDON_JSONDB_EXPORT JsonDbQuery : public JsonDbResultBase
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery)
    Q_PROPERTY(int queryOffset READ queryOffset WRITE setQueryOffset)
    Q_PROPERTY(int queryLimit READ queryLimit WRITE setQueryLimit)

    Q_PROPERTY(quint32 stateNumber READ stateNumber)
    Q_PROPERTY(QString sortKey READ sortKey)

public:
    ~JsonDbQuery();

    quint32 stateNumber() const;
    QString sortKey() const;

    QString query() const;
    void setQuery(const QString &);

    int queryOffset() const;
    void setQueryOffset(int offset);

    int queryLimit() const;
    void setQueryLimit(int limit);

public Q_SLOTS:
    virtual void start();

    void bindValue(const QString &placeHolder, const QVariant &val);
    QVariant boundValue(const QString &placeHolder) const;
    QMap<QString,QVariant> boundValues() const;

private:
    JsonDbQuery(JsonDbClient *client, QObject *parent = 0);

    Q_DECLARE_PRIVATE(JsonDbQuery)
    Q_PRIVATE_SLOT(d_func(), void _q_response(int,QVariant))
    Q_PRIVATE_SLOT(d_func(), void _q_error(int,QString))
    Q_PRIVATE_SLOT(d_func(), void _q_emitMoreData())
    friend class JsonDbClient;
};

class Q_ADDON_JSONDB_EXPORT JsonDbChangesSince : public JsonDbResultBase
{
    Q_OBJECT
    Q_PROPERTY(QString partition READ partition WRITE setPartition)
    Q_PROPERTY(QStringList types READ types WRITE setTypes)
    Q_PROPERTY(quint32 stateNumber READ stateNumber WRITE setStateNumber)

    Q_PROPERTY(quint32 startingStateNumber READ startingStateNumber)
    Q_PROPERTY(quint32 currentStateNumber READ currentStateNumber)

public:
    ~JsonDbChangesSince();

    quint32 startingStateNumber() const;
    quint32 currentStateNumber() const;

    QStringList types() const;
    void setTypes(const QStringList &types);

    quint32 stateNumber() const;
    void setStateNumber(quint32 stateNumber);

public Q_SLOTS:
    virtual void start();

private:
    JsonDbChangesSince(JsonDbClient *client, QObject *parent = 0);

    Q_DECLARE_PRIVATE(JsonDbChangesSince)
    Q_PRIVATE_SLOT(d_func(), void _q_response(int,QVariant))
    Q_PRIVATE_SLOT(d_func(), void _q_error(int,QString))
    Q_PRIVATE_SLOT(d_func(), void _q_emitMoreData())
    friend class JsonDbClient;
};

} } // end namespace QtAddOn::JsonDb

QT_END_HEADER

#endif // JSONDB_QUERY_H
