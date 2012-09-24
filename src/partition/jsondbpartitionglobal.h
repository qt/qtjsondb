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

#ifndef JSONDB_PARTITION_GLOBAL_H
#define JSONDB_PARTITION_GLOBAL_H

#include "QtCore/qglobal.h"

#ifndef QT_STATIC
# if defined(QT_BUILD_JSONDBPARTITION_LIB)
#  define Q_JSONDB_PARTITION_EXPORT Q_DECL_EXPORT
# else
#  define Q_JSONDB_PARTITION_EXPORT Q_DECL_IMPORT
# endif
#else
# define Q_JSONDB_PARTITION_EXPORT
#endif

#if defined(QT_NAMESPACE)
#  define QT_BEGIN_NAMESPACE_JSONDB_PARTITION namespace QT_NAMESPACE { namespace QtJsonDb { namespace Partition {
#  define QT_END_NAMESPACE_JSONDB_PARTITION } } }
#  define QT_USE_NAMESPACE_JSONDB_PARTITION using namespace QT_NAMESPACE::QtJsonDb::Partition;
#  define QT_PREPEND_NAMESPACE_JSONDB_PARTITION(name) ::QT_NAMESPACE::QtJsonDb::Partition::name
#else
#  define QT_BEGIN_NAMESPACE_JSONDB_PARTITION namespace QtJsonDb { namespace Partition {
#  define QT_END_NAMESPACE_JSONDB_PARTITION } }
#  define QT_USE_NAMESPACE_JSONDB_PARTITION using namespace QtJsonDb::Partition;
#  define QT_PREPEND_NAMESPACE_JSONDB_PARTITION(name) ::QtJsonDb::Partition::name
#endif

// a workaround for moc - if there is a header file that doesn't use jsondb
// namespace, we still force moc to do "using namespace" but the namespace have to
// be defined, so let's define an empty namespace here
QT_BEGIN_NAMESPACE_JSONDB_PARTITION
QT_END_NAMESPACE_JSONDB_PARTITION

#endif // JSONDB_PARTITION_GLOBAL_H
