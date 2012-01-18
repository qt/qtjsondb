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

#ifndef JSONDB_WRITE_REQUEST_H
#define JSONDB_WRITE_REQUEST_H

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <QtJsonDb/qjsondbrequest.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbWriteRequestPrivate;
class Q_JSONDB_EXPORT QJsonDbWriteRequest : public QJsonDbRequest
{
    Q_OBJECT
    Q_PROPERTY(QList<QJsonObject> objects READ objects WRITE setObjects)

    Q_PROPERTY(quint32 stateNumber READ stateNumber)

public:
    QJsonDbWriteRequest(QObject *parent = 0);
    ~QJsonDbWriteRequest();

    enum ErrorCode {
        NoError = QJsonDbRequest::NoError,
        MissingObject = QJsonDbRequest::MissingObject
    };

    void setObjects(const QList<QJsonObject> &);
    QList<QJsonObject> objects() const;

    // read request results. Data is only available after started() was emitted.
    quint32 stateNumber() const;

private:
    Q_DISABLE_COPY(QJsonDbWriteRequest)
    Q_DECLARE_PRIVATE(QJsonDbWriteRequest)
};

class Q_JSONDB_EXPORT QJsonDbCreateRequest : public QJsonDbWriteRequest
{
public:
    QJsonDbCreateRequest(const QJsonObject &object, QObject *parent = 0);
    QJsonDbCreateRequest(const QList<QJsonObject> &objects, QObject *parent = 0);
};

class Q_JSONDB_EXPORT QJsonDbUpdateRequest : public QJsonDbWriteRequest
{
public:
    QJsonDbUpdateRequest(const QJsonObject &object, QObject *parent = 0);
    QJsonDbUpdateRequest(const QList<QJsonObject> &objects, QObject *parent = 0);
};

class Q_JSONDB_EXPORT QJsonDbRemoveRequest : public QJsonDbWriteRequest
{
public:
    QJsonDbRemoveRequest(const QJsonObject &object, QObject *parent = 0);
    QJsonDbRemoveRequest(const QList<QJsonObject> &objects, QObject *parent = 0);
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif // JSONDB_WRITE_REQUEST_H
