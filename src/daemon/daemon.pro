TARGET = jsondb
DESTDIR = $$QT.jsondb.bins
target.path = $$[QT_INSTALL_PREFIX]/bin
INSTALLS += target

include($$PWD/../jsonstream/jsonstream.pri)

LIBS += -L$$QT.jsondb.libs

QT = core network qml jsondbpartition

mac:CONFIG -= app_bundle

# HACK, remove when jsondbpartition separates private api from public api
include(../3rdparty/btree/btree.pri)
include(../hbtree/hbtree.pri)
INCLUDEPATH += $$PWD/../common

HEADERS += \
    $$PWD/dbserver.h \
    $$PWD/jsondbephemeralpartition.h \
    $$PWD/jsondbsignals.h \
    $$PWD/../common/jsondbsocketname_p.h

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/dbserver.cpp \
    $$PWD/jsondbephemeralpartition.cpp \
    $$PWD/jsondbsignals.cpp

systemd {
    DEFINES += USE_SYSTEMD
    LIBS += -lsystemd-daemon
}
