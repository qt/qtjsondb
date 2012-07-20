TEMPLATE = subdirs
SUBDIRS = \
    client \
    cmake \
    partition \
    accesscontrol \
    qbtree \
    jsondb-listmodel \
    jsondblistmodel \
    jsondbsortinglistmodel \
    jsondbcachinglistmodel \
    jsondbpartition \
    jsondbnotification \
    jsondbqueryobject \
    jsondbscriptengine \
    queries \
    qjsondbrequest \
    qjsondbwatcher \
    qjsondbflushrequest \
    jsonstream \
    hbtree \
    headersclean \
    storage

testcocoon: SUBDIRS -= headersclean
