!win32 {
QT.jsondbpartition.VERSION = 1.0.0
QT.jsondbpartition.MAJOR_VERSION = 1
QT.jsondbpartition.MINOR_VERSION = 0
QT.jsondbpartition.PATCH_VERSION = 0

QT.jsondbpartition.name = QtJsonDbPartition
QT.jsondbpartition.bins = $$QT_MODULE_BIN_BASE
QT.jsondbpartition.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtJsonDbPartition
QT.jsondbpartition.private_includes = $$QT_MODULE_INCLUDE_BASE/QtJsonDbPartition/$$QT.jsondbpartition.VERSION
QT.jsondbpartition.sources = $$QT_MODULE_BASE/src
QT.jsondbpartition.libs = $$QT_MODULE_LIB_BASE
QT.jsondbpartition.plugins = $$QT_MODULE_PLUGIN_BASE
QT.jsondbpartition.imports = $$QT_MODULE_IMPORT_BASE
QT.jsondbpartition.depends = core network qml

QT_CONFIG += jsondbpartition
}
