TEMPLATE = app
TARGET = tst_bench_listmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib gui declarative jsondb-private jsondbqson-private
CONFIG -= app_bundle

include($$PWD/../../shared/shared.pri)
include($$PWD/../../../qtjsondb.pri)
include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"

INCLUDEPATH += $$PWD/../../../src/imports/jsondb-listmodel
HEADERS += $$PWD/../../../src/imports/jsondb-listmodel/jsondb-listmodel.h
SOURCES += $$PWD/../../../src/imports/jsondb-listmodel/jsondb-listmodel.cpp

# Input
HEADERS += listmodel-benchmark.h
SOURCES += listmodel-benchmark.cpp
