TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += 3rdparty jsonstream partition client daemon

qtHaveModule(quick): SUBDIRS += imports
