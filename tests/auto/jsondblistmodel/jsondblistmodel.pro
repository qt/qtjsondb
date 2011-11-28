TEMPLATE = app
TARGET = tst_jsondblistmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib gui declarative jsondb-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)
include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += testjsondblistmodel.h
SOURCES += testjsondblistmodel.cpp
