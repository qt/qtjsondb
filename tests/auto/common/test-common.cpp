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

#include <QtTest/QtTest>

#include <QtJsonDbQson/private/qson_p.h>
#include <QtJsonDbQson/private/qsonpage_p.h>
#include <QtJsonDbQson/private/qsonversion_p.h>
#include <QtJsonDbQson/private/qsonparser_p.h>

#include "qsonconversion.h"

#include <QUuid>
#include <QDebug>
#include <QJSEngine>
#include <QMap>

#include <json.h>

using namespace QtAddOn::JsonDb;

namespace QTest {
template<>
bool qCompare<qint64, int>(qint64 const &t1, int const &t2,
                           const char *actual, const char *expected,
                           const char *file, int line)
{
    return QTest::qCompare(t1, (qint64)t2, actual, expected, file, line);
}
template<>
bool qCompare<quint64, unsigned int>(quint64 const &t1, unsigned int const &t2,
                            const char *actual, const char *expected,
                            const char *file, int line)
{
    return QTest::qCompare(t1, (quint64)t2, actual, expected, file, line);
}
}

class TestCommon: public QObject
{
    Q_OBJECT


private slots:
    void testQsonDocumentHeaderAndFooterPage();
    void testQsonMap();
    void testQsonMapAttributes();
    void testQsonSubMap();
    void testQsonList();
    void testQsonSubList();
    void testMeta();
    void implicitSharing();
    void testQsonVersion();
    void testQsonVersionLiteral();
    void testVersionList();
    void testMerging();
    void testRecreate();
    void testQsonElement();
    void testEquals();
    void testQsonParser();
    void testQsonParserStreaming();
    void testQsonParseDocument();
    void testComplexElement();
    void testInsertEmptyObject();
    void testIsEmpty();
    void testInsertUuid();
    void testQsonToJSValue();
    void testRemove();
    void testDoubleInsert();

private:
    void validateHeader(const char* qson, QsonPage::PageType type, int size)
    {
        qson_size *actualSize;
        QCOMPARE(*(qson+0), 'Q');
        switch (type) {
        case QsonPage::KEY_VALUE_PAGE:
        case QsonPage::ARRAY_VALUE_PAGE:
            QCOMPARE(*(qson+1), (char) type);
            actualSize = (qson_size*) (qson + 2);
            QCOMPARE(*actualSize, (qson_size) size);
            break;
        default:
            QCOMPARE(*(qson+1), 'S');
            QCOMPARE(*(qson+2), 'N');
            QCOMPARE(*(qson+3), (char) type);
        }

    }

    int qsizeAt(const QByteArray &bytes, int pos)
    {
        qson_size *value = (qson_size*) (bytes.constData() + pos);
        return (int) *value;
    }

    qint32 int32At(const QByteArray &bytes, int pos)
    {
        qint32 *value = (qint32*) (bytes.constData() + pos);
        return *value;
    }

    quint32 uint32At(const QByteArray &bytes, int pos)
    {
        quint32 *value = (quint32*) (bytes.constData() + pos);
        return *value;
    }

    QString stringAt(const QByteArray &bytes, int pos)
    {
        int size = qsizeAt(bytes, pos);
        QString result((QChar *) (bytes.constData() + pos + 2), size / 2);
        return result;
    }

    double doubleAt(const QByteArray &bytes, int pos)
    {
        double *value = (double*) (bytes.constData() + pos);
        return (double) *value;
    }

    bool typeAt(const QByteArray &bytes, int pos, QsonPage::DataType type)
    {
        char stored = bytes.constData()[pos];
        char sanity = bytes.constData()[pos + 1];

        if (stored == type && sanity == 0) {
            return true;
        } else {
            qDebug() << "at" << pos << ": expected" << (quint8) type << ", found" << (quint8) stored;
            qDebug() << "sanity bytes is" << (quint8) sanity;
            return false;
        }
    }
};

void TestCommon::testQsonDocumentHeaderAndFooterPage()
{
    QsonPage documentHeader(QsonPage::DOCUMENT_HEADER_PAGE);
    validateHeader(documentHeader.data(), QsonPage::DOCUMENT_HEADER_PAGE, 44);
    QCOMPARE(documentHeader.data()[4], (char) QsonPage::VERSION_TYPE);
    QCOMPARE(documentHeader.data()[5], '\x00');
    QCOMPARE(documentHeader.data()[26], (char) QsonPage::UUID_TYPE);
    QCOMPARE(documentHeader.data()[27], '\x00');
    QCOMPARE(documentHeader.dataSize(), 44);

    QsonPage documentFooter(QsonPage::DOCUMENT_FOOTER_PAGE);
    validateHeader(documentFooter.data(), QsonPage::DOCUMENT_FOOTER_PAGE, 26);
    QCOMPARE(documentHeader.data()[4], (char) QsonPage::VERSION_TYPE);
    QCOMPARE(documentHeader.data()[5], '\x00');
    QCOMPARE(documentFooter.dataSize(), 26);

    QsonPage listHeader(QsonPage::LIST_HEADER_PAGE);
    validateHeader(listHeader.data(), QsonPage::LIST_HEADER_PAGE, 4);
    QCOMPARE(listHeader.dataSize(), 4);

    QsonPage listFooter(QsonPage::LIST_FOOTER_PAGE);
    validateHeader(listFooter.data(), QsonPage::LIST_FOOTER_PAGE, 4);
    QCOMPARE(listFooter.dataSize(), 4);
}

void TestCommon::testQsonMap()
{
    QsonMap qson;
    qson.insert(QString("hello"), QString("world"));

    QByteArray bytes = qson.data();

    QCOMPARE(qson.dataSize(), 40);
    validateHeader(bytes.constData(), QsonPage::OBJECT_HEADER_PAGE, 4);
    validateHeader(bytes.constData() + 4, QsonPage::KEY_VALUE_PAGE, 32);
    QVERIFY(typeAt(bytes, 8, QsonPage::KEY_TYPE));
    QCOMPARE(stringAt(bytes, 10), QString("hello"));
    QVERIFY(typeAt(bytes, 22, QsonPage::STRING_TYPE));
    QCOMPARE(stringAt(bytes, 24), QString("world"));
    validateHeader(bytes.constData() + 36, QsonPage::OBJECT_FOOTER_PAGE, 4);

    QStringList keys = qson.keys();
    QCOMPARE(keys.size(), 1);
    QCOMPARE(keys[0], QString("hello"));

    QCOMPARE(qson.valueString("hello"), QString("world"));

    qson.insert(QLatin1String("hello"), QLatin1String("foobar"));
    QCOMPARE(qson.keys().size(), 1);
    QCOMPARE(qson.keys().at(0), QLatin1String("hello"));
    QCOMPARE(qson.valueString("hello"), QString("foobar"));
}

