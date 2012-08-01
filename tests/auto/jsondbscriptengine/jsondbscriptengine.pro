TARGET = tst_jsondbscriptengine

QT = testlib jsondbpartition jsondbpartition-private
CONFIG -= app_bundle
CONFIG += testcase

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += \
    test-jsondbscriptengine.cpp

OTHER_FILES += \
    test_inject.js

data.files = $$OTHER_FILES
data.path = $$[QT_INSTALL_TESTS]/$$TARGET
INSTALLS += data
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
