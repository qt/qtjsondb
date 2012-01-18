INCLUDEPATH += $$PWD

include(../../qtjsondb.pri)

HEADERS += $$PWD/util.h $$PWD/qmltestutil.h

contains(QT, jsondbcompat|jsondbcompat-private) {
    HEADERS += $$PWD/clientwrapper.h
    SOURCES += $$PWD/clientwrapper.cpp
}

RESOURCES += \
    ../../json.qrc

