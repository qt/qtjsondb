TARGET = tst_bench_daemon

target.path = $$[QT_INSTALL_PREFIX]/bin
INSTALLS += target

QT = network declarative testlib
CONFIG -= app_bundle
CONFIG += testcase

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

DEFINES += SRCDIR=\\\"$$PWD/\\\"
include($$PWD/../../../src/daemon/daemon.pri)
RESOURCES+=../../json.qrc
SOURCES += \
    bench_daemon.cpp \
