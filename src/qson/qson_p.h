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

#ifndef QSON_H
#define QSON_H

#include <QtJsonDbQson/qsonglobal.h>
#include <QtJsonDbQson/private/qsonobject_p.h>
#include <QtJsonDbQson/private/qsonmap_p.h>
#include <QtJsonDbQson/private/qsonlist_p.h>
#include <QtJsonDbQson/private/qsonelement_p.h>
#include <QtJsonDbQson/private/qsonstrings_p.h>

#include <QDebug>

namespace QtAddOn { namespace JsonDb {

template <> inline QsonList QsonMap::value(const QString &key) const
{ return subList(key); }

template <> inline QsonElement QsonMap::value(const QString &key) const
{
    if ((mHeader->type() == QsonPage::DOCUMENT_HEADER_PAGE) && (key == QsonStrings::kLastVersionStr))
        return QsonElement(QByteArray(mHeader->constData() + 4, 22));

    QsonEntry entry = index()->value(key);
    switch (entry.pageNumber) {
    case -2: // QsonStrings::kUuidStr
        return QsonElement(QByteArray(mHeader->constData() + 26, 18));
    case -3: // QsonStrings::kVersionStr
        return QsonElement(QByteArray(mFooter->constData() + 4, 22));
    default:
        return QsonElement(mBody, entry);
    }
}

template <> inline QsonMap QsonList::at(int pos) const
{ return objectAt(pos); }

template <> inline QsonElement QsonList::at(int pos) const
{ return QsonElement(mBody, index()->at(pos)); }

template <> inline QsonMap QsonElement::value() const
{ return QsonMap(*this); }

template <> inline QsonList QsonElement::value() const
{ return QsonList(*this); }

Q_ADDON_JSONDB_QSON_EXPORT QDebug operator<<(QDebug debug, const QsonObject &obj);

Q_ADDON_JSONDB_QSON_EXPORT QVariant qsonToVariant(const QsonObject &object);
Q_ADDON_JSONDB_QSON_EXPORT QsonObject variantToQson(const QVariant &object);

} } // end namespace QtAddOn::JsonDb

#endif // QSON_H
