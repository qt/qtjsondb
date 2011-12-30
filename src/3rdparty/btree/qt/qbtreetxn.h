#ifndef QBTREETXN_H
#define QBTREETXN_H

#include "qbtreedata.h"

class QBtree;
struct btree_txn;

class QBtreeTxn
{
public:
    bool get(const QByteArray &baKey, QByteArray *baValue) const;
    bool get(const char *key, int keySize, QBtreeData *value) const;
    bool get(const QBtreeData &key, QBtreeData *value) const;
    bool put(const QByteArray &baKey, const QByteArray &baValue);
    bool put(const char *key, int keySize, const char *value, int valueSize);
    bool put(const QBtreeData &baKey, const QBtreeData &baValue);
    bool remove(const QByteArray &baKey);
    bool remove(const QBtreeData &baKey);
    bool remove(const char *key, int keySize);

    bool commit(quint32 tag);
    void abort();

    quint32 tag() const;

    inline QBtree *btree() const { return mBtree; }
    inline btree_txn *handle() const { return mTxn; }

    bool isReadOnly() const;
    bool isReadWrite() const;

private:
    QBtree *mBtree;
    btree_txn *mTxn;

    QBtreeTxn(QBtree *btree, btree_txn *txn);
    ~QBtreeTxn();
    QBtreeTxn(const QBtreeTxn &); // forbid copy constructor
    friend class QBtree;
};

#endif // QBTREETXN_H
