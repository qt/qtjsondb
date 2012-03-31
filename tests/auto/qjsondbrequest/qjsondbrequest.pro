TARGET = tst_qjsondbrequest

QT = network testlib jsondb-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES += ../daemon/daemon.qrc

SOURCES += testqjsondbrequest.cpp

OTHER_FILES += \
    partitions.json
