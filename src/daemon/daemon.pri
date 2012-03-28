include($$PWD/../3rdparty/btree/btree.pri)
include($$PWD/../common/common.pri)

DEFINES += $$quote(QT_BEGIN_MOC_NAMESPACE=\"QT_USE_NAMESPACE QT_USE_NAMESPACE_JSONDB\")

RESOURCES = $$PWD/jsondb.qrc
HEADERS += \
    $$PWD/jsondbowner.h \
    $$PWD/jsondbproxy.h \
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
    $$PWD/jsondbmanagedbtree.h \
    $$PWD/jsondbmanagedbtreetxn.h \
    $$PWD/jsondbobjecttypes_impl_p.h \
    $$PWD/jsondbobjecttypes_p.h \
    $$PWD/jsondbreducedefinition.h \
    $$PWD/schema-validation/checkpoints.h \
    $$PWD/schema-validation/object.h \
    $$PWD/jsondbschemamanager_impl_p.h \
    $$PWD/jsondbschemamanager_p.h \
    $$PWD/jsondbscriptengine.h \
    $$PWD/jsondbsignals.h \
    $$PWD/jsondbsettings.h \
    $$PWD/jsondbindexquery.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    $$PWD/jsondbowner.cpp \
    $$PWD/jsondbproxy.cpp \
    $$PWD/jsondbephemeralpartition.cpp \
    $$PWD/jsondbindex.cpp \
    $$PWD/jsondbobject.cpp \
    $$PWD/jsondbpartition.cpp \
    $$PWD/jsondbquery.cpp \
    $$PWD/jsondbview.cpp \
    $$PWD/jsondbmapdefinition.cpp \
    $$PWD/jsondbnotification.cpp \
    $$PWD/jsondbobjecttable.cpp \
    $$PWD/jsondbmanagedbtree.cpp \
    $$PWD/jsondbmanagedbtreetxn.cpp \
    $$PWD/jsondbreducedefinition.cpp \
    $$PWD/jsondbscriptengine.cpp \
    $$PWD/jsondbsignals.cpp \
    $$PWD/jsondbsettings.cpp \
    $$PWD/jsondbindexquery.cpp

SOURCES += $$QSONCONVERSION_SOURCES
