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
# that line does not seem to work and tests are always disabled
#!contains(QT_BUILD_PARTS,tests):module_qtjsondb_tests.CONFIG += no_default_target
SUBDIRS += module_qtjsondb_src \
           module_qtjsondb_tools \
           module_qtjsondb_tests \

include(doc/doc.pri)
