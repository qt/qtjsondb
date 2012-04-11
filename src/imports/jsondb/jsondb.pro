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
!cross_compile {
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
    jsondbmodelutils.h \
    jsondbmodelcache.h \
    jsondblistmodel.h \
    jsondblistmodel_p.h \
    jsondbsortinglistmodel_p.h \
    jsondbsortinglistmodel.h \
    jsondbcachinglistmodel_p.h \
    jsondbcachinglistmodel.h

SOURCES += \
    jsondbpartition.cpp \
    jsondbnotification.cpp \
    plugin.cpp \
    jsondatabase.cpp \
    jsondbqueryobject.cpp \
    jsondbmodelutils.cpp \
    jsondbmodelcache.cpp \
    jsondblistmodel.cpp \
    jsondbsortinglistmodel.cpp \
    jsondbcachinglistmodel.cpp

OTHER_FILES += jsondb.json
