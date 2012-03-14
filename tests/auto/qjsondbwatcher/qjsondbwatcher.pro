TARGET = tst_qjsondbwatcher

QT = network testlib jsondb-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += testqjsondbwatcher.cpp
