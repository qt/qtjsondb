#-------------------------------------------------
#
# Project created by QtCreator 2012-05-09T16:09:50
#
#-------------------------------------------------

QT       += testlib

QT       -= gui

TARGET = tst_storage
CONFIG   += console
CONFIG   -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

# allow overriding the prefix for /etc/passwd and friends
NSS_PREFIX = $$(NSS_PREFIX)
DEFINES += NSS_PREFIX=\\\"$$NSS_PREFIX\\\"

TEMPLATE = app

SOURCES += tst_storage.cpp
