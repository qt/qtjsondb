TEMPLATE = app
TARGET = tst_jsondbsortinglistmodel
DEPENDPATH += .
INCLUDEPATH += . ../../shared/

QT = core network testlib gui qml jsondb
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += testjsondbsortinglistmodel.h \
           $$PWD/../../shared/requestwrapper.h
SOURCES += testjsondbsortinglistmodel.cpp

OTHER_FILES += \
    partitions.json