void TestCommon::testQsonMapAttributes()
{
    // insert each type
    // read it immediately to make sure we can read at the end of a page

    QsonMap qson;

    qson.insert("null", QsonObject::NullValue);
    QVERIFY(qson.contains("null"));
    QCOMPARE(qson.valueType("null"), QsonObject::NullType);
    QCOMPARE(qson.isNull("null"), true);

    qson.insert("false", false);
    QVERIFY(qson.contains("false"));
    QCOMPARE(qson.valueType("false"), QsonObject::BoolType);
    QCOMPARE(qson.valueBool("false"), false);

    qson.insert("true", true);
    QVERIFY(qson.contains("true"));
    QCOMPARE(qson.valueType("true"), QsonObject::BoolType);
    QCOMPARE(qson.valueBool("true"), true);

    qson.insert("uint", (quint32) 1);
    QVERIFY(qson.contains("uint"));
    QCOMPARE(qson.valueType("uint"), QsonObject::UIntType);
    QCOMPARE(qson.valueUInt("uint"), (quint32) 1);

    qson.insert("int", (qint32) 2);
    QVERIFY(qson.contains("int"));
    QCOMPARE(qson.valueType("int"), QsonObject::IntType);
    QCOMPARE(qson.valueInt("int"), (qint32) 2);

    qson.insert("int64", (qint64) 1234567890123456789ll);
    QVERIFY(qson.contains("int64"));
    QCOMPARE(qson.valueType("int64"), QsonObject::IntType);
    QCOMPARE(qson.valueInt("int64"), (qint64) 1234567890123456789ll);

    qson.insert("uint64", (quint64) 18446744073709551615ull);
    QVERIFY(qson.contains("uint64"));
    QCOMPARE(qson.valueType("uint64"), QsonObject::UIntType);
    QCOMPARE(qson.valueUInt("uint64"), (quint64) 18446744073709551615ull);

    qson.insert("double", 1.2);
    QVERIFY(qson.contains("double"));
    QCOMPARE(qson.valueType("double"), QsonObject::DoubleType);
    QCOMPARE(qson.valueDouble("double"), 1.2);

    qson.insert(QLatin1String("char"), QLatin1String("value"));
    QVERIFY(qson.contains("char"));
    QCOMPARE(qson.valueType("char"), QsonObject::StringType);
    QCOMPARE(qson.valueString("char"), QString("value"));

    qson.insert(QLatin1String("string"), QLatin1String("qvalue"));
    QVERIFY(qson.contains("string"));
    QCOMPARE(qson.valueType("string"), QsonObject::StringType);
    QCOMPARE(qson.valueString("string"), QString("qvalue"));
}

void TestCommon::testQsonSubMap()
{
    QsonMap child;
    child.insert(QLatin1String("hello"), QLatin1String("world"));

    QsonMap parent;
    parent.insert("before", false);
    parent.insert("child", child);
    parent.insert("after", true);

    QsonMap readChild = parent.subObject("child");
    QCOMPARE(readChild.keys().length(), 1);
    QVERIFY(readChild.contains("hello"));
    QCOMPARE(readChild.valueString("hello"), QString("world"));
}

void TestCommon::testQsonList()
{
    QsonList list;
    QCOMPARE(list.size(), 0);

    list.append(QsonObject::NullValue);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.isNull(0), true);
    QCOMPARE(list.typeAt(0), QsonObject::NullType);

    list.append(true);
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.boolAt(1), true);
    QCOMPARE(list.typeAt(1), QsonObject::BoolType);

    list.append((quint32) 1);
    QCOMPARE(list.size(), 3);
    QCOMPARE(list.uintAt(2), (quint32) 1);
    QCOMPARE(list.typeAt(2), QsonObject::UIntType);

    list.append((qint32) 2);
    QCOMPARE(list.size(), 4);
    QCOMPARE(list.intAt(3), 2);
    QCOMPARE(list.typeAt(3), QsonObject::IntType);

    list.append(1.2);
    QCOMPARE(list.size(), 5);
    QCOMPARE(list.doubleAt(4), 1.2);
    QCOMPARE(list.typeAt(4), QsonObject::DoubleType);

    list.append(QLatin1String("char"));
    QCOMPARE(list.size(), 6);
    QCOMPARE(list.stringAt(5), QString("char"));
    QCOMPARE(list.typeAt(5), QsonObject::StringType);

    list.append(QString("string"));
    QCOMPARE(list.size(), 7);
    QCOMPARE(list.stringAt(6), QString("string"));
    QCOMPARE(list.typeAt(6), QsonObject::StringType);
}

void TestCommon::testQsonSubList()
{
    QsonMap qson;
    qson.insert(QLatin1String("hello"), QLatin1String("world"));

    QsonMap qson1;
    qson1.insert(QLatin1String("yes"), QLatin1String("no"));

    QsonList subList;
    subList.append(qson);
    subList.append(true);
    subList.append(qson1);
    subList.append(false);

    QsonList list;
    list.append(subList);

    QsonMap container;
    container.insert("list", list);

    QCOMPARE(container.size(), 1);
    QVERIFY(container.contains("list"));
    QCOMPARE(container.valueType("list"), QsonObject::ListType);

    QCOMPARE(container.subList("list").size(), 1);
    QCOMPARE(container.subList("list").typeAt(0), QsonObject::ListType);
    QCOMPARE(container.subList("list").listAt(0).size(), 4);
    QCOMPARE(container.subList("list").listAt(0).typeAt(0), QsonObject::MapType);
    QCOMPARE(container.subList("list").listAt(0).typeAt(1), QsonObject::BoolType);
    QCOMPARE(container.subList("list").listAt(0).typeAt(2), QsonObject::MapType);
    QCOMPARE(container.subList("list").listAt(0).typeAt(3), QsonObject::BoolType);

    QCOMPARE(container.subList("list").listAt(0).objectAt(0).size(), 1);
    QCOMPARE(container.subList("list").listAt(0).objectAt(0).valueString("hello"), QString("world"));
    QCOMPARE(container.subList("list").listAt(0).objectAt(2).size(), 1);
    QCOMPARE(container.subList("list").listAt(0).objectAt(2).valueString("yes"), QString("no"));
}

