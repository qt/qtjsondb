!win32 {
QT.jsondbcompat.VERSION = 1.0.0
QT.jsondbcompat.MAJOR_VERSION = 1
QT.jsondbcompat.MINOR_VERSION = 0
QT.jsondbcompat.PATCH_VERSION = 0

QT.jsondbcompat.name = QtJsonDbCompat
QT.jsondbcompat.bins = $$QT_MODULE_BIN_BASE
QT.jsondbcompat.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtJsonDbCompat
QT.jsondbcompat.private_includes = $$QT_MODULE_INCLUDE_BASE/QtJsonDbCompat/$$QT.jsondbcompat.VERSION
QT.jsondbcompat.sources = $$QT_MODULE_BASE/src
QT.jsondbcompat.libs = $$QT_MODULE_LIB_BASE
QT.jsondbcompat.plugins = $$QT_MODULE_PLUGIN_BASE
QT.jsondbcompat.imports = $$QT_MODULE_IMPORT_BASE
QT.jsondbcompat.depends = core network qml

QT_CONFIG += jsondbcompat
}
