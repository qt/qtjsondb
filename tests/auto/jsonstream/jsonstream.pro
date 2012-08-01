TARGET = tst_jsonstream

QT = qml network testlib
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../../src/jsonstream/jsonstream.pri)

SOURCES += \
    test-jsonstream.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
