/****************************************************************************
**
** Copyright (c) 2011 Denis Dzyubenko <shadone@gmail.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#include <QtCore>
#include <QTest>

#include "json.h"

class tst_Json : public QObject
{
    Q_OBJECT
private slots:
    void testByteArray();
    void testString();
    void testNumbers();
};

void tst_Json::testNumbers()
{
    QFile file(QLatin1String("numbers.json"));
    file.open(QFile::ReadOnly);
    QByteArray ba = file.readAll();
    QString data = QString::fromLocal8Bit(ba.constData(), ba.size());

    QBENCHMARK {
        JsonReader reader;
        if (!reader.parse(data)) {
            qDebug() << "Failed to parse: " << reader.errorString();
            return;
        }
        QVariant result = reader.result();
    }
}

void tst_Json::testString()
{
    QFile file(QLatin1String("test.json"));
    file.open(QFile::ReadOnly);
    QByteArray ba = file.readAll();
    QString data = QString::fromLocal8Bit(ba.constData(), ba.size());

    QBENCHMARK {
        JsonReader reader;
        if (!reader.parse(data)) {
            qDebug() << "Failed to parse: " << reader.errorString();
            return;
        }
        QVariant result = reader.result();
    }
}

void tst_Json::testByteArray()
{
    QFile file(QLatin1String("test.json"));
    file.open(QFile::ReadOnly);
    QByteArray testJson = file.readAll();

    QBENCHMARK {
        JsonReader reader;
        if (!reader.parse(testJson)) {
            qDebug() << "Failed to parse: " << reader.errorString();
            return;
        }
        QVariant result = reader.result();
    }
}

QTEST_MAIN(tst_Json)
#include "main.moc"
