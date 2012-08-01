TARGET = tst_bench_jsondbcachinglistmodel

QT = core network testlib gui qml jsondb
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += jsondbcachinglistmodel-bench.h \
            $$PWD/../../shared/requestwrapper.h
SOURCES += jsondbcachinglistmodel-bench.cpp

OTHER_FILES += partitions.json

data.files = $$OTHER_FILES
data.path = $$[QT_INSTALL_TESTS]/$$TARGET
INSTALLS += data
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