void TestCommon::testMeta()
{
    QsonMap meta;
    meta.insert(QLatin1String("hello"), QLatin1String("world"));
    QVERIFY(!meta.isMeta());

    QsonMap qson;
    QVERIFY(!qson.isDocument());
    QVERIFY(!qson.isMeta());
    qson.insert("_meta", meta);

    QVERIFY(qson.isDocument());
    qson.computeVersion();
    QsonVersion version = QsonVersion::version(qson);
    QVERIFY(version.isValid());
    QCOMPARE(qson.valueString("_version"), version.toString());
    QCOMPARE(qson.valueInt("_version"), 1);

    QVERIFY(qson.isDocument());
    QCOMPARE(qson.valueType("_meta"), QsonObject::MapType);
    meta = qson.subObject("_meta");
    QVERIFY(meta.isMeta());
    QCOMPARE(meta.valueString("hello"), QString("world"));
    meta.insert("yes", true);
    qson.insert("_meta", meta);

    // make sure, the version wasn't touched by inserting a modified meta
    qson.computeVersion();
    QCOMPARE(QsonVersion::version(qson), version);
    QCOMPARE(QsonVersion::lastVersion(qson), version);
}

void TestCommon::implicitSharing()
{
    QsonMap bar;
    bar.insert(QLatin1String("a"), QLatin1String("b"));
    QCOMPARE(bar.mBody.size(), 1);
    QCOMPARE(bar.mBody[0].constData()->ref.load(), 1);

    QsonList list;
    list.append(42);
    list.append(QLatin1String("foobar"));
    list.append(bar);

    QCOMPARE(bar.mBody.size(), 1);
    QCOMPARE(bar.mBody[0].constData()->ref.load(), 2);

    bar.insert(QLatin1String("zoo"), QLatin1String("zebra")); // detaches

    QCOMPARE(bar.mBody.size(), 1);
    QCOMPARE(bar.mBody[0].constData()->ref.load(), 1);

    QsonMap map;
    map.insert(QLatin1String("hello"), QLatin1String("world"));
    map.insert("foo", bar);
    map.insert("list", list);

    QCOMPARE(bar.mBody.size(), 1);
    QCOMPARE(bar.mBody[0].constData()->ref.load(), 2);
    bar.insert(QLatin1String("zzz"), QLatin1String("sleep")); // detaches
    QCOMPARE(bar.mBody.size(), 1);
    QCOMPARE(bar.mBody[0].constData()->ref.load(), 1);

    list.append(4242); // detaches

    QCOMPARE(list.size(), 4);
    QCOMPARE(list.at<int>(0), 42);
    QCOMPARE(list.at<QString>(1), QLatin1String("foobar"));
    QCOMPARE(list.at<QsonMap>(2).keys().size(), 1);
    QCOMPARE(list.at<QsonMap>(2).keys().at(0), QLatin1String("a"));
    QCOMPARE(list.at<QsonMap>(2).value<QString>("a"), QLatin1String("b"));

    QCOMPARE(bar.keys().size(), 3);
    QCOMPARE(bar.value<QString>("a"), QLatin1String("b"));
    QCOMPARE(bar.value<QString>("zoo"), QLatin1String("zebra"));
    QCOMPARE(bar.value<QString>("zzz"), QLatin1String("sleep"));

    QCOMPARE(map.keys().size(), 3);
    QCOMPARE(map.value<QString>("hello"), QLatin1String("world"));
    QCOMPARE(map.value<QsonMap>("foo").keys().size(), 2);
    QCOMPARE(map.value<QsonMap>("foo").value<QString>("a"), QLatin1String("b"));
    QCOMPARE(map.value<QsonMap>("foo").value<QString>("zoo"), QLatin1String("zebra"));

    QCOMPARE(map.value<QsonList>("list").size(), 3);
    QCOMPARE(map.value<QsonList>("list").at<int>(0), 42);
    QCOMPARE(map.value<QsonList>("list").at<QString>(1), QLatin1String("foobar"));
    QCOMPARE(map.value<QsonList>("list").at<QsonMap>(2).keys().size(), 1);
    QCOMPARE(map.value<QsonList>("list").at<QsonMap>(2).keys().at(0), QLatin1String("a"));
    QCOMPARE(map.value<QsonList>("list").at<QsonMap>(2).value<QString>("a"), QLatin1String("b"));
}

void TestCommon::testQsonVersion()
{
    QsonMap qson1;
    qson1.generateUuid();
    QsonMap qson2(qson1);

    qson1.insert("hello", QString("World"));
    qson1.computeVersion();

    qson2.insert("hello", QString("world"));
    qson2.computeVersion();

    //QsonVersion lastVersion = QsonVersion::lastVersion(qson1);
    //QVERIFY(!lastVersion.isValid());
    QsonVersion version1 = QsonVersion::version(qson1);
    QVERIFY(version1.isValid());
    QsonVersion version2 = QsonVersion::version(qson2);
    QVERIFY(version2.isValid());

    QMap<QsonVersion, QsonMap> container;
    container[version1] = qson1;
    container[version2] = qson2;

    QCOMPARE(container.size(), 2);
    QList<QsonVersion> keys = container.keys();

    QCOMPARE(keys[0], version2);
    QCOMPARE(keys[1], version1);

    QSet<QsonVersion> set;
    set.insert(version1);
    set.insert(version2);
    set.insert(version1);

    QCOMPARE(set.size(), 2);
    QVERIFY(set.contains(version1));
    QVERIFY(set.contains(version2));
}

void TestCommon::testQsonVersionLiteral()
{
    QsonMap map;
    map.computeVersion();
    map.insert("hello", QString("world"));
    map.computeVersion();

    QsonVersion source = QsonVersion::version(map);
    QsonVersion target = QsonVersion::fromLiteral(source.toString());
    QCOMPARE(target, source);

    QsonMap test;
    test.insert("_version", map.valueString("_version"));

    QCOMPARE(QsonVersion::lastVersion(test), QsonVersion::version(map));
    QCOMPARE(QsonVersion::version(test), QsonVersion::version(map));

    test.insert("_lastVersion", map.valueString("_lastVersion"));

    QCOMPARE(QsonVersion::lastVersion(test), QsonVersion::lastVersion(map));
    QCOMPARE(QsonVersion::version(test), QsonVersion::version(map));
}

void TestCommon::testVersionList()
{
    QsonMap q1;
    q1.insert("one", 1);
    q1.computeVersion();

    QsonMap q2;
    q2.insert("two", 2);
    q2.computeVersion();

    QsonPagePtr page(new QsonPage(QsonPage::ARRAY_VALUE_PAGE));
    page->writeVersion(QsonVersion::version(q1));
    page->writeVersion(QsonVersion::version(q2));

    QsonList list;
    list.mBody.append(page);

    QCOMPARE(list.size(), 2);
    QCOMPARE(list.stringAt(0), QsonVersion::version(q1).toString());
    QCOMPARE(list.stringAt(1), QsonVersion::version(q2).toString());

    QsonObject::CachedIndex *idx = list.index();
    QsonObject::CachedIndex::Cleaner cleanerIdx(*idx);
    int count = 0;
    foreach (const QsonEntry entry, *idx) {
        count++;
    }
    QCOMPARE(count, 2);
}

