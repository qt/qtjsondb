TARGET = tst_bench_daemon

QT = network declarative testlib jsondbqson-private
CONFIG -= app_bundle

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../../src/daemon/daemon.pri)

SOURCES += \
    bench_daemon.cpp \

check.target = check
check.commands = rm -f *.db* && LD_LIBRARY_PATH=$$PWD/../../../lib QT_QPA_PLATFORM=xcb ./tst_bench_daemon -xunitxml -silent > ../../../tst_bench_daemon.xml
QMAKE_EXTRA_TARGETS = check
