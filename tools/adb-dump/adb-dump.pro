include($$PWD/../../src/3rdparty/btree/btree.pri)

TARGET = adb-dump
DESTDIR = $$QT.jsondb.bins

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target

QT = core

mac:CONFIG -= app_bundle

!mac:LIBS += -lcrypto

INCLUDEPATH += ../../src/daemon

HEADERS += ../../src/daemon/aodb.h

SOURCES += main.cpp ../../src/daemon/aodb.cpp
