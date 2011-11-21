CONFIG += debug

TARGET = jsondb
DESTDIR = $$QT.jsondb.bins
target.path = $$[QT_INSTALL_BINS]
INSTALLS += target

LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

QT = core network declarative jsondbqson-private

mac:CONFIG -= app_bundle

include(daemon.pri)

HEADERS += \
    $$PWD/dbserver.h

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/dbserver.cpp
