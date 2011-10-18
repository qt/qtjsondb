INCLUDEPATH += $$PWD/qt $$PWD/src $$PWD/compat

HEADERS += \
    $$PWD/src/btree.h \
    $$PWD/compat/sys/queue.h \
    $$PWD/compat/sys/tree.h \
    $$PWD/qt/qbtreedata.h \
    $$PWD/qt/qbtree.h

SOURCES += \
    $$PWD/src/btree.cpp \
    $$PWD/qt/qbtreedata.cpp \
    $$PWD/qt/qbtree.cpp

!mac:LIBS += -lssl -lcrypto
