TARGET = QtJsonDbJsonStream

TEMPLATE = lib
DESTDIR = $$QT_MODULE_LIB_BASE
CONFIG += qt staticlib

QT = core network

HEADERS += jsonstream.h
SOURCES += jsonstream.cpp

# We don't need to install this tool, it's only used for building.
# However we do have to make sure that 'make install' builds it.
dummytarget.CONFIG = dummy_install
INSTALLS += dummytarget
