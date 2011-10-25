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

check.target = check
check.commands = QT_QPA_PLATFORM=minimal ./tst_bench_listmodel -xunitxml -silent > ../../../tst_bench_listmodel.xml
QMAKE_EXTRA_TARGETS = check
