TARGET = tst_bench_jsondbsortinglistmodel

QT = core network testlib gui qml jsondb
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += jsondbsortinglistmodel-bench.h \
            $$PWD/../../shared/requestwrapper.h
SOURCES += jsondbsortinglistmodel-bench.cpp

OTHER_FILES += partitions.json

data.files = $$OTHER_FILES
data.path = $$[QT_INSTALL_TESTS]/$$TARGET
INSTALLS += data
