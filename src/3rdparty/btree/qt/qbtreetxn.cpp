#include <QDebug>
#include <errno.h>
#include "btree.h"
#include "qbtree.h"
#include "qbtreetxn.h"

QBtreeTxn::QBtreeTxn(QBtree *db, btree_txn *txn)
    : mDb(db), mTxn(txn)
{
    Q_ASSERT(mDb);
    mDb->addTxn(this);
}

QBtreeTxn::~QBtreeTxn()
{
    mDb->removeTxn(this);
    if (mTxn) {
        qCritical() << "QBtreeTxn: transaction in progress aborted.";
        btree_txn_abort(mTxn);
        mTxn = 0;
    }
}

bool QBtreeTxn::commit(quint32 tag)
{
    bool ret = false;
    if (mTxn) {
        unsigned int flags = (mDb->mAutoSyncRate && mDb->mCommitCount % mDb->mAutoSyncRate == 0) ? BT_FORCE_MARKER : 0;
        ret = btree_txn_commit(mTxn, tag, flags) == BT_SUCCESS;
        mTxn = 0;
    } else {
        qCritical() << "QBtreeTxn::commit()" << "no txn" << mDb->fileName();
    }
    bool needCompact = ret && mDb->mAutoCompactRate && mDb->mCommitCount++ > mDb->mAutoCompactRate;
    QBtree *db = mDb;

    mDb->mTxns.removeOne(this);
    delete this;

    if (needCompact) {
        db->compact();
        db->mCommitCount = 0;
    }
    return ret;
}

bool QBtreeTxn::abort()
{
    bool ret = false;
    if (mTxn) {
        btree_txn_abort(mTxn);
        mTxn = 0;
        ret = true;
    } else {
        qCritical() << "QBtree::abort()" << "no txn";
    }
    mDb->mTxns.removeOne(this);
    delete this;
    return ret;
}

quint32 QBtreeTxn::tag() const
{
    return btree_txn_get_tag(mTxn);
}

bool QBtreeTxn::get(const QByteArray &baKey, QByteArray *baValue) const
{
    Q_ASSERT(baValue);
    QBtreeData value;
    bool ret = get(baKey.constData(), baKey.size(), &value);
    *baValue = value.toByteArray();
    return ret;
}

bool QBtreeTxn::get(const QBtreeData &key, QBtreeData *value) const
{
    return get(key.constData(), key.size(), value);
}

bool QBtreeTxn::get(const char *key, int keySize, QBtreeData *value) const
{
    Q_ASSERT(value);
    struct btval btkey;
    btkey.data = (void *)key;
    btkey.size = keySize;
    btkey.free_data = 0;
    btkey.mp = 0;

    struct btval btvalue;
    int ok = btree_txn_get(mDb->handle(), mTxn, &btkey, &btvalue);
    if (ok != BT_SUCCESS)
        return false;
    *value = QBtreeData(&btvalue);
    return true;
}

bool QBtreeTxn::put(const QByteArray &baKey, const QByteArray &baValue)
{
    return put(baKey.constData(), baKey.size(), baValue.constData(), baValue.size());
}

bool QBtreeTxn::put(const QBtreeData &key, const QBtreeData &value)
{
    return put(key.constData(), key.size(), value.constData(), value.size());
}

bool QBtreeTxn::put(const char *key, int keySize, const char *value, int valueSize)
{
    struct btval btkey;
    btkey.data = (void *)key;
    btkey.size = keySize;
    btkey.free_data = 0;
    btkey.mp = 0;
    struct btval btvalue;
    btvalue.data = (void *)value;
    btvalue.size = valueSize;
    btvalue.free_data = 0;
    btvalue.mp = 0;

    int ok = btree_txn_put(mDb->mBtree, mTxn, &btkey, &btvalue, 0);
    if (ok != BT_SUCCESS)
        qDebug() << "btree_txn_put" << ok << errno << endl << mDb->fileName();
    return ok == BT_SUCCESS;
}

bool QBtreeTxn::remove(const QByteArray &baKey)
{
    return remove(baKey.constData(), baKey.size());
}

bool QBtreeTxn::remove(const QBtreeData &key)
{
    return remove(key.constData(), key.size());
}

bool QBtreeTxn::remove(const char *key, int keySize)
{
    struct btval btkey;
    btkey.data = (void *)key;
    btkey.size = keySize;
    btkey.free_data = 0;
    btkey.mp = 0;
    if (btree_txn_del(mDb->mBtree, mTxn, &btkey, 0) != BT_SUCCESS)
        qDebug() << "db->del" << errno << QByteArray(key, keySize).toHex() << endl << mDb->fileName();
    return true;
}
