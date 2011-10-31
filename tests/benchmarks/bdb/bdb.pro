include($$PWD/../../../src/3rdparty/btree/btree.pri)

TARGET = tst_bench_bdb

QT = testlib
CONFIG -= app_bundle

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += \
    bench_bdb.cpp \
    ../../../src/daemon/aodb.cpp


check.target = check
check.commands = rm -f *.db* && LD_LIBRARY_PATH=$$PWD/../../../lib QT_QPA_PLATFORM=xcb ./tst_bench_bdb -xunitxml -silent > ../../../tst_bench_bdb.xml
QMAKE_EXTRA_TARGETS = check
