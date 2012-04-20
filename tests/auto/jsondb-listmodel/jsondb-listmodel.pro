TEMPLATE = app
TARGET = tst_jsondb-listmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib gui qml jsondbcompat-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += $$PWD/../../../src/imports/jsondb-listmodel
HEADERS += $$PWD/../../../src/imports/jsondb-listmodel/jsondb-listmodel.h
SOURCES += $$PWD/../../../src/imports/jsondb-listmodel/jsondb-listmodel.cpp

HEADERS += test-jsondb-listmodel.h
SOURCES += test-jsondb-listmodel.cpp

OTHER_FILES += \
    partitions.json

data.files = $$OTHER_FILES
data.path = $$[QT_INSTALL_TESTS]/$$TARGET
INSTALLS += data

