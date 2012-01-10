#ifndef QBTREECURSOR_H
#define QBTREECURSOR_H

#include "qbtreedata.h"
#include "qmanagedbtree.h"

class QBtreeTxn;
struct cursor;
class QManagedBtree;

class QBtreeCursor
{
public:
    QBtreeCursor();
    explicit QBtreeCursor(QBtreeTxn *txn);
    explicit QBtreeCursor(QManagedBtree *btree, bool commitedOnly = false);
    ~QBtreeCursor();

    bool current(QByteArray *baKey, QByteArray *baValue) const;
    bool current(QBtreeData *baKey, QBtreeData *baValue) const;

    const QBtreeData &key() const { return mKey; }
    const QBtreeData &value() const { return mValue; }

    bool first();
    bool last();

    bool next();
    bool prev();

    bool seek(const QByteArray &baKey);
    bool seek(const QBtreeData &key);

    bool seekRange(const QByteArray &baKey);
    bool seekRange(const QBtreeData &key);

private:
    QManagedBtreeTxn mTxn;
    struct cursor *mCursor;
    QBtreeData mKey;
    QBtreeData mValue;

    bool moveHelper(const char *key, int size, int cursorOp);

    QBtreeCursor(const QBtreeCursor &other);
    QBtreeCursor &operator=(const QBtreeCursor &other);
};

#endif // QBTREECURSOR_H
