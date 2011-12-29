TARGET = tst_bench_daemon

target.path = $$[QT_INSTALL_PREFIX]/bin
INSTALLS += target

QT = network declarative testlib jsondbqson-private
CONFIG -= app_bundle

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../../src/daemon/daemon.pri)

SOURCES += \
    bench_daemon.cpp \
