include($$PWD/../3rdparty/btree/btree.pri)
include($$PWD/../common/common.pri)

RESOURCES = $$PWD/jsondb.qrc
HEADERS += \
    $$PWD/aodb.h \
    $$PWD/jsondb-map-reduce.h \
    $$PWD/jsondb-owner.h \
    $$PWD/jsondb-proxy.h \
    $$PWD/jsondb-trace.h \
    $$PWD/jsondb.h \
    $$PWD/jsondbbtreestorage.h \
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
    $$PWD/qsonobjecttypes_impl_p.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    $$PWD/aodb.cpp \
    $$PWD/jsondb-map-reduce.cpp \
    $$PWD/jsondb-owner.cpp \
    $$PWD/jsondb-proxy.cpp \
    $$PWD/jsondb-trace.cpp \
    $$PWD/jsondb.cpp \
    $$PWD/jsondbbtreestorage.cpp \
    $$PWD/jsondbindex.cpp \
    $$PWD/jsondbquery.cpp \
    $$PWD/notification.cpp \
    $$PWD/objecttable.cpp \
    $$PWD/signals.cpp

SOURCES += $$QSONCONVERSION_SOURCES
