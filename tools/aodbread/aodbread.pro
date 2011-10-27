TARGET = aodbread

QT = testlib jsondbqson-private
CONFIG -= app_bundle
CONFIG += debug

INCLUDEPATH += $$PWD/../../src/daemon
LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

DEFINES += SRCDIR=\\\"$$PWD/\\\"

#include($$PWD/../../src/daemon/daemon.pri)
include($$PWD/../../src/3rdparty/btree/btree.pri)
include($$PWD/../../src/3rdparty/qjson/qjson.pri)

HEADERS += \
    $$PWD/../../src/daemon/aodb.h

SOURCES += \
    $$PWD/../../src/daemon/aodb.cpp \
    main.cpp

