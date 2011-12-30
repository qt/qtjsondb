#include <QDebug>
#include <errno.h>
#include "btree.h"
#include "qbtree.h"
#include "qbtreetxn.h"

QBtreeTxn::QBtreeTxn(QBtree *btree, btree_txn *txn)
    : mBtree(btree), mTxn(txn)
{
    Q_ASSERT(mBtree && mTxn);
}

QBtreeTxn::~QBtreeTxn()
{
    Q_ASSERT(mTxn && mBtree);
}

bool QBtreeTxn::commit(quint32 tag)
{
    if (isReadOnly()) {
        qWarning() << "QBtreeTxn::commit:" << "commiting read only txn doesn't make sense. Aborting instead";
        mBtree->abort(this);
        return true;
    }
    return mBtree->commit(this, tag);
}

void QBtreeTxn::abort()
{
    mBtree->abort(this);
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
    int ok = btree_txn_get(mBtree->handle(), mTxn, &btkey, &btvalue);
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

    int ok = btree_txn_put(mBtree->handle(), mTxn, &btkey, &btvalue, 0);
    if (btree_txn_is_error(mTxn))
        qDebug() << "btree_txn_put" << ok << errno << endl << mBtree->fileName();
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

    int ok = btree_txn_del(mBtree->handle(), mTxn, &btkey, 0);
    if (btree_txn_is_error(mTxn))
        qDebug() << "btree_txn_del" << ok << errno << endl << mBtree->fileName();
    return ok == BT_SUCCESS;
}

bool QBtreeTxn::isReadOnly() const
{
    Q_ASSERT(mTxn);
    return btree_txn_is_read(mTxn) == 1;
}

bool QBtreeTxn::isReadWrite() const
{
    Q_ASSERT(mTxn);
    return btree_txn_is_read(mTxn) == 0;
}
