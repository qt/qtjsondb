TEMPLATE = subdirs

module_qtjsondb_src.subdir = src
module_qtjsondb_src.target = module-qtjsondb-src

module_qtjsondb_tools.subdir = tools
module_qtjsondb_tools.target = module-qtjsondb-tools
module_qtjsondb_tools.depends = module_qtjsondb_src

module_qtjsondb_tests.subdir = tests
module_qtjsondb_tests.target = module-qtjsondb-tests
module_qtjsondb_tests.depends = module_qtjsondb_src
module_qtjsondb_tests.CONFIG = no_default_install
!contains(QT_BUILD_PARTS,tests):module_qtjsondb_tests.CONFIG += no_default_target

module_qtjsondb_examples.subdir = examples
module_qtjsondb_examples.target = module-qtjsondb-examples
module_qtjsondb_examples.depends = module_qtjsondb_src
module_qtjsondb_examples.CONFIG = no_default_install
!contains(QT_BUILD_PARTS,examples):module_qtjsondb_examples.CONFIG += no_default_target

win32 {
    message("QtJsonDb is not currently supported on Windows - will not be built")
} else {
    SUBDIRS += module_qtjsondb_src \
               module_qtjsondb_tools \
               module_qtjsondb_tests \
               module_qtjsondb_examples
}

include(doc/doc.pri)
