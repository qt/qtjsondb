TARGET     = jsondblistmodelplugin
TARGETPATH = QtAddOn/JsonDb

include(../qimportbase.pri)

QT += network qml jsondbcompat-private

DESTDIR = $$QT.jsondb.imports/$$TARGETPATH
target.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

qmldir.files += $$PWD/qmldir
qmldir.path += $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

INSTALLS += target qmldir

#rules for qmltypes
contains(QMAKE_HOST.arch, x86) | contains(QMAKE_HOST.arch, x86_64) {
    qtPrepareTool(QMLPLUGINDUMP, qmlplugindump)
    QMLTYPESFILE = $$QT.jsondb.imports/$$TARGETPATH/plugin.qmltypes
    mac: !exists($$QMLPLUGINDUMP): QMLPLUGINDUMP = "$${QMLPLUGINDUMP}.app/Contents/MacOS/qmlplugindump"
    unix:!mac: QMLPLUGINDUMP = "$${QMLPLUGINDUMP} -platform minimal"
    QMAKE_POST_LINK += LD_LIBRARY_PATH=$$QT.jsondb.libs $$QMLPLUGINDUMP QtAddOn.JsonDb 1.0 $$QT.jsondb.imports > $$QMLTYPESFILE

    qmltypes.files = $$QMLTYPESFILE
    qmltypes.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH
    INSTALLS += qmltypes
}

VERSION = 1.0

HEADERS += \
    jsondb-listmodel.h \
    jsondb-listmodel_p.h \
    jsondb-component.h \
    plugin.h

SOURCES += \
    jsondb-listmodel.cpp \
    jsondb-component.cpp \
    plugin.cpp

OTHER_FILES += jsondb.json
