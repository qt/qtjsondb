#include "btree.h"
#include "qbtree.h"
#include "qbtreetxn.h"

#include "qbtreecursor.h"


QBtreeCursor::QBtreeCursor()
    : mTxn(0), mCursor(0)
{
}

QBtreeCursor::QBtreeCursor(QBtreeTxn *txn)
    : mTxn(txn), mCursor(0)
{
    if (mTxn)
        mCursor = btree_txn_cursor_open(mTxn->btree()->handle(), mTxn->handle());
}

QBtreeCursor::~QBtreeCursor()
{
    if (mCursor)
        btree_cursor_close(mCursor);
}

QBtreeCursor::QBtreeCursor(const QBtreeCursor &other)
    : mTxn(other.mTxn), mCursor(0), mKey(other.mKey), mValue(other.mValue)
{
    if (mTxn) {
        mCursor = btree_txn_cursor_open(mTxn->btree()->handle(), mTxn->handle());
        // go to the same position as the other cursor
        if (!mKey.isNull())
            seek(mKey);
    }
}

QBtreeCursor &QBtreeCursor::operator=(const QBtreeCursor &other)
{
    if (this != &other) {
        if (mCursor)
            btree_cursor_close(mCursor);
        mTxn = other.mTxn;
        mKey = other.mKey;
        mValue = other.mValue;
        if (mTxn) {
            mCursor = btree_txn_cursor_open(mTxn->btree()->handle(), mTxn->handle());
            // go to the same position as the other cursor
            if (!mKey.isNull())
                seek(mKey);
        }
    }
    return *this;
}

bool QBtreeCursor::current(QBtreeData *key, QBtreeData *value) const
{
    if (key)
        *key = mKey;
    if (value)
        *value = mValue;
    return true;
}

bool QBtreeCursor::current(QByteArray *key, QByteArray *value) const
{
    if (key)
        *key = mKey.toByteArray();
    if (value)
        *value = mValue.toByteArray();
    return true;
}

bool QBtreeCursor::first()
{
    return moveHelper(0, 0, BT_FIRST);
}

bool QBtreeCursor::last()
{
    return moveHelper(0, 0, BT_LAST);
}

bool QBtreeCursor::next()
{
    return moveHelper(0, 0, BT_NEXT);
}

bool QBtreeCursor::prev()
{
    return moveHelper(0, 0, BT_PREV);
}

bool QBtreeCursor::seek(const QByteArray &baKey)
{
    return moveHelper(baKey.constData(), baKey.size(), BT_CURSOR_EXACT);
}

bool QBtreeCursor::seek(const QBtreeData &key)
{
    return moveHelper(key.constData(), key.size(), BT_CURSOR_EXACT);
}

bool QBtreeCursor::seekRange(const QByteArray &baKey)
{
    return moveHelper(baKey.constData(), baKey.size(), BT_CURSOR);
}

bool QBtreeCursor::seekRange(const QBtreeData &key)
{
    return moveHelper(key.constData(), key.size(), BT_CURSOR);
}

bool QBtreeCursor::moveHelper(const char *key, int size, int cursorOp)
{
    Q_ASSERT(mCursor);
    Q_ASSERT(key || (cursorOp == BT_PREV || cursorOp == BT_NEXT || cursorOp == BT_LAST || cursorOp == BT_FIRST));
    Q_ASSERT(!key || (cursorOp == BT_CURSOR || cursorOp == BT_CURSOR_EXACT));

    struct btval btkey;

    btkey.data = (void *)key;
    btkey.size = size;
    btkey.free_data = 0;
    btkey.mp = 0;

    struct btval btvalue;
    btvalue.data = 0;
    btvalue.size = 0;
    btvalue.free_data = 0;
    btvalue.mp = 0;

    if (btree_cursor_get(mCursor, &btkey, &btvalue, static_cast<cursor_op>(cursorOp)) != BT_SUCCESS) {
        btval_reset(&btkey);
        btval_reset(&btvalue);
        mKey = mValue = QBtreeData();
        return false;
    }

    mKey = QBtreeData(&btkey);
    mValue = QBtreeData(&btvalue);
    return true;
}
