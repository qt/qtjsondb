/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef HBTREE_P_H
#define HBTREE_P_H

#include "hbtree.h"
#include "hbtreetransaction.h"
#include "hbtreecursor.h"
#include <QDebug>
#include <QMap>
#include <QSharedPointer>
#include <QStack>

#define HBtree_ATTRIBUTE_PACKED __attribute__((packed))

class HBtreePrivate
{
    Q_DECLARE_PUBLIC(HBtree)
public:

    // All pages have this structure at the beggining.
    struct PageInfo {
        enum Type {
            Marker = 1,
            Leaf,
            Branch,
            Overflow,
            Spec,
            Unknown
        };
        static const quint32 INVALID_PAGE;
        PageInfo()
            : checksum(0), type(0), number(INVALID_PAGE), upperOffset(0), lowerOffset(0)
        {}
        explicit PageInfo(Type pageType)
            : checksum(0), type(pageType), number(INVALID_PAGE), upperOffset(0), lowerOffset(0)
        {}
        PageInfo(Type pageType, quint32 pageNumber)
            : checksum(0), type(pageType), number(pageNumber), upperOffset(0), lowerOffset(0)
        {}
        quint32 checksum;       // Must always be first in all pages on disk
        quint32 type;
        quint32 number;         // page number on disk
        quint16 upperOffset;    // offset to end of actual data
        quint16 lowerOffset;    // offset to index with offsets in to individual data

        bool isValid()          const { return number != PageInfo::INVALID_PAGE; }
        bool isTypeValid()      const { return type > 0 && type < PageInfo::Unknown; }
        bool hasPayload()       const { return upperOffset > 0 || lowerOffset > 0; }
        quint16 headerSize()    const;

        static const size_t OFFSETOF_CHECKSUM = 0;
        static const size_t OFFSETOF_TYPE = sizeof(quint32);
        static const size_t OFFSETOF_NUMBER = sizeof(quint32) * 2;
    } HBtree_ATTRIBUTE_PACKED;
    Q_STATIC_ASSERT(sizeof(PageInfo) == 16);

    // Defines the constants of the btree
    struct Spec {
        Spec()
            : version(0), pageSize(), keySize(0), overflowThreshold(1000), pageFillThreshold(25)
        {}
        quint32 version;
        quint16 pageSize;
        quint16 keySize;
        quint32 overflowThreshold; // in bytes
        quint32 pageFillThreshold; // in percent
    } HBtree_ATTRIBUTE_PACKED;
    Q_STATIC_ASSERT(sizeof(Spec) == 16);

    // Stored with every data section in a page on disk. Tells what kind of data is
    // in a node on disk
    struct NodeHeader {
        enum Flags {
            Overflow = (1 << 0)     // If set in flags, the overflowPage is used instead of valueSize
        };
        quint16 keySize;
        union {
            quint32 overflowPage;   // Used for overflow page number, or in the case of a branch page for child page number
            quint32 valueSize;      // In the case of a leaf page, size of data
        } context;
        quint16 flags;
    } HBtree_ATTRIBUTE_PACKED;
    Q_STATIC_ASSERT(sizeof(NodeHeader) == 8);

    // In memory key
    struct NodeKey {
        NodeKey()
            : compareFunction(0)
        {}
        explicit NodeKey(HBtree::CompareFunction cmp)
            : compareFunction(cmp)
        {}
        NodeKey(HBtree::CompareFunction cmp, const QByteArray &d)
            : data(d), compareFunction(cmp)
        {}
        QByteArray data;
        HBtree::CompareFunction compareFunction;

        inline bool operator < (const NodeKey &rhs) const;
        inline bool operator == (const NodeKey &rhs) const;
        inline bool operator > (const NodeKey &rhs) const;
        inline bool operator != (const NodeKey &rhs) const { return !operator==(rhs); }
        inline bool operator <= (const NodeKey &rhs) const { return !operator > (rhs); }
        inline bool operator >= (const NodeKey &rhs) const { return !operator < (rhs); }
    };

    // In memory value
    struct NodeValue {
        NodeValue()
            : flags(0), overflowPage(PageInfo::INVALID_PAGE)
        {}
        explicit NodeValue(const QByteArray &v)
            : flags(0), data(v), overflowPage(PageInfo::INVALID_PAGE)
        {}
        explicit NodeValue(quint32 ofPage)
            : flags(0), overflowPage(ofPage)
        {}
        quint16 flags;          // Mirror of NodeHeader::flags
        QByteArray data;        // This has to be empty for branches and overflow data
        quint32 overflowPage;   // This can also be used as childPage pointer in the case of branches
    };

    typedef QMap<NodeKey, NodeValue> KeyValueMap;
    typedef KeyValueMap::const_iterator Node;
    typedef KeyValueMap::iterator MutableNode;

    // Base of all pages so "fake" page type identification can be used
    // form just the page header.
    struct Page {
        PageInfo info;
        bool commited;  // runtime information on whether a page has been commited or not (
    protected:
        Page(PageInfo::Type type)
            : info(type), commited(false)
        {}
        Page(PageInfo::Type type, quint32 pageNumber)
            : info(type, pageNumber), commited(false)
        {}
    };

