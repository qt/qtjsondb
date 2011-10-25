TARGET = tst_client

QT = network testlib jsondb-private jsondbqson-private
CONFIG -= app_bundle

include($$PWD/../../shared/shared.pri)
include($$PWD/../../../src/3rdparty/qjson/qjson.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += test-jsondb-client.cpp

check.target = check
check.commands = rm -f *.db* && LD_LIBRARY_PATH=$$PWD/../../../lib QT_QPA_PLATFORM=xcb ./tst_client -xunitxml -silent > ../../../tst_client.xml
QMAKE_EXTRA_TARGETS = check

