TARGET     = jsondbplugin
TARGETPATH = QtJsonDb

include(../qimportbase.pri)

QT += network declarative jsondb-private

DESTDIR = $$QT.jsondb.imports/$$TARGETPATH
target.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

qmldir.files += $$PWD/qmldir
qmldir.path += $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

INSTALLS += target qmldir

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
    jsondbchangessinceobject.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    jsondbpartition.cpp \
    jsondbnotification.cpp \
    jsondbsortinglistmodel.cpp \
    jsondblistmodel.cpp \
    plugin.cpp \
    jsondatabase.cpp \
    jsondbqueryobject.cpp \
    jsondbchangessinceobject.cpp

SOURCES += $$QSONCONVERSION_SOURCES
