TARGET = tst_bench_partition

QT = network qml testlib jsondbpartition
CONFIG -= app_bundle
CONFIG += testcase

LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES+=../../json.qrc partition.qrc

SOURCES += \
    bench_partition.cpp \
