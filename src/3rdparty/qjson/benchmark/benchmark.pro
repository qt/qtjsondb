TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .

QT += testlib
QT -= gui

include(../src/json.pri)

# Input
SOURCES += main.cpp
