TARGET = tst_partition
CONFIG += debug

QT = network qml testlib jsondbpartition
CONFIG -= app_bundle
CONFIG += testcase

LIBS += -L$$QT.jsondb.libs

DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES += json-validation.qrc partition.qrc

unix:!mac:contains(QT_CONFIG,icu) {
    LIBS += -licuuc -licui18n
} else {
    DEFINES += NO_COLLATION_SUPPORT
}

SOURCES += \
    testpartition.cpp \
