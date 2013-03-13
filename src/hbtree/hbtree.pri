INCLUDEPATH += $$PWD/

include($$PWD/../3rdparty/zlib/zlib.pri)

HEADERS += \
    $$PWD/hbtreeglobal.h \
    $$PWD/orderedlist_p.h \
    $$PWD/hbtree.h \
    $$PWD/hbtreetransaction.h \
    $$PWD/hbtreecursor.h \
    $$PWD/hbtree_p.h \
    $$PWD/hbtreeassert_p.h

SOURCES += \
    $$PWD/orderedlist.cpp \
    $$PWD/hbtree.cpp \
    $$PWD/hbtreetransaction.cpp \
    $$PWD/hbtreecursor.cpp \
    $$PWD/hbtreeassert.cpp



