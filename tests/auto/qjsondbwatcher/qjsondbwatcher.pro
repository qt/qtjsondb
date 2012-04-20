TARGET = tst_qjsondbwatcher

QT = network testlib jsondb-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES += ../partition/partition.qrc

SOURCES += testqjsondbwatcher.cpp

OTHER_FILES += \
    partitions.json

data.files = $$OTHER_FILES
data.path = $$[QT_INSTALL_TESTS]/$$TARGET
INSTALLS += data
