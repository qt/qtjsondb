TEMPLATE = app
TARGET = tst_jsondb-listmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib declarative jsondb-private jsondbqson-private
CONFIG -= app_bundle

include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += $$PWD/../../../src/imports/jsondb
HEADERS += $$PWD/../../../src/imports/jsondb/jsondb-listmodel.h
SOURCES += $$PWD/../../../src/imports/jsondb/jsondb-listmodel.cpp

HEADERS += test-jsondb-listmodel.h
SOURCES += test-jsondb-listmodel.cpp

check.target = check
check.commands = rm -f *.db* && LD_LIBRARY_PATH=$$PWD/../../../lib QT_QPA_PLATFORM=minimal ./tst_jsondb-listmodel -xunitxml -silent > ../../../tst_jsondb-listmodel.xml
QMAKE_EXTRA_TARGETS = check
