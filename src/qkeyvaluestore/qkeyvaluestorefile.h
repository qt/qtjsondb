#ifndef QKEYVALUESTOREFILE_H
#define QKEYVALUESTOREFILE_H

#include <QDebug>
#include <QString>

#include "qkeyvaluestorefile_p.h"

class QKeyValueStoreFile
{
    QKeyValueStoreFilePrivate *p;
public:
    QKeyValueStoreFile(QString name, bool truncate = false);
    ~QKeyValueStoreFile();
    bool open();
    int read(void *buffer, quint32 count);
    int write(void *buffer, quint32 count);
    void sync();
    qint64 size() const;
    qint64 offset() const { return p->m_offset; }
    void setOffset(qint64 offset) { p->m_offset = offset; }
    bool close();
    bool truncate() const { return p->m_truncate; }
};

#endif // QKEYVALUESTOREFILE_H
