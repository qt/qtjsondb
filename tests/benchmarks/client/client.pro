TEMPLATE = app
TARGET = tst_bench_client
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib jsondb jsondb-private

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"

INCLUDEPATH += "../../../src/common"
INCLUDEPATH += "../../../src/3rdparty/qjson/src"
SOURCES += ../../../src/3rdparty/qjson/src/json.cpp

CONFIG += qtestlib
CONFIG -= app_bundle

include($$PWD/../../shared/shared.pri)

# Input
HEADERS += client-benchmark.h
SOURCES += client-benchmark.cpp
