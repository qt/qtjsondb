TARGET = tst_queries

QT = network qml testlib jsondbpartition
CONFIG -= app_bundle
CONFIG += testcase

LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES = queries.qrc

SOURCES += \
    testjsondbqueries.cpp
