include($$PWD/../../../src/hbtree/hbtree.pri)
include($$PWD/../../../src/3rdparty/btree/btree.pri)

TARGET = tst_bench_btrees

QT = core testlib
CONFIG -= app_bundle
CONFIG += testcase

SOURCES += \
    main.cpp
