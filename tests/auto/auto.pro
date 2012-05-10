TEMPLATE = subdirs
SUBDIRS = \
    client \
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
    queries \
    qjsondbrequest \
    qjsondbwatcher \
    qjsondbflushrequest \
    jsonstream \
    hbtree \
    headersclean

testcocoon: SUBDIRS -= headersclean
