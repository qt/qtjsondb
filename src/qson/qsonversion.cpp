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

#include "qsonversion_p.h"
#include "qsonmap_p.h"

#include <stdio.h>
#include <QStringBuilder>

namespace QtAddOn { namespace JsonDb {

QsonVersion::QsonVersion(const char *data)
{
    if (data[0] != QsonPage::VERSION_TYPE || data[1] != 0) {
        mIsValid = false;
    } else {
        mContent = QByteArray(data, 22);
        mIsValid = (mContent.at(0) == QsonPage::VERSION_TYPE)
                && (mContent.at(1) == 0)
                && (updateCount() > 0);
    }
}

QsonVersion QsonVersion::version(const QsonMap &map)
{
    return QsonVersion(map.mFooter->constData() + 4);
}

QsonVersion QsonVersion::lastVersion(const QsonMap &map)
{
    return QsonVersion(map.mHeader->constData() + 4);
}

QsonVersion QsonVersion::fromLiteral(const QString &literal)
{
    QByteArray result;
    QStringList parts = literal.split(QLatin1Char('-'));
    if (parts.size() == 2) {
        result.resize(2 + sizeof(quint32));
        result[0] = QsonPage::VERSION_TYPE;
        result[1] = 0;
        quint32 updateCount = parts.at(0).toUInt();
        memcpy(result.data() + 2, &updateCount, sizeof(quint32));
        result.append(QByteArray::fromHex(parts.at(1).toLatin1()).leftJustified(16, 0, true));
    } else {
        result.fill(0, 22);
    }

    return QsonVersion(result.constData());
}

bool QsonVersion::operator<(const QsonVersion& other) const
{
    if (updateCount() < other.updateCount()) {
        return true;
    }
    return mContent < other.mContent;
}

bool QsonVersion::operator==(const QsonVersion& other) const
{
    return mContent == other.mContent;
}

bool QsonVersion::operator!=(const QsonVersion& other) const
{
    return mContent != other.mContent;
}

bool QsonVersion::isValid() const
{
    return mIsValid;
}

quint32 QsonVersion::updateCount() const
{
    quint32 *count = (quint32*) (mContent.constData() + 2);
    return *count;
}

void QsonVersion::setUpdateCount(quint32 newCount)
{
    quint32 *count = (quint32*) (mContent.constData() + 2);
    *count = newCount;
}

QByteArray QsonVersion::hash() const
{
    return mContent.mid(2+sizeof(quint32));
}

QString QsonVersion::toString() const
{
    if (mIsValid)
        return QString::number(updateCount()) % QLatin1Char('-') % QString::fromLatin1(hash().toHex());
    return QString();
}

uint qHash(const QsonVersion &version)
{
    return qHash(version.content());
}

} } // end namespace QtAddOn::JsonDb
