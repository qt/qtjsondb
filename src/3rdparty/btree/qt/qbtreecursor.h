#ifndef QBTREECURSOR_H
#define QBTREECURSOR_H

#include "qbtreedata.h"

class QBtree;
class QBtreeTxn;
struct cursor;

class QBtreeCursor
{
public:
    QBtreeCursor();
    explicit QBtreeCursor(QBtreeTxn *txn);
    explicit QBtreeCursor(QBtree *btree, bool commitedOnly = false);
    ~QBtreeCursor();

    QBtreeCursor(const QBtreeCursor &other);
    QBtreeCursor &operator=(const QBtreeCursor &other);

    bool current(QByteArray *baKey, QByteArray *baValue) const;
    bool current(QBtreeData *baKey, QBtreeData *baValue) const;

    const QBtreeData &key() const { return mKey; }
    const QBtreeData &value() const { return mValue; }

    bool first();
    bool last();

    bool next();
    bool previous();

    bool seek(const QByteArray &baKey);
    bool seek(const QBtreeData &key);

    bool seekRange(const QByteArray &baKey);
    bool seekRange(const QBtreeData &key);

private:
    struct cursor *mCursor;
    QBtreeData mKey;
    QBtreeData mValue;

    bool moveHelper(const char *key, int size, int cursorOp);
};

#endif // QBTREECURSOR_H
