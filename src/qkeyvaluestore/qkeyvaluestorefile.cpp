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

#include "qkeyvaluestorefile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

QKeyValueStoreFile::QKeyValueStoreFile(QString name, bool truncate)
{
    p = new QKeyValueStoreFilePrivate(name, truncate);
}

QKeyValueStoreFile::~QKeyValueStoreFile()
{
    delete p;
}

bool QKeyValueStoreFile::open()
{
    return p->open();
}

int QKeyValueStoreFile::read(void *buffer, quint32 count)
{
    return p->read(buffer, count);
}

int QKeyValueStoreFile::write(void *buffer, quint32 count)
{
    return p->write(buffer, count);
}

void QKeyValueStoreFile::sync()
{
    p->sync();
}

qint64 QKeyValueStoreFile::size() const
{
    return p->size();
}

bool QKeyValueStoreFile::close()
{
    return p->close();
}

/*
 * PRIVATE API BELOW!!!!
 */

int doOpen(const char *name, bool truncate)
{
    if (truncate)
        return open(name,
                    O_RDWR | O_CREAT | O_TRUNC,
                    S_IRWXU |S_IRGRP );
    return open(name,
                O_RDWR | O_CREAT,
                S_IRWXU |S_IRGRP );
}

int doClose(int fd)
{
    return close(fd);
}

QKeyValueStoreFilePrivate::QKeyValueStoreFilePrivate(QString name, bool truncate) :
    m_name(name)
{
    m_fd = -1;
    m_size = 0;
    m_truncate = truncate;
    m_offset = 0;
}

QKeyValueStoreFilePrivate::~QKeyValueStoreFilePrivate()
{
    if (m_fd >= 0)
        doClose(m_fd);
}

bool QKeyValueStoreFilePrivate::open()
{
    m_fd = doOpen(m_name.toLocal8Bit().constData(), m_truncate);
    if (m_fd < 0)
        return false;
    struct stat s;
    int result = fstat(m_fd, &s);
    if (result < 0) {
        doClose(m_fd);
        return false;
    }
    m_size = s.st_size;
    return true;
}

int QKeyValueStoreFilePrivate::read(void *buffer, quint32 count)
{
    if (!buffer || m_fd < 0)
        return -1;
    ssize_t s = pread(m_fd, buffer, count, m_offset);
    if (s < 0)
        return s;
    m_offset += s;
    return s;
}

int QKeyValueStoreFilePrivate::write(void *buffer, quint32 count)
{
    if (!buffer || m_fd < 0)
        return -1;
    ssize_t s = pwrite(m_fd, buffer, count, m_size);
    if (s < 0)
        return s;
    // The write was successful, update the counters.
    m_size += s;
    return s;
}

void QKeyValueStoreFilePrivate::sync()
{
    if (m_fd < 0)
        return;
    fsync(m_fd);
}

qint64 QKeyValueStoreFilePrivate::size()
{
    return m_size;
}

bool QKeyValueStoreFilePrivate::close()
{
    int result = doClose(m_fd);
    if (result < 0)
        return false;
    m_fd = -1;
    return true;
}
