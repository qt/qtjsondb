include($$PWD/../../../qtjsondb.pri)
include($$PWD/../../../src/3rdparty/btree/btree.pri)

TARGET = tst_bdb

QT = network testlib
CONFIG -= app_bundle
CONFIG += testcase

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -lssl -lcrypto

SOURCES += \
    tst_jsondb_bdb.cpp \
    ../../../src/daemon/aodb.cpp
