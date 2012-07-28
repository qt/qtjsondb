TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += 3rdparty jsonstream clientcompat partition client daemon

!isEmpty(QT.quick.name): SUBDIRS += imports
