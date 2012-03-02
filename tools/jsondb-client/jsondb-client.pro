TARGET = jsondb-client
DESTDIR = $$QT.jsondb.bins

target.path = $$[QT_INSTALL_PREFIX]/bin
INSTALLS += target

QT = core jsondb declarative gui

LIBS += -ledit

mac:CONFIG -= app_bundle

HEADERS += client.h
SOURCES += main.cpp client.cpp

!contains(DEFINES, QTJSONDB_NO_DEPRECATED) {
    HEADERS += jsondbproxy.h
    SOURCES += jsondbproxy.cpp
}

