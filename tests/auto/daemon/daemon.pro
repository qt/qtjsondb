TARGET = tst_daemon
CONFIG += debug

QT = network qml testlib
CONFIG -= app_bundle
CONFIG += testcase

INCLUDEPATH += $$PWD/../../../src/daemon
LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../../src/daemon/daemon.pri)
RESOURCES += json-validation.qrc daemon.qrc

SOURCES += \
    testjsondb.cpp \
