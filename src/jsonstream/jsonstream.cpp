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

#include "jsonstream.h"

#include <QDebug>
#include <QDataStream>
#include <QLocalSocket>
#include <QAbstractSocket>
#include <QtEndian>
#include <QJsonDocument>

#ifdef QT_DEBUG
#include <QUdpSocket>
#include <QDateTime>
#include <QCoreApplication>
#endif

QT_BEGIN_NAMESPACE

namespace QtJsonDbJsonStream {

JsonStream::JsonStream(QObject *parent)
    : QObject(parent), mDevice(0)
{
}

QIODevice *JsonStream::device() const
{
    return mDevice;
}

/** Set the device used by the JsonStream.
    The stream does not take ownership of the device.
*/
void JsonStream::setDevice(QIODevice *device)
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

bool JsonStream::send(const QJsonObject &object)
{
    Q_ASSERT(mDevice != 0);
    QByteArray data = QJsonDocument(object).toBinaryData();
    int shouldWrite = data.size();
    if (mWriteBuffer.isEmpty()) {
        int didWrite = mDevice->write(data);
        if (didWrite < 0) {
            qWarning() << "Error writing to socket" << mDevice->errorString();
        } else if (didWrite < shouldWrite) {
            mWriteBuffer = data.mid(didWrite);
        }
        QLocalSocket *s = qobject_cast<QLocalSocket *>(mDevice);
        if (s)
            s->flush();
    } else {
        qWarning() << "Buffering, slow down your writes";
        mWriteBuffer.append(data);
    }
    return true;
}

void JsonStream::deviceReadyRead()
{
    struct JsonHeader
    {
        struct Header
        {
            unsigned int tag;
            unsigned int version;
        } h;
        struct Base
        {
            unsigned int size;
        } b;
    };

    while (!mDevice->atEnd()) {
        int bytesAvailable = mDevice->bytesAvailable();
        int offset = mReadBuffer.size();
        mReadBuffer.resize(offset + bytesAvailable);
        int bytesRead = mDevice->read(mReadBuffer.data()+offset, bytesAvailable);
        if (bytesRead < 0) {
            qWarning() << "Error reading from socket" << mDevice->errorString();
            continue;
        }
        while (mReadBuffer.size() > static_cast<int>(sizeof(JsonHeader))) {
            JsonHeader header;
            memcpy(&header, mReadBuffer.constData(), sizeof(header));
            if (header.h.tag != QJsonDocument::BinaryFormatTag || header.h.version != qToLittleEndian(1u)) {
                mReadBuffer.clear();
                qWarning() << "Got invalid binary json data";
                break;
            }
            QJsonDocument doc(QJsonDocument::fromBinaryData(mReadBuffer, QJsonDocument::Validate));
            if (doc.isEmpty())
                break;
            receive(doc.object());
            mReadBuffer.remove(0, sizeof(header.h) + header.b.size);
        }
    }
}

void JsonStream::deviceBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);
    if (!mWriteBuffer.isEmpty()) {
        int didWrite = mDevice->write(mWriteBuffer);
        mWriteBuffer = mWriteBuffer.mid(didWrite);
    }
}

} // namespace QtJsonDbJsonStream

QT_END_NAMESPACE

#include "moc_jsonstream.cpp"
