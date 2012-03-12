TEMPLATE = app
TARGET = tst_jsondbqueryobject
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib gui qml jsondbcompat-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)
include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += testjsondbqueryobject.h
SOURCES += testjsondbqueryobject.cpp
