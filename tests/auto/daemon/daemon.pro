TARGET = tst_daemon
CONFIG += debug

QT = network qml testlib jsondbpartition
CONFIG -= app_bundle
CONFIG += testcase

LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

unix:!mac:contains(QT_CONFIG,icu) {
    LIBS += -licuuc -licui18n
} else {
    DEFINES += NO_COLLATION_SUPPORT
}

RESOURCES += json-validation.qrc daemon.qrc

SOURCES += \
    testjsondb.cpp \
