TEMPLATE = lib
TARGET = $$QT.jsondbpartition.name
MODULE = jsondbpartition

load(qt_module)
load(qt_module_config)

DESTDIR = $$QT.jsondbpartition.libs
VERSION = $$QT.jsondbpartition.VERSION

QT = core network qml

CONFIG += module create_prl
MODULE_PRI = ../../modules/qt_jsondbpartition.pri

include(../3rdparty/btree/btree.pri)
include(../hbtree/hbtree.pri)

RESOURCES = jsondb.qrc

HEADERS += \
    jsondbutils_p.h \
    jsondbowner.h \
    jsondbproxy.h \
    jsondbindex.h \
    jsondbindex_p.h \
    jsondbobject.h \
    jsondbpartition.h \
    jsondbquery.h \
    jsondbstat.h \
    jsondbview.h \
    jsondbmapdefinition.h \
    jsondbnotification.h \
    jsondbobjectkey.h \
    jsondbobjecttable.h \
    jsondbbtree.h \
    jsondbobjecttypes_impl_p.h \
    jsondbobjecttypes_p.h \
    jsondbreducedefinition.h \
    jsondbschemamanager_impl_p.h \
    jsondbschemamanager_p.h \
    jsondbscriptengine.h \
    jsondbsettings.h \
    jsondbindexquery.h \
    jsondberrors.h \
    jsondbstrings.h \
    jsondbpartitionglobal.h \
    jsondbcollator.h \
    jsondbcollator_p.h \
    jsondbpartition_p.h \
    jsondbpartitionspec.h \
    jsondbquerytokenizer_p.h \
    jsondbqueryparser.h \
    jsondbschema_p.h \
    jsondbcheckpoints_p.h

SOURCES += \
    jsondbowner.cpp \
    jsondbproxy.cpp \
    jsondbindex.cpp \
    jsondbobject.cpp \
    jsondbpartition.cpp \
    jsondbquery.cpp \
    jsondbview.cpp \
    jsondbmapdefinition.cpp \
    jsondbnotification.cpp \
    jsondbobjecttable.cpp \
    jsondbbtree.cpp \
    jsondbreducedefinition.cpp \
    jsondbscriptengine.cpp \
    jsondbsettings.cpp \
    jsondbindexquery.cpp \
    jsondberrors.cpp \
    jsondbstrings.cpp \
    jsondbcollator.cpp \
    jsondbquerytokenizer.cpp \
    jsondbqueryparser.cpp

mac:QMAKE_FRAMEWORK_BUNDLE_NAME = $$QT.jsondbpartition.name

contains(config_test_icu, yes) {
    LIBS += -licuuc -licui18n
} else {
    DEFINES += NO_COLLATION_SUPPORT
}
