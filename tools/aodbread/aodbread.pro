TARGET = aodbread

QT = network declarative testlib
CONFIG -= app_bundle
CONFIG += debug

include($$PWD/../../src/common/common.pri)

INCLUDEPATH += $$PWD/../../src/daemon
LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../src/daemon/daemon.pri)

SOURCES += \
    main.cpp \

