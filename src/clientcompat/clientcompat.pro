MODULE = jsondbcompat
TARGET = QtJsonDbCompat
VERSION = 1.0.0

QT = core
QT_FOR_PRIVATE = network
CONFIG += internal_module

load(qt_module)

include(../jsonstream/jsonstream.pri)

INCLUDEPATH += $$PWD/../common

HEADERS += \
    jsondb-error.h \
    jsondb-client.h \
    jsondb-object.h \
    jsondb-client_p.h \
    jsondb-connection_p.h \
    jsondb-connection_p_p.h \
    jsondb-query.h \
    jsondb-oneshot_p.h \
    jsondb-strings_p.h \
    jsondb-notification.h

SOURCES += \
    jsondb-error.cpp \
    jsondb-client.cpp \
    jsondb-object.cpp \
    jsondb-connection.cpp \
    jsondb-query.cpp \
    jsondb-oneshot.cpp \
    jsondb-notification.cpp \
    jsondb-strings.cpp
