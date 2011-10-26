TARGET = tst_common

QT = declarative network testlib jsondbqson-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../../src/common/common.pri)

HEADERS += \
    $$QSONCONVERSION_HEADERS

SOURCES += \
    test-common.cpp \
    $$QSONCONVERSION_SOURCES
