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

#include "jsondbsettings.h"

#include <QDebug>
#include <QMetaObject>
#include <QMetaProperty>

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

Q_GLOBAL_STATIC(JsonDbSettings, staticInstance)

JsonDbSettings::JsonDbSettings() :
    mRejectStaleUpdates(false)
  , mDebug(false)
  , mVerbose(false)
  , mPerformanceLog(false)
  , mCacheSize(128)
  , mCompactRate(1000)
  , mEnforceAccessControl(false)
  , mTransactionSize(100)
  , mValidateSchemas(false)
  , mSoftValidation(false)
  , mSyncInterval(5000)
  , mIndexSyncInterval(12000)
  , mDebugQuery(false)
  , mIndexFieldValueSize(512) // Should be even and no bigger than maxBtreeKeySize - 20 (the 20 for uuid + index type data)
{
    loadEnvironment();
}

void JsonDbSettings::loadEnvironment()
{
    // loop through all the properties, converting from the property
    // name to the environment variable name
    const QMetaObject *meta = metaObject();

    for (int i = 0; i < meta->propertyCount(); i++) {
        QMetaProperty property(meta->property(i));
        QString propertyName = QString::fromLatin1(property.name());
        QString envVariable(QStringLiteral("JSONDB_"));

        for (int j = 0; j < propertyName.count(); j++) {
            if (j > 0 && propertyName.at(j).isUpper() && !propertyName.at(j - 1).isUpper())
                envVariable += QLatin1Char('_') + propertyName.at(j);
            else
                envVariable += propertyName.at(j).toUpper();
        }

        QByteArray value = qgetenv(envVariable.toLatin1());
        if (value.size()) {
            switch (property.type()) {
            case QVariant::Bool:
                property.write(this, value == "true");
                break;
            case QVariant::Int:
                property.write(this, value.toInt());
                break;
            case QVariant::String:
                property.write(this, QString::fromLatin1(value));
                break;
            case QVariant::StringList:
                property.write(this, QString::fromLatin1(value).split(QLatin1Char(':')));
                break;
            default:
                qWarning() << "JsonDbSettings: unknown property type" << property.name() << property.type();
            }
        }
    }
}

JsonDbSettings *JsonDbSettings::instance()
{
    return staticInstance();
}

#include "moc_jsondbsettings.cpp"

QT_END_NAMESPACE_JSONDB_PARTITION
