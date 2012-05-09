TARGET = tst_accesscontrol
CONFIG += debug

QT = network qml testlib jsondbpartition jsondbpartition-private
CONFIG -= app_bundle
CONFIG += testcase

LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES+= accesscontrol.qrc
SOURCES += \
    testjsondb.cpp \
