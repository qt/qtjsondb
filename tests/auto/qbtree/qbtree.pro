include($$PWD/../../../src/3rdparty/btree/btree.pri)

TARGET = tst_qbtree

QT = core testlib
CONFIG -= app_bundle
CONFIG += testcase

SOURCES += \
    main.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
