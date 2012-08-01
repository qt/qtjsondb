TARGET = tst_accesscontrol
CONFIG += debug

QT = network qml testlib jsondbpartition jsondbpartition-private
CONFIG -= app_bundle
CONFIG += testcase

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES+= accesscontrol.qrc
SOURCES += \
    testjsondb.cpp \

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
