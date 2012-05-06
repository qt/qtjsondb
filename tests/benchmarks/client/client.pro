TEMPLATE = app
TARGET = tst_bench_client

QT = core network testlib jsondb jsondb-private

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"

CONFIG += qtestlib
CONFIG -= app_bundle

include($$PWD/../../shared/shared.pri)

SOURCES += client-benchmark.cpp
OTHER_FILES += partitions.json
