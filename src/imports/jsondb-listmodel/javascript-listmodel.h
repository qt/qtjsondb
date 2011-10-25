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

#ifndef JavaScriptListModel_H
#define JavaScriptListModel_H

#include <QDeclarativeParserStatus>
#include <QStringList>
#include <QObject>
#include <QHash>

#if (QT_VERSION == 0x040700)
#include "qlistmodelinterface_p_4.7.0.h"
#else
#include "qlistmodelinterface_p.h"
#endif
#include "jsondb-client.h"
#include "../jsondb/jsondb-component.h"
#include "cuid.h"

class JavaScriptListModel : public QListModelInterface, public QDeclarativeParserStatus
{
  Q_OBJECT
  Q_INTERFACES(QDeclarativeParserStatus)
public:
    JavaScriptListModel(QObject *parent = 0);
    virtual ~JavaScriptListModel();

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QVariant list READ list WRITE setList NOTIFY listChanged)
    Q_PROPERTY(QStringList roleNames READ roleNames WRITE setRoleNames NOTIFY roleNamesChanged)

    virtual void classBegin();
    virtual void componentComplete();
    virtual int count() const;
    virtual QHash<int,QVariant> data(int index, const QList<int>& roles = QList<int>()) const;
    virtual QVariant data(int index, int role) const;
    virtual bool setData(int index, const QHash<int,QVariant>& values);

    QVariant list() const;
    void setList(const QVariant &newList);

    QStringList roleNames() const;
    void setRoleNames(const QStringList &roles);

    virtual QList<int> roles() const;
    virtual QString toString(int role) const;

 signals:
    void countChanged();
    void listChanged();
    void roleNamesChanged();

 private:
    QVariantList mList;
    QStringList mRoleNames;
    QList<int> mRoles;
    bool mComponentComplete;
};

#endif
