TARGET = QtJsonDbJsonStream

TEMPLATE = lib
CONFIG += static

QT = core network

HEADERS += jsonstream.h
SOURCES += jsonstream.cpp

# We don't need to install this tool, it's only used for building.
# However we do have to make sure that 'make install' builds it.
dummytarget.CONFIG = dummy_install
INSTALLS += dummytarget
