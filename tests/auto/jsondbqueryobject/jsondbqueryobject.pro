TEMPLATE = app
TARGET = tst_jsondbqueryobject
DEPENDPATH += .
INCLUDEPATH += . ../../shared/

QT = core network testlib gui qml jsondb
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += testjsondbqueryobject.h \
           $$PWD/../../shared/requestwrapper.h
SOURCES += testjsondbqueryobject.cpp

OTHER_FILES += \
    partitions.json
