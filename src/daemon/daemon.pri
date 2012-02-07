include($$PWD/../3rdparty/btree/btree.pri)
include($$PWD/../common/common.pri)

DEFINES += $$quote(QT_BEGIN_MOC_NAMESPACE=\"QT_USE_NAMESPACE QT_USE_NAMESPACE_JSONDB\")

RESOURCES = $$PWD/jsondb.qrc
HEADERS += \
    $$PWD/jsondb-map-reduce.h \
    $$PWD/jsondb-owner.h \
    $$PWD/jsondb-proxy.h \
    $$PWD/jsondb-response.h \
    $$PWD/jsondb.h \
    $$PWD/jsondbbtreestorage.h \
    $$PWD/jsondbephemeralstorage.h \
    $$PWD/jsondbobject.h \
    $$PWD/jsondbindex.h \
    $$PWD/jsondbquery.h \
    $$PWD/notification.h \
    $$PWD/objectkey.h \
    $$PWD/objecttable.h \
    $$PWD/signals.h \
    $$PWD/schema-validation/object.h \
    $$PWD/schema-validation/checkpoints.h \
    $$PWD/schemamanager_impl_p.h \
    $$PWD/schemamanager_p.h \
    $$PWD/qsonobjecttypes_p.h \
    $$PWD/qsonobjecttypes_impl_p.h \
    $$PWD/qmanagedbtreetxn.h \
    $$PWD/qmanagedbtree.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    $$PWD/jsondb-map-reduce.cpp \
    $$PWD/jsondb-owner.cpp \
    $$PWD/jsondb-proxy.cpp \
    $$PWD/jsondb-response.cpp \
    $$PWD/jsondb.cpp \
    $$PWD/jsondbbtreestorage.cpp \
    $$PWD/jsondbephemeralstorage.cpp \
    $$PWD/jsondbobject.cpp \
    $$PWD/jsondbindex.cpp \
    $$PWD/jsondbquery.cpp \
    $$PWD/notification.cpp \
    $$PWD/objecttable.cpp \
    $$PWD/signals.cpp \
    $$PWD/qmanagedbtreetxn.cpp \
    $$PWD/qmanagedbtree.cpp

SOURCES += $$QSONCONVERSION_SOURCES