void TestCommon::testMerging()
{
    QsonMap qson1;
    qson1.computeVersion();
    QsonVersion version1 = QsonVersion::version(qson1);
    QCOMPARE(qson1.valueInt("_version"), 1);

    // same version should not merge
    QsonMap master(qson1);
    QVERIFY(!master.mergeVersions(qson1));
    QCOMPARE(QsonVersion::version(master), version1);

    // recompute just to be sure
    master.computeVersion();
    QCOMPARE(QsonVersion::version(master), version1);

    // non-document should not merge
    QVERIFY(!master.mergeVersions(QsonMap()));
    QCOMPARE(QsonVersion::version(master), version1);
    // recompute just to be sure
    master.computeVersion();
    QCOMPARE(QsonVersion::version(master), version1);

    // document with different uuid should not merge
    QsonMap alien;
    alien.generateUuid();
    QVERIFY(alien.valueString("_uuid") != master.valueString("_uuid"));
    QVERIFY(!master.mergeVersions(alien));
    QCOMPARE(QsonVersion::version(master), version1);
    // recompute just to be sure
    master.computeVersion();
    QCOMPARE(QsonVersion::version(master), version1);

    // create an update
    QsonMap qson2(qson1);
    qson2.insert("hello", QString("world2"));
    qson2.computeVersion();
    QCOMPARE(qson2.valueInt("_version"), 2);

    // create a conflicting update
    QsonMap qson3(qson1);
    qson3.insert("hello", QString("world3"));
    qson3.computeVersion();
    QCOMPARE(qson3.valueInt("_version"), 2);

    QsonVersion version2 = QsonVersion::version(qson2);
    QsonVersion version3 = QsonVersion::version(qson3);

    // make sure they are different
    QVERIFY(version2 != version3);

    // merge qson2 in
    QVERIFY(master.mergeVersions(qson2));

    // master should now look like qson2
    QCOMPARE(QsonVersion::version(master), version2);
    QCOMPARE(QsonVersion::lastVersion(master), version2);

    // master should now contain _meta information about the merge
    QVERIFY(master.contains("_meta"));
    QCOMPARE(master.valueType("_meta"), QsonObject::MapType);
    QVERIFY(master.subObject("_meta").isMeta());
    QCOMPARE(master.subObject("_meta").size(), 1);
    QCOMPARE(master.subObject("_meta").subList("ancestors").size(), 1);
    QCOMPARE(master.subObject("_meta").subList("ancestors").stringAt(0), version1.toString());
    QVERIFY(!master.subObject("_meta").contains("conflicts"));

    // make sure a computeVersion() doesn't change the version
    master.computeVersion();
    QCOMPARE(QsonVersion::lastVersion(master), QsonVersion::version(qson2));
    QCOMPARE(QsonVersion::version(master), QsonVersion::version(qson2));

    // let's keep a copy of master for later
    QsonMap qson21(master);

    // now we merge a conflict
    QVERIFY(master.mergeVersions(qson3));

    QCOMPARE(master.valueType("_meta"), QsonObject::MapType);
    QVERIFY(master.subObject("_meta").isMeta());
    QCOMPARE(master.subObject("_meta").size(), 2);
    QVERIFY(master.subObject("_meta").contains("conflicts"));
    QCOMPARE(master.subObject("_meta").valueType("conflicts"), QsonObject::ListType);
    QCOMPARE(master.subObject("_meta").subList("conflicts").size(), 1);

    QsonMap loser = master.subObject("_meta").subList("conflicts").objectAt(0);
    QVERIFY(loser.isDocument());

    // check the versions
    QsonVersion versionM = QsonVersion::version(master);
    QsonVersion versionL1 = QsonVersion::version(loser);

    // make sure the last version is set to current one
    QCOMPARE(QsonVersion::lastVersion(master), versionM);

    // sanity check
    QVERIFY(versionM.isValid());
    QVERIFY(version1.isValid());
    QVERIFY(version2.isValid());
    QVERIFY(version3.isValid());
    QVERIFY(versionL1.isValid());

    // winner is only determined by compare function
    if (version2 < version3) {
        QCOMPARE(versionM, version3);
        QCOMPARE(master.valueString("hello"), QString("world3"));
        QCOMPARE(versionL1, version2);
        QCOMPARE(loser.valueString("hello"), QString("world2"));
    } else {
        QFAIL("version2 should be less than version 3");
    }

    // check ancestors of winner
    QSet<QString> expectedVersions;
    expectedVersions.insert(version1.toString());
    QCOMPARE(master.subObject("_meta").subList("ancestors").size(), 1);
    QVERIFY(expectedVersions.contains(master.subObject("_meta").subList("ancestors").stringAt(0)));

    // check ancestors
    QVERIFY(!loser.contains("_meta"));
    QCOMPARE(QsonVersion::lastVersion(loser), versionL1);

    // let's create another conflict (from scratch)
    QsonMap qson4;
    qson4.insert("_uuid", master.valueString("_uuid"));
    QCOMPARE(qson4.valueString("_uuid"), master.valueString("_uuid"));
    qson4.insert("hello", QString("world4"));
    qson4.computeVersion();
    QsonVersion version4 = QsonVersion::version(qson4);

    // merge it in, version4 *must* lose, because its count is less
    QVERIFY(master.mergeVersions(qson4));
    QCOMPARE(QsonVersion::version(master), version3);
    QCOMPARE(QsonVersion::lastVersion(master), version3);
    QVERIFY(!master.contains("_lastVersion"));

    // check master's _meta
    QCOMPARE(master.valueType("_meta"), QsonObject::MapType);
    QVERIFY(master.subObject("_meta").isMeta());
    QCOMPARE(master.subObject("_meta").size(), 2);
    QVERIFY(master.subObject("_meta").contains("ancestors"));
    QCOMPARE(master.subObject("_meta").valueType("ancestors"), QsonObject::ListType);
    QVERIFY(master.subObject("_meta").contains("conflicts"));
    QCOMPARE(master.subObject("_meta").valueType("conflicts"), QsonObject::ListType);

    // check ancestors
    QCOMPARE(master.subObject("_meta").subList("ancestors").size(), 1);
    QVERIFY(expectedVersions.contains(master.subObject("_meta").subList("ancestors").stringAt(0)));

    // check conflicts
    QVERIFY(version4 < version2); // ensure sorting order
    QsonList conflicts = master.subObject("_meta").subList("conflicts");
    QCOMPARE(conflicts.size(), 2);
    QsonMap conflict0 = conflicts.objectAt(0);
    QCOMPARE(QsonVersion::version(conflict0), version4);
    QCOMPARE(QsonVersion::lastVersion(conflict0), version4);
    QCOMPARE(conflict0.valueString("hello"), QString("world4"));
    QVERIFY(!conflict0.contains("_meta"));
    QsonMap conflict1 = conflicts.objectAt(1);
    QCOMPARE(QsonVersion::version(conflict1), version2);
    QCOMPARE(QsonVersion::lastVersion(conflict1), version2);
    QCOMPARE(conflict1.valueString("hello"), QString("world2"));
    QVERIFY(!conflict1.contains("_meta"));

    QsonMap beforeReplay(master);
    // let's try a replay, not should merge
    QVERIFY(!master.mergeVersions(qson1));
    QVERIFY(!master.mergeVersions(qson2));
    QVERIFY(!master.mergeVersions(qson3));
    QVERIFY(!master.mergeVersions(qson4));
    QCOMPARE(master, beforeReplay);

    // let's try advancing a conflict
    qson21.insert("advanced", true);
    qson21.computeVersion();
    QsonVersion version21 = QsonVersion::version(qson21);
    QCOMPARE(QsonVersion::lastVersion(qson21), version2);
    QVERIFY(version21 != version2);

    // merge it in
    QVERIFY(master.mergeVersions(qson21));

    // ensure sorting order
    QVERIFY(version4 < version3);
    QVERIFY(version3 < version21);

    QCOMPARE(QsonVersion::version(master), version21);
    QCOMPARE(master.valueString("hello"), QString("world2"));
    QCOMPARE(master.valueBool("advanced"), true);

    // check master's _meta
    QCOMPARE(master.valueType("_meta"), QsonObject::MapType);
    QVERIFY(master.subObject("_meta").isMeta());
    QCOMPARE(master.subObject("_meta").size(), 2);
    QVERIFY(master.subObject("_meta").contains("ancestors"));
    QCOMPARE(master.subObject("_meta").valueType("ancestors"), QsonObject::ListType);
    QVERIFY(master.subObject("_meta").contains("conflicts"));
    QCOMPARE(master.subObject("_meta").valueType("conflicts"), QsonObject::ListType);

    // ensure conflicts
    conflicts = master.subObject("_meta").subList("conflicts");
    QCOMPARE(conflicts.size(), 2);
    QCOMPARE(QsonVersion::version(conflicts.objectAt(0)), version4);
    QCOMPARE(conflicts.objectAt(0).valueString("hello"), QString("world4"));
    QCOMPARE(QsonVersion::version(conflicts.objectAt(1)), version3);
    QCOMPARE(conflicts.objectAt(1).valueString("hello"), QString("world3"));

    // ensure ancestors
    expectedVersions.clear();
    expectedVersions.insert(version1.toString());
    expectedVersions.insert(version2.toString());
    QsonList ancestors = master.subObject("_meta").subList("ancestors");
    QCOMPARE(ancestors.size(), 2);
    QVERIFY(expectedVersions.remove(ancestors.stringAt(0)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(1)));
    QCOMPARE(expectedVersions.size(), 0);

    // let's try tombstoning a conflict
    QsonMap tombStone3 = conflicts.objectAt(1); // version3
    tombStone3.insert("_deleted", true);
    tombStone3.computeVersion();
    QVERIFY(master.mergeVersions(tombStone3));

    QCOMPARE(QsonVersion::version(master), version21);
    QCOMPARE(QsonVersion::lastVersion(master), version21);
    QCOMPARE(master.valueString("hello"), QString("world2"));
    QCOMPARE(master.valueBool("advanced"), true);

    // check master's _meta
    QCOMPARE(master.valueType("_meta"), QsonObject::MapType);
    QVERIFY(master.subObject("_meta").isMeta());
    QCOMPARE(master.subObject("_meta").size(), 2);
    QVERIFY(master.subObject("_meta").contains("ancestors"));
    QCOMPARE(master.subObject("_meta").valueType("ancestors"), QsonObject::ListType);
    QVERIFY(master.subObject("_meta").contains("conflicts"));
    QCOMPARE(master.subObject("_meta").valueType("conflicts"), QsonObject::ListType);

    // ensure conflicts
    conflicts = master.subObject("_meta").subList("conflicts");
    QCOMPARE(conflicts.size(), 1);
    QCOMPARE(QsonVersion::version(conflicts.objectAt(0)), version4);
    QCOMPARE(conflicts.objectAt(0).valueString("hello"), QString("world4"));

    // ensure ancestors
    expectedVersions.clear();
    expectedVersions.insert(version1.toString());
    expectedVersions.insert(version2.toString());
    expectedVersions.insert(version3.toString());
    expectedVersions.insert(QsonVersion::version(tombStone3).toString());
    ancestors = master.subObject("_meta").subList("ancestors");
    QCOMPARE(ancestors.size(), 4);
    QVERIFY(expectedVersions.remove(ancestors.stringAt(0)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(1)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(2)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(3)));
    QCOMPARE(expectedVersions.size(), 0);

    // now we tombstone the master
    QsonMap tombStone21(master);
    tombStone21.insert("_deleted", true);
    tombStone21.computeVersion();

    // merge it in
    master.mergeVersions(tombStone21);

    // master should now be dead with a conflict (the tombstone is ahead of conflict)
    QCOMPARE(QsonVersion::version(master), QsonVersion::version(tombStone21));
    QCOMPARE(QsonVersion::lastVersion(master), QsonVersion::version(tombStone21));
    QCOMPARE(master.valueBool("_deleted"), true);

    // check master's _meta
    QCOMPARE(master.valueType("_meta"), QsonObject::MapType);
    QVERIFY(master.subObject("_meta").isMeta());
    QCOMPARE(master.subObject("_meta").size(), 2);
    QVERIFY(master.subObject("_meta").contains("ancestors"));
    QCOMPARE(master.subObject("_meta").valueType("ancestors"), QsonObject::ListType);
    QVERIFY(master.subObject("_meta").contains("conflicts"));
    QCOMPARE(master.subObject("_meta").valueType("conflicts"), QsonObject::ListType);

    // ensure conflicts
    conflicts = master.subObject("_meta").subList("conflicts");
    QCOMPARE(conflicts.size(), 1);
    QCOMPARE(QsonVersion::version(conflicts.objectAt(0)), version4);
    QCOMPARE(conflicts.objectAt(0).valueString("hello"), QString("world4"));

    // ensure ancestors
    expectedVersions.clear();
    expectedVersions.insert(version1.toString());
    expectedVersions.insert(version2.toString());
    expectedVersions.insert(version3.toString());
    expectedVersions.insert(QsonVersion::version(tombStone3).toString());
    expectedVersions.insert(version21.toString());
    ancestors = master.subObject("_meta").subList("ancestors");
    QCOMPARE(ancestors.size(), 5);
    QVERIFY(expectedVersions.remove(ancestors.stringAt(0)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(1)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(2)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(3)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(4)));
    QCOMPARE(expectedVersions.size(), 0);

    // check that master is still serializable
    QsonParser parser;
    parser.append(master.data());
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QsonMap reread = parser.getObject();
    QCOMPARE(reread, master);
}

