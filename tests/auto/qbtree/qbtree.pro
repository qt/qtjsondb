include($$PWD/../../../src/3rdparty/btree/btree.pri)

TARGET = tst_qbtree

QT = core testlib
CONFIG -= app_bundle
CONFIG += debug

LIBS += -lssl -lcrypto

SOURCES += \
    main.cpp

check.target = check
check.commands = rm -f *.db* && QT_QPA_PLATFORM=xcb ./tst_qbtree -xunitxml -silent > ../../../tst_qbtree.xml
QMAKE_EXTRA_TARGETS = check
