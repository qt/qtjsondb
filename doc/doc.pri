OTHER_FILES += \
               $$PWD/jsondb.qdocconf \
               $$PWD/jsondb-dita.qdocconf \
               $$PWD/src/*.qdoc

qtPrepareTool(QDOC, qdoc)
docs_target.target = docs
docs_target.commands = $$QDOC $$PWD/jsondb.qdocconf

ditadocs_target.target = ditadocs
ditadocs_target.commands = $$QDOC $$PWD/jsondb-dita.qdocconf

QMAKE_EXTRA_TARGETS = docs_target ditadocs_target
QMAKE_CLEAN += \
               "-r $$PWD/html" \
               "-r $$PWD/ditaxml"

