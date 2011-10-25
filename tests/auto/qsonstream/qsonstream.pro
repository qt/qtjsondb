TARGET = tst_qsonstream

QT = core testlib network declarative jsondbqson-private
CONFIG -= app_bundle

include($$PWD/../../../src/common/common.pri)

SOURCES += test-qsonstream.cpp

check.target = check
check.commands = ./tst_qsonstream -xunitxml -silent > ../../../tst_qsonstream.xml
QMAKE_EXTRA_TARGETS = check
