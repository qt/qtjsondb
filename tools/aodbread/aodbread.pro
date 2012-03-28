TARGET = aodbread

QT = testlib
CONFIG -= app_bundle
CONFIG += debug

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../src/3rdparty/btree/btree.pri)

SOURCES += \
    main.cpp

