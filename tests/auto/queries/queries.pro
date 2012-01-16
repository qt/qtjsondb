TARGET = tst_queries

QT = network declarative testlib jsondbqson-private
CONFIG -= app_bundle
CONFIG += debug
CONFIG += testcase

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../../src/daemon/daemon.pri)

SOURCES += \
    testjsondbqueries.cpp
