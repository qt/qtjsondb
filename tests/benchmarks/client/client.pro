TEMPLATE = app
TARGET = tst_bench_client

QT = core network testlib jsondb jsondb-private

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"

CONFIG += testcase
CONFIG -= app_bundle

include($$PWD/../../shared/shared.pri)

SOURCES += client-benchmark.cpp
OTHER_FILES += partitions.json

data.files = $$OTHER_FILES
data.path = $$[QT_INSTALL_TESTS]/$$TARGET
INSTALLS += data
