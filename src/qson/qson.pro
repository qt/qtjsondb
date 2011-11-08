TEMPLATE = lib
TARGET = $$QT.jsondbqson.name
MODULE = jsondbqson

load(qt_module)
load(qt_module_config)

include(../../qtjsondb.pri)

DESTDIR = $$QT.jsondbqson.libs
VERSION = $$QT.jsondbqson.VERSION
DEFINES += QT_ADDON_JSONDB_QSON_LIB

QT = core

CONFIG += module create_prl
MODULE_PRI = ../../modules/qt_jsondb_qson.pri

include(../3rdparty/qjson/qjson.pri)

HEADERS += \
    qsonelement_p.h \
    qson_p.h \
    qsonlist_p.h \
    qsonmap_p.h \
    qsonobject_p.h \
    qsonpage_p.h \
    qsonparser_p.h \
    qsonstrings_p.h \
    qsonuuid_p.h \
    qsonversion_p.h \

SOURCES += \
    qson.cpp \
    qsonelement.cpp \
    qsonlist.cpp \
    qsonmap.cpp \
    qsonobject.cpp \
    qsonpage.cpp \
    qsonparser.cpp \
    qsonstrings.cpp \
    qsonuuid.cpp \
    qsonversion.cpp \

mac:QMAKE_FRAMEWORK_BUNDLE_NAME = $$QT.jsondbqson.name
