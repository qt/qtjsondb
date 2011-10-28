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

#ifndef JsonDbSingletonWatcher_H
#define JsonDbSingletonWatcher_H

#include <QObject>
#include <QVariant>
#include <QDeclarativeParserStatus>

class JsonDbWatcher;

class JsonDbSingletonWatcher : public QObject, public QDeclarativeParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QDeclarativeParserStatus)

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariant value READ value NOTIFY valueChanged)
    Q_PROPERTY(QVariant defaultValue READ defaultValue WRITE setDefaultValue NOTIFY defaultValueChanged)

public:
    JsonDbSingletonWatcher(QObject *parent = 0);
    virtual ~JsonDbSingletonWatcher();

    QString query() const
        { return mQuery; }
    QVariant value() const;
    QVariant defaultValue() const
        { return mDefaultValue; }

    virtual void classBegin();
    virtual void componentComplete();

public slots:
    void setQuery(const QString &query);
    void setDefaultValue(const QVariant &val);

signals:
    void queryChanged();
    void valueChanged();
    void defaultValueChanged();
    void disconnected();

 protected slots:
    void handleResponse( const QVariant& object );
    void start();

 private:
    JsonDbWatcher *mWatcher;
    QString mQuery;
    QVariant mValue;
    QVariant mDefaultValue;
    bool mComponentComplete;
};

#endif
