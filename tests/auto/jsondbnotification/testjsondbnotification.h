/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef TESTJSONDBNOTIFICATION_H
#define TESTJSONDBNOTIFICATION_H

#include <QAbstractItemModel>
#include "requestwrapper.h"
#include "qmltestutil.h"

QT_USE_NAMESPACE_JSONDB

struct CallbackData {
    int action;
    int stateNumber;
    QVariantMap result;
};

class TestJsonDbNotification: public RequestWrapper
{
    Q_OBJECT
public:
    TestJsonDbNotification();
    ~TestJsonDbNotification();

    // Tricking moc to accept the signal in JsonDbNotify.Action
    Q_ENUMS(Actions)
    enum Actions { Create = 1, Update = 2, Remove = 4 };

    void deleteDbFiles();
private slots:
    void initTestCase();
    void cleanupTestCase();

    void singleObjectNotifications();
    void multipleObjectNotifications();
    void createNotification();

public slots:
    void notificationSlot(QVariant result, int action, int stateNumber);
    void errorSlot(int code, const QString &message);
    void notificationSlot2(QJSValue result, Actions action, int stateNumber);
    void statusChangedSlot2();

public:
    void timeout();

private:
    ComponentData *createComponent();
    ComponentData *createPartitionComponent();
    void deleteComponent(ComponentData *componentData);

private:
    QProcess *mProcess;
    QStringList mNotificationsReceived;
    QList<ComponentData*> mComponents;
    QString mPluginPath;

    // Response values
    bool mTimedOut;
    QList<CallbackData> callbackData;
};

#endif
