TARGET = jsondb
DESTDIR = $$QT.jsondb.bins
target.path = $$[QT_INSTALL_PREFIX]/bin
INSTALLS += target

LIBS += -L$$QT.jsondb.libs

QT = core network qml

mac:CONFIG -= app_bundle

include(daemon.pri)

HEADERS += \
    $$PWD/dbserver.h

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/dbserver.cpp

systemd {
    DEFINES += USE_SYSTEMD
    LIBS += -lsystemd-daemon
}
