INCLUDEPATH += $$PWD

include(../3rdparty/qjson/qjson.pri)

QSONCONVERSION_HEADERS = \
    $$PWD/qsonconversion.h
QSONCONVERSION_SOURCES = \
    $$PWD/qsonconversion.cpp

HEADERS += \
    $$PWD/jsondb-global.h \
    $$PWD/jsondb-error.h \
    $$PWD/jsondb-strings.h \
    $$PWD/qsonstream.h

SOURCES += \
    $$PWD/jsondb-strings.cpp \
    $$PWD/qsonstream.cpp
