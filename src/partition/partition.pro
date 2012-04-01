TEMPLATE = lib
TARGET = $$QT.jsondbpartition.name
MODULE = jsondbpartition

load(qt_module)
load(qt_module_config)

DESTDIR = $$QT.jsondbpartition.libs
VERSION = $$QT.jsondbpartition.VERSION
DEFINES += QT_JSONDB_PARTITION_LIB

QT = core network qml

CONFIG += module create_prl
MODULE_PRI = ../../modules/qt_jsondbpartition.pri

include(../3rdparty/btree/btree.pri)
include(../common/common.pri)

RESOURCES = jsondb.qrc

HEADERS += \
    jsondbowner.h \
    jsondbproxy.h \
    jsondbindex.h \
    jsondbobject.h \
    jsondbpartition.h \
    jsondbquery.h \
    jsondbstat.h \
    jsondbview.h \
    jsondbmapdefinition.h \
    jsondbnotification.h \
    jsondbobjectkey.h \
    jsondbobjecttable.h \
    jsondbmanagedbtree.h \
    jsondbmanagedbtreetxn.h \
    jsondbobjecttypes_impl_p.h \
    jsondbobjecttypes_p.h \
    jsondbreducedefinition.h \
    schema-validation/checkpoints.h \
    schema-validation/object.h \
    jsondbschemamanager_impl_p.h \
    jsondbschemamanager_p.h \
    jsondbscriptengine.h \
    jsondbsettings.h \
    jsondbindexquery.h \
    jsondberrors.h \
    jsondbstrings.h \
    jsondbpartitionglobal.h \
    jsondbcollator.h \
    jsondbcollator_p.h

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
    jsondbmanagedbtree.cpp \
    jsondbmanagedbtreetxn.cpp \
    jsondbreducedefinition.cpp \
    jsondbscriptengine.cpp \
    jsondbsettings.cpp \
    jsondbindexquery.cpp \
    jsondberrors.cpp \
    jsondbstrings.cpp \
    jsondbcollator.cpp

mac:QMAKE_FRAMEWORK_BUNDLE_NAME = $$QT.jsondbpartition.name

unix:!mac:contains(QT_CONFIG,icu) {
    LIBS += -licuuc -licui18n
} else {
    DEFINES += NO_COLLATION_SUPPORT
}
