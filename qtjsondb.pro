load(configure)
qtCompileTest(icu)
qtCompileTest(libedit)

load(qt_parts)

win32 {
    message("QtJsonDb is not currently supported on Windows - will not be built")
    SUBDIRS =
}

include(doc/doc.pri)
