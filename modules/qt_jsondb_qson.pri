QT.jsondbqson.VERSION = 1.0.0
QT.jsondbqson.MAJOR_VERSION = 1
QT.jsondbqson.MINOR_VERSION = 0
QT.jsondbqson.PATCH_VERSION = 0

QT.jsondbqson.name = QtJsonDbQson
QT.jsondbqson.bins = $$QT_MODULE_BIN_BASE
QT.jsondbqson.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtJsonDbQson
QT.jsondbqson.private_includes = $$QT_MODULE_INCLUDE_BASE/QtJsonDbQson/$$QT.jsondbqson.VERSION
QT.jsondbqson.sources = $$QT_MODULE_BASE/src
QT.jsondbqson.libs = $$QT_MODULE_LIB_BASE
QT.jsondbqson.plugins = $$QT_MODULE_PLUGIN_BASE
QT.jsondbqson.imports = $$QT_MODULE_IMPORT_BASE
QT.jsondbqson.depends = core

QT_CONFIG += jsondbqson
