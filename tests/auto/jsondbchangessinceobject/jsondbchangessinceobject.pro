TEMPLATE = app
TARGET = tst_jsondbchangessinceobject
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib gui declarative jsondbcompat-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)
include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += testjsondbchangessinceobject.h
SOURCES += testjsondbchangessinceobject.cpp
