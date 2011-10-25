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

check.target = check
check.commands = LD_LIBRARY_PATH=../../../lib ./tst_bench_client -xunitxml -silent > ../../../tst_bench_client.xml
QMAKE_EXTRA_TARGETS = check
