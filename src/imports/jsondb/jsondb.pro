TARGET     = jsondbplugin
TARGETPATH = QtJsonDb

include(../qimportbase.pri)

QT += network declarative jsondb-private jsondbqson-private

DESTDIR = $$QT.jsondb.imports/$$TARGETPATH
target.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

qmldir.files += $$PWD/qmldir
qmldir.path += $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

INSTALLS += target qmldir

VERSION = 1.0

include(../../common/common.pri)

HEADERS += \
    jsondblistmodel.h \
    jsondblistmodel_p.h \
    jsondbcomponent.h \
    plugin.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    jsondblistmodel.cpp \
    jsondbcomponent.cpp \
    plugin.cpp

SOURCES += $$QSONCONVERSION_SOURCES
