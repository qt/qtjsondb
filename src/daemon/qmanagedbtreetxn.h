#ifndef QMANAGEDBTREETXN_H
#define QMANAGEDBTREETXN_H


class QBtree;
class QBtreeTxn;
class QManagedBtree;

class QManagedBtreeTxn
{
public:
    QManagedBtreeTxn();
    ~QManagedBtreeTxn();
    QManagedBtreeTxn(const QManagedBtreeTxn &other);
    QManagedBtreeTxn &operator = (const QManagedBtreeTxn &other);

    bool isValid() const { return mTxn != NULL; }
    bool operator == (const QManagedBtreeTxn &rhs) const
    { return mTxn == rhs.mTxn; }
    bool operator != (const QManagedBtreeTxn &rhs) const
    { return mTxn != rhs.mTxn; }

    bool get(const QByteArray &baKey, QByteArray *baValue) const;
    bool put(const QByteArray &baKey, const QByteArray &baValue);
    bool remove(const QByteArray &baKey);

    bool commit(quint32 tag);
    void abort();

    const QBtreeTxn *txn() const { return mTxn; }
    const QManagedBtree *btree() const { return mBtree; }

    const QString errorMessage() const;

    quint32 tag() const { return mTag; }

private:
    friend class QManagedBtree;
    void reset(QManagedBtree *mbtree, QBtreeTxn *txn);
    QManagedBtreeTxn(QManagedBtree *mbtree, QBtreeTxn *txn);

    QBtreeTxn *mTxn;
    QManagedBtree *mBtree;
    quint32 mTag;

    typedef void (QManagedBtreeTxn::*SafeBool)() const;
    void noBoolComparisons () const {}
public:
    operator SafeBool() const {
        return isValid() ? &QManagedBtreeTxn::noBoolComparisons : 0;
    }
};


#endif // QMANAGEDBTREETXN_H
