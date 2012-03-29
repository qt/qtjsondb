TARGET = tst_jsonstream

QT = qml network testlib
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../../src/jsonstream/jsonstream.pri)

SOURCES += \
    test-jsonstream.cpp

