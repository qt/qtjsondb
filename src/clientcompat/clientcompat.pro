load(qt_module)

MODULE = jsondbcompat
TARGET = QtJsonDbCompat
VERSION = 1.0.0

QT = core
QT_PRIVATE = network

load(qt_module_config)

CONFIG += module create_prl

include(../jsonstream/jsonstream.pri)

INCLUDEPATH += $$PWD/../common

HEADERS += qtjsondbcompatversion.h

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

mac:QMAKE_FRAMEWORK_BUNDLE_NAME = $$QT.jsondbcompat.name
