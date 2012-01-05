/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "qsonuuid_p.h"

#include <QString>
#include <QByteArray>
#include <QCryptographicHash>

QT_BEGIN_NAMESPACE_JSONDB

qson_uuid_t QsonUuidNs = {
    0x6ba7b811,
    0x9dad,
    0x11d1,
    0x80,
    0xb4,
    {0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8}
};

QByteArray QsonUUIDv3(const QString &source) {
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData((char *) &QsonUuidNs, sizeof(QsonUuidNs));
    md5.addData((char *) source.constData(), source.size() * 2);
    
    QByteArray result = md5.result();
    
    qson_uuid_t *uuid = (qson_uuid_t*) result.data();
    uuid->time_hi_and_version &= 0x0FFF;
    uuid->time_hi_and_version |= (3 << 12);
    uuid->clock_seq_hi_and_reserved &= 0x3F;
    uuid->clock_seq_hi_and_reserved |= 0x80;
        
    return result;
}

QT_END_NAMESPACE_JSONDB
