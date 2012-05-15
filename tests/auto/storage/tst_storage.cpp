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

#include <QCoreApplication>
#include <QList>
#include <QTest>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QEventLoop>
#include <QDebug>
#include <QLocalSocket>
#include <QTimer>
#include <QDir>
#include <QJsonArray>
#include <QStringList>
#include <QString>

#include "qjsondbconnection.h"
#include "qjsondbobject.h"
#include "qjsondbreadrequest.h"
#include "qjsondbwriterequest.h"
#include "testhelper.h"

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#endif

// To get env variables
#include <stdlib.h>
#include <sys/statvfs.h>

Q_DECLARE_METATYPE(QUuid)

QT_USE_NAMESPACE_JSONDB

class StorageTest : public TestHelper
{
    Q_OBJECT
    QString m_path;
    qint64 m_availableSpace;
    qint64 m_pid;
public:
    StorageTest();
    void checkAvailableSpace();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void outOfSpace();

private:
    inline bool sendWaitTake(QJsonDbRequest *request, QList<QJsonObject> *results = 0)
    {
        Q_ASSERT(mConnection && request);
        if (!mConnection->send(request))
            return false;
        if (!waitForResponse(request))
            return false;
        QList<QJsonObject> res = request->takeResults();
        if (results)
            *results = res;
        return true;
    }
};

StorageTest::StorageTest() :
    m_availableSpace(-1),
    m_pid(-1)
{
    const char *envVarName = "QT_TEST_SMALL_FS";
    char *envVar = 0;
    envVar = getenv(envVarName);
    if (envVar) {
        m_path = QString::fromLocal8Bit(envVar);
        if (!m_path.endsWith('/'))
            m_path += '/';
    }
}

void StorageTest::initTestCase()
{
    if (!m_path.isEmpty()) {
        // Write the partitions.json file.
        QFile file("partitions.json");
        file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        QTextStream stream(&file);
        QLatin1String beginning("[ { \"name\": \"");
        QLatin1String ending("test-jsondb-storage\", \"default\": true } ]");
        stream << beginning;
        stream << m_path;
        stream << ending;
        file.close();
    } else
        return;
    QStringList arg_list = QStringList();
    m_pid = launchJsonDbDaemonDetached(arg_list, __FILE__);
}

void StorageTest::cleanupTestCase()
{
    if (-1 == m_pid)
        return;
    // Stop the daemon
    ::kill(m_pid, SIGTERM);
    // Make sure we leave the directory as clean as when we found it!
    QFile::remove("partitions.json");
    if (!m_path.isEmpty()) {
        QDir dir(m_path);
        QStringList files = dir.entryList();
        foreach (QString file, files) {
            QFile::remove(m_path + file);
        }
    }
}

void StorageTest::init()
{
    connectToServer();
}

void StorageTest::cleanup()
{
    disconnectFromServer();
}

/*
 * This method creates a file on the file system and then
 * calls statvfs. The reason for creating a file is because
 * we need a file on the file system to be able to use statvfs.
 */
void StorageTest::checkAvailableSpace()
{
    QString filePath = m_path + "test";
    QFile file(filePath);
    if (file.open(QIODevice::ReadWrite)) {
        QDataStream stream(&file);
        stream << m_path;
        file.close();
        struct statvfs vfs;
        QByteArray local8BitPath = filePath.toLocal8Bit();
        const char *path = local8BitPath.constData();
        int error = statvfs(path, &vfs);
        if (error < 0)
            return;
        m_availableSpace = vfs.f_bsize * vfs.f_bavail;
    }
}

/*
 *  This test starts the database in $QT_TEST_SMALL_FS and the starts populating
 * it. Once we run out of space, we issue some queries to check if it is still
 * responding or not.
 */
void StorageTest::outOfSpace()
{
    QVERIFY(mConnection);
    if (m_path.isEmpty()) {
        QSKIP("This test requires the env variable QT_TEST_SMALL_FS");
        QWARN("QT_TEST_SMALL_FS must point to a mounted filesystem which is 2MB in size. If not, this test will fail.");
    }
    checkAvailableSpace();
    QVERIFY2(m_availableSpace > 0, "File system size is <= 0!");
    // Now we populate the db until we run out of space or 2000 elements
    // 2000 elements ~ 3.6 Mb, which is almost as double as our FS.
    int elements = 0;
    while ((m_availableSpace > 0) && (elements < 2000)) {
        QJsonDbWriteRequest request;
        QJsonDbObject item;
        item.setUuid(QJsonDbObject::createUuid());
        item.insert("_type", QLatin1String("com.test.storage-test"));
        item.insert("create-test", elements);

        // Create an item
        request.setObjects(QList<QJsonObject>() << item);
        QList<QJsonObject> results;
        QVERIFY(sendWaitTake(&request, &results));
        QCOMPARE(results.size(), 1);
        QVERIFY(results[0].contains("_uuid"));
        QVERIFY(results[0].contains("_version"));

        // Finally update the counters
        checkAvailableSpace();
        elements++;
    }
    // Fail if we didn't run out of space or if we wrote more than 1000 elements
    QVERIFY(m_availableSpace <= 0);
    QVERIFY(elements < 2000);
    // Check if the server is still responding
    QVERIFY(mConnection);
    {
        QJsonDbReadRequest query;
        QList<QJsonObject> results;

        query.setQuery("[?_type=\"com.test.storage-test\"]");
        QVERIFY(sendWaitTake(&query, &results));
        QCOMPARE(results.size(), elements);
    }
}

QTEST_MAIN(StorageTest)

#include "tst_storage.moc"
