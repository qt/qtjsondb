include($$PWD/../3rdparty/btree/btree.pri)
include($$PWD/../hbtree/hbtree.pri)
include($$PWD/../common/common.pri)

DEFINES += $$quote(QT_BEGIN_MOC_NAMESPACE=\"QT_USE_NAMESPACE QT_USE_NAMESPACE_JSONDB\")

RESOURCES = $$PWD/jsondb.qrc
HEADERS += \
    $$PWD/jsondbowner.h \
    $$PWD/jsondbproxy.h \
    $$PWD/jsondbresponse.h \
    $$PWD/jsondb.h \
    $$PWD/jsondbephemeralpartition.h \
    $$PWD/jsondbindex.h \
    $$PWD/jsondbobject.h \
    $$PWD/jsondbpartition.h \
    $$PWD/jsondbquery.h \
    $$PWD/jsondbstat.h \
    $$PWD/jsondbview.h \
    $$PWD/jsondbmapdefinition.h \
    $$PWD/jsondbnotification.h \
    $$PWD/jsondbobjectkey.h \
    $$PWD/jsondbobjecttable.h \
    $$PWD/jsondbbtree.h \
    $$PWD/jsondbobjecttypes_impl_p.h \
    $$PWD/jsondbobjecttypes_p.h \
    $$PWD/jsondbreducedefinition.h \
    $$PWD/schema-validation/checkpoints.h \
    $$PWD/schema-validation/object.h \
    $$PWD/jsondbschemamanager_impl_p.h \
    $$PWD/jsondbschemamanager_p.h \
    $$PWD/jsondbsignals.h \
    $$PWD/jsondbsettings.h \

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    $$PWD/jsondbowner.cpp \
    $$PWD/jsondbproxy.cpp \
    $$PWD/jsondbresponse.cpp \
    $$PWD/jsondb.cpp \
    $$PWD/jsondbephemeralpartition.cpp \
    $$PWD/jsondbindex.cpp \
    $$PWD/jsondbobject.cpp \
    $$PWD/jsondbpartition.cpp \
    $$PWD/jsondbquery.cpp \
    $$PWD/jsondbview.cpp \
    $$PWD/jsondbmapdefinition.cpp \
    $$PWD/jsondbnotification.cpp \
    $$PWD/jsondbobjecttable.cpp \
    $$PWD/jsondbbtree.cpp \
    $$PWD/jsondbreducedefinition.cpp \
    $$PWD/jsondbsignals.cpp \
    $$PWD/jsondbsettings.cpp

SOURCES += $$QSONCONVERSION_SOURCES
