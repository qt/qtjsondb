TARGET = tst_bench_partition

QT = network qml testlib jsondbpartition jsondbpartition-private
CONFIG -= app_bundle
CONFIG += testcase

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES+=../../json.qrc partition.qrc

# HACK, remove when jsondbpartition separates private api from public api
include(../../../src/3rdparty/btree/btree.pri)
include(../../../src/hbtree/hbtree.pri)

SOURCES += \
    bench_partition.cpp \
