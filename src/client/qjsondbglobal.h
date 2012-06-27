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

#ifndef QJSONDB_GLOBAL_H
#define QJSONDB_GLOBAL_H

#include "QtCore/qglobal.h"

#if defined(QT_BUILD_JSONDB_LIB)
#  define Q_JSONDB_EXPORT Q_DECL_EXPORT
#else
#  define Q_JSONDB_EXPORT Q_DECL_IMPORT
#endif

#if defined(QT_NAMESPACE)
#  define QT_BEGIN_NAMESPACE_JSONDB namespace QT_NAMESPACE { namespace QtJsonDb {
#  define QT_END_NAMESPACE_JSONDB } }
#  define QT_USE_NAMESPACE_JSONDB using namespace QT_NAMESPACE::QtJsonDb;
#  define QT_PREPEND_NAMESPACE_JSONDB(name) ::QT_NAMESPACE::QtJsonDb::name
#else
#  define QT_BEGIN_NAMESPACE_JSONDB namespace QtJsonDb {
#  define QT_END_NAMESPACE_JSONDB }
#  define QT_USE_NAMESPACE_JSONDB using namespace QtJsonDb;
#  define QT_PREPEND_NAMESPACE_JSONDB(name) ::QtJsonDb::name
#endif

// a workaround for moc - if there is a header file that doesn't use jsondb
// namespace, we still force moc to do "using namespace" but the namespace have to
// be defined, so let's define an empty namespace here
QT_BEGIN_NAMESPACE_JSONDB
QT_END_NAMESPACE_JSONDB

#endif // QJSONDB_GLOBAL_H
