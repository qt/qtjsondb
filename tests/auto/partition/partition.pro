TARGET = tst_partition
CONFIG += debug

QT = network qml testlib jsondbpartition jsondbpartition-private
CONFIG -= app_bundle
CONFIG += testcase

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

config_icu {
    LIBS += -licuuc -licui18n
} else {
    DEFINES += NO_COLLATION_SUPPORT
}

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
