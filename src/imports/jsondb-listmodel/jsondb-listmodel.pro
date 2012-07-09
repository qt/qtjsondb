CXX_MODULE = jsondb
TARGET     = jsondblistmodelplugin
TARGETPATH = QtAddOn/JsonDb

QT += network qml jsondbcompat-private

HEADERS += \
    jsondb-listmodel.h \
    jsondb-listmodel_p.h \
    jsondb-component.h \
    plugin.h

SOURCES += \
    jsondb-listmodel.cpp \
    jsondb-component.cpp \
    plugin.cpp

load(qml_plugin)

OTHER_FILES += jsondb.json
