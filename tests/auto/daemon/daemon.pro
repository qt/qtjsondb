TARGET = tst_daemon

QT = network declarative testlib
CONFIG -= app_bundle
CONFIG += testcase

TESTDATA += \
    array.json \
    capabilities-test.json \
    largeContactsTest.json \
    map-join.json \
    map-join-sourceuuids.json \
    map-reduce.json \
    map-reduce-schema.json \
    map-sametarget.json \
    pk-capability.json \
    reduce-array.json \
    reduce-data.json \
    reduce.json \
    reduce-subprop.json

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../../src/daemon/daemon.pri)
RESOURCES += json-validation.qrc

SOURCES += \
    testjsondb.cpp \
