include($$PWD/../3rdparty/btree/btree.pri)
include($$PWD/../common/common.pri)

DEFINES += $$quote(QT_BEGIN_MOC_NAMESPACE=\"QT_USE_NAMESPACE QT_USE_NAMESPACE_JSONDB\")

RESOURCES = $$PWD/jsondb.qrc
HEADERS += \
    $$PWD/jsondb-owner.h \
    $$PWD/jsondb-proxy.h \
    $$PWD/jsondb-response.h \
    $$PWD/jsondb.h \
    $$PWD/jsondbephemeralstorage.h \
    $$PWD/jsondbindex.h \
    $$PWD/jsondbobject.h \
    $$PWD/jsondbbtreestorage.h \
    $$PWD/jsondbquery.h \
    $$PWD/jsondbstat.h \
    $$PWD/jsondbview.h \
    $$PWD/jsondbmapdefinition.h \
    $$PWD/notification.h \
    $$PWD/objectkey.h \
    $$PWD/objecttable.h \
    $$PWD/qmanagedbtree.h \
    $$PWD/qmanagedbtreetxn.h \
    $$PWD/qsonobjecttypes_impl_p.h \
    $$PWD/qsonobjecttypes_p.h \
    $$PWD/jsondbreducedefinition.h \
    $$PWD/schema-validation/checkpoints.h \
    $$PWD/schema-validation/object.h \
    $$PWD/schemamanager_impl_p.h \
    $$PWD/schemamanager_p.h \
    $$PWD/signals.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    $$PWD/jsondb-owner.cpp \
    $$PWD/jsondb-proxy.cpp \
    $$PWD/jsondb-response.cpp \
    $$PWD/jsondb.cpp \
    $$PWD/jsondbephemeralstorage.cpp \
    $$PWD/jsondbindex.cpp \
    $$PWD/jsondbobject.cpp \
    $$PWD/jsondbbtreestorage.cpp \
    $$PWD/jsondbquery.cpp \
    $$PWD/jsondbview.cpp \
    $$PWD/jsondbmapdefinition.cpp \
    $$PWD/notification.cpp \
    $$PWD/objecttable.cpp \
    $$PWD/qmanagedbtree.cpp \
    $$PWD/qmanagedbtreetxn.cpp \
    $$PWD/jsondbreducedefinition.cpp \
    $$PWD/signals.cpp

SOURCES += $$QSONCONVERSION_SOURCES
