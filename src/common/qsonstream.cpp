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

#include "qsonstream.h"
#include <QDebug>
#include <QDataStream>
#include <QLocalSocket>
#include <QAbstractSocket>
#include <QtEndian>

#ifdef QT_DEBUG
#include <QUdpSocket>
#include <QDateTime>
#include <QCoreApplication>
#endif

QT_ADDON_JSONDB_BEGIN_NAMESPACE

QsonStream::QsonStream(QIODevice *device, QObject *parent) :
    QObject(parent),
    mDevice(0)
{
    setDevice(device);
}

QIODevice *QsonStream::device() const
{
    return mDevice;
}

/** Set the device used by the QsonStream.
    The stream does not take ownership of the device.
*/
void QsonStream::setDevice(QIODevice *device)
{
    if (mDevice) {
        disconnect(mDevice, SIGNAL(readyRead()), this, SLOT(deviceReadyRead()));
        disconnect(mDevice, SIGNAL(bytesWritten(qint64)), this, SLOT(deviceBytesWritten(qint64)));
        disconnect(mDevice, SIGNAL(aboutToClose()), this, SIGNAL(aboutToClose()));
    }
    mDevice = device;
    if (mDevice) {
        connect(mDevice, SIGNAL(readyRead()), this, SLOT(deviceReadyRead()));
        connect(mDevice, SIGNAL(bytesWritten(qint64)), this, SLOT(deviceBytesWritten(qint64)));
        connect(mDevice, SIGNAL(aboutToClose()), this, SIGNAL(aboutToClose()));
    }
}

bool QsonStream::send(const QsonObject &qson)
{
    sendQsonPage(*qson.mHeader);
    foreach (const QsonPagePtr body, qson.mBody)
        sendQsonPage(*body);
    sendQsonPage(*qson.mFooter);
    return true;
}

void QsonStream::sendQsonPage(const QsonPage &page)
{
    int shouldWrite = page.dataSize();
    if (mWriteBuffer.isEmpty()) {
        int didWrite = mDevice->write(page.constData(), shouldWrite);
        if (didWrite < 0) {
            qWarning() << "Error writing to socket" << mDevice->errorString();
        } else if (didWrite < shouldWrite) {
            mWriteBuffer.append(page.constData() + didWrite, shouldWrite - didWrite);
        }
        QLocalSocket *s = qobject_cast<QLocalSocket *>(mDevice);
        if (s)
            s->flush();
    } else {
        qWarning() << "Buffering, slow down your writes";
        mWriteBuffer.append(page.constData(), shouldWrite);
    }
}

void QsonStream::deviceReadyRead()
{
    mReadBuffer.resize(0xFFFF);
    while (!mDevice->atEnd()) {
        int bytesRead = mDevice->read(mReadBuffer.data(), mReadBuffer.size());
        if (bytesRead < 0) {
            qWarning() << "Error reading from socket" << mDevice->errorString();
            continue;
        }
        mReadBuffer.truncate(bytesRead);
        mParser.append(mReadBuffer);

        while (mParser.isObjectReady()) {
            QsonObject qson = mParser.getObject();
            receive(qson);
        }

        if (mParser.hasError()) {
            qWarning() << "Parser error, trying to recover!";
            // TODO: Improve!
            mParser = QsonParser();
        }
    }
}

void QsonStream::deviceBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);
    if (!mWriteBuffer.isEmpty()) {
        int didWrite = mDevice->write(mWriteBuffer);
        mWriteBuffer = mWriteBuffer.mid(didWrite);
    }
}

QsonStream& operator<<(QsonStream& s, const QsonObject& map)
{
    s.send(map);
    return s;
}

#include "moc_qsonstream.cpp"

QT_ADDON_JSONDB_END_NAMESPACE
