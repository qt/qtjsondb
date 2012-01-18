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

#include "qjsondbauthrequest_p.h"
#include "qjsondbstrings_p.h"

#include <QJsonArray>

QT_BEGIN_NAMESPACE_JSONDB

class QJsonDbAuthRequestPrivate : public QJsonDbRequestPrivate
{
    Q_DECLARE_PUBLIC(QJsonDbAuthRequest)
public:
    QJsonDbAuthRequestPrivate(QJsonDbAuthRequest *q);

    QJsonObject getRequest() const;
    void handleResponse(const QJsonObject &);
    void handleError(int, const QString &);

    QByteArray token;
};

QJsonDbAuthRequestPrivate::QJsonDbAuthRequestPrivate(QJsonDbAuthRequest *q)
    : QJsonDbRequestPrivate(q)
{
}

QJsonDbAuthRequest::QJsonDbAuthRequest(QObject *parent)
    : QJsonDbRequest(new QJsonDbAuthRequestPrivate(this), parent)
{
}

QJsonDbAuthRequest::~QJsonDbAuthRequest()
{
}

QByteArray QJsonDbAuthRequest::token() const
{
    Q_D(const QJsonDbAuthRequest);
    return d->token;
}

void QJsonDbAuthRequest::setToken(const QByteArray &token)
{
    Q_D(QJsonDbAuthRequest);
    JSONDB_CHECK_REQUEST_STATUS;
    d->token = token;
}

QJsonObject QJsonDbAuthRequestPrivate::getRequest() const
{
    QJsonObject request;
    request.insert(JsonDbStrings::Protocol::action(), JsonDbStrings::Protocol::token());
    request.insert(JsonDbStrings::Protocol::object(), QString::fromLatin1(token));
    request.insert(JsonDbStrings::Protocol::requestId(), requestId);
    return request;
}

void QJsonDbAuthRequestPrivate::handleResponse(const QJsonObject &response)
{
    Q_Q(QJsonDbAuthRequest);
    Q_UNUSED(response);
    setStatus(QJsonDbRequest::Receiving);
    emit q->started();
    setStatus(QJsonDbRequest::Finished);
    emit q->finished();
}

void QJsonDbAuthRequestPrivate::handleError(int code, const QString &message)
{
    Q_Q(QJsonDbAuthRequest);
    setStatus(QJsonDbRequest::Error);
    emit q->error(QJsonDbRequest::ErrorCode(code), message);
}

#include "moc_qjsondbauthrequest_p.cpp"

QT_END_NAMESPACE_JSONDB
