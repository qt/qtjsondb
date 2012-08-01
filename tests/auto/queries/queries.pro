TARGET = tst_queries

QT = network qml testlib jsondbpartition
CONFIG -= app_bundle
CONFIG += testcase

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES = queries.qrc

SOURCES += \
    testjsondbqueries.cpp

OTHER_FILES += \
    dataset.json
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
