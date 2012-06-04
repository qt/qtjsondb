TARGET     = jsondbplugin
TARGETPATH = QtJsonDb

include(../qimportbase.pri)

QT += network qml jsondb jsondb-private

DESTDIR = $$QT.jsondb.imports/$$TARGETPATH
target.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

qmldir.files += $$PWD/qmldir
qmldir.path += $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

INSTALLS += target qmldir

#rules for qmltypes
contains(QMAKE_HOST.arch, x86) | contains(QMAKE_HOST.arch, x86_64) {
    qtPrepareTool(QMLPLUGINDUMP, qmlplugindump)
    mac: !exists($$QMLPLUGINDUMP): QMLPLUGINDUMP = "$${QMLPLUGINDUMP}.app/Contents/MacOS/qmlplugindump"
    unix:!mac: QMLPLUGINDUMP = "$${QMLPLUGINDUMP} -platform minimal"
    QMLTYPESFILE = $$QT.jsondb.imports/$$TARGETPATH/plugin.qmltypes
    QMAKE_POST_LINK += LD_LIBRARY_PATH=$$QT.jsondb.libs $$QMLPLUGINDUMP QtJsonDb 1.0 $$QT.jsondb.imports > $$QMLTYPESFILE

    qmltypes.files = $$QMLTYPESFILE
    qmltypes.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH
    INSTALLS += qmltypes
}

VERSION = 1.0

HEADERS += \
    jsondbpartition.h \
    jsondbnotification.h \
    plugin.h \
    jsondatabase.h \
    jsondbqueryobject.h \
    jsondbsortinglistmodel_p.h \
    jsondbsortinglistmodel.h \
    jsondblistmodel.h

SOURCES += \
    jsondbpartition.cpp \
    jsondbnotification.cpp \
    plugin.cpp \
    jsondatabase.cpp \
    jsondbqueryobject.cpp \
    jsondbsortinglistmodel.cpp \
    jsondblistmodel.cpp

OTHER_FILES += jsondb.json
