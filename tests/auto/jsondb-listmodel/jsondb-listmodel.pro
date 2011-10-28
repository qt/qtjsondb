TEMPLATE = app
TARGET = tst_jsondb-listmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib declarative jsondb-private jsondbqson-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += $$PWD/../../../src/imports/jsondb-listmodel
HEADERS += $$PWD/../../../src/imports/jsondb-listmodel/jsondb-listmodel.h
SOURCES += $$PWD/../../../src/imports/jsondb-listmodel/jsondb-listmodel.cpp

HEADERS += test-jsondb-listmodel.h
SOURCES += test-jsondb-listmodel.cpp
