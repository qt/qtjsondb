include($$PWD/../../../src/3rdparty/btree/btree.pri)

TARGET = tst_bdb

QT = network testlib
CONFIG -= app_bundle
CONFIG += debug

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -lssl -lcrypto

SOURCES += \
    tst_jsondb_bdb.cpp \
    ../../../src/daemon/aodb.cpp

check.target = check
check.commands = rm -f *.db* && QT_QPA_PLATFORM=xcb ./tst_bdb -xunitxml -silent > ../../../tst_bdb.xml
QMAKE_EXTRA_TARGETS = check
