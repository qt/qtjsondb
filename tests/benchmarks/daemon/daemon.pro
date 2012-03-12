TARGET = tst_bench_daemon

QT = network qml testlib
CONFIG -= app_bundle
CONFIG += testcase

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"
include($$PWD/../../../src/daemon/daemon.pri)
RESOURCES+=../../json.qrc daemon.qrc
SOURCES += \
    bench_daemon.cpp \
