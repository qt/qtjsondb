DESTDIR = bin

CONFIG -= app_bundle
CONFIG -= qt
CONFIG += debug

include($$PWD/../../../src/3rdparty/btree/btree.pri)

SOURCES += \
    main.cpp \

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
