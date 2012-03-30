TEMPLATE = app
TARGET = tst_bench_jsondbsortinglistmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib gui qml jsondbcompat-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += jsondbsortinglistmodel-bench.h
SOURCES += jsondbsortinglistmodel-bench.cpp
