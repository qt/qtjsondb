TEMPLATE = app
TARGET = tst_bench_client

QT = core network testlib jsondb jsondbcompat-private

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"

CONFIG += qtestlib
CONFIG -= app_bundle

include($$PWD/../../shared/shared.pri)

HEADERS += client-benchmark.h
SOURCES += client-benchmark.cpp
