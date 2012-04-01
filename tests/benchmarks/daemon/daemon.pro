TARGET = tst_bench_daemon

QT = network qml testlib jsondbpartition
CONFIG -= app_bundle
CONFIG += testcase

LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES+=../../json.qrc daemon.qrc
SOURCES += \
    bench_daemon.cpp \
