JSONDB_SOURCE_TREE = $$PWD

testcocoon {
    # Add missing paths to instrument with code coverage which are used by different projects
    # within QtJsonDb. Headers files not located in the current directory are not detected
    # by the overall implementation which can break the instrumentation.
    TESTCOCOON_COVERAGE_JSONDB_OPTIONS = \
        --cs-include-file-abs-regex=\'^$$JSONDB_SOURCE_TREE[/\\\\]src[/\\\\].*\\.h\$\$\'

    QMAKE_CFLAGS += $$TESTCOCOON_COVERAGE_JSONDB_OPTIONS
    QMAKE_CXXFLAGS += $$TESTCOCOON_COVERAGE_JSONDB_OPTIONS
    QMAKE_LFLAGS = $$TESTCOCOON_COVERAGE_JSONDB_OPTIONS
    QMAKE_AR += $$TESTCOCOON_COVERAGE_JSONDB_OPTIONS
}
