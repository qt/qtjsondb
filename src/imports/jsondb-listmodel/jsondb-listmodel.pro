TARGET     = jsondblistmodelplugin
TARGETPATH = QtAddOn/JsonDb

include(../qimportbase.pri)

QT += network declarative jsondbcompat-private

DESTDIR = $$QT.jsondb.imports/$$TARGETPATH
target.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

qmldir.files += $$PWD/qmldir
qmldir.path += $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

INSTALLS += target qmldir

VERSION = 1.0

include(../../common/common.pri)

HEADERS += \
    jsondb-listmodel.h \
    jsondb-listmodel_p.h \
    jsondb-component.h \
    plugin.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    jsondb-listmodel.cpp \
    jsondb-component.cpp \
    plugin.cpp

SOURCES += $$QSONCONVERSION_SOURCES
