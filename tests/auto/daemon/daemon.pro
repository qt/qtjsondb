TARGET = tst_daemon

QT = network declarative testlib jsondbqson-private
CONFIG -= app_bundle
CONFIG += debug

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs
!mac:LIBS += -lssl -lcrypto

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../../src/daemon/daemon.pri)
RESOURCES += json-validation.qrc

SOURCES += \
    testjsondb.cpp \


check.target = check
check.commands = rm -f *.db* && LD_LIBRARY_PATH=$$PWD/../../../lib QT_QPA_PLATFORM=xcb ./tst_daemon -xunitxml -silent > ../../../tst_server.xml
QMAKE_EXTRA_TARGETS = check
