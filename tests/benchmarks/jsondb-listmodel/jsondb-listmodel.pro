TEMPLATE = app
TARGET = tst_bench_listmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib declarative jsondb-private jsondbqson-private
CONFIG -= app_bundle

include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"

INCLUDEPATH += $$PWD/../../../src/imports/jsondb
HEADERS += $$PWD/../../../src/imports/jsondb/jsondb-listmodel.h
SOURCES += $$PWD/../../../src/imports/jsondb/jsondb-listmodel.cpp

# Input
HEADERS += listmodel-benchmark.h
SOURCES += listmodel-benchmark.cpp
