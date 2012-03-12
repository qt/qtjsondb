TARGET = tst_queries

QT = network qml testlib
CONFIG -= app_bundle
CONFIG += testcase

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../../src/daemon/daemon.pri)

RESOURCES = queries.qrc

SOURCES += \
    testjsondbqueries.cpp
