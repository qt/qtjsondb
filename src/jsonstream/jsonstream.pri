INCLUDEPATH += $$PWD
LIBS_PRIVATE += -L$$shadowed($$PWD) -lQtJsonDbJsonStream
POST_TARGETDEPS += $$shadowed($$PWD)/libQtJsonDbJsonStream.a
