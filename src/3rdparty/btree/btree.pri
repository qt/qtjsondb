INCLUDEPATH += $$PWD/qt $$PWD/src $$PWD/compat

HEADERS += \
    $$PWD/src/btree.h \
    $$PWD/compat/sys/queue.h \
    $$PWD/compat/sys/tree.h \
    $$PWD/qt/qbtree.h \
    $$PWD/qt/qbtreedata.h \
    $$PWD/qt/qbtreelocker.h \
    $$PWD/qt/qbtreetxn.h \
    $$PWD/qt/qbtreecursor.h \
    $$PWD/src/btree_p.h

SOURCES += \
    $$PWD/src/btree.cpp \
    $$PWD/qt/qbtree.cpp \
    $$PWD/qt/qbtreedata.cpp \
    $$PWD/qt/qbtreelocker.cpp \
    $$PWD/qt/qbtreetxn.cpp \
    $$PWD/qt/qbtreecursor.cpp

!mac:LIBS += -lssl -lcrypto