void TestCommon::testRecreate()
{
    QSet<QString> expectedVersions;

    QsonMap qson;
    qson.insert("run", 1);
    qson.computeVersion();
    QCOMPARE(qson.valueInt("_version"), 1);
    expectedVersions.insert(qson.valueString("_version"));

    qson.insert("_deleted", true);
    qson.insert("_lastVersion", qson.valueString("_version"));
    qson.computeVersion();
    QCOMPARE(qson.valueInt("_version"), 2);
    expectedVersions.insert(qson.valueString("_version"));

    QsonMap again;
    again.insert("_uuid", qson.valueString("_uuid"));
    again.insert("run", 2);
    again.computeVersion();

    // keep a copy for 2nd test
    QsonMap replication(qson);

    // let's test the recreate test
    qson.mergeVersions(again);

    QsonVersion mergedVersion = QsonVersion::version(qson);
    QCOMPARE(mergedVersion.updateCount(), quint32(3));
    QCOMPARE(mergedVersion.hash(), QsonVersion::version(again).hash());

    QVERIFY(!qson.subObject("_meta").contains("conflicts"));

    QsonList ancestors = qson.subObject("_meta").subList("ancestors");
    QCOMPARE(ancestors.size(), 2);
    QVERIFY(expectedVersions.remove(ancestors.stringAt(0)));
    QVERIFY(expectedVersions.remove(ancestors.stringAt(1)));
    QCOMPARE(expectedVersions.size(), 0);

    // make sure in replication mode we actually create a conflict
    replication.mergeVersions(again, true);
    QCOMPARE(!qson.subObject("_meta").subList("conflicts").size(), 1);
}

