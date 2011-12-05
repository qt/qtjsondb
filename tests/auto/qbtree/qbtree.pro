include($$PWD/../../../src/3rdparty/btree/btree.pri)

TARGET = tst_qbtree

QT = core testlib
CONFIG -= app_bundle
CONFIG += testcase

LIBS += -lssl -lcrypto

SOURCES += \
    main.cpp
