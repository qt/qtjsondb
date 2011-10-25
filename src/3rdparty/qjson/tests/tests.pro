QT += testlib

include(../src/json.pri)
SOURCES += tst_json.cpp

TARGET = tst_json

wince {
   DEFINES += SRCDIR=\\\"\\\"
} else {
    DEFINES += SRCDIR=\\\"$$PWD/\\\"
}

