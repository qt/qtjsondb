TARGET     = jsondbplugin
TARGETPATH = QtJsonDb

include(../qimportbase.pri)

QT += network qml jsondbcompat-private

DESTDIR = $$QT.jsondbcompat.imports/$$TARGETPATH
target.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

qmldir.files += $$PWD/qmldir
qmldir.path += $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

INSTALLS += target qmldir

#rules for qmltypes
!cross_compile {
    qtPrepareTool(QMLPLUGINDUMP, qmlplugindump)
    mac: !exists($$QMLPLUGINDUMP): QMLPLUGINDUMP = "$${QMLPLUGINDUMP}.app/Contents/MacOS/qmlplugindump"
    QMLTYPESFILE = $$QT.jsondbcompat.imports/$$TARGETPATH/plugin.qmltypes
    QMAKE_POST_LINK += LD_LIBRARY_PATH=$$QT.jsondbcompat.libs $$QMLPLUGINDUMP QtJsonDb 1.0 $$QT.jsondbcompat.imports > $$QMLTYPESFILE

    qmltypes.files = $$QMLTYPESFILE
    qmltypes.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH
    INSTALLS += qmltypes
}

VERSION = 1.0

include(../../common/common.pri)

HEADERS += \
    jsondbpartition.h \
    jsondbnotification.h \
    jsondbsortinglistmodel.h \
    jsondbsortinglistmodel_p.h \
    jsondblistmodel.h \
    jsondblistmodel_p.h \
    plugin.h \
    jsondatabase.h \
    jsondbqueryobject.h \
    jsondbchangessinceobject.h \
    jsondbmodelcache.h \
    jsondbcachinglistmodel_p.h \
    jsondbcachinglistmodel.h \
    jsondbmodelutils.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    jsondbpartition.cpp \
    jsondbnotification.cpp \
    jsondbsortinglistmodel.cpp \
    jsondblistmodel.cpp \
    plugin.cpp \
    jsondatabase.cpp \
    jsondbqueryobject.cpp \
    jsondbchangessinceobject.cpp \
    jsondbcachinglistmodel.cpp \
    jsondbmodelcache.cpp \
    jsondbmodelutils.cpp

SOURCES += $$QSONCONVERSION_SOURCES
