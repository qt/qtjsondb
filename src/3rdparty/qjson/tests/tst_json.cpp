/****************************************************************************
**
** Copyright (c) 2011 Girish Ramakrishnan <girish@forwardbias.in>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#include <QtTest/QtTest>
#include "json.h"

class tst_Json : public QObject
{
    Q_OBJECT

private slots:
    void parseAndStringify_data();
    void parseAndStringify();

    void stringify_data();
    void stringify();

    void parseError_data();
    void parseError();
};

static QByteArray unformat(const QByteArray &input)
{
    QByteArray output;
    bool inQuote = false, escape = false;
    for (int i = 0; i < input.count(); i++) {
        char c = input[i];
        if (escape) {
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else {
            if (c == '\"')
                inQuote = !inQuote;
            if (!inQuote && (c == ' ' || c == '\r' || c == '\n' || c == '\t'))
                continue;
        }
        output += c;
    }
    return output;
}

void tst_Json::parseAndStringify_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QByteArray>("reference"); // always UTF-8
    QDir dir;
    dir.cd(QLatin1String(SRCDIR "data/"));
    foreach(QString filename , dir.entryList(QStringList() << QLatin1String("*.json"))) {
        QFile jsonFile(dir.filePath(filename));
        jsonFile.open(QFile::ReadOnly);
        QByteArray json = jsonFile.readAll();
        QFile refFile(dir.filePath(filename + QLatin1String(".ref")));
        QByteArray reference;
        if (refFile.open(QFile::ReadOnly)) {
            reference = refFile.readAll();
        } else {
            reference = json;
        }
        if (filename.contains(QLatin1String("utf-16"))) {
            reference = QTextCodec::codecForName("UTF-16LE")->toUnicode(reference).toUtf8();
        } else if (filename.contains(QLatin1String("utf-32"))) {
            reference = QTextCodec::codecForName("UTF-32LE")->toUnicode(reference).toUtf8();
        }
        reference = unformat(reference);
        QTest::newRow(dir.filePath(filename).toLatin1().data()) << json << reference;
    }
}

void tst_Json::parseAndStringify()
{
    QFETCH(QByteArray, json);
    QFETCH(QByteArray, reference);

    JsonReader reader;
    QVERIFY2(reader.parse(json), reader.errorString().toLocal8Bit().constData());
    QVariant result = reader.result();

    JsonWriter writer;
    QByteArray jsonAgain = writer.toByteArray(result);
    QVERIFY(jsonAgain == reference);

    // test pretty writing
    writer.setAutoFormatting(true);
    jsonAgain = writer.toByteArray(result);
    QVERIFY(unformat(jsonAgain) == reference);
}

struct CustomType {
    int x;
};
Q_DECLARE_METATYPE(CustomType)

void tst_Json::stringify_data()
{
    QTest::addColumn<QVariant>("input");
    QTest::addColumn<QString>("output");

    QTest::newRow("null_variant") << QVariant() << "null";

    // bool
    QTest::newRow("true") << QVariant(true) << "true";
    QTest::newRow("false") << QVariant(false) << "false";

    // lists
    QTest::newRow("empty list") << QVariant(QVariantList()) << "[]";

    // objects
    QTest::newRow("empty object") << QVariant(QVariantMap()) << "{}";

    // numbers
    QTest::newRow("number") << QVariant(42) << "42";
    QTest::newRow("nan") << QVariant(double(NAN)) << "null";
    QTest::newRow("inf") << QVariant(double(INFINITY)) << "null";
    QTest::newRow("-inf") << QVariant(double(-INFINITY)) << "null";

    // unicode
    QTest::newRow("non-ascii/latin1") << QVariant(QChar(0xFB)) << "\"\\u00fb\"";

    const char utf8[] = {0xE0, 0xAE, 0x9F, 0xE0, 0xAE, 0xBF, 0x00 };
    QTest::newRow("tamil tee") << QVariant(QString::fromUtf8(utf8)) << "\"\\u0b9f\\u0bbf\"";
 
    // various QVariant types
    QVariantList vlist;
    vlist = QVariantList() << QVariant(float(1)) << QVariant(qlonglong(13))
                           << QVariant(qulonglong(14)) << QVariant(quint32(15))
                           << QVariant(char('x')) << QVariant(QChar(QLatin1Char('X')));
    QTest::newRow("various types") << QVariant(vlist) << "[1,13,14,15,120,\"X\"]";

    // custom types
    // It is out of the scope of the standard to specify what happens when
    // a custom type that cannot be stringized is present. We generate a null
    // for unsupported types
    CustomType c;
    QTest::newRow("unpresentable object") << qVariantFromValue(c) << "null";
    
    vlist = QVariantList() << 1 << qVariantFromValue(c) << 2;
    QTest::newRow("unpresentable object[]") << QVariant(vlist) << "[1,null,2]";
    
    QVariantMap map;
    map[QLatin1String("one")] = 1;
    map[QLatin1String("two")] = 2;
    map[QLatin1String("custom")] = qVariantFromValue(c);
    QTest::newRow("unpresentable object{}") << QVariant(map) << "{\"custom\":null,\"one\":1,\"two\":2}";
}

void tst_Json::stringify()
{
    QFETCH(QVariant, input);
    QFETCH(QString, output);

    JsonWriter writer;
    QCOMPARE(writer.toString(input), output);

    // just check if it parses, just in case
    JsonReader reader;
    QVERIFY(reader.parse(writer.toString(input)));
}

void tst_Json::parseError_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("line");
    QTest::addColumn<int>("pos");
    QTest::addColumn<QString>("expect");

    // Most of the time the error message itself is not useful. These tests exist
    // primarily to make sure that parsing fails
    QTest::newRow("error 1") << "!" << 1 << 0 << "";
    QTest::newRow("error 2") << "[1,2," << 1 << 5 << "";
    QTest::newRow("error 3") << "[1,2" << 1 << 4 << "Expected ']', ','";
    QTest::newRow("error 4") << "[1,2p" << 1 << 5 << "Expected ']', ','";
    QTest::newRow("error 5") << "[1,2] {" << 1 << 7 << "Expected 'end of file'";
    QTest::newRow("error 6") << "{ foo: bar }" << 1 << 5 << "Expected '}', ','";
    QTest::newRow("error 7") << "\n\n{ \"cat\" : \"pillar\"" << 3 << 20 << "Expected '}', ','";
    QTest::newRow("success") << "true" << -1 << -1 << "";
}

void tst_Json::parseError()
{
    QFETCH(QString, input);
    QFETCH(int, line);
    QFETCH(int, pos);
    QFETCH(QString, expect);

    JsonReader reader;
    QVERIFY(!reader.parse(input) || line == -1);

    if (line != -1)
        QCOMPARE(reader.errorString(), QString::fromUtf8("%1 at line %2 pos %3").arg(expect).arg(line).arg(pos));
}

QTEST_MAIN(tst_Json)

#include "tst_json.moc"

