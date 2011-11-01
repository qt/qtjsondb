INCLUDEPATH += $$PWD/qt $$PWD/src $$PWD/compat

HEADERS += \
    $$PWD/src/btree.h \
    $$PWD/compat/sys/queue.h \
    $$PWD/compat/sys/tree.h \
    $$PWD/qt/qbtree.h \
    $$PWD/qt/qbtreedata.h \
    $$PWD/qt/qbtreelocker.h

SOURCES += \
    $$PWD/src/btree.cpp \
    $$PWD/qt/qbtree.cpp \
    $$PWD/qt/qbtreedata.cpp \
    $$PWD/qt/qbtreelocker.cpp

!mac:LIBS += -lssl -lcrypto
