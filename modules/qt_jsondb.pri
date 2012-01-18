!win32 {
QT.jsondb.VERSION = 1.0.0
QT.jsondb.MAJOR_VERSION = 1
QT.jsondb.MINOR_VERSION = 0
QT.jsondb.PATCH_VERSION = 0

QT.jsondb.name = QtJsonDb
QT.jsondb.bins = $$QT_MODULE_BIN_BASE
QT.jsondb.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtJsonDb
QT.jsondb.private_includes = $$QT_MODULE_INCLUDE_BASE/QtJsonDb/$$QT.jsondb.VERSION
QT.jsondb.sources = $$QT_MODULE_BASE/src
QT.jsondb.libs = $$QT_MODULE_LIB_BASE
QT.jsondb.plugins = $$QT_MODULE_PLUGIN_BASE
QT.jsondb.imports = $$QT_MODULE_IMPORT_BASE
QT.jsondb.depends = core network declarative

QT_CONFIG += jsondb
}
