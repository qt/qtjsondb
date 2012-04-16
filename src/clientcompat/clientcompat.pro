TEMPLATE = lib
TARGET = $$QT.jsondbcompat.name
MODULE = jsondbcompat

load(qt_module)
load(qt_module_config)

DESTDIR = $$QT.jsondbcompat.libs
VERSION = $$QT.jsondbcompat.VERSION

QT = core network

CONFIG += module create_prl
MODULE_PRI = ../../modules/qt_jsondbcompat.pri

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
