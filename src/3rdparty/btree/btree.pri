INCLUDEPATH += $$PWD/src $$PWD/compat

HEADERS += \
    $$PWD/src/btree.h \
    $$PWD/compat/sys/queue.h \
    $$PWD/compat/sys/tree.h

SOURCES += \
    $$PWD/src/btree.cpp

!mac:LIBS += -lssl -lcrypto
