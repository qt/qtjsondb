TARGET = tst_bench_object

QT = network qml testlib jsondbpartition
CONFIG -= app_bundle
CONFIG += testcase

DEFINES += SRCDIR=\\\"$$PWD/\\\"

# HACK, remove when jsondbpartition separates private api from public api
include(../../../src/3rdparty/btree/btree.pri)
include(../../../src/hbtree/hbtree.pri)

SOURCES += \
    bench_jsondbobject.cpp \

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
