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

#include "qsonobject_p.h"
#include "qsonmap_p.h"
#include "qsonlist_p.h"

#include <QDebug>

namespace QtAddOn { namespace JsonDb {

/*!
  \class QtAddOn::JsonDb::QsonObject
  \brief The QsonObject class provides binary representation of a JavaScript object.
  \sa QsonMap, QsonList
*/

QsonObject::QsonObject() :
    mHeader(new QsonPage(QsonPage::EMPTY_PAGE)),
    mFooter(new QsonPage(QsonPage::EMPTY_PAGE))
{
}

QsonObject::QsonObject(const QsonContent &pages)
{
    mBody = pages;
    if (pages.size() >= 2) {
        mHeader = mBody.takeFirst();
        mFooter = mBody.takeLast();
    } else {
        qWarning() << "broken qsonobject";
    }
}

QsonObject::QsonObject(Type type)
{
    switch (type) {
    case MapType:
        mHeader = new QsonPage(QsonPage::OBJECT_HEADER_PAGE);
        mFooter = new QsonPage(QsonPage::OBJECT_FOOTER_PAGE);
        break;
    case ListType:
        mHeader = new QsonPage(QsonPage::LIST_HEADER_PAGE);
        mFooter = new QsonPage(QsonPage::LIST_FOOTER_PAGE);
        break;
    case DocumentType:
        mHeader = new QsonPage(QsonPage::LIST_HEADER_PAGE);
        mFooter = new QsonPage(QsonPage::LIST_FOOTER_PAGE);
        break;
    case MetaType:
        mHeader = new QsonPage(QsonPage::META_HEADER_PAGE);
        mFooter = new QsonPage(QsonPage::META_FOOTER_PAGE);
        break;
    default:
        mHeader = new QsonPage(QsonPage::EMPTY_PAGE);
        mFooter = new QsonPage(QsonPage::EMPTY_PAGE);
        break;
    }
}

QsonObject::Type QsonObject::type() const
{
    switch (mHeader->type()) {
    case QsonPage::OBJECT_HEADER_PAGE:
    case QsonPage::DOCUMENT_HEADER_PAGE:
    case QsonPage::META_HEADER_PAGE:
        return QsonObject::MapType;
    case QsonPage::LIST_HEADER_PAGE:
        return QsonObject::ListType;
    default:
        if (mBody.size() == 1) {
            switch (mBody.at(0)->constData()[0]) {
            case QsonPage::NULL_TYPE:
                return QsonObject::NullType;
            case QsonPage::TRUE_TYPE:
            case QsonPage::FALSE_TYPE:
                return QsonObject::BoolType;
            case QsonPage::INT_TYPE:
                return QsonObject::IntType;
            case QsonPage::UINT_TYPE:
                return QsonObject::UIntType;
            case QsonPage::DOUBLE_TYPE:
                return QsonObject::DoubleType;
            case QsonPage::STRING_TYPE:
            case QsonPage::KEY_TYPE:
            case QsonPage::UUID_TYPE:
            case QsonPage::VERSION_TYPE:
                return QsonObject::StringType;
            default:
                break;
            }
        }
        return QsonObject::UnknownType;
    }
}

QsonList QsonObject::toList() const
{
    return QsonList(*this);
}

QsonMap QsonObject::toMap() const
{
    return QsonMap(*this);
}

int QsonObject::dataSize() const
{
    int size = mHeader->dataSize() + mFooter->dataSize();
    foreach (const QsonPagePtr &page, mBody)
        size += page->dataSize();
    return size;
}

QByteArray QsonObject::data() const
{
    QByteArray result;
    result.append(mHeader->constData(), mHeader->dataSize());
    foreach (const QsonPagePtr &page, mBody)
        result.append(page->constData(), page->dataSize());
    result.append(mFooter->constData(), mFooter->dataSize());
    return result;
}

} } // end namespace QtAddOn::JsonDb
