TEMPLATE = app
TARGET = tst_bench_jsondbcachinglistmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib gui declarative jsondbcompat-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)
include($$PWD/../../../qtjsondb.pri)
include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += jsondbcachinglistmodel-bench.h
SOURCES += jsondbcachinglistmodel-bench.cpp
