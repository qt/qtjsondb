load(qt_build_config)

MODULE = jsondb
TARGET = QtJsonDb
VERSION = 1.0.0

QT = core
QT_FOR_PRIVATE = network qml jsondbpartition

load(qt_module_config)

include(../jsonstream/jsonstream.pri)
INCLUDEPATH += $$PWD/../common

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
    qjsondbprivatepartition_p.h \
    qjsondbstandardpaths_p.h \
    qjsondblogrequest_p.h \
    qjsondblogrequest_p_p.h \
    qjsondbquerymodel_p_p.h \
    qjsondbmodelcache_p.h \
    qjsondbmodelutils_p.h \
    qjsondbquerymodel_p.h


SOURCES += \
    qjsondbconnection.cpp \
    qjsondbrequest.cpp \
    qjsondbreadrequest.cpp \
    qjsondbwriterequest.cpp \
    qjsondbflushrequest_p.cpp \
    qjsondbwatcher.cpp \
    qjsondbobject.cpp \
    qjsondbprivatepartition.cpp \
    qjsondbstandardpaths.cpp \
    qjsondblogrequest.cpp \
    qjsondbmodelcache_p.cpp \
    qjsondbmodelutils_p.cpp \
    qjsondbquerymodel_p.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
