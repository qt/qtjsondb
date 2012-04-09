TEMPLATE = lib
TARGET = $$QT.jsondb.name
MODULE = jsondb

load(qt_module)
load(qt_module_config)

DESTDIR = $$QT.jsondb.libs
VERSION = $$QT.jsondb.VERSION
DEFINES += QT_JSONDB_LIB

QT = core network jsondbpartition

CONFIG += module create_prl
MODULE_PRI = ../../modules/qt_jsondb.pri

include(../jsonstream/jsonstream.pri)

HEADERS += qtjsondbversion.h

HEADERS += \
    qjsondbglobal.h \
    qjsondbstrings_p.h \
    qjsondbconnection_p.h \
    qjsondbconnection.h \
    qjsondbrequest_p.h \
    qjsondbrequest.h \
    qjsondbreadrequest_p.h \
    qjsondbreadrequest.h \
    qjsondbwriterequest_p.h \
    qjsondbwriterequest.h \
    qjsondbflushrequest_p_p.h \
    qjsondbflushrequest_p.h \
    qjsondbwatcher_p.h \
    qjsondbwatcher.h \
    qjsondbobject.h \
    qjsondbprivatepartition_p.h

SOURCES += \
    qjsondbconnection.cpp \
    qjsondbrequest.cpp \
    qjsondbreadrequest.cpp \
    qjsondbwriterequest.cpp \
    qjsondbflushrequest_p.cpp \
    qjsondbwatcher.cpp \
    qjsondbobject.cpp \
    qjsondbprivatepartition.cpp

mac:QMAKE_FRAMEWORK_BUNDLE_NAME = $$QT.jsondb.name
