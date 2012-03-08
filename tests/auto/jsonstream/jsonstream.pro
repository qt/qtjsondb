TARGET = tst_jsonstream

QT = qml network testlib
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../../src/common/common.pri)

SOURCES += \
    test-jsonstream.cpp

