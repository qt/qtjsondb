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

#ifndef QSONPARSER_H
#define QSONPARSER_H

#include <QtJsonDbQson/qsonglobal.h>
#include <QtJsonDbQson/private/qsonobject_p.h>

#include <QList>
#include <QByteArray>

namespace QtAddOn { namespace JsonDb {

class Q_ADDON_JSONDB_QSON_EXPORT QsonParser
{
public:
    QsonParser(bool streamMode = false);

    void append(const QByteArray& buffer);
    void append(const char *data, int size);

    bool hasError() const;
    bool recover();

    bool isObjectReady() const;
    QsonObject getObject();

    bool isStream() const;
    bool streamDone();

    static int pageSize(const char *data, int maxSize);

    static QsonObject fromRawData(const QByteArray &buffer);

#ifndef QT_TESTLIB_LIB
private:
#endif
    void scanBuffer();

    inline QsonPage::PageType headerFor(QsonPage::PageType type) const
    {
        switch (type) {
        case QsonPage::OBJECT_FOOTER_PAGE:
            return QsonPage::OBJECT_HEADER_PAGE;
        case QsonPage::DOCUMENT_FOOTER_PAGE:
            return QsonPage::DOCUMENT_HEADER_PAGE;
        case QsonPage::LIST_FOOTER_PAGE:
            return QsonPage::LIST_HEADER_PAGE;
        case QsonPage::META_FOOTER_PAGE:
            return QsonPage::META_HEADER_PAGE;
        default:
            return QsonPage::UNKNOWN_PAGE;
        }
    }

    QByteArray mBuffer;
    QList<QsonPage::PageType> mStack;
    QsonContent mContent;

    bool mObjectReady;
    bool mHasError;
    bool mStreamMode;
    bool mStreamReady;
    bool mStreamDone;
};

} } // end namespace QtAddOn::JsonDb

#endif // QSONPARSER_H
