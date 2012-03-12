OTHER_FILES += \
               $$PWD/jsondb.qdocconf \
               $$PWD/jsondb-dita.qdocconf

docs_target.target = docs
docs_target.commands = qdoc $$PWD/jsondb.qdocconf

ditadocs_target.target = ditadocs
ditadocs_target.commands = qdoc $$PWD/jsondb-dita.qdocconf

QMAKE_EXTRA_TARGETS = docs_target ditadocs_target
QMAKE_CLEAN += \
               "-r $$PWD/html" \
               "-r $$PWD/ditaxml"