    struct MarkerPage : Page {
        MarkerPage()
            : Page(PageInfo::Marker)
        {}
        explicit MarkerPage(quint32 pageNumber)
            : Page(PageInfo::Marker, pageNumber)
        {}
        struct Meta {
            Meta()
                : rootPage(PageInfo::INVALID_PAGE), revision(0), syncedRevision(0), tag(0), size(0)
            {}
            quint32 rootPage;
            quint32 revision;
            quint32 syncedRevision;
            quint64 tag;
            quint32 size; // TODO: this should be max page number instead
        } HBtree_ATTRIBUTE_PACKED;
        Q_STATIC_ASSERT(sizeof(Meta) == 24);
        Meta meta;
    };

    // Keeps track of revisions of touched node pages
    struct NodePage;
    struct HistoryNode {
        HistoryNode()
            : pageNumber(PageInfo::INVALID_PAGE), commitNumber(0), syncNumber(0)
        {}
        HistoryNode(quint32 pageNo, quint32 commitNo, quint32 syncNo)
            : pageNumber(pageNo), commitNumber(commitNo), syncNumber(syncNo)
        {}
        explicit HistoryNode(NodePage *np);
        quint32 pageNumber;
        quint32 commitNumber;
        quint32 syncNumber;
    } HBtree_ATTRIBUTE_PACKED;
    Q_STATIC_ASSERT(sizeof(HistoryNode) == 12);

    struct NodePage : Page {
        NodePage()
            : Page(PageInfo::Type(0)), parent(0),
              leftPageNumber(PageInfo::INVALID_PAGE), rightPageNumber(PageInfo::INVALID_PAGE)
        {}

        NodePage(int type, quint32 pageNumber)
            : Page(PageInfo::Type(type), pageNumber), parent(0),
              leftPageNumber(PageInfo::INVALID_PAGE), rightPageNumber(PageInfo::INVALID_PAGE)
        {}

        struct Meta {
            Meta()
                : syncId(0), commitId(0), historySize(0), flags(0)
            {}
            quint32 syncId;         // Which revision it was synced at
            quint32 commitId;       // Which revision it was commited at
            quint16 historySize;    // Number of revisions of this page (or other pages)
            quint16 flags;          // Not used. Contains NodeHeader::Overflow when this node page has a node referencing overflows
        } HBtree_ATTRIBUTE_PACKED;
        Q_STATIC_ASSERT(sizeof(Meta) == 12);

        Meta meta;
        KeyValueMap nodes;
        QVector<HistoryNode> history;
        NodePage *parent;
        NodeKey parentKey;
        quint32 leftPageNumber;
        quint32 rightPageNumber;
    };

    struct OverflowPage : Page {
        OverflowPage()
            : Page(PageInfo::Overflow), nextPage(PageInfo::INVALID_PAGE)
        {}

        OverflowPage(int type, quint32 pageNumber)
            : Page(PageInfo::Type(type), pageNumber), nextPage(PageInfo::INVALID_PAGE)
        {}

        QByteArray data;
        quint32 nextPage;
    };

    HBtreePrivate(HBtree *q, const QString &name = QString());
    ~HBtreePrivate();

    bool open(int fd);
    void close();

    QByteArray readPage(quint32 pageNumber);
    bool writePage(QByteArray *buffer);

    QByteArray serializePage(const Page &page) const;
    Page *deserializePage(const QByteArray &buffer, Page *page) const;
    PageInfo deserializePageInfo(const QByteArray &buffer) const;
    void serializePageInfo(const PageInfo &info, QByteArray *buffer) const;

    Page *newDeserializePage(const QByteArray &buffer) const;

    void serializeChecksum(quint32 checksum, QByteArray *buffer) const;
    quint32 deserializePageNumber(const QByteArray &buffer) const;
    quint32 deserializePageType(const QByteArray &buffer) const;
    quint32 deserializePageChecksum(const QByteArray &buffer) const;

    MarkerPage deserializeMarkerPage(const QByteArray &buffer) const;
    QByteArray serializeMarkerPage(const MarkerPage &page) const;

    NodePage deserializeNodePage(const QByteArray &buffer) const;
    QByteArray serializeNodePage(const NodePage &page) const;
    NodePage::Meta deserializeNodePageMeta(const QByteArray &buffer) const;

    OverflowPage deserializeOverflowPage(const QByteArray &buffer) const;
    QByteArray serializeOverflowPage(const OverflowPage &page) const;

    quint32 calculateChecksum(const char *begin, const char *end) const;
    quint32 calculateChecksum(const QByteArray &buffer) const;

    HBtreeTransaction *beginTransaction(HBtreeTransaction::Type type);
    bool put(HBtreeTransaction *transaction, const QByteArray &keyData, const QByteArray &valueData);
    QByteArray get(HBtreeTransaction *transaction, const QByteArray &keyData);
    bool del(HBtreeTransaction *transaction, const QByteArray &keyData);
    bool commit(HBtreeTransaction *transaction, quint64 tag);
    void abort(HBtreeTransaction *transaction);
    bool sync(const MarkerPage &mp);

