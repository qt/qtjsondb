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

#include "jsondbscriptengine.h"
#include "jsondbsettings.h"
#include <QtJsonDbPartition/jsondbpartitionglobal.h>
#include <QtTest/QtTest>

QT_USE_NAMESPACE_JSONDB_PARTITION

class TestJsonDbScriptEngine: public QObject
{
    Q_OBJECT
private slots:
    void injectScript();
};

void TestJsonDbScriptEngine::injectScript()
{
    qputenv("JSONDB_INJECTION_SCRIPT", "");
    QJSEngine *engine = JsonDbScriptEngine::scriptEngine();
    QJSValue obj = engine->evaluate("isEqual(3, 3);");
    QVERIFY(obj.isError());
    JsonDbScriptEngine::releaseScriptEngine();

    qputenv("JSONDB_INJECTION_SCRIPT", QFINDTESTDATA("test_inject.js").toLocal8Bit());
    jsondbSettings->reload();
    engine = JsonDbScriptEngine::scriptEngine();
    obj = engine->evaluate("isEqual(3, 3);");
    QCOMPARE(obj.toBool(), true);
    obj = engine->evaluate("isEqual(3, 0);");
    QCOMPARE(obj.toBool(), false);
}

QTEST_MAIN(TestJsonDbScriptEngine)

#include "test-jsondbscriptengine.moc"
