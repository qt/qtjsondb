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

#include "javascript-listmodel.h"
#include <QDebug>

// #define DEBUG_LIST_MODEL

#ifdef DEBUG_LIST_MODEL
#define DEBUG() qDebug() << Q_FUNC_INFO
#else
#define DEBUG() if (0) qDebug()
#endif

/*!
    \qmlclass JavaScriptListModel
    \internal
    \inqmlmodule QtAddOn.JsonDb 1
    \inherits ListModel
    \inherits ListModel
    \since 1.x

    The JavaScriptListModel provides a ListModel usable with views such as
    ListView or GridView displaying data items matching a query.

    \code
    JavaScriptListModel {
      id: contactsModel
      query: "[?_type=\"Contact\"]"
      roleNames: ["firstName", "lastName", "phoneNumber"]
    }
    ListView {
	model: contactsModel
	Row {
	    spacing: 10
	    Text {
		text: firstName + " " + lastName
	    }
	    Text {
		text: phoneNumber
	    }
	}
    }
    \endcode

    \sa ListView, GridView

*/
JavaScriptListModel::JavaScriptListModel(QObject *parent)
    : QListModelInterface(parent)
{
    DEBUG();
}

JavaScriptListModel::~JavaScriptListModel()
{
}

void JavaScriptListModel::classBegin()
{
}

void JavaScriptListModel::componentComplete()
{
    mComponentComplete = true;
}

/*!
  \qmlproperty int JavaScriptListModel::count

  Returns the number of items in the model.
*/
int JavaScriptListModel::count() const
{
    //DEBUG() << mData.size();
    //qDebug() << Q_FUNC_INFO << mList.size();
    return mList.size();
}

static QVariant lookupProperty(const QVariant &item, const QStringList &propertyChain)
{
    QVariant result = item;
    for (int i = 0; i < propertyChain.size(); i++) {
	QString property = propertyChain[i];
	QVariantMap map = result.toMap();
	if (map.contains(property)) {
	    result = map.value(property);
	} else {
	    DEBUG() << item << property << propertyChain;
	    return QVariant();
	}
    }
    return result;
}

QHash<int,QVariant> JavaScriptListModel::data(int index, const QList<int>& roles) const
{
    QHash<int,QVariant> result;

    QVariant item;
    if ((index >= 0) && (index < mList.size()))
	item = mList[index];
    for (int i = 0; i < roles.size(); i++) {
	int role = roles[i];
	
	QString property = mRoleNames[i];

	//qDebug() << Q_FUNC_INFO << index << property << lookupProperty(item, property.split('.'));

	result.insert(role, lookupProperty(item, property.split('.')));
    }

    return result;
}

QVariant JavaScriptListModel::data(int index, int role) const
{
    QVariant item;

    if ((index >= 0) && (index < mList.size()))
	item = mList[index];
    QVariant result;
    QString property = mRoleNames[role];
    result = lookupProperty(item, property.split('.'));

    DEBUG() << index << role << result;
    //qDebug() << Q_FUNC_INFO << index << role << result;

    return result;
}

bool JavaScriptListModel::setData(int index, const QHash<int,QVariant>& newValues)
{
    QVariantMap object = mList[index].toMap();
    QList<int> changedRoles;
    for (QHash<int,QVariant>::const_iterator it = newValues.begin(); it != newValues.end(); ++it) {
	int role = it.key();
	QVariant newValue = it.value();
	QVariant oldValue = object.value(mRoleNames[role]);

	//qDebug() << Q_FUNC_INFO << index << role << mRoleNames[role] << oldValue << newValue;

	if (oldValue != newValue)
	    changedRoles << role;
	object.insert(mRoleNames[role], newValue);
    }
    mList[index] = object;
    if (changedRoles.size())
	emit itemsChanged(index, 1, changedRoles);

    return true;
}

void JavaScriptListModel::setList(const QVariant &_newList)
{
    QVariantList newList;
    if (_newList.isValid()) {
	if (_newList.type() == QVariant::List)
	    newList = _newList.toList();
	else if (_newList.type() == QVariant::Map)
	    newList << _newList;
    }
    int currentSize = mList.size();
    int newSize = newList.size();
    int minSize = qMin<int>(currentSize, newSize);
    if (currentSize > newSize) {
	emit itemsRemoved(newSize, currentSize - newSize);
    }
    for (int i = 0; i < minSize; i++) {
	QList<int> changedRoles;
	for (int j = 0; j < mRoles.size(); j++) {
	    QString roleName = mRoleNames[j];
	    if (mList[i].toMap().value(roleName) != newList[i].toMap().value(roleName))
		changedRoles.append(j);
	}
	if (changedRoles.size())
	    emit itemsChanged(i, 1, changedRoles);
    }
    mList = newList;
    if (currentSize < newSize) {
	emit itemsInserted(currentSize, newSize - currentSize);
    }
    emit listChanged();
    if (currentSize != newSize)
      emit countChanged();
}

/*!
  \qmlproperty string JavaScriptListModel::query

  Returns the query used by the model to fetch items from the database.

  In the following example, the \a JavaScriptListModel would contain all the objects with \a _type contains the value \a "CONTACT"
 
  \code
  JavaScriptListModel {
      id: listModel
      query: "[?_type=\"CONTACT\"]"
  }
  \endcode

*/
QVariant JavaScriptListModel::list() const
{
    return mList;
}

/*!
  \qmlproperty ListOrObject JavaScriptListModel::roleNames

  Controls which properties to expose from the objects matching the query.

  Setting \a roleNames to a list of strings causes the model to fetch
  those properties of the objects and expose them as roles to the
  delegate for each item viewed.

  \code
  JavaScriptListModel {
    query: "[?_type=\"MyType\"]"
    roleNames: ['a', 'b']
  }
  ListView {
      model: listModel
      Text {
          text: a + ":" + b
      }
  \endcode

  Setting \a roleNames to a dictionary remaps properties in the object
  to the specified roles in the model.

  In the following example, role \a a would yield the value of
  property \a aLongName in the objects. Role \a liftedProperty would
  yield the value of \a o.nested.property for each matching object \a
  o in the database.

  \code
  function makeRoleNames() {
    return { 'a': 'aLongName', 'liftedProperty': 'nested.property' };
  }
  JavaScriptListModel {
    id: listModel
    query: "[?_type=\"MyType\"]"
    roleNames: makeRoleNames()
  }
  ListView {
      model: listModel
      Text {
          text: a + " " + liftedProperty
      }
  }
  \endcode
*/
QStringList JavaScriptListModel::roleNames() const
{
    return mRoleNames;
}

void JavaScriptListModel::setRoleNames(const QStringList &roleNames)
{
    mRoleNames = roleNames;
    mRoles.clear();
    for (int i = 0; i < mRoleNames.size(); i++)
	mRoles << i;
    emit roleNamesChanged();
}

QList<int> JavaScriptListModel::roles() const
{
    //qDebug() << Q_FUNC_INFO << mRoles;
    return mRoles;
}

QString JavaScriptListModel::toString(int role) const
{
    if ((role >= 0) && (role < mRoleNames.size())) {
	QString name = mRoleNames[role];
	//qDebug() << Q_FUNC_INFO << role << name;
	return name;
    } else
	return QString();
}

//Q_EXPORT_STATIC_PLUGIN(JavaScriptListModelPlugin)
//Q_EXPORT_PLUGIN2(jsondblistmodelplugin, JavaScriptListModelPlugin)