void TestCommon::testQsonElement()
{
    QsonMap qson;
    qson.insert("null", QsonObject::NullValue);
    QsonElement test = qson.value<QsonElement>("null");
    QCOMPARE(test.type(), QsonObject::NullType);
    QCOMPARE(test.isNull(), true);

    qson.insert("false", false);
    test = qson.value<QsonElement>("false");
    QCOMPARE(test.type(), QsonObject::BoolType);
    QCOMPARE(test.value<bool>(), false);

    qson.insert("true", true);
    test = qson.value<QsonElement>("true");
    QCOMPARE(test.type(), QsonObject::BoolType);
    QCOMPARE(test.value<bool>(), true);

    qson.insert("uint", (quint32) 1);
    test = qson.value<QsonElement>("uint");
    QCOMPARE(test.type(), QsonObject::UIntType);
    QCOMPARE(test.value<quint32>(), (quint32) 1);

    qson.insert("int", (qint32) 2);
    test = qson.value<QsonElement>("int");
    QCOMPARE(test.type(), QsonObject::IntType);
    QCOMPARE(test.value<qint32>(), (qint32) 2);

    qson.insert("double", 1.2);
    test = qson.value<QsonElement>("double");
    QCOMPARE(test.type(), QsonObject::DoubleType);
    QCOMPARE(test.value<double>(), 1.2);

    qson.insert(QLatin1String("string"), QLatin1String("qvalue"));
    test = qson.value<QsonElement>("string");
    QCOMPARE(test.type(), QsonObject::StringType);
    QCOMPARE(test.value<QString>(), QLatin1String("qvalue"));

    // test map.insert
    QsonMap map;
    map.insert(QLatin1String("insert"), test);
    QCOMPARE(map.size(), 1);
    QCOMPARE(map.valueType("insert"), QsonObject::StringType);
    QCOMPARE(map.value<QString>("insert"), QLatin1String("qvalue"));

    // test list.insert
    QsonList list;
    list.append(test);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.typeAt(0), QsonObject::StringType);
    QCOMPARE(list.at<QString>(0), QLatin1String("qvalue"));

    list.append(qson);
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.typeAt(0), QsonObject::StringType);
    QCOMPARE(list.at<QString>(0), QLatin1String("qvalue"));
    QCOMPARE(list.typeAt(1), QsonObject::MapType);
    QCOMPARE(list.at<QsonMap>(1), qson);

    {
        // test set-functions
        QsonList list;
        QsonElement element;
        element.setValue(QsonObject::NullValue);
        list.append(element);
        QVERIFY(element.isNull());
        QCOMPARE(list.typeAt(0), QsonObject::NullType);

        element.setValue(true);
        QCOMPARE(element.value<bool>(), true);
        list.append(element);
        QCOMPARE(list.typeAt(1), QsonObject::BoolType);

        element.setValue(42);
        QCOMPARE(element.value<qint32>(), 42);
        list.append(element);
        QCOMPARE(list.typeAt(2), QsonObject::IntType);

        element.setValue(42.42);
        QCOMPARE(element.value<double>(), 42.42);
        list.append(element);
        QCOMPARE(list.typeAt(3), QsonObject::DoubleType);

        element.setValue(QString::fromLatin1("foobar"));
        QCOMPARE(element.value<QString>(), QLatin1String("foobar"));
        list.append(element);
        QCOMPARE(list.typeAt(4), QsonObject::StringType);
    }
}

