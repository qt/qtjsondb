#ifndef QKEYVALUESTOREFILE_P_H
#define QKEYVALUESTOREFILE_P_H

#include <QString>

int doOpen(const char *name, bool truncate);
int doClose(int fd);

class QKeyValueStoreFilePrivate {
public:
    QKeyValueStoreFilePrivate(QString name, bool truncate = false);
    ~QKeyValueStoreFilePrivate();

    // Public API mirror
    bool open();
    int read(void *buffer, quint32 count);
    int write(void *buffer, quint32 count);
    void sync();
    qint64 size();
    bool close();

    QString m_name;
    qint64 m_size;
    qint64 m_offset;
    qint32 m_fd;
    bool m_truncate;
};

#endif // QKEYVALUESTOREFILE_P_H
