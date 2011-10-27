TARGET     = jsondblistmodelplugin
TARGETPATH = QtAddOn/JsonDb

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
    jsondb-listmodel.h \
    jsondb-listmodel_p.h \
    javascript-listmodel.h \
    jsondb-singletonwatcher.h \
    jsondb-watcher.h \
    jsondb-component.h \
    cuid.h \
    plugin.h

HEADERS += $$QSONCONVERSION_HEADERS

SOURCES += \
    jsondb-listmodel.cpp \
    javascript-listmodel.cpp \ 
    jsondb-singletonwatcher.cpp \
    jsondb-watcher.cpp \
    jsondb-component.cpp \
    cuid.cpp \
    plugin.cpp

SOURCES += $$QSONCONVERSION_SOURCES