void TestCommon::testEquals()
{
    {
        QsonMap map1;
        QsonMap map2;
        QVERIFY(map1 == map2);
        QVERIFY(!(map1 != map2));

        map2 = map1;
        QVERIFY(map1 == map2);
        QVERIFY(!(map1 != map2));

        map1.insert(QLatin1String("foo"), QLatin1String("bar"));
        QVERIFY(map1 != map2);
        QVERIFY(!(map1 == map2));

        map2 = map1;
        QVERIFY(map1 == map2);
        QVERIFY(!(map1 != map2));

        QsonMap map3;
        QVERIFY(map1 != map3);
        QVERIFY(!(map1 == map3));
        map3.insert(QLatin1String("foo"), QLatin1String("bar"));
        QVERIFY(map1 == map3);
        QVERIFY(!(map1 != map3));
    }

    {
        QsonList list1;
        QsonList list2;
        QVERIFY(list1 == list2);
        QVERIFY(!(list1 != list2));

        list2 = list1;
        QVERIFY(list1 == list2);
        QVERIFY(!(list1 != list2));

        list1.append(QLatin1String("foo"));
        QVERIFY(list1 != list2);
        QVERIFY(!(list1 == list2));

        list2 = list1;
        QVERIFY(list1 == list2);
        QVERIFY(!(list1 != list2));
    }

    {
        QsonMap map1;
        QsonMap map2;

        map1.insert(QLatin1String("foo"), QLatin1String("bar"));
        map2.insert(QLatin1String("foo"), QLatin1String("bar"));

        QsonElement element1 = map1.value<QsonElement>(QLatin1String("foo"));
        QsonElement element2 = map2.value<QsonElement>(QLatin1String("foo"));
        QVERIFY(element1 == element2);
        QVERIFY(!(element1 != element2));

        QsonList list;
        list.append(42);
        list.append(QLatin1String("bar"));
        QsonElement element3 = list.at<QsonElement>(1);
        QVERIFY(element1 == element3);
        QVERIFY(!(element1 != element3));
    }

    QsonMap map;
    QsonList list;
    QVERIFY(map != list);
    QVERIFY(!(map == list));
    QsonMap m; m.insert(QLatin1String("foo"), 42);
    QsonList l; l.append(42);
}

void TestCommon::testQsonParser()
{
    QsonMap qson;
    qson.insert(QLatin1String("hello"), QLatin1String("world"));

    QsonMap qson1;
    qson1.insert(QLatin1String("yes"), QLatin1String("no"));

    QsonList subList;
    subList.append(qson);
    subList.append(true);
    subList.append(qson1);
    subList.append(false);

    QsonList list;
    list.append(subList);

    QsonMap container;
    container.insert("list", list);

    QByteArray raw = container.data();

    QsonParser parser;
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());

    // read once as a complete qbytearray
    parser.append(raw);
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());

    QsonMap reread = QsonMap(parser.getObject());
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());

    QCOMPARE(reread.dataSize(), container.dataSize());
    QCOMPARE(reread.size(), container.size());
    QCOMPARE(reread.data(), container.data());
    QCOMPARE(reread, container);

    // read once as a complete char array
    parser.append(raw.constData(), raw.size());
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());

    reread = QsonMap(parser.getObject());
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());

    QCOMPARE(reread.dataSize(), container.dataSize());
    QCOMPARE(reread.size(), container.size());
    QCOMPARE(reread.data(), container.data());
    QCOMPARE(reread, container);

    // read chunked
    parser.append(raw.left(10));
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());

    parser.append(raw.constData() + 10, 10);
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());

    parser.append(raw.mid(20));
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());

    reread = QsonMap(parser.getObject());
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());

    QCOMPARE(reread.dataSize(), container.dataSize());
    QCOMPARE(reread.size(), container.size());
    QCOMPARE(reread.data(), container.data());
    QCOMPARE(reread, container);
}

void TestCommon::testQsonParserStreaming()
{
    QsonMap qson;
    qson.insert(QLatin1String("hello"), QLatin1String("world"));

    QsonMap qson1;
    qson1.insert(QLatin1String("yes"), QLatin1String("no"));

    QsonList subList;
    subList.append(qson);
    subList.append(true);
    subList.append(qson1);
    subList.append(false);

    QsonList list;
    list.append(subList);
    list.append(qson);
    list.append(subList);
    list.append(qson1);

    QsonParser parser(true);
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());
    QVERIFY(!parser.isStream());
    QVERIFY(!parser.streamDone());

    parser.append(qson.data());
    parser.append(list.data());
    parser.append(qson1.data());
    parser.append(list.data());

    // before stream, read qson
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(!parser.isStream());
    QVERIFY(!parser.streamDone());
    QsonObject reread = parser.getObject();
    QCOMPARE(QsonMap(reread), qson);

    // streaming list, read list[0]
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonList(reread), subList);

    // streaming list, read list[1]
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonMap(reread), qson);

    // streaming list, read list[2]
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonList(reread), subList);

    // streaming list, read list[3]
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonMap(reread), qson1);

    // after stream
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());
    QVERIFY(!parser.isStream());
    QVERIFY(parser.streamDone());

    // read qson1
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(!parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonMap(reread), qson1);

    // streaming list, read list[0]
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonList(reread), subList);

    // streaming list, read list[1]
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonMap(reread), qson);

    // streaming list, read list[2]
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonList(reread), subList);

    // streaming list, read list[3]
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QVERIFY(parser.isStream());
    QVERIFY(!parser.streamDone());
    reread = parser.getObject();
    QCOMPARE(QsonMap(reread), qson1);

    // after stream
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());
    QVERIFY(!parser.isStream());
    QVERIFY(parser.streamDone());

    // no more data available
    QVERIFY(!parser.hasError());
    QVERIFY(!parser.isObjectReady());
    QVERIFY(!parser.isStream());
    QVERIFY(!parser.streamDone());
}

void TestCommon::testQsonParseDocument()
{
    QsonMap document;
    document.generateUuid();
    QsonMap map;
    map.insert(QLatin1String("error"), QsonObject::NullValue);
    map.insert(QLatin1String("id"), 1);
    map.insert(QLatin1String("result"), document);

    QByteArray data = map.data();
    QCOMPARE(map.dataSize(), data.size());

    QsonParser parser;
    parser.append(data);
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    QsonObject result = parser.getObject();
    QCOMPARE(result, static_cast<QsonObject>(map));

    QsonList list;
    list.append(42);
    list.append(document);
    list.append(64);
    data = list.data();
    QsonParser parser2;
    parser2.append(data);
    QVERIFY(!parser2.hasError());
    QVERIFY(parser2.isObjectReady());
    result = parser2.getObject();
    QCOMPARE(result, static_cast<QsonObject>(list));
}

void TestCommon::testComplexElement()
{
    QString json = "{\"_type\":\"Contact\",\"displayName\":\"Joe Smith\",\"name\":{\"firstName\":\"joe\",\"lastName\":\"smith\"},\"phoneNumbers\":[{\"number\":\"+15555551212\",\"type\":\"mobile\"},{\"number\":\"+17812232323\",\"type\":\"work\"},{\"number\":\"+16174532300\",\"type\":\"home\"}],\"preferredNumber\":\"+15555551212\"}";
    JsonReader jparser;
    jparser.parse(json);
    QsonMap qson = variantToQson(jparser.result());
    
    QByteArray data = qson.data();
    
    QsonParser parser;
    parser.append(data);
    
    QVERIFY(!parser.hasError());
    QVERIFY(parser.isObjectReady());
    
    QsonMap qson2 = parser.getObject();
    QsonList list = qson2.subList("phoneNumbers");    
    QsonElement element = qson2.value<QsonElement>("phoneNumbers");

    QCOMPARE(element.type(), list.type());
    QCOMPARE(element.mBody.size(), list.mBody.size());

    QsonList list2(element);

    QVERIFY(element == list);
    QVERIFY(list2 == list);
    
    QsonList emptyList;
    QsonMap map;
    map.insert("empty", emptyList);
    QsonElement emptyElement = map.value<QsonElement>("empty");
    QsonElement emptyList2 = emptyElement.toList();
    
    QCOMPARE(emptyElement.type(), emptyList.type());
    QVERIFY(emptyElement == emptyList);
    QVERIFY(emptyList2 == emptyList);
}

