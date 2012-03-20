TEMPLATE = app
TARGET = tst_jsondbcachinglistmodel
DEPENDPATH += .
INCLUDEPATH += . ../../shared/

QT = core network testlib gui qml jsondb
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += testjsondbcachinglistmodel.h \
           $$PWD/../../shared/requestwrapper.h
SOURCES += testjsondbcachinglistmodel.cpp
