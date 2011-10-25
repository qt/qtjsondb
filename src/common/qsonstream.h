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

#ifndef QSONSTREAM_H
#define QSONSTREAM_H

#include <QIODevice>
#include <QtJsonDbQson/private/qson_p.h>
#include <QtJsonDbQson/private/qsonparser_p.h>

namespace QtAddOn { namespace JsonDb {

class QsonStream : public QObject
{
    Q_OBJECT
public:
    explicit QsonStream(QIODevice *device = 0, QObject *parent = 0);

    QIODevice *device() const;
    void       setDevice(QIODevice *device);

    bool send(const QsonObject &data);

signals:
    void receive(const QsonObject &data);
    void aboutToClose();
    void readyWrite();

protected slots:
    void deviceReadyRead();
    void deviceBytesWritten(qint64 bytes);

private:
    void sendQsonPage(const QsonPage &page);

    QIODevice *mDevice;
    QByteArray mWriteBuffer;
    QsonParser mParser;
    QByteArray mReadBuffer;

};

QsonStream& operator<<( QsonStream&, const QsonObject& );

} } // end namespace QtAddOn::JsonDb

#endif // QSONSTREAM_H
