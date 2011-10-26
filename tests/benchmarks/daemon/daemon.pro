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