void TestCommon::testInsertEmptyObject()
{
    QsonMap qson;
    qson.insert("hello", QString("world"));
    qson.insert("null", QsonObject());
    qson.insert("foo", QString("bar"));
    QCOMPARE(qson.size(), 3);
    QCOMPARE(qson.valueString("hello"), QString("world"));
    QVERIFY(qson.isNull("null"));
    QCOMPARE(qson.valueString("foo"), QString("bar"));

    QsonList list;
    list.append(QString("world"));
    list.append(QsonObject());
    list.append(QString("bar"));

    QCOMPARE(list.size(), 3);
    QCOMPARE(list.stringAt(0), QString("world"));
    QVERIFY(list.isNull(1));
    QCOMPARE(list.stringAt(2), QString("bar"));
}

void TestCommon::testIsEmpty()
{
    QsonMap qson;
    QVERIFY(qson.isEmpty());
    qson.insert("a", 1);
    QVERIFY(!qson.isEmpty());
    
    QsonMap doc;
    QVERIFY(doc.isEmpty());
    doc.computeVersion();
    QVERIFY(!doc.isEmpty());    
}

void TestCommon::testInsertUuid()
{
    QsonMap qson;
    qson.ensureDocument();

    QsonMap copy;
    copy.insert("_uuid", qson.valueString("_uuid"));

    QCOMPARE(copy.valueString("_uuid"), qson.valueString("_uuid"));
    QCOMPARE(copy.valueString("_uuid").size(), 38);

    QVERIFY(!QUuid(copy.valueString("_uuid")).isNull());
    QCOMPARE((quint8) (QUuid(copy.valueString("_uuid")).version()), quint8(4));

    copy.computeVersion();

    QCOMPARE(copy.value<QsonElement>("_uuid").value<QString>(), copy.valueString("_uuid"));
    QCOMPARE(copy.value<QsonElement>("_version").value<QString>(), copy.valueString("_version"));
    QCOMPARE(copy.value<QsonElement>("_lastVersion").value<QString>(), copy.valueString("_lastVersion"));
}

void TestCommon::testQsonToJSValue()
{
    QsonMap qson;
    QsonList values;
    values.append(QString("Test1"));
    values.append(QString("Test2"));
    qson.insert("values", values);
    QsonMap subObject;
    subObject.insert("name", QString("John"));
    qson.insert("sub", subObject);

    QJSEngine *engine = new QJSEngine(this);
    QJSValue jsv = qsonToJSValue(qson, engine);
    QVERIFY(jsv.isObject());
    QVERIFY(jsv.property("values").isArray());
    QCOMPARE(jsv.property("values").property(0).toString(),QString("Test1"));
    QCOMPARE(jsv.property("values").property(1).toString(), QString("Test2"));
    QVERIFY(jsv.property("sub").isObject());
    QCOMPARE(jsv.property("sub").property("name").toString(), QString("John"));

}

void TestCommon::testRemove()
{
    QsonMap qson;
    QCOMPARE(qson.size(), 0);
    QVERIFY(qson.isEmpty());

    qson.insert("a", 1);
    QCOMPARE(qson.size(), 1);
    QVERIFY(!qson.isEmpty());

    qson.remove("a");
    QCOMPARE(qson.size(), 0);
    QVERIFY(qson.isEmpty());

    qson.insert("a", 1);
    qson.insert("b", 2);
    QCOMPARE(qson.size(), 2);
    QVERIFY(!qson.isEmpty());

    qson.remove("a");
    QCOMPARE(qson.size(), 1);
    QVERIFY(!qson.isEmpty());

    QVERIFY(qson.contains("b"));
    QCOMPARE(qson.valueInt("b"), 2);

    QsonMap c;
    c.insert("value", true);
    qson.insert("c", c);
    qson.insert("d", 3);

    QCOMPARE(qson.size(), 3);
    QVERIFY(!qson.isEmpty());

    qson.remove("c");
    QVERIFY(!qson.isEmpty());
    QVERIFY(!qson.contains("c"));
    QCOMPARE(qson.size(), 2);
    QCOMPARE(qson.valueInt("b"), 2);
    QCOMPARE(qson.valueInt("d"), 3);

    QsonMap subObjectOnly;
    subObjectOnly.insert("c", c);
    subObjectOnly.remove("c");
    QVERIFY(subObjectOnly.isEmpty());
    QVERIFY(!subObjectOnly.contains("c"));
    QCOMPARE(subObjectOnly.size(), 0);

    QsonMap subObjectFirst;
    subObjectFirst.insert("c", c);
    subObjectFirst.insert("d", 3);
    subObjectFirst.remove("c");
    QVERIFY(!subObjectFirst.isEmpty());
    QVERIFY(!subObjectFirst.contains("c"));
    QVERIFY(subObjectFirst.contains("d"));
    QCOMPARE(subObjectFirst.size(), 1);
    QCOMPARE(subObjectFirst.valueInt("d"), 3);

    // this will fail if page.updateOffset() is not called correctly
    QsonMap checkSerialization;
    checkSerialization.insert("a", 1);
    checkSerialization.insert("b", 2);
    checkSerialization.remove("b");
    QsonMap reread = QsonParser::fromRawData(checkSerialization.data());
    QCOMPARE(reread.size(), 1);
    QCOMPARE(reread.valueInt("a"), 1);
}

void TestCommon::testDoubleInsert()
{
    QsonMap qson;
    qson.insert("a", 1);

    QsonMap b;
    b.insert("value", false);
    qson.insert("b", b);

    qson.insert("a", 2);
    b.insert("value", true);
    qson.insert("b", b);

    QsonMap replay;
    replay.insert("a", 2);
    QsonMap breplay;
    breplay.insert("value", true);
    replay.insert("b", breplay);

    QCOMPARE(qson.dataSize(), replay.dataSize());
    QCOMPARE(qson, replay);

    qson.computeVersion();
    replay.computeVersion();

    QCOMPARE(QsonVersion::version(qson), QsonVersion::version(replay));
}


QTEST_MAIN(TestCommon)
#include "test-common.moc"
