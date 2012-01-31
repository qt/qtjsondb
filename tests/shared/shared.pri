INCLUDEPATH += $$PWD

include(../../qtjsondb.pri)

HEADERS += $$PWD/util.h $$PWD/clientwrapper.h $$PWD/qmltestutil.h
SOURCES += $$PWD/clientwrapper.cpp

RESOURCES += \
    ../../json.qrc

