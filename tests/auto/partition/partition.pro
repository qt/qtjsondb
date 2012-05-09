TARGET = tst_partition
CONFIG += debug

QT = network qml testlib jsondbpartition jsondbpartition-private
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

# HACK, remove when jsondbpartition separates private api from public api
include(../../../src/3rdparty/btree/btree.pri)
include(../../../src/hbtree/hbtree.pri)

SOURCES += \
    testpartition.cpp \

contains(config_test_icu, yes) {
    LIBS += -licuuc -licui18n
} else {
    DEFINES += NO_COLLATION_SUPPORT
}

