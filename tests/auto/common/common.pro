TARGET = tst_common

QT = declarative network testlib jsondbqson-private
CONFIG -= app_bundle

include($$PWD/../../../src/common/common.pri)

HEADERS += \
    $$QSONCONVERSION_HEADERS

SOURCES += \
    test-common.cpp \
    $$QSONCONVERSION_SOURCES

check.target = check
check.commands = ./tst_common -xunitxml -silent > ../../../tst_common.xml
QMAKE_EXTRA_TARGETS = check
