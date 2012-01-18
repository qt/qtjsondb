TARGET = jsondb-client
DESTDIR = $$QT.jsondb.bins

target.path = $$[QT_INSTALL_PREFIX]/bin
INSTALLS += target

QT = core jsondb

LIBS += -ledit

include(../../qtjsondb.pri)

mac:CONFIG -= app_bundle

HEADERS += client.h
SOURCES += main.cpp client.cpp
