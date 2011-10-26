TARGET = tst_qsonstream

QT = core testlib network declarative jsondbqson-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../../src/common/common.pri)

SOURCES += test-qsonstream.cpp
