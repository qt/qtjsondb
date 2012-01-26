
#include <QDebug>
#include "qbtreetxn.h"
#include "qmanagedbtree.h"
#include "qmanagedbtreetxn.h"

QManagedBtreeTxn::QManagedBtreeTxn()
    : mTxn(0), mBtree(0), mTag(0), mIsRead(false)
{
}

QManagedBtreeTxn::~QManagedBtreeTxn()
{
    reset(0, 0);
}

QManagedBtreeTxn::QManagedBtreeTxn(QManagedBtree *mbtree, QBtreeTxn *txn)
    : mTxn(0), mBtree(0), mTag(0), mIsRead(false)
{
    reset(mbtree, txn);
}

QManagedBtreeTxn::QManagedBtreeTxn(const QManagedBtreeTxn &other)
    : mTxn(0), mBtree(0), mTag(0), mIsRead(false)
{
    reset(other.mBtree, other.mTxn);
}

QManagedBtreeTxn &QManagedBtreeTxn::operator = (const QManagedBtreeTxn &other)
{
    if (this == &other)
        return *this;
    reset(other.mBtree, other.mTxn);
    return *this;
}

bool QManagedBtreeTxn::get(const QByteArray &baKey, QByteArray *baValue) const
{
    Q_ASSERT(mTxn);
    return mTxn->get(baKey, baValue);
}

bool QManagedBtreeTxn::put(const QByteArray &baKey, const QByteArray &baValue)
{
    Q_ASSERT(mTxn);
    return mTxn->put(baKey, baValue);
}

bool QManagedBtreeTxn::remove(const QByteArray &baKey)
{
    Q_ASSERT(mTxn);
    return mTxn->remove(baKey);
}

bool QManagedBtreeTxn::commit(quint32 tag)
{
    Q_ASSERT(mTxn);
    if (mTxn->isReadOnly()) {
        qWarning() << "QManagedBtreeTxn::commit: commit on read only txn doesn't make sense. Aborting.";
        mBtree->abort(this);
        return true;
    }
    return mBtree->commit(this, tag);
}

void QManagedBtreeTxn::abort()
{
    Q_ASSERT(mTxn);
    mBtree->abort(this);
}

const QString QManagedBtreeTxn::errorMessage() const
{
    return mBtree->errorMessage();
}

void QManagedBtreeTxn::reset(QManagedBtree *mbtree, QBtreeTxn *txn)
{
    if (mTxn && mBtree) {
        mBtree->remove(this);
    }

    mTxn = txn;
    mBtree = mbtree;

    if (txn && mbtree) {
        Q_ASSERT(mbtree->handle() == txn->btree()->handle());
        Q_ASSERT(txn->handle());
        mTag = txn->tag();
        mIsRead = txn->isReadOnly();
        mbtree->add(this);
    }
}
