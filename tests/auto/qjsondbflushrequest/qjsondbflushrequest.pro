TARGET = tst_qjsondbflushrequest

QT = network testlib jsondb-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES += ../daemon/daemon.qrc

SOURCES += testqjsondbflushrequest.cpp
