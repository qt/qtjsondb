QT += jsondb jsondb-private

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/util.h \
    $$PWD/qmltestutil.h

contains(QT, jsondbcompat|jsondbcompat-private) {
    HEADERS += $$PWD/clientwrapper.h
    SOURCES += $$PWD/clientwrapper.cpp
} else {
    HEADERS += $$PWD/testhelper.h
    SOURCES += $$PWD/testhelper.cpp
}

RESOURCES += \
    $$PWD/../json.qrc

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
