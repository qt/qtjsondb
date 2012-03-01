include($$PWD/../../../src/hbtree/hbtree.pri)

TARGET = tst_hbtree

QT = core testlib
CONFIG -= app_bundle
CONFIG += testcase

SOURCES += \
    main.cpp
