TARGET = tst_client

QT = network testlib jsondbcompat-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

# allow overriding the prefix for /etc/passwd and friends
NSS_PREFIX = $$(NSS_PREFIX)
DEFINES += NSS_PREFIX=\\\"$$NSS_PREFIX\\\"

SOURCES += test-jsondb-client.cpp

OTHER_FILES += \
    partitions.json
