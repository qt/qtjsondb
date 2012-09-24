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

#include "jsondbscriptengine.h"

#include <QJSValue>
#include <QJSEngine>
#include <QDebug>
#include <QFile>

#include "jsondbsettings.h"
#include "jsondbproxy.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

static QJSEngine *sScriptEngine;

static void injectScript()
{
    QString fileName =jsondbSettings->injectionScript();
    if (fileName.isEmpty())
        return;

    QFile scriptFile(fileName);
    if (!scriptFile.exists()) {
        if (jsondbSettings->verbose())
            qDebug() << "QtJsonDb::Partition::injectScript file does not exist:" << fileName;
        return;
    }

    if (!scriptFile.open(QIODevice::ReadOnly)) {
        if (jsondbSettings->verbose())
            qDebug() << "QtJsonDb::Partition::injectScript can't open file:" << fileName;
        return;
    }
    QString contents = QString::fromUtf8(scriptFile.readAll());
    scriptFile.close();

    QJSValue result = sScriptEngine->evaluate(contents, fileName);
    if (result.isError() && jsondbSettings->verbose())
        qDebug() << "QtJsonDb::Partition::injectScript error evaluating script:" << fileName;
}

QJSEngine *JsonDbScriptEngine::scriptEngine()
{
    if (!sScriptEngine) {
        if (jsondbSettings->useStrictMode()) {
            // require 'use strict';
            QByteArray v8Args = qgetenv("V8ARGS");
            v8Args.append(" --use_strict");
            qputenv("V8ARGS", v8Args);
        }
        sScriptEngine = new QJSEngine();
        QJSValue globalObject = sScriptEngine->globalObject();
        globalObject.setProperty(QStringLiteral("console"), sScriptEngine->newQObject(new Console()));
        injectScript();
    }
    return sScriptEngine;
}

void JsonDbScriptEngine::releaseScriptEngine()
{
    if (jsondbSettings->verbose())
        qDebug() << "JsonDbScriptEngine::releaseScriptEngine";
    delete sScriptEngine;
    sScriptEngine = 0;
}

QT_END_NAMESPACE_JSONDB_PARTITION
