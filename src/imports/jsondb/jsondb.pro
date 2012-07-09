CXX_MODULE = jsondb
TARGET     = jsondbplugin

QT += network qml jsondb jsondb-private

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

load(qml_plugin)

OTHER_FILES += jsondb.json
