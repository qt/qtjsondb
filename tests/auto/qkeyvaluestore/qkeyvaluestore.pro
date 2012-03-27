include($$PWD/../../../src/qkeyvaluestore/qkeyvaluestore.pri)
QT       += testlib
QT       -= gui

TARGET = tst_qkeyvaluestore
CONFIG   += testcase
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += tst_qkeyvaluestore.cpp