    Page *newPage(PageInfo::Type type);
    Page *getPage(quint32 pageNumber);
    void deletePage(Page *page);
    NodePage *touchNodePage(NodePage *page);
    quint32 putDataOnOverflow(const QByteArray &value);
    QByteArray getDataFromNode(const NodeValue &nval);
    QByteArray getOverflowData(quint32 startPage);
    QVector<quint32> getOverflowPageNumbers(quint32 startPage);

    enum SearchType {
        SearchKey,
        SearchLast,
        SearchFirst
    };

    bool searchPage(HBtreeCursor *cursor, HBtreeTransaction *transaction, const NodeKey &key, SearchType searchType, bool modify, NodePage **pageOut);
    bool searchPageRoot(HBtreeCursor *cursor, NodePage *root, const NodeKey &key, SearchType searchType, bool modify, NodePage **pageOut);
    quint32 getRightSibling(QStack<quint32> rightQ);
    quint32 getLeftSibling(QStack<quint32> leftQ);

    bool willCauseOverflow(const QByteArray &key, const QByteArray &value) const;
    quint16 spaceNeededForNode(const QByteArray &key, const QByteArray &value) const;
    quint16 headerSize(const Page *page) const;
    quint16 nodeSpaceLeft(const NodePage *page) const;
    quint16 nodeSpaceUsed(const NodePage *page) const;
    quint16 capacity(const Page *page) const;
    quint32 pageFullEnough(NodePage *page) const;
    bool hasSpaceFor(NodePage *page, const NodeKey &key, const NodeValue &value) const;

    bool insertNode(NodePage *page, const NodeKey &key, const NodeValue &value);
    bool removeNode(NodePage *page, const NodeKey &key);
    bool split(NodePage *page, const NodeKey &key, const NodeValue &value, NodePage **rightOut = 0);
    bool rebalance(NodePage *page);
    bool moveNode(NodePage *src, NodePage *dst, Node node);
    bool mergePages(NodePage *page, NodePage *dst);
    bool distributeHistoryNodes(NodePage *primary, const QList<HistoryNode> &historyNodes) const;
    bool distributeHistoryNodes(NodePage *primary, const HistoryNode &historyNode) const;

    const MarkerPage &currentMarker() const { return markers_[mi_]; }

    void dump();
    void dumpPage(NodePage *page, int depth);
    const QStack<quint32> collectibleStack() const;

    bool cursorLast(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut);
    bool cursorFirst(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut);
    bool cursorNext(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut);
    bool cursorPrev(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut);
    bool cursorSet(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut, const QByteArray &matchKey, bool exact);

    HBtree              *q_ptr;
    QString              fileName_;
    int                  fd_;
    HBtree::OpenMode     openMode_;
    quint32              size_;
    quint32              lastSyncedRevision_;

    typedef QMap<quint32, Page *> PageMap;
    const Spec spec_;
    MarkerPage markers_[2];
    uint mi_;
    PageMap dirtyPages_;
    HBtree::CompareFunction compareFunction_;
    HBtreeTransaction *writeTransaction_;
    QStack<quint32> collectiblePages_;
    PageMap cache_;
    quint32 lastPage_;

    QByteArray qInitializedByteArray() const;
    QByteArray qUninitializedByteArray() const;
    bool verifyIntegrity(const Page *page) const;
};

extern QDebug operator << (QDebug dbg, const HBtreePrivate::PageInfo &pi);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::MarkerPage &p);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::NodeKey &k);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::NodeValue &value);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::NodePage::Meta &m);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::NodePage &p);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::OverflowPage &p);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::NodeHeader &n);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::HistoryNode &hn);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::KeyValueMap::iterator &node);
extern QDebug operator << (QDebug dbg, const HBtreePrivate::KeyValueMap::const_iterator &node);



bool HBtreePrivate::NodeKey::operator < (const HBtreePrivate::NodeKey &rhs) const
{
    // Since the btree uses an empty key as an implicit lowest value,
    // we can account for it here and avoid a call to the compare
    // function.
    // Same for all the other comparison operators

//    if (data.isEmpty() && !rhs.data.isEmpty())
//        return true;
//    if (rhs.data.isEmpty())
//        return false;

    if (compareFunction)
        return compareFunction(data, rhs.data) < 0;
    return data < rhs.data;
}

bool HBtreePrivate::NodeKey::operator == (const HBtreePrivate::NodeKey &rhs) const
{
//    if (data.isEmpty() && rhs.data.isEmpty())
//        return true;
//    if (data.isEmpty() || rhs.data.isEmpty())
//        return false;

    if (compareFunction)
        return compareFunction(data, rhs.data) == 0;
    return data == rhs.data;
}

bool HBtreePrivate::NodeKey::operator > (const HBtreePrivate::NodeKey &rhs) const
{
//    if (rhs.data.isEmpty() && !data.isEmpty())
//        return true;
//    if (data.isEmpty())
//        return false;

    if (compareFunction)
        return compareFunction(data, rhs.data) > 0;
    return data > rhs.data;
}

#endif // HBTREE_P_H
