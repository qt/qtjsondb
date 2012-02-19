#ifndef JSONDBPROXY_H
#define JSONDBPROXY_H

#include <QJsonDbConnection>
#include <QVariantMap>

class JsonDbProxy : public QObject
{
    Q_OBJECT
public:
    explicit JsonDbProxy(QtJsonDb::QJsonDbConnection *conn, QObject *parent = 0);
    Q_SCRIPTABLE QVariantMap find(QVariantMap object);
    Q_SCRIPTABLE QVariantMap create(QVariantMap object);
    Q_SCRIPTABLE QVariantMap update(QVariantMap object);
    Q_SCRIPTABLE QVariantMap remove(QVariantMap object);

    Q_SCRIPTABLE QVariantMap createList(QVariantList list);
    Q_SCRIPTABLE QVariantMap updateList(QVariantList list);
    Q_SCRIPTABLE QVariantMap removeList(QVariantList list);

    Q_SCRIPTABLE void log(const QString &msg);
    Q_SCRIPTABLE void debug(const QString &msg);

private:
    QtJsonDb::QJsonDbConnection *mConnection;
};

#endif // JSONDBPROXY_H
