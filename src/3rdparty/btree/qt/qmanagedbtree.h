#ifndef QMANAGEDBTREE_H
#define QMANAGEDBTREE_H

#include <QMap>
#include <QSet>
#include "qbtree.h"
#include "qmanagedbtreetxn.h"

class QBtree;

class QManagedBtree
{
public:
    QManagedBtree();
    ~QManagedBtree();

    bool open(const QString &filename, QBtree::DbFlags flags = QBtree::Default);
    void close();

    QManagedBtreeTxn beginRead(quint32 tag);
    QManagedBtreeTxn beginRead();
    QManagedBtreeTxn beginWrite();

    bool isWriteTxnActive() const
    { return mWriter.txn != NULL; }
    bool isReadTxnActive(quint32 tag) const;
    bool isReadTxnActive() const;
    bool numActiveReadTxns() const
    { return mReaders.size() > 0; }

    QManagedBtreeTxn existingWriteTxn();

    bool putOne(const QByteArray &key, const QByteArray &value);
    bool getOne(const QByteArray &key, QByteArray *value);
    bool removeOne(const QByteArray &key);

    bool clearData();

    QString errorMessage() const;

    QString fileName() const
    { Q_ASSERT(mBtree); return mBtree->fileName(); }
    quint64 count() const
    { Q_ASSERT(mBtree); return mBtree->count(); }
    quint32 tag() const
    { Q_ASSERT(mBtree); return mBtree->tag(); }
    bool compact()
    { Q_ASSERT(mBtree); return mBtree->compact(); }
    bool rollback()
    { Q_ASSERT(mBtree && !numActiveReadTxns() && !isWriteTxnActive()); return mBtree->rollback(); }
    struct btree *handle() const
    { Q_ASSERT(mBtree); return mBtree->handle(); }
    void setAutoCompactRate(int rate) const
    { Q_ASSERT(mBtree); mBtree->setAutoCompactRate(rate); }
    void setAutoSyncRate(int rate) const
    { Q_ASSERT(mBtree); mBtree->setAutoSyncRate(rate); }
    void setCmpFunc(QBtree::CmpFunc cmp)
    { Q_ASSERT(mBtree); mBtree->setCmpFunc(cmp); }
    void setCacheSize(int size)
    { Q_ASSERT(mBtree); mBtree->setCacheSize(size); }

private:
    friend class QManagedBtreeTxn;
    void remove(QManagedBtreeTxn *txn);
    void add(QManagedBtreeTxn *txn);

    bool commit(QManagedBtreeTxn *txn, quint32 tag);
    void abort(QManagedBtreeTxn *txn);

    struct RefedTxn {
        QBtreeTxn *txn;
        QSet<QManagedBtreeTxn* > clients;
    };
    typedef QMap<quint32, RefedTxn> RefedTxnMap;

    QBtree *mBtree;
    RefedTxn mWriter;
    RefedTxnMap mReaders;

    QManagedBtree(const QManagedBtree&);
};

#endif // QMANAGEDBTREE_H
