TARGET = jsondb-client
DESTDIR = $$QT.jsondb.bins

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target

QT = core network declarative jsondb

LIBS += -ledit

include(../../qtjsondb.pri)
include(../../src/3rdparty/qjson/qjson.pri)

mac:CONFIG -= app_bundle

HEADERS += client.h
SOURCES += main.cpp client.cpp
