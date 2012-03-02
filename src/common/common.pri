INCLUDEPATH += $$PWD

include(../3rdparty/qjson/qjson.pri)

HEADERS += \
    $$PWD/jsondb-global.h \
    $$PWD/jsondb-error.h \
    $$PWD/jsondb-strings.h \
    $$PWD/jsonstream.h

SOURCES += \
    $$PWD/jsondb-strings.cpp \
    $$PWD/jsonstream.cpp
