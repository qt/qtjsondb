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

#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stddef.h>

#include <QDebug>

#include "hbtree.h"
#include "hbtree_p.h"

#define HBTREE_DEBUG_OUTPUT 0
#define HBTREE_VERBOSE_OUTPUT 0

#define HBTREE_VERSION 0xdeadc0de
#define HBTREE_DEFAULT_PAGE_SIZE 4096

#if HBTREE_VERBOSE_OUTPUT && !HBTREE_DEBUG_OUTPUT
#   undef HBTREE_VERBOSE_OUTPUT
#   define HBTREE_VERBOSE_OUTPUT 0
#endif

#define HBTREE_DEBUG(qDebugStatement) if (HBTREE_DEBUG_OUTPUT) qDebug() << "HBtree::" << __FUNCTION__ << ":" << qDebugStatement
#define HBTREE_VERBOSE(qDebugStatement) if (HBTREE_VERBOSE_OUTPUT) HBTREE_DEBUG(qDebugStatement)
#define HBTREE_ERROR(qDebugStatement) qCritical() << "HBtree Error::" << __FUNCTION__ << ":" << qDebugStatement


// NOTES:

// What happens when marker revision overflows? Maybe you need to reset revisions from time to time?

// Choosing current markers and assuring read transactions have their pages depends on revisions.

// Do we want to open up markers ever transaction?? Old btree could just check for a size change to know if
// things changes. This implementation can't do that...

// Might need a new type of page that stored garbage information for when you close a db or crash and there're
// a lot of reusable pages lying around...

const quint32 HBtreePrivate::PageInfo::INVALID_PAGE = 0xFFFFFFFF;

// ######################################################################
// ### Creation destruction
// ######################################################################

HBtreePrivate::HBtreePrivate(HBtree *q, const QString &name)
    : q_ptr(q), fileName_(name), fd_(-1), openMode_(HBtree::ReadOnly), size_(0), lastSyncedRevision_(0),
      mi_(0), compareFunction_(0),
      writeTransaction_(0), lastPage_(PageInfo::INVALID_PAGE)
{
}

HBtreePrivate::~HBtreePrivate()
{
}

bool HBtreePrivate::open(int fd)
{
    close();

    Q_Q(HBtree);
    q->stats_ = HBtree::Stat();

    if (fd == -1)
        return false;
    fd_ = fd;

    QByteArray binaryData(HBTREE_DEFAULT_PAGE_SIZE, (char)0);

    // Read spec page
    int rc = pread(fd_, (void *)binaryData.data(), HBTREE_DEFAULT_PAGE_SIZE, 0);
    q->stats_.reads++;

    markers_[0] = MarkerPage(3);
    markers_[1] = MarkerPage(4);

    if (rc != HBTREE_DEFAULT_PAGE_SIZE) {
        // Write spec
        if (rc == 0) {

            HBTREE_DEBUG("New file:" << "[" << "fd:" << fd_ << "]");

            if (!writeSpec()) {
                HBTREE_ERROR("failed to write spec");
                return false;
            }

            const quint32 initSize = spec_.pageSize * 5;

            // Write sync markers
            MarkerPage synced0(1);
            MarkerPage synced1(2);
            synced0.meta.size = synced1.meta.size = initSize;

            if (!serializeAndWrite(synced0)) {
                HBTREE_ERROR("failed to write sync0");
                return false;
            }

            if (!serializeAndWrite(synced1)) {
                HBTREE_ERROR("failed to write sync1");
                return false;
            }

            // Write ping and pong
            MarkerPage ping(3);
            MarkerPage pong(4);
            ping.meta.size = pong.meta.size = initSize;

            if (!serializeAndWrite(ping)) {
                HBTREE_ERROR("failed to write ping");
                return false;
            }

            if (!serializeAndWrite(pong)) {
                HBTREE_ERROR("failed to write pong");
                return false;
            }

            if (fsync(fd_) != 0) {
                HBTREE_DEBUG("failed to sync markers");
                return false;
            }

            mi_ = 0;
            lastSyncedRevision_ = 0;
            size_ = initSize;
            copy(ping, &markers_[0]);
            copy(pong, &markers_[1]);

        } else {
            HBTREE_ERROR("failed to read spec page: rc" << rc);
            return false;
        }
    } else {

        if (!readSpec(binaryData)) {
            HBTREE_ERROR("failed to read spec information");
            return false;
        }

        // Get synced marker
        MarkerPage syncedMarker;
        bool hasSyncedMarker = readSyncedMarker(&syncedMarker);

        QByteArray pingData = readPage(3);
        QByteArray pongData = readPage(4);

        MarkerPage pingMarker;
        MarkerPage pongMarker;

        bool result1 = pingData.isEmpty() ? false : deserializePage(pingData, &pingMarker) && pingMarker.info.isValid();
        bool result2 = pongData.isEmpty() ? false : deserializePage(pongData, &pongMarker) && pongMarker.info.isValid();

        bool hasAnotherMarker = false;
        bool doTheWalk = false;
        if (result1 && result2) {
            // If both are ok then use whichever has a bigger revision
            mi_ = pingMarker.meta.revision > pongMarker.meta.revision ? 0 : 1;
            copy(mi_ == 0 ? pingMarker : pongMarker, &markers_[0]);
            copy(mi_ == 0 ? pongMarker : pingMarker, &markers_[1]);
            hasAnotherMarker = true;
        } else if (result1) {
            HBTREE_DEBUG("pong marker is invalid");
            copy(pingMarker, &markers_[0]);
            copy(pingMarker, &markers_[1]);
        } else if (result2) {
            HBTREE_DEBUG("ping marker is invalid");
            copy(pongMarker, &markers_[0]);
            copy(pongMarker, &markers_[1]);
        } else {
            // If neither are ok then use synced marker
            if (hasSyncedMarker) {
                copy(syncedMarker, &markers_[0]);
                copy(syncedMarker, &markers_[1]);
                doTheWalk = true;
            } else {
                HBTREE_ERROR("failed to load valid marker");
                // Is this an empty tree?
                return false;
            }
        }

        mi_ = 0;

        // 3 levels of awesomeness
        // If we have a synced marker and the revision of the marker we decided to use matches it,
        // then we don't need to verify the tree
        QList<quint32> pingPagesWalked;
        QList<quint32> pongPagesWalked;
        QList<quint32> syncedPagesWalked;
        bool mustCollect = false;
        if (!hasSyncedMarker || currentMarker().meta.revision != syncedMarker.meta.revision || doTheWalk) {
            mustCollect = true;
            // If verification fails, try the other marker
            if (!walkTree(currentMarker(), &pingPagesWalked) && hasAnotherMarker) {
                copy(markers_[!mi_], &markers_[mi_]);
                if (!walkTree(currentMarker(), &pongPagesWalked) && hasSyncedMarker) {
                    // If that fails we still have the synced marker
                    copy(syncedMarker, &markers_[0]);
                    copy(syncedMarker, &markers_[1]);
                    if (!walkTree(currentMarker(), &syncedPagesWalked)) {
                        HBTREE_ERROR("Unable to verify tree");
                        return false;
                    }
                }
            }
        }

        if (openMode_ == HBtree::ReadWrite) {
            off_t currentSize = lseek(fd_, 0, SEEK_END);
            if (static_cast<off_t>(currentMarker().meta.size) < currentSize) {// cut off garbage (TODO: check reverse case)
                if (ftruncate(fd_, currentMarker().meta.size) != 0) {
                    HBTREE_ERROR("failed to truncate from" << currentSize << "for" << currentMarker());
                    return false;
                }
            }

            size_ = currentMarker().meta.size;
            lastSyncedRevision_ = currentMarker().meta.syncedRevision;

            if (!sync(currentMarker())) {
                HBTREE_ERROR("failed to sync" << currentMarker());
                return false;
            }

            if (mustCollect) {
                QList<quint32> totalPagesWalked = syncedPagesWalked + pingPagesWalked + pongPagesWalked;
                qSort(totalPagesWalked);
                collectPages(totalPagesWalked);
            } else {
                collectiblePages_ = currentMarker().collectiblePages;
            }
        } else {
            size_ = currentMarker().meta.size;
        }
    }

    lastPage_ = size_ / spec_.pageSize;
    Q_ASSERT(verifyIntegrity(&currentMarker()));

    return true;
}

void HBtreePrivate::close(bool doSync)
{
    if (fd_ != -1) {
        HBTREE_DEBUG("closing btree with fd:" << fd_);
        if (doSync)
            sync(currentMarker());
        ::close(fd_);
        fd_ = -1;
        if (dirtyPages_.size()) {
            HBTREE_DEBUG("aborting" << dirtyPages_.size() << "dirty pages");
            dirtyPages_.clear();
        }
        foreach (Page * page, cache_) {
            deletePage(page);
        }
        cache_.clear();
        collectiblePages_.clear();
        spec_ = Spec();
    }
}

bool HBtreePrivate::readSpec(const QByteArray &binaryData)
{
    PageInfo info;
    Spec spec;
    memcpy(&info, binaryData.constData(), sizeof(PageInfo));
    memcpy(&spec, binaryData.constData() + sizeof(PageInfo), sizeof(Spec));

    if (info.type != PageInfo::Spec) {
        HBTREE_ERROR("failed to read spec:" << info);
        return false;
    }

    if (info.number != 0) {
        HBTREE_ERROR("failed to read spec:" << info);
        return false;
    }

    if (spec.version != HBTREE_VERSION) {
        HBTREE_ERROR("failed to read spec version:" << spec.version);
        return false;
    }

    memcpy(&spec_, &spec, sizeof(Spec));

    if (calculateChecksum(binaryData) != info.checksum) {
        HBTREE_ERROR("failed to verify spec checksum");
        return false;
    }

    return true;
}

bool HBtreePrivate::writeSpec()
{
    struct stat sb;
    if (fstat(fd_, &sb) != 0)
        return false;

    Spec spec;
    spec.version = HBTREE_VERSION;
    spec.keySize = 255;
    spec.pageSize = sb.st_blksize > HBTREE_DEFAULT_PAGE_SIZE ? sb.st_blksize : HBTREE_DEFAULT_PAGE_SIZE;
    spec.overflowThreshold = 1000;

    QByteArray ba(spec.pageSize, (char)0);

    PageInfo info(PageInfo::Spec, 0);
    memcpy(ba.data(), &info, sizeof(PageInfo));
    memcpy(ba.data() + sizeof(PageInfo), &spec, sizeof(Spec));

    memcpy(&spec_, &spec, sizeof(Spec));

    if (!writePage(&ba)) {
        HBTREE_ERROR("failed to write spec page");
        return false;
    }
    return true;
}

// ######################################################################
// ### Serialization and deserialization
// ######################################################################

QByteArray HBtreePrivate::serializePage(const HBtreePrivate::Page &page) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);

    switch (page.info.type) {
    case PageInfo::Marker:
        return serializeMarkerPage(static_cast<const MarkerPage &>(page));
    case PageInfo::Branch:
    case PageInfo::Leaf:
        return serializeNodePage(static_cast<const NodePage &>(page));
    case PageInfo::Overflow:
        return serializeOverflowPage(static_cast<const OverflowPage &>(page));
    default:
        Q_ASSERT(0);
    }
    return QByteArray();
}

HBtreePrivate::Page *HBtreePrivate::deserializePage(const QByteArray &buffer, Page *page) const
{
    Q_ASSERT(page);
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);

    quint32 pageType = deserializePageType(buffer);
    switch (pageType) {
    case PageInfo::Marker:
        static_cast<MarkerPage &>(*page) = deserializeMarkerPage(buffer);
        break;
    case PageInfo::Leaf:
    case PageInfo::Branch:
        static_cast<NodePage &>(*page) = deserializeNodePage(buffer);
        break;
    case PageInfo::Overflow:
        static_cast<OverflowPage &>(*page) = deserializeOverflowPage(buffer);
        break;
    default:
        Q_ASSERT(0);
    }
    return page;
}

HBtreePrivate::PageInfo HBtreePrivate::deserializePageInfo(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);

    PageInfo info;
    memcpy(&info, buffer.constData(), sizeof(PageInfo));
    return info;
}

void HBtreePrivate::serializePageInfo(const HBtreePrivate::PageInfo &info, QByteArray *buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer->isDetached());
    Q_ASSERT(buffer->size() == (int)spec_.pageSize);
    memcpy(buffer->data(), &info, sizeof(PageInfo));
}

HBtreePrivate::Page *HBtreePrivate::newDeserializePage(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);

    PageInfo pi = deserializePageInfo(buffer);
    Page *page = 0;
    switch (pi.type) {
        case PageInfo::Leaf:
        case PageInfo::Branch:
            page = new NodePage;
            break;
        case PageInfo::Marker:
            page = new MarkerPage;
            break;
        case PageInfo::Overflow:
            page = new OverflowPage;
            break;
        case PageInfo::Spec:
            Q_ASSERT(0);
            return 0;
    }
    if (!deserializePage(buffer, page)) {
        deletePage(page);
        return 0;
    }
    return page;
}

bool HBtreePrivate::serializeAndWrite(const HBtreePrivate::Page &page) const
{
    Q_ASSERT(page.info.type == PageInfo::Branch ||
             page.info.type == PageInfo::Leaf ||
             page.info.type == PageInfo::Overflow ||
             page.info.type == PageInfo::Marker);
    QByteArray ba = serializePage(page);
    if (ba.isEmpty()) {
        HBTREE_ERROR("failed to serialize" << page.info);
        return false;
    }

    if (!writePage(&ba)) {
        HBTREE_ERROR("failed to write" << page.info);
        return false;
    }

    return true;
}

void HBtreePrivate::serializeChecksum(quint32 checksum, QByteArray *buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(checksum != 0);
    Q_ASSERT(buffer->isDetached());
    Q_ASSERT(buffer->size() == (int)spec_.pageSize);
    memcpy(buffer->data() + PageInfo::OFFSETOF_CHECKSUM, &checksum, sizeof(quint32));
}

quint32 HBtreePrivate::deserializePageNumber(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);
    quint32 pageNumber;
    memcpy(&pageNumber, buffer.constData() + PageInfo::OFFSETOF_NUMBER, sizeof(quint32));
    return pageNumber;
}

quint32 HBtreePrivate::deserializePageType(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);
    quint32 pageType;
    memcpy(&pageType, buffer.constData() + PageInfo::OFFSETOF_TYPE, sizeof(quint32));
    return pageType;
}

quint32 HBtreePrivate::deserializePageChecksum(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);
    quint32 checksum;
    memcpy(&checksum, buffer.constData() + PageInfo::OFFSETOF_CHECKSUM, sizeof(quint32));
    return checksum;
}

HBtreePrivate::MarkerPage HBtreePrivate::deserializeMarkerPage(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);

    MarkerPage page;
    memcpy(&page.info, buffer.constData(), sizeof(PageInfo));

    HBTREE_DEBUG("deserializing" << page.info);

    memcpy(&page.meta, buffer.constData() + sizeof(PageInfo), sizeof(MarkerPage::Meta));

    if (page.info.hasPayload()) {
        Q_ASSERT(page.info.lowerOffset > 0);
        Q_ASSERT(page.info.upperOffset == 0);
        const char *ptr = buffer.constData() + sizeof(PageInfo) + sizeof(MarkerPage::Meta);
        for (int i = 0; i < page.info.lowerOffset; ++i) {
            quint32 pgno;
            memcpy(&pgno, ptr, sizeof(quint32));
            page.collectiblePages.insert(pgno);
            ptr += sizeof(quint32);
        }
    }

    HBTREE_DEBUG("deserialized" << page);

    return page;
}

QByteArray HBtreePrivate::serializeMarkerPage(const HBtreePrivate::MarkerPage &page) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);

    HBTREE_DEBUG("serializing" << page.info);

    QByteArray buffer = qInitializedByteArray();

    memcpy(buffer.data(), &page.info, sizeof(PageInfo));
    memcpy(buffer.data() + sizeof(PageInfo), &page.meta, sizeof(MarkerPage::Meta));

    Q_ASSERT(page.info.lowerOffset == page.collectiblePages.size());
    if (page.info.hasPayload()) {
        Q_ASSERT(page.info.lowerOffset > 0);
        Q_ASSERT(page.info.upperOffset == 0);
        char *ptr = buffer.data() + sizeof(PageInfo) + sizeof(MarkerPage::Meta);
        foreach (quint32 pgno, page.collectiblePages) {
            memcpy(ptr, &pgno, sizeof(quint32));
            ptr += sizeof(quint32);
        }
    }

    HBTREE_VERBOSE("serialized" << page);

    return buffer;
}

HBtreePrivate::NodePage HBtreePrivate::deserializeNodePage(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);

    NodePage page;
    page.info = deserializePageInfo(buffer);

    HBTREE_DEBUG("deserializing" << page.info);

    memcpy(&page.meta, buffer.constData() + sizeof(PageInfo), sizeof(NodePage::Meta));

    // deserialize history
    HBTREE_VERBOSE("deserialising" << page.meta.historySize << "history nodes");
    size_t offset = sizeof(PageInfo) + sizeof(NodePage::Meta);
    for (int i = 0; i < page.meta.historySize; ++i) {
        HistoryNode hn;
        memcpy(&hn, buffer.constData() + offset, sizeof(HistoryNode));
        offset += sizeof(HistoryNode);
        HBTREE_VERBOSE("deserialized:" << hn);
        page.history.append(hn);
    }

    // deserialize page nodes
    HBTREE_VERBOSE("deserialising" << page.info.lowerOffset / sizeof(quint16) << "nodes");

    if (page.info.hasPayload()) {
        page.nodes.reserve(page.info.lowerOffset / sizeof(quint16));
        quint16 *indices = (quint16 *)(buffer.constData() + offset);
        for (size_t i = 0; i < page.info.lowerOffset / sizeof(quint16); ++i) {
            const char *nodePtr = (buffer.constData() + buffer.size()) - indices[i];
            NodeHeader node;
            memcpy(&node, nodePtr, sizeof(NodeHeader));

            NodeKey key(compareFunction_, QByteArray(nodePtr + sizeof(NodeHeader), node.keySize));
            NodeValue value;

            if (node.flags & NodeHeader::Overflow || page.info.type == PageInfo::Branch) {
                value.overflowPage = node.context.overflowPage;
                if (page.info.type == PageInfo::Leaf)
                    value.flags = NodeHeader::Overflow;
            } else {
                value.data = QByteArray(nodePtr + sizeof(NodeHeader) + node.keySize, node.context.valueSize);
            }

            HBTREE_VERBOSE("deserialized node" << i << "->" << node << "<- [" << key << "," << value << "]");
            page.nodes.uncheckedAppend(key, value);
        }
    }
    return page;
}

QByteArray HBtreePrivate::serializeNodePage(const HBtreePrivate::NodePage &page) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(page.history.size() == page.meta.historySize);

    HBTREE_DEBUG("serializing" << page.info);

    QByteArray buffer = qInitializedByteArray();

    serializePageInfo(page.info, &buffer);
    memcpy(buffer.data() + sizeof(PageInfo), &page.meta, sizeof(NodePage::Meta));

    size_t offset = sizeof(PageInfo) + sizeof(NodePage::Meta);
    foreach (const HistoryNode &hn, page.history) {
        memcpy(buffer.data() + offset, &hn, sizeof(HistoryNode));
        offset += sizeof(HistoryNode);
    }

    HBTREE_VERBOSE("serializing" << page.info.lowerOffset / sizeof(quint16) << "page nodes");

    if (page.info.hasPayload()) {
        int i = 0;
        quint16 *indices = (quint16 *)(buffer.data() + offset);
        char *upperPtr = buffer.data() + buffer.size();
        Node it = page.nodes.constBegin();
        while (it != page.nodes.constEnd()) {
            const NodeKey &key = it.key();
            const NodeValue &value = it.value();
            quint16 nodeSize = value.data.size() + key.data.size() + sizeof(NodeHeader);
            upperPtr -= nodeSize;
            NodeHeader node;
            node.flags = value.flags;
            node.keySize = key.data.size();
            if ((value.flags & NodeHeader::Overflow) || page.info.type == PageInfo::Branch) {
                Q_ASSERT(value.data.size() == 0);
                node.context.overflowPage = value.overflowPage;
            } else {
                Q_ASSERT(page.info.type == PageInfo::Leaf);
                node.context.valueSize = value.data.size();
            }
            memcpy(upperPtr, &node, sizeof(NodeHeader));
            memcpy(upperPtr + sizeof(NodeHeader), key.data.constData(), key.data.size());
            memcpy(upperPtr + sizeof(NodeHeader) + key.data.size(), value.data.constData(), value.data.size());
            quint16 upperOffset = (quint16)((buffer.data() + buffer.size()) - upperPtr);
            indices[i++] = upperOffset;
            HBTREE_VERBOSE("serialized node" << i << "->" << node
                           << "@offset" << upperOffset << "<- [" << key<< "," << value << "]");
            ++it;
        }
    }

    return buffer;
}

HBtreePrivate::NodePage::Meta HBtreePrivate::deserializeNodePageMeta(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);
    NodePage::Meta meta;
    memcpy(&meta, buffer.constData() + sizeof(PageInfo), sizeof(NodePage::Meta));
    return meta;
}

HBtreePrivate::OverflowPage HBtreePrivate::deserializeOverflowPage(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);

    OverflowPage page;
    page.info = deserializePageInfo(buffer);

    HBTREE_DEBUG("deserializing" << page.info);

    NodeHeader node;
    memcpy(&node, buffer.constData() + sizeof(PageInfo), sizeof(NodeHeader));
    page.nextPage = node.context.overflowPage;
    page.data.resize(node.keySize);
    memcpy(page.data.data(), buffer.constData() + sizeof(PageInfo) + sizeof(NodeHeader), node.keySize);

    HBTREE_VERBOSE("deserialized" << page);
    return page;

}

QByteArray HBtreePrivate::serializeOverflowPage(const HBtreePrivate::OverflowPage &page) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT((size_t)page.data.size() <= capacity(&page));

    HBTREE_DEBUG("serializing" << page.info);

    QByteArray buffer = qInitializedByteArray();
    serializePageInfo(page.info, &buffer);
    NodeHeader node;
    node.flags = 0;
    node.keySize = page.data.size();
    node.context.overflowPage = page.nextPage;
    memcpy(buffer.data() + sizeof(PageInfo), &node, sizeof(NodeHeader));
    memcpy(buffer.data() + sizeof(PageInfo) + sizeof(NodeHeader), page.data.constData(), page.data.size());

    HBTREE_VERBOSE("serialized" << page);

    return buffer;
}

// ######################################################################
// ### Page reading/writing/commiting/syncing
// ######################################################################

QByteArray HBtreePrivate::readPage(quint32 pageNumber) const
{
    QByteArray buffer = qUninitializedByteArray();

    const off_t offset = pageNumber * spec_.pageSize;
    if (lseek(fd_, offset, SEEK_SET) != offset)
        return QByteArray();

    ssize_t rc = read(fd_, (void *)buffer.data(), spec_.pageSize);
    if (rc != spec_.pageSize)
        return QByteArray();

    PageInfo pageInfo = deserializePageInfo(buffer);

    if (pageInfo.number != pageNumber)
        return QByteArray();

    if (pageInfo.checksum != calculateChecksum(buffer))
        return QByteArray();

    HBTREE_DEBUG("read page:" << pageInfo);

    const Q_Q(HBtree);
    const_cast<HBtree*>(q)->stats_.reads++;

    return buffer;
}

bool HBtreePrivate::writePage(QByteArray *buffer) const
{
    Q_ASSERT(buffer);
    Q_ASSERT(buffer->isDetached());
    Q_ASSERT(spec_.pageSize > 0);
    Q_ASSERT(buffer->size() == spec_.pageSize);

    quint32 checksum = calculateChecksum(*buffer);
    serializeChecksum(checksum, buffer);

    quint32 pageNumber = deserializePageNumber(*buffer);

    Q_ASSERT(pageNumber != PageInfo::INVALID_PAGE);

    const off_t offset = pageNumber * spec_.pageSize;
    if (lseek(fd_, offset, SEEK_SET) != offset)
        return false;

    ssize_t rc = write(fd_, (const void *)buffer->constData(), spec_.pageSize);
    if (rc != spec_.pageSize)
        return false;

    HBTREE_DEBUG("wrote page" << deserializePageInfo(*buffer));

    const Q_Q(HBtree);
    const_cast<HBtree *>(q)->stats_.writes++;

    return true;
}

bool HBtreePrivate::sync(const MarkerPage &mp)
{
    Q_ASSERT(verifyIntegrity(&mp));
    HBTREE_DEBUG("syncing" << mp);

    if (openMode_ == HBtree::ReadOnly)
        return true;

    if (mp.meta.syncedRevision == lastSyncedRevision_)
        return true;

    QByteArray ba;

    MarkerPage synced0(1);
    MarkerPage synced1(2);
    copy(mp, &synced0);
    copy(mp, &synced1);

    if (fsync(fd_) != 0)
        return false;

    ba = serializePage(synced0);
    if (!writePage(&ba))
        return false;

    lastSyncedRevision_++;

    HBTREE_VERBOSE("synced marker 1 and upped revision to" << lastSyncedRevision_);

    copy(synced0, &markers_[0]);
    copy(synced0, &markers_[1]);

    if (fsync(fd_) != 0)
        return false;

    Q_Q(HBtree);
    q->stats_.numSyncs++;

    ba = serializePage(synced1);
    if (!writePage(&ba))
        return false;

    HBTREE_VERBOSE("synced marker 2");

    return true;
}

bool HBtreePrivate::readSyncedMarker(HBtreePrivate::MarkerPage *markerOut)
{
    Q_ASSERT(markerOut);
    QByteArray syncedMarkerData = readPage(1);
    if (syncedMarkerData.isEmpty() || !deserializePage(syncedMarkerData, markerOut)) {
        HBTREE_DEBUG("synced marker 1 invalid. Checking synced marker 2.");
        syncedMarkerData = readPage(2);
        if (syncedMarkerData.isEmpty() || !deserializePage(syncedMarkerData, markerOut)) {
            HBTREE_DEBUG("sync markers both invalid.");
            return false;
        }
    }
    return true;
}

bool HBtreePrivate::rollback()
{
    // Rollback from current marker.
    // If other marker is the same or is invalid, then rollback
    // to synced marker.
//    QList<quint32> pongWalk;
//    if (markers_[!mi_].info.isValid()) {
//        if (walkTree(markers[!mi_], &pongWalk)) {

//        }
//    }

//    MarkerPage syncedMarker;
//    if (!readSyncedMarker(&syncedMarker))
//        return false;

    return false;
}

bool HBtreePrivate::commit(HBtreeTransaction *transaction, quint64 tag)
{
    Q_ASSERT(transaction);
    HBTREE_DEBUG("commiting" << dirtyPages_.size() << "pages");

    PageMap::iterator it = dirtyPages_.begin();
    while (it != dirtyPages_.constEnd()) {
        switch (it.value()->info.type) {
        case PageInfo::Branch:
        case PageInfo::Leaf: {
            NodePage *np = static_cast<NodePage *>(it.value());

            // reclaim
            QVector<HistoryNode>::iterator it = np->history.begin();
            while (it != np->history.end()) {

                // collect pages before last sync
                bool canCollect = currentMarker().meta.syncedRevision && it->syncNumber < lastSyncedRevision_;

                // or collect pages that have not been synced yet (sync revision greater than last sync)
                // and are no longer referenced by the 2 ping pong markers (ie. commit number is less
                // than current commit number - 1)
                canCollect |= currentMarker().meta.revision
                        && it->commitNumber < (currentMarker().meta.revision - 1)
                        && it->commitNumber > lastSyncedRevision_;

                if (canCollect) {
                    HBTREE_DEBUG("adding page from history" << *it << "to collectible list");
                    collectiblePages_.insert(it->pageNumber);
                    if (cache_.contains(it->pageNumber)) {
                        // delete from cache since we'll be reusing... or
                        // maybe not delete and keep a bool collectible in Page?
                        Q_ASSERT(cache_[it->pageNumber]->commited);
                        deletePage(cache_[it->pageNumber]);
                        cache_.remove(it->pageNumber);
                    }
                    it = np->history.erase(it);
                    np->meta.historySize--;
                    continue;
                }
                ++it;
            }
            break;
        }
        case PageInfo::Overflow:
            break;
        default:
            Q_ASSERT(0);
        }

        Q_ASSERT(verifyIntegrity(it.value()));
        QByteArray ba = serializePage(*it.value());
        if (!writePage(&ba))
            return false;
        it.value()->commited = true;
        it = dirtyPages_.erase(it);
    }

    size_ = lseek(fd_, 0, SEEK_END);
    dirtyPages_.clear();

    // Write marker
    uint mi = !mi_;
    MarkerPage &mp = markers_[mi];
    mp.meta.revision = currentMarker().meta.revision + 1;
    mp.meta.syncedRevision = lastSyncedRevision_ + 1;
    mp.meta.rootPage = transaction->rootPage_;
    mp.meta.tag = tag;
    mp.meta.size = size_;
    mp.collectiblePages = collectiblePages_;
    mp.info.lowerOffset = collectiblePages_.size();
    QByteArray ba = serializePage(mp);
    if (!writePage(&ba))
        return false;
    mi_ = mi;

    abort(transaction);

    Q_Q(HBtree);
    q->stats_.numCommits++;

    // TODO: don't delete everything from the cache.
    foreach (Page *page, cache_) {
        deletePage(page);
    }
    cache_.clear();

    return true;
}

void HBtreePrivate::abort(HBtreeTransaction *transaction)
{
    Q_ASSERT(transaction);
    HBTREE_DEBUG("aborting transaction with" << dirtyPages_.size() << "dirty pages");
    foreach (Page *page, dirtyPages_) {
        Q_ASSERT(cache_.contains(page->info.number));
        cache_.remove(page->info.number);
        deletePage(page);
    }
    dirtyPages_.clear();
    if (transaction->isReadWrite()) {
        if (::flock(fd_, LOCK_UN) != 0)
            HBTREE_ERROR("failed to unlock file with transaction @" << transaction);
        writeTransaction_ = 0;
    }
    delete transaction;
}

quint32 HBtreePrivate::calculateChecksum(const char *begin, const char *end) const
{
    const quint32 *begin32 = (const quint32*)begin;
    const quint32 *end32 = (const quint32*)(end - ((end - begin) % 4));
    if (begin32 >= end32)
        return 0;
    /* code derived from 32-bit CRC calculation by Gary S. Brown - Copyright (C) 1986. */
    static const quint32 crctable[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    quint32 crc = ~(*begin32++);
    while (begin32 < end32) {
        begin = (const char*)begin32++;
        crc = (crc >> 8) ^ crctable[(crc ^ *(begin + 0)) & 0x000000ff];
        crc = (crc >> 8) ^ crctable[(crc ^ *(begin + 1)) & 0x000000ff];
        crc = (crc >> 8) ^ crctable[(crc ^ *(begin + 2)) & 0x000000ff];
        crc = (crc >> 8) ^ crctable[(crc ^ *(begin + 3)) & 0x000000ff];
    }

    // Hash up remaining bytes
    if ((const char *)end32 < end) {
            begin = (const char *)end32;
            while (begin != end)
                crc = (crc >> 8) ^ crctable[(crc ^ *begin++) & 0x000000ff];
    }
    return ~crc;
}

quint32 HBtreePrivate::calculateChecksum(const QByteArray &buffer) const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    Q_ASSERT(buffer.size() == (int)spec_.pageSize);

    const size_t crcOffset = sizeof(quint32);
    const char *begin = buffer.constData();
    const char *end = buffer.constData() + spec_.pageSize;
    PageInfo info = deserializePageInfo(buffer);
    quint32 crc = 0;
    HBTREE_VERBOSE("calculating checksum for" << info);

    if (info.type == PageInfo::Spec) {
        crc = calculateChecksum(begin + crcOffset, begin + sizeof(PageInfo) + sizeof(Spec));
    } else if (info.type == PageInfo::Branch || info.type == PageInfo::Leaf) {
        NodePage::Meta meta = deserializeNodePageMeta(buffer);
        size_t lower = info.headerSize() + meta.historySize * sizeof(HistoryNode) + info.lowerOffset;
        quint32 c1 = calculateChecksum(begin + crcOffset, begin + lower);
        quint32 c2 = calculateChecksum(end - info.upperOffset, end);
        crc = c1 ^ c2;
    } else if (info.type == PageInfo::Marker) {
        crc = calculateChecksum(begin + crcOffset, begin + info.headerSize());
    } else if (info.type == PageInfo::Overflow) {
        Q_ASSERT(info.hasPayload()); // lower offset represents size of overflow
        crc = calculateChecksum(begin + crcOffset, begin + info.headerSize() + info.lowerOffset);
    } else
        Q_ASSERT(0);

    HBTREE_VERBOSE("checksum:" << ~crc);

    return crc;
}

// ######################################################################
// ### btree operations
// ######################################################################


HBtreeTransaction *HBtreePrivate::beginTransaction(HBtreeTransaction::Type type)
{
    Q_Q(HBtree);

    if (type == HBtreeTransaction::ReadWrite && writeTransaction_) {
        HBTREE_ERROR("cannot open write transaction when one in progress");
        return 0;
    }

    if (type == HBtreeTransaction::ReadWrite && openMode_ == HBtree::ReadOnly) {
        HBTREE_ERROR("cannot open write transaction on read only btree");
        return 0;
    }

//    if (type == HBtreeTransaction::ReadOnly && currentMarker().meta.rootPage == PageInfo::INVALID_PAGE) {
//        HBTREE_ERROR("nothing to read");
//        return 0;
//    }

    if (type == HBtreeTransaction::ReadWrite) {
        Q_ASSERT(dirtyPages_.isEmpty());
        if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
            HBTREE_ERROR("failed to take write lock");
            return 0;
        }
    }

    // Do we check here if a write process wrote some more data?
    // The readAndWrite auto test will not pass without some kind of
    // marker update check.

    HBtreeTransaction *transaction = new HBtreeTransaction(q, type);
    transaction->rootPage_ = currentMarker().meta.rootPage;
    transaction->tag_ = currentMarker().meta.tag;
    transaction->revision_ = currentMarker().meta.revision;
    if (type == HBtreeTransaction::ReadWrite)
        writeTransaction_ = transaction;
    HBTREE_DEBUG("began" << (transaction->isReadOnly() ? "read" : "write")
                 << "transaction @" << transaction
                 << "[root:" << transaction->rootPage_
                 << ", tag:" << transaction->tag_
                 << ", revision:" << transaction->revision_
                 << "]");
    return transaction;
}

bool HBtreePrivate::put(HBtreeTransaction *transaction, const QByteArray &keyData, const QByteArray &valueData)
{
    HBTREE_DEBUG( "put => [" << keyData
              #if HBTREE_VERBOSE_OUTPUT
                  << "," << valueData
              #endif
                  << "] @" << transaction);

    if (transaction->isReadOnly()) {
        HBTREE_ERROR("can't write with read only transaction");
        return false;
    }

    NodeKey nkey(compareFunction_, keyData);
    NodeValue nval(valueData);

    bool closeTransaction = transaction == NULL;
    if (closeTransaction) {
        HBTREE_DEBUG("transaction not provided. Creating one.");
        transaction = beginTransaction(HBtreeTransaction::ReadWrite);
        if (!transaction)
            return false;
    }

    // If new file, create page
    NodePage *page = 0;
    if (transaction->rootPage_ == PageInfo::INVALID_PAGE) {
        HBTREE_DEBUG("btree empty. Creating new root");
        page = static_cast<NodePage *>(newPage(PageInfo::Leaf));
        transaction->rootPage_ = page->info.number;
        Q_ASSERT(verifyIntegrity(page));
    }

    Q_Q(HBtree);
    if (!page && searchPage(NULL, transaction, nkey, SearchKey, true, &page)) {
        if (page->nodes.contains(nkey)) {
            HBTREE_DEBUG("already contains key. Removing");
            if (!removeNode(page, nkey)) {
                HBTREE_ERROR("failed to remove previous value of key");
                return false;
            }
            q->stats_.numEntries--;
        }
    }

    bool ok = false;
    if (spaceNeededForNode(keyData, valueData) <= nodeSpaceLeft(page))
        ok = insertNode(page, nkey, nval);
    else
        ok = split(page, nkey, nval);

    Q_ASSERT(verifyIntegrity(page));

    if (closeTransaction) {
        ok = transaction->commit(0);
    }

    q->stats_.numEntries++;

    return ok;
}

QByteArray HBtreePrivate::get(HBtreeTransaction *transaction, const QByteArray &keyData)
{
    HBTREE_DEBUG( "get => [" << keyData << "] @" << transaction);

    NodeKey nkey(compareFunction_, keyData);
    NodePage *page;
    if (!searchPage(NULL, transaction, nkey, SearchKey, false, &page)) {
        HBTREE_DEBUG("failed to find page for transaction" << transaction);
        return QByteArray();
    }

    NodeValue nval = page->nodes.value(nkey, NodeValue());
    return getDataFromNode(nval);
}

bool HBtreePrivate::del(HBtreeTransaction *transaction, const QByteArray &keyData)
{
    HBTREE_DEBUG( "del => [" << keyData << "] @" << transaction);

    if (transaction->isReadOnly()) {
        HBTREE_ERROR("can't delete with read only transaction");
        return false;
    }

    NodeKey nkey(compareFunction_, keyData);

    bool closeTransaction = transaction == NULL;
    if (closeTransaction) {
        transaction = beginTransaction(HBtreeTransaction::ReadWrite);
        if (!transaction)
            return false;
    }

    bool ok = false;
    NodePage *page = 0;
    if (searchPage(NULL, transaction, nkey, SearchKey, true, &page)) {
        if (page->nodes.contains(nkey)) {
            ok = removeNode(page, nkey);
        }
    }

    if (ok && !rebalance(page)) {
        HBTREE_ERROR("failed to rebalance" << *page);
        return false;
    }

    if (closeTransaction) {
        ok |= transaction->commit(0);
    }

    Q_Q(HBtree);
    q->stats_.numEntries--;

    return ok;
}

HBtreePrivate::Page *HBtreePrivate::newPage(HBtreePrivate::PageInfo::Type type)
{
    int pageNumber = PageInfo::INVALID_PAGE;

    if (collectiblePages_.size()) {
        quint32 n = *collectiblePages_.constBegin();
        collectiblePages_.erase(collectiblePages_.begin()); // TODO: remove from here after page is commited. Or use a different queue
        pageNumber = n;
    } else {
        pageNumber = lastPage_++;
    }

    Page *page = 0;
    switch (type) {
        case PageInfo::Leaf:
        case PageInfo::Branch: {
            NodePage *np = new NodePage(type, pageNumber);
            np->meta.commitId = currentMarker().meta.revision + 1;
            np->meta.syncId = lastSyncedRevision_ + 1;
            page = np;
            break;
        }
        case PageInfo::Overflow: {
            OverflowPage *ofp = new OverflowPage(type, pageNumber);
            page = ofp;
            break;
        }
        case PageInfo::Marker:
        case PageInfo::Spec:
        case PageInfo::Unknown:
        default:
            Q_ASSERT(0);
            return 0;
    }

    HBTREE_DEBUG("created new page" << page->info);

    cache_.insert(pageNumber, page);
    dirtyPages_.insert(pageNumber, page);

    Q_Q(HBtree);
    if (type == PageInfo::Branch)
        q->stats_.numBranchPages++;
    else if (type == PageInfo::Leaf)
        q->stats_.numLeafPages++;
    else if (type == PageInfo::Overflow)
        q->stats_.numOverflowPages++;

    return page;
}

HBtreePrivate::NodePage *HBtreePrivate::touchNodePage(HBtreePrivate::NodePage *page)
{
    Q_ASSERT(page->info.type == PageInfo::Branch || page->info.type == PageInfo::Leaf);
    if (page->commited == false) {
        HBTREE_DEBUG(page->info << "not commited yet, no need to touch");
        return page;
    }

    NodePage *copy = static_cast<NodePage *>(newPage(PageInfo::Type(page->info.type)));
    quint32 newPageNumber = copy->info.number;
    HBTREE_DEBUG("page" << page->info.number << "touched to page" << newPageNumber;)
    *copy = *page;
    copy->info.number = newPageNumber;
    copy->meta.commitId = currentMarker().meta.revision + 1;
    copy->meta.syncId = lastSyncedRevision_ + 1;
    if (copy->parent) {
        Q_ASSERT(copy->parent->info.type == PageInfo::Branch);
        Q_ASSERT(copy->parent->nodes.contains(copy->parentKey));
        NodeValue &val = copy->parent->nodes[copy->parentKey];
        val.overflowPage = copy->info.number;
    }
    if (copy->rightPageNumber != PageInfo::INVALID_PAGE) {
        Q_ASSERT(copy->info.type == PageInfo::Leaf);
        NodePage *right = static_cast<NodePage *>(getPage(copy->rightPageNumber));
        right->leftPageNumber = newPageNumber;
    }
    if (copy->leftPageNumber != PageInfo::INVALID_PAGE) {
        Q_ASSERT(copy->info.type == PageInfo::Leaf);
        NodePage *left = static_cast<NodePage *>(getPage(copy->leftPageNumber));
        left->rightPageNumber = newPageNumber;
    }
    copy->commited = false;
    distributeHistoryNodes(copy, HistoryNode(page));

    Q_ASSERT(verifyIntegrity(copy));

    return copy;
}

quint32 HBtreePrivate::putDataOnOverflow(const QByteArray &value)
{
    HBTREE_DEBUG("putting data on overflow page");
    int sizePut = 0;
    quint32 overflowPageNumber = PageInfo::INVALID_PAGE;
    OverflowPage *prevPage = 0;
    while (sizePut < value.size()) {
        OverflowPage *overflowPage = static_cast<OverflowPage *>(newPage(PageInfo::Overflow));
        if (overflowPageNumber == PageInfo::INVALID_PAGE)
            overflowPageNumber = overflowPage->info.number;
        quint32 sizeToPut = qMin((quint16)(value.size() - sizePut), capacity(overflowPage));
        HBTREE_DEBUG("putting" << sizeToPut << "bytes @ offset" << sizePut);
        overflowPage->data.resize(sizeToPut);
        memcpy(overflowPage->data.data(), value.constData() + sizePut, sizeToPut);
        if (prevPage)
            prevPage->nextPage = overflowPage->info.number;
        overflowPage->info.lowerOffset = (quint16)sizeToPut; // put it here too for quicker checksum checking
        sizePut += sizeToPut;
        prevPage = overflowPage;

        Q_ASSERT(verifyIntegrity(overflowPage));
    }
    return overflowPageNumber;
}

QByteArray HBtreePrivate::getDataFromNode(const HBtreePrivate::NodeValue &nval) const
{
    if (nval.flags & NodeHeader::Overflow) {
        QByteArray data;
        if (!getOverflowData(nval.overflowPage, &data))
            return QByteArray();
        return data;
    } else {
        return nval.data;
    }
}

bool HBtreePrivate::getOverflowData(quint32 startPage, QByteArray *data) const
{
    Q_ASSERT(data);
    data->clear();
    if (startPage != PageInfo::INVALID_PAGE) {
        // When insertInCache is false, then getPage does not modify anything so it is safe to const_cast this
        OverflowPage *page = static_cast<OverflowPage *>(const_cast<HBtreePrivate *>(this)->getPage(startPage, false));
        if (!page)
            return false;
        bool ok = getOverflowData(page->nextPage, data);
        if (!ok)
            data->clear();
        else
            data->append(page->data);
        deletePage(page);
        return ok;
    }
    return true;
}

bool HBtreePrivate::getOverflowPageNumbers(quint32 startPage, QList<quint32> *pages) const
{
    Q_ASSERT(pages);
    pages->clear();
    if (startPage != PageInfo::INVALID_PAGE) {
        OverflowPage *page = static_cast<OverflowPage *>(const_cast<HBtreePrivate *>(this)->getPage(startPage, false));
        if (!page)
            return false;
        bool ok = getOverflowPageNumbers(page->nextPage, pages);
        if (!ok)
            pages->clear();
        else
            pages->append(startPage);
        deletePage(page);
        return ok;
    }
    return true;
}

bool HBtreePrivate::walkTree(const HBtreePrivate::MarkerPage &marker, QList<quint32> *pagesWalked) const
{
    if (pagesWalked)
        pagesWalked->clear();
    quint32 root = marker.meta.rootPage;
    bool ok = true;
    if (root != PageInfo::INVALID_PAGE) {
        NodePage *page = static_cast<NodePage *>(const_cast<HBtreePrivate *>(this)->getPage(root, false));
        if (!page)
            return false;
        switch (page->info.type) {
        case PageInfo::Branch:
            ok = walkBranch(page, pagesWalked);
            break;
        case PageInfo::Leaf:
            ok = walkLeaf(page, pagesWalked);
            break;
        default:
            Q_ASSERT(0);
            ok = false;
        }
        if (ok && pagesWalked)
            pagesWalked->append(root);
        deletePage(page);
    }
    if (pagesWalked && !ok)
        pagesWalked->clear();

    return ok;
}

bool HBtreePrivate::walkBranch(HBtreePrivate::NodePage *branch, QList<quint32> *pagesWalked) const
{
    if (pagesWalked)
        pagesWalked->clear();
    bool ok = true;
    Node it = branch->nodes.constBegin();
    while (it != branch->nodes.constEnd()) {
        NodePage *page = static_cast<NodePage *>(const_cast<HBtreePrivate *>(this)->getPage(it.value().overflowPage, false));
        if (!page)
            return false;
        if (pagesWalked)
            pagesWalked->append(page->info.number);
        QList<quint32> pages;
        if (page->info.type == PageInfo::Leaf) {
            ok = walkLeaf(page, &pages);
        } else if (page->info.type == PageInfo::Branch) {
            ok = walkBranch(page, &pages);
        } else {
            Q_ASSERT(0);
            ok = false;
        }
        if (ok && pagesWalked)
            pagesWalked->append(pages);
        deletePage(page);
        ++it;
    }
    if (!ok && pagesWalked)
        pagesWalked->clear();
    return true;
}

bool HBtreePrivate::walkLeaf(HBtreePrivate::NodePage *leaf, QList<quint32> *overflowPages) const
{
    // Nothing to walk if there're no nodes with an overflow page
    if (overflowPages)
        overflowPages->clear();
    bool ok = true;
    if (leaf->meta.flags & NodeHeader::Overflow) {
        Node it = leaf->nodes.constBegin();
        while (it != leaf->nodes.constEnd()) {
            if (it.value().flags & NodeHeader::Overflow) {
                QList<quint32> pages;
                if (!getOverflowPageNumbers(it.value().overflowPage, &pages)) {
                    ok = false;
                    break;
                }
                if (overflowPages)
                    overflowPages->append(pages);
            }
            ++it;
        }
    }
    if (!ok && overflowPages)
        overflowPages->clear();
    return ok;
}

void HBtreePrivate::collectPages(const QList<quint32> &pagesUsedSorted)
{
    // pagesUsedSorted may have duplicates...
    HBTREE_DEBUG("collecting pages not in" << pagesUsedSorted);
    quint32 n = 5;
    quint32 numPages = currentMarker().meta.size / spec_.pageSize;
    quint32 i = 0;
    while (n < numPages && i < (quint32)pagesUsedSorted.size()) {
        while (n < pagesUsedSorted.at(i)) {
            collectiblePages_.insert(n++);
        }
        n = pagesUsedSorted.at(i++) + 1;
    }
    while (n < numPages)
        collectiblePages_.insert(n++);
}

bool HBtreePrivate::searchPage(HBtreeCursor *cursor, HBtreeTransaction *transaction, const NodeKey &key, SearchType searchType,
                               bool modify, HBtreePrivate::NodePage **pageOut)
{
    quint32 root = 0;

    if (!transaction)
        root = currentMarker().meta.rootPage;
    else
        root = transaction->rootPage_;

    if (root == PageInfo::INVALID_PAGE) {
        HBTREE_DEBUG("btree is empty");
        *pageOut = 0;
        return false;
    }

    NodePage *page = static_cast<NodePage *>(getPage(root));

    if (page && modify && page->commited) {
        page = touchNodePage(page);
        transaction->rootPage_ = page->info.number;
    }

    return searchPageRoot(cursor, page, key, searchType, modify, pageOut);
}

bool HBtreePrivate::searchPageRoot(HBtreeCursor *cursor, HBtreePrivate::NodePage *root, const NodeKey &key, SearchType searchType,
                                   bool modify, NodePage **pageOut)
{
    if (!root)
        return false;

    QStack<quint32> rightQ, leftQ;

    NodePage *child = root;
    NodePage *parent = 0;
    Node parentIter;

    while (child->info.type == PageInfo::Branch) {
        if (searchType == SearchLast) {
            parentIter = (child->nodes.constEnd() - 1);
        } else if (searchType == SearchFirst) {
            parentIter = child->nodes.constBegin();
        } else {
            HBTREE_DEBUG("searching upper bound for" << key);
            parentIter = child->nodes.upperBound(key) - 1;
            HBTREE_DEBUG("found key" << parentIter.key() << "to page" << parentIter.value());
        }

        if (parentIter == child->nodes.constBegin())
            leftQ.push(HBtreePrivate::PageInfo::INVALID_PAGE);
        else
            leftQ.push((parentIter - 1).value().overflowPage);

        if (parentIter == (child->nodes.constEnd() - 1))
            rightQ.push(HBtreePrivate::PageInfo::INVALID_PAGE);
        else
            rightQ.push((parentIter + 1).value().overflowPage);

        parent = child;
        child = static_cast<NodePage *>(getPage(parentIter.value().overflowPage));

        if (child->info.type == PageInfo::Branch) {
            child->leftPageNumber = PageInfo::INVALID_PAGE;
            child->rightPageNumber = PageInfo::INVALID_PAGE;
        }

        child->parent = parent;
        child->parentKey = parentIter.key();

        if (modify && child->commited && (child = touchNodePage(child)) == NULL) {
            HBTREE_ERROR("failed to touch page" << child->info);
            return false;
        }

        Q_ASSERT(verifyIntegrity(parent));
    }

    Q_ASSERT(child->info.type == PageInfo::Leaf);

    if (cursor) {
        child->rightPageNumber = getRightSibling(rightQ);
        child->leftPageNumber = getLeftSibling(leftQ);
        cursor->lastLeaf_ = child->info.number;
        cursor->initialized_ = true;

        HBTREE_DEBUG("set right sibling of" << child->info << "to" << child->rightPageNumber);
        HBTREE_DEBUG("set left sibling of" << child->info << "to" << child->leftPageNumber);
    }

    *pageOut = child;
    return true;
}

quint32 HBtreePrivate::getRightSibling(QStack<quint32> rightQ)
{
    HBTREE_DEBUG("rightQ" << rightQ);

    int idx = 0;

    while (rightQ.size() && rightQ.top() == PageInfo::INVALID_PAGE) {
        idx++;
        rightQ.pop();
    }

    HBTREE_DEBUG("must go up" << idx << "levels");

    if (!rightQ.size())
        return PageInfo::INVALID_PAGE;

    if (idx == 0)
        return rightQ.top();

    quint32 pageNumber = rightQ.top();
    do {
        NodePage *page = static_cast<NodePage *>(getPage(pageNumber));
        pageNumber = (page->nodes.constBegin()).value().overflowPage;
    } while (--idx);

    return pageNumber;
}

quint32 HBtreePrivate::getLeftSibling(QStack<quint32> leftQ)
{
    HBTREE_DEBUG("leftQ" << leftQ);

    int idx = 0;

    while (leftQ.size() && leftQ.top() == PageInfo::INVALID_PAGE) {
        idx++;
        leftQ.pop();
    }

    HBTREE_DEBUG("must go up" << idx << "levels");

    if (!leftQ.size())
        return PageInfo::INVALID_PAGE;

    if (idx == 0)
        return leftQ.top();

    quint32 pageNumber = leftQ.top();
    do {
        NodePage *page = static_cast<NodePage *>(getPage(pageNumber));
        pageNumber = (page->nodes.constEnd() - 1).value().overflowPage;
    } while (--idx);

    return pageNumber;
}

HBtreePrivate::Page *HBtreePrivate::getPage(quint32 pageNumber, bool insertInCache)
{
    HBTREE_DEBUG("getting page #" << pageNumber);

    // Only check cache if we want to insert in cache. Otherwise everything is the caller's responsibility
    if (insertInCache) {
        if (cache_.contains(pageNumber)) {
            Q_Q(HBtree);
            q->stats_.hits++;
            return cache_[pageNumber];
        }
    }

    QByteArray buffer;
    buffer = readPage(pageNumber);

    if (buffer.isEmpty()) {
        HBTREE_ERROR("failed to read page" << pageNumber);
        return 0;
    }

    Page *page = newDeserializePage(buffer);
    if (!page) {
        HBTREE_ERROR("failed to deserialize page" << pageNumber);
        return 0;
    }

    page->commited = true;
    Q_ASSERT(verifyIntegrity(page));

    if (insertInCache)
        cache_.insert(pageNumber, page);
    return page;
}

void HBtreePrivate::deletePage(HBtreePrivate::Page *page) const
{
    HBTREE_DEBUG("deleting page" << page->info);
    switch (page->info.type) {
    case PageInfo::Overflow:
        delete static_cast<OverflowPage *>(page);
        break;
    case PageInfo::Leaf:
    case PageInfo::Branch:
        delete static_cast<NodePage *>(page);
        break;
    default:
        Q_ASSERT(0);
    }
}

quint16 HBtreePrivate::spaceNeededForNode(const QByteArray &key, const QByteArray &value) const
{
    // Check for space for the data plus a history node since when it gets touched the
    // next time, it should have space for at least one history node.

    // TODO: Change flags in node page to account for if there're overflow pages
    // and how many pages they use. We would need to account for that in this calculation
    // as well.
    if (willCauseOverflow(key, value))
        return sizeof(NodeHeader) + key.size() + sizeof(quint16) + sizeof(HistoryNode);
    else
        return sizeof(NodeHeader) + key.size() + value.size() + sizeof(quint16) + sizeof(HistoryNode);
}

bool HBtreePrivate::willCauseOverflow(const QByteArray &key, const QByteArray &value) const
{
    Q_UNUSED(key);
    return value.size() > (int)spec_.overflowThreshold;
}

quint16 HBtreePrivate::headerSize(const Page *page) const
{
    switch (page->info.type) {
    case HBtreePrivate::PageInfo::Marker:
        return sizeof(HBtreePrivate::PageInfo)
                + sizeof(HBtreePrivate::MarkerPage::Meta);
    case HBtreePrivate::PageInfo::Leaf:
    case HBtreePrivate::PageInfo::Branch:
        Q_ASSERT(static_cast<const NodePage *>(page)->meta.historySize == static_cast<const NodePage *>(page)->history.size());
        return sizeof(HBtreePrivate::PageInfo)
                + sizeof(HBtreePrivate::NodePage::Meta)
                + static_cast<const NodePage *>(page)->meta.historySize * sizeof(HistoryNode);
    case HBtreePrivate::PageInfo::Overflow:
        return sizeof(HBtreePrivate::PageInfo) + sizeof(HBtreePrivate::NodeHeader);
    default:
        Q_ASSERT(0);
    }
    return 0;
}

quint16 HBtreePrivate::nodeSpaceLeft(const NodePage *page) const
{
    Q_ASSERT(nodeSpaceUsed(page) <= capacity(page));
    return capacity(page) - nodeSpaceUsed(page);
}

quint16 HBtreePrivate::nodeSpaceUsed(const NodePage *page) const
{
    return page->info.upperOffset + page->info.lowerOffset;
}

quint16 HBtreePrivate::capacity(const Page *page) const
{
    return spec_.pageSize - headerSize(page);
}

quint32 HBtreePrivate::pageFullEnough(HBtreePrivate::NodePage *page) const
{
    double pageFill = 1.0 - (float)nodeSpaceLeft(page) / (float)capacity(page);
    pageFill *= 100.0f;
    HBTREE_DEBUG("page fill" << pageFill << "is" << (pageFill > spec_.pageFillThreshold ? "" : "not") << "grater than fill threshold" << spec_.pageFillThreshold);
    return pageFill > spec_.pageFillThreshold;
}

bool HBtreePrivate::hasSpaceFor(HBtreePrivate::NodePage *page, const HBtreePrivate::NodeKey &key, const HBtreePrivate::NodeValue &value) const
{
    quint16 spaceLeft = nodeSpaceLeft(page);
    quint16 spaceRequired = spaceNeededForNode(key.data, value.data);
    return spaceLeft >= spaceRequired;
}

bool HBtreePrivate::insertNode(NodePage *page, const NodeKey &key, const NodeValue &value)
{
    HBTREE_DEBUG("inserting" << key << "in" << page->info);
    Q_ASSERT(page);
    NodeValue valueCopy;
    Q_ASSERT(page->commited == false);

    // Careful here. value could be coming in from put (in which case it won't have the overflow flag
    // set but it will have overflow data in value.data
    // or it could be coming from somewhere else that doesn't have value.data set, but it already
    // has the overflow flag set
    //
    // Currently this can happend from split or merge or move.

    if (page->info.type == PageInfo::Leaf && willCauseOverflow(key.data, value.data)) {
        valueCopy.overflowPage = putDataOnOverflow(value.data);
        valueCopy.flags = NodeHeader::Overflow;
        page->meta.flags |= NodeHeader::Overflow;
        HBTREE_DEBUG("overflow page set to" << valueCopy.overflowPage);
    } else {
        if (page->info.type == PageInfo::Branch) {
            valueCopy.overflowPage = value.overflowPage;
        } else if (value.flags & NodeHeader::Overflow) {
            // if value.flags is already initialized to overflow
            // then it's being reinserted form within split
            // or mergePages or moveNode
            valueCopy.flags = value.flags;
            valueCopy.overflowPage = value.overflowPage;
        } else {
            valueCopy.data = value.data;
        }
    }

    quint16 lowerOffset = page->info.lowerOffset
            + sizeof(quint16);
    quint16 upperOffset = page->info.upperOffset
            + sizeof(NodeHeader)
            + key.data.size()
            + valueCopy.data.size();

    HBTREE_VERBOSE("offsets [ lower:" << page->info.lowerOffset << "->" << lowerOffset
                 << ", upper:" << page->info.upperOffset << "->" << upperOffset << "]");

    page->nodes.insert(key, valueCopy);
    page->info.lowerOffset = lowerOffset;
    page->info.upperOffset = upperOffset;
    return true;
}

bool HBtreePrivate::removeNode(HBtreePrivate::NodePage *page, const HBtreePrivate::NodeKey &key)
{
    Q_ASSERT(page->commited == false);
    HBTREE_DEBUG("removing" << key << "from" << page->info);

    if (!page->nodes.contains(key)) {
        HBTREE_DEBUG("nothing to remove for key" << key);
        return true;
    }

    NodeValue value = page->nodes.value(key);

    if (value.flags & NodeHeader::Overflow) {
        Q_ASSERT(page->info.type == PageInfo::Leaf);
        QList<quint32> overflowPages;
        if (!getOverflowPageNumbers(value.overflowPage, &overflowPages)) {
            HBTREE_ERROR("falsed to get overflow page numbers");
            return false;
        }
        QList<HistoryNode> historyNodes;
        foreach (quint32 pageNumber, overflowPages) {
            HistoryNode hn;
            hn.commitNumber = currentMarker().meta.revision + 1;
            hn.pageNumber = pageNumber;
            hn.syncNumber = lastSyncedRevision_ + 1;
            historyNodes.append(hn);
        }
        // There might not be enough space in this page. We can distribute the history to other
        // pages though...
        if (!distributeHistoryNodes(page, historyNodes)) {
            HBTREE_ERROR("failed to redistribute history pages");
            // Should this be an error?
            return false;
        }
    }

    quint16 lowerOffset = page->info.lowerOffset
            - sizeof(quint16);
    quint16 upperOffset = page->info.upperOffset
            - (sizeof(NodeHeader)
               + key.data.size()
               + value.data.size());

    HBTREE_VERBOSE("offsets [ lower:" << page->info.lowerOffset << "->" << lowerOffset
                 << ", upper:" << page->info.upperOffset << "->" << upperOffset << "]");

    page->nodes.remove(key);
    page->info.lowerOffset = lowerOffset;
    page->info.upperOffset = upperOffset;

    return true;
}

bool HBtreePrivate::split(HBtreePrivate::NodePage *page, const NodeKey &key, const NodeValue &value, NodePage **rightOut)
{
    HBTREE_DEBUG("splitting (implicit left) page" << page->info);
    NodePage *left = page;
    if (left->parent == NULL) {
        HBTREE_DEBUG("no parent. Creating parent for left");

        // Note: this empty key implies that comparison functions must know
        // that when an empty key comes in, it is an implicit loest value.
        // This can technically be handled transparently by the comparison
        // operators in struct NodeKey
        NodeKey nkey(compareFunction_, QByteArray(""));
        NodeValue nval;
        nval.overflowPage = left->info.number;
        left->parent = static_cast<NodePage *>(newPage(PageInfo::Branch));
        left->parentKey = nkey;
        HBTREE_DEBUG("set left parentKey" << nkey);
        HBTREE_DEBUG("set left parent ->" << left->parent->info);
        writeTransaction_->rootPage_ = left->parent->info.number;
        HBTREE_DEBUG("root changed to" << writeTransaction_->rootPage_);
        insertNode(left->parent, nkey, nval);
        HBTREE_DEBUG("inserted" << nkey << nval << "in" << left->parent->info);
        Q_Q(HBtree);
        q->stats_.depth++;
    }

    HBTREE_DEBUG("creating new right sibling");
    NodePage *right = static_cast<NodePage *>(newPage(PageInfo::Type(left->info.type)));

    HBTREE_DEBUG("making copy of left page and clearing left");
    NodePage copy = *left;
    left->nodes.clear();
    left->info.lowerOffset = 0;
    left->info.upperOffset = 0;
    // keep node history in left.

    insertNode(&copy, key, value); // no need to insert full value here??
    HBTREE_DEBUG("inserted key/value" << key << "in copy");

    int splitIndex = copy.nodes.size() / 2 + 1; // bias for left page
    HBTREE_DEBUG("splitIndex =" << splitIndex << "from" << copy.nodes.size() << "nodes in copy");
    Node splitIter = copy.nodes.constBegin() + splitIndex;
    NodeKey splitKey = splitIter.key();
    NodeValue splitValue(right->info.number);

    right->parent = left->parent;
    HBTREE_DEBUG("set right parent to left parent");

    // split branch if no space
    if (spaceNeededForNode(splitKey.data, splitValue.data) >= nodeSpaceLeft(right->parent)) {
        HBTREE_DEBUG("not enough space in right parent - splitting:");
        if (!split(right->parent, splitKey, splitValue)) {
            HBTREE_ERROR("failed to split parent" << *right->parent);
            return false;
        } else {
            if (right->parent != left->parent) {
                Q_ASSERT(0);
                // TODO: Original btree does something here...
                // WHAAAAAT ISSSSS IIIITTTTTTT?????
                // Never seem to hit this assert though...
            }
        }
    } else {
        right->parentKey = splitKey;
        HBTREE_DEBUG("set right parentKey" << splitKey);
        if (!insertNode(right->parent, splitKey, splitValue)) {
            HBTREE_ERROR("failed to split keys in right parent");
            return false;
        }
        HBTREE_DEBUG("inserted" << splitKey << splitValue << "in" << right->parent->info);
    }

    int index = 0;
    Node node = copy.nodes.constBegin();
    while (node != copy.nodes.constEnd()) {
        if (index++ < splitIndex) {

            // There's a corner case where a 3-way split becomes necessary, when the new node
            // is too big for the left page. If this is true then key should be <= than node.key
            // (since it may have been already inserted) and value should not be an overflow.
            if (spaceNeededForNode(node.key().data, node.value().data) > nodeSpaceLeft(left)) {
                Q_ASSERT(left->info.type != PageInfo::Branch); // This should never happen with a branch page
                Q_ASSERT(key <= node.key());
                Q_ASSERT(willCauseOverflow(key.data, value.data) == false);
                if (!split(left, node.key(), node.value(), &left)) {
                    HBTREE_ERROR("3-way split fail");
                    return false;
                }
                ++node;
                continue;
            }

            Q_ASSERT(spaceNeededForNode(node.key().data, node.value().data) <= nodeSpaceLeft(left));

            if (!insertNode(left, node.key(), node.value())) {
                HBTREE_ERROR("failed to insert key in to left");
                return false;
            }
            HBTREE_VERBOSE("inserted" << node.key() << "in left");
        }
        else {
            Q_ASSERT(spaceNeededForNode(node.key().data, node.value().data) <= nodeSpaceLeft(right));
            if (!insertNode(right, node.key(), node.value())) {
                HBTREE_ERROR("failed to insert key in to right");
                return false;
            }
            HBTREE_VERBOSE("inserted" << node.key() << "in right");
        }
        ++node;
    }

    HBTREE_DEBUG("left:" << *left);
    HBTREE_DEBUG("right:" << *right);

    HBTREE_DEBUG("left parent:" << *left->parent);
    HBTREE_DEBUG("right parent:" << *right->parent);

    // adjust right/left sibling pointers.
    Q_ASSERT(left->info.type == right->info.type);
    if (left->info.type == PageInfo::Leaf) {
        if (left->rightPageNumber != PageInfo::INVALID_PAGE)
            static_cast<NodePage *>(getPage(left->rightPageNumber))->leftPageNumber = right->info.number;
        left->rightPageNumber = right->info.number;

        if (right->leftPageNumber != PageInfo::INVALID_PAGE)
            static_cast<NodePage *>(getPage(right->leftPageNumber))->rightPageNumber = left->info.number;
        right->leftPageNumber = left->info.number;
    }


    Q_ASSERT(verifyIntegrity(right));
    if (rightOut)
        *rightOut = right;

    return true;
}

bool HBtreePrivate::rebalance(HBtreePrivate::NodePage *page)
{
    NodePage *parent = page->parent;

    if (pageFullEnough(page))
        return true;

    HBTREE_DEBUG("rebalancing" << *page);

    if (parent == NULL) { // root
        Q_ASSERT(writeTransaction_->rootPage_ != PageInfo::INVALID_PAGE);
        Q_ASSERT(page->info.number == writeTransaction_->rootPage_);
        Q_Q(HBtree);
        if (page->nodes.size() == 0) {
            // TODO: add lonely page to its own history.
            HBTREE_DEBUG("making root invalid, btree empty");
            writeTransaction_->rootPage_ = PageInfo::INVALID_PAGE;
        } else if (page->info.type == PageInfo::Branch && page->nodes.size() == 1) {
            // TODO: add branch page to history of leaf.
            // Will there be enough space?

            // Set parent to null since this baby might be in the cache
            // TODO: add a better way to do this.
            NodePage *root = static_cast<NodePage *>(getPage(page->nodes.constBegin().value().overflowPage));
            root->parent = NULL;
            writeTransaction_->rootPage_ = page->nodes.constBegin().value().overflowPage;
            HBTREE_DEBUG("One node in root branch" << page->info << "setting root to page" << writeTransaction_->rootPage_);
            q->stats_.depth--;
        } else {
            HBTREE_DEBUG("No need to rebalance root page");
        }
        return true;
    }

    HBTREE_DEBUG(*parent);
    Q_ASSERT(parent->nodes.size() >= 2);
    Q_ASSERT(parent->info.type == PageInfo::Branch);

    NodePage *neighbour = 0;
    Node pageBranchNode = page->parent->nodes.find(page->parentKey);
    Node neighbourBranchNode;
    Node sourceNode;
    Node destNode;
    if (pageBranchNode == page->parent->nodes.constBegin()) { // take right neightbour
        HBTREE_DEBUG("taking right neighbour");
        neighbourBranchNode = pageBranchNode + 1;
        neighbour = static_cast<NodePage *>(getPage(neighbourBranchNode.value().overflowPage));
        sourceNode = neighbour->nodes.constBegin();
        destNode = page->nodes.constEnd();
    } else { // take left neighbour
        HBTREE_DEBUG("taking left neighbour");
        neighbourBranchNode = pageBranchNode - 1;
        neighbour = static_cast<NodePage *>(getPage(neighbourBranchNode.value().overflowPage));
        sourceNode = neighbour->nodes.constEnd() - 1;
        destNode = page->nodes.constBegin();
    }

    HBTREE_DEBUG("will use:" << sourceNode << "from" << neighbourBranchNode);

    neighbour->parent = page->parent;
    neighbour->parentKey = neighbourBranchNode.key();
    //HBTREE_DEBUG("neighbour:" << neighbour->info);

    if (pageFullEnough(neighbour) && neighbour->nodes.size() > 2
            && hasSpaceFor(page, sourceNode.key(), sourceNode.value())) {

        bool canUpdate = true;
        if (neighbourBranchNode != neighbour->parent->nodes.constBegin()
                && sourceNode == neighbour->nodes.constBegin()) {
            // key in neighbour's parent can change
            // make sure sourceNode.key can fit in neighbour parent
            if (sourceNode.key().data.size() >
                    neighbour->parentKey.data.size()) {
                quint16 diff = sourceNode.key().data.size() - neighbour->parentKey.data.size();
                if (diff > nodeSpaceLeft(neighbour->parent))
                    canUpdate = false;
            }
        }

        if (canUpdate
                && destNode == page->nodes.constBegin()
                && pageBranchNode != page->parent->nodes.constBegin()) {
            // key in page parent can change
            // make sure sourceNode.key can fit in page parent
            if (sourceNode.key().data.size() >
                    page->parentKey.data.size()) {
                quint16 diff = sourceNode.key().data.size() - page->parentKey.data.size();
                if (diff > nodeSpaceLeft(page->parent))
                    canUpdate = false;
            }
        }

        if (canUpdate && !moveNode(neighbour, page, sourceNode)) {
            return false;
        }
    } else {
        // Account for transfer of history
        // Account for extra history node in dst page in the case of touch page.
        if (nodeSpaceLeft(page) >= (nodeSpaceUsed(neighbour) + sizeof(HistoryNode) * (neighbour->history.size() + 1))) {
            if (!mergePages(neighbour, page))
                return false;
        } else if (nodeSpaceLeft(neighbour) >= (nodeSpaceUsed(page) + sizeof(HistoryNode) * (page->history.size() + 1))) {
            if (!mergePages(page, neighbour))
                return false;
        }
    }

    return true;
}

// TODO: turn in to transferNode(src, dst) // only with same parents.
bool HBtreePrivate::moveNode(HBtreePrivate::NodePage *src, HBtreePrivate::NodePage *dst, HBtreePrivate::Node node)
{
    Q_ASSERT(src->parent);
    Q_ASSERT(dst->parent);
    Q_ASSERT(dst->parent == src->parent);
    Q_ASSERT(src->info.type == dst->info.type);
    Q_ASSERT(src->parentKey <= node.key());

    HBTREE_DEBUG("moving" << node << "from" << src->info << "to" << dst->info);
    HBTREE_VERBOSE("src parent ->" << src->parent->nodes);
    HBTREE_VERBOSE("dst parent ->" << dst->parent->nodes);

    src = touchNodePage(src);
    dst = touchNodePage(dst);

    if (!src || !dst)
        return false;

    bool decending = src->parentKey > dst->parentKey;

    NodeKey nkey = node.key();

    insertNode(dst, node.key(), node.value());
    removeNode(src, node.key());

    if (dst->parentKey > nkey) {
        // must change destination parent key
        NodePage *lowest;
        searchPageRoot(NULL, dst, NodeKey(compareFunction_), SearchFirst, false, &lowest);
        removeNode(dst->parent, dst->parentKey);
        dst->parentKey = lowest->nodes.constBegin().key();
        insertNode(dst->parent, dst->parentKey, NodeValue(dst->info.number));
    }

    if (src->parentKey <= nkey && decending) {
        Q_ASSERT(!src->parentKey.data.isEmpty());
        // must change source parent key
        NodePage *lowest;
        searchPageRoot(NULL, src, NodeKey(compareFunction_), SearchFirst, false, &lowest);
        removeNode(src->parent, src->parentKey);
        src->parentKey = lowest->nodes.constBegin().key();
        insertNode(src->parent, src->parentKey, NodeValue(src->info.number));
    }
//    // If we move
//    if (rightToLeft) {
//        // Source parent key can change.
//        // Find lowest key from source and set as source parent key
//        HBTREE_DEBUG("took first node from source, must adjust source parent key");
//        Node branchNode = src->parent->nodes.find(src->parentKey);
//        if (branchNode.key() < src->nodes.constBegin().key()) {
//            // Should set branchNode to lowest key reachable from src
//            NodePage *lowest = src;
//            searchPageRoot(NULL, src, NodeKey(), SearchFirst, false, &lowest);
//            HBTREE_DEBUG("setting" << branchNode << "to" << lowest->nodes.constBegin().key());
//            removeNode(src->parent, src->parentKey);
//            insertNode(src->parent, lowest->nodes.constBegin().key(), branchNode.value());
//            src->parentKey = lowest->nodes.constBegin().key();
//        }
//    }

//    // If we move
//    if (leftToRight) {
//        // Destination parent key can chance.
//        // FInd lowest key from destination and set as source parent key
//        HBTREE_DEBUG("took first node of source, must adjust dest parent key");
//        Node branchNode = dst->parent->nodes.find(dst->parentKey);
//        if (branchNode.key() > dst->nodes.constBegin().key()) {
//            // Should set branchNode to lowest key reachable from dst
//            NodePage *lowest = dst;
//            searchPageRoot(NULL, dst, NodeKey(), SearchFirst, false, &lowest);
//            HBTREE_DEBUG("setting" << branchNode << "to" << lowest->nodes.constBegin().key());
//            dst->parent->nodes.remove(dst->parentKey);
//            dst->parent->nodes.insert(lowest->nodes.constBegin().key(), branchNode.value());
//            removeNode(dst, dst->parentKey);
//            insertNode(dst, lowest->nodes.constBegin().key(), branchNode.value());
//            dst->parentKey = lowest->nodes.constBegin().key();
//        }
//    }

    HBTREE_DEBUG("src" << *src);
    HBTREE_DEBUG("dst" << *dst);
    HBTREE_DEBUG("parent" << *src->parent);

    return true;
}

bool HBtreePrivate::mergePages(HBtreePrivate::NodePage *page, HBtreePrivate::NodePage *dst)
{
    Q_ASSERT(dst->parent);
    Q_ASSERT(page->parent);
    Q_ASSERT(page->parent == dst->parent);

    HBTREE_DEBUG("merging" << page->info << "in to" << dst->info);
    HBTREE_DEBUG(*page);
    HBTREE_DEBUG(*dst);
    HBTREE_DEBUG(*dst->parent);

    // No need to touch page since only dst is changing.
    dst = touchNodePage(dst);

    if (!dst || !page)
        return false;

    Node it = page->nodes.constBegin();
    while (it != page->nodes.constEnd()) {
        insertNode(dst, it.key(), it.value());
        ++it;
    }

    foreach (const HistoryNode &hn, page->history) {
        dst->history.append(hn);
        dst->meta.historySize++;
    }

    HistoryNode hn;
    hn.pageNumber = page->info.number;
    hn.commitNumber = page->meta.commitId;
    hn.syncNumber = page->meta.syncId;
    dst->history.append(hn);
    dst->meta.historySize++;

    if (page->parentKey > dst->parentKey) {
        // Merging a page with larger key values in to a page
        // with smaller key values. Just delete the key to
        // greater page form parent
        removeNode(dst->parent, page->parentKey);
    } else {
        // Merging page with smaller keys in to page
        // with bigger keys. Change dst parent key.
        removeNode(dst->parent, dst->parentKey);
        removeNode(page->parent, page->parentKey);
        NodeKey nkey = page->parentKey;
        NodeValue nval;
        nval.overflowPage = dst->info.number;
        insertNode(dst->parent, nkey, nval);
        dst->parentKey = page->parentKey;
    }

    // Overflow pages may have been transfered over
    if (page->meta.flags & NodeHeader::Overflow)
        dst->meta.flags |= NodeHeader::Overflow;

    return rebalance(dst->parent);
}

bool HBtreePrivate::distributeHistoryNodes(HBtreePrivate::NodePage *primary, const QList<HBtreePrivate::HistoryNode> &historyNodes) const
{
    Q_ASSERT(!primary->commited);
    HBTREE_DEBUG("ditributing history nodes" << historyNodes);

    // check primary.
    // check other dirty pages...

    int requiredBytes = historyNodes.size() * sizeof(HistoryNode);
    if (nodeSpaceLeft(primary) >= requiredBytes) { // all good, add them all to primary candidate
        HBTREE_DEBUG("adding to primary candidate" << primary->info);
        foreach (const HistoryNode &hn, historyNodes) {
            primary->history.append(hn);
            primary->meta.historySize++;
        }
        return true;
    }

    HBTREE_DEBUG("could not fit in primary canddiate. Checking dirty pages");
    // Brute force the dirty page queue
    foreach (Page *page, dirtyPages_.values()) {
        if (page != primary
                && (page->info.type != PageInfo::Leaf || page->info.type != PageInfo::Branch)
                && nodeSpaceLeft(static_cast<NodePage *>(page)) >= requiredBytes) {
            NodePage *np = static_cast<NodePage *>(page);
            HBTREE_DEBUG("found dirty node page" << np->info);
            foreach (const HistoryNode &hn, historyNodes) {
                np->history.append(hn);
                np->meta.historySize++;
            }
            return true;
        }
    }

    // Pray we never hit this...
    HBTREE_ERROR("Lost history pages" << historyNodes);
    Q_ASSERT(0);

    // Alternatively, build a list of candidates and check if it can be distributed around
//    QList<NodePage *> candidates;
    // Or... add them to a marker page?

    // uh oh... now what? do we need some sort of compaction??
    // or can we just lose out?
    return false;
}

bool HBtreePrivate::distributeHistoryNodes(HBtreePrivate::NodePage *primary, const HBtreePrivate::HistoryNode &historyNode) const
{
    QList<HistoryNode> nodes;
    nodes.append(historyNode);
    return distributeHistoryNodes(primary, nodes);
}

void HBtreePrivate::dump()
{
    qDebug() << "dumping tree from marker" << currentMarker();
    NodePage *root = static_cast<NodePage *>(getPage(currentMarker().meta.rootPage));
    dumpPage(root, 0);
}

void HBtreePrivate::dumpPage(HBtreePrivate::NodePage *page, int depth)
{
    QByteArray tabs(depth, '\t');
    qDebug() << tabs << page->info;
    qDebug() << tabs << page->meta;
    qDebug() << tabs << page->history;
    qDebug() << tabs << "right =>" << (page->rightPageNumber == PageInfo::INVALID_PAGE ? "Unavailable" : QString::number(page->rightPageNumber).toAscii());
    qDebug() << tabs << "left =>" << (page->leftPageNumber == PageInfo::INVALID_PAGE ? "Unavailable" : QString::number(page->leftPageNumber).toAscii());
    switch (page->info.type) {
    case PageInfo::Branch:
        qDebug() << tabs << page->nodes;
        for (Node it = page->nodes.constBegin(); it != page->nodes.constEnd(); ++it) {
            const NodeValue &value = it.value();
            dumpPage(static_cast<NodePage *>(getPage(value.overflowPage)), depth + 1);
        }
        break;
    case PageInfo::Leaf:
        qDebug() << tabs << page->nodes;
        break;
    default:
        Q_ASSERT(0);
    }
}

// ######################################################################
// ### btree cursors
// ######################################################################

bool HBtreePrivate::cursorLast(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut)
{
    NodePage *page = 0;
    searchPage(cursor, cursor->transaction_, NodeKey(), SearchLast, false, &page);
    if (page && page->nodes.size()) {
        Node node = page->nodes.constEnd() - 1;
        if (keyOut)
            *keyOut = node.key().data;
        if (valueOut)
            *valueOut = getDataFromNode(node.value());
        cursor->eof_ = true;
        return true;
    }
    return false;

}

bool HBtreePrivate::cursorFirst(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut)
{
    NodePage *page = 0;
    searchPage(cursor, cursor->transaction_, NodeKey(), SearchFirst, false, &page);
    if (page && page->nodes.size()) {
        Node node = page->nodes.constBegin();
        if (keyOut)
            *keyOut = node.key().data;
        if (valueOut)
            *valueOut = getDataFromNode(node.value());
        cursor->eof_ = false;
        return true;
    }
    return false;
}

bool HBtreePrivate::cursorNext(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut)
{
    Q_ASSERT(cursor->initialized_);
    if (cursor->eof_)
        return false;
    NodeKey nkey(compareFunction_, cursor->key_);
    NodePage *page = 0;
    searchPage(cursor, cursor->transaction_, nkey, SearchKey, false, &page);
    Node node = page->nodes.find(nkey);
    // Could've been deleted so check if node == end.
    if (node == page->nodes.constEnd() || ++node == page->nodes.constEnd()) {
        if (page->rightPageNumber != PageInfo::INVALID_PAGE) {
            NodePage *right = static_cast<NodePage *>(getPage(page->rightPageNumber));
            node = right->nodes.constBegin();
            if (node != right->nodes.constEnd()) { // can a right page be empty and point to another right?
                if (keyOut)
                    *keyOut = node.key().data;
                if (valueOut)
                    *valueOut = getDataFromNode(node.value());
                return true;
            } else {
                // This should never happen if rebalancing is working properly
                Q_ASSERT(0);
            }
        }
    } else {
        if (keyOut)
            *keyOut = node.key().data;
        if (valueOut)
            *valueOut = getDataFromNode(node.value());
        return true;
    }
    cursor->eof_ = true;
    return false;
}

bool HBtreePrivate::cursorPrev(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut)
{
    Q_ASSERT(cursor->initialized_);
    NodeKey nkey(compareFunction_, cursor->key_);
    NodePage *page = 0;
    searchPage(cursor, cursor->transaction_, nkey, SearchKey, false, &page);
    Node node = page->nodes.find(nkey);
    if (node == page->nodes.constBegin()) {
        if (page->leftPageNumber != PageInfo::INVALID_PAGE) {
            NodePage *left = static_cast<NodePage *>(getPage(page->leftPageNumber));
            if (left->nodes.size() > 1)
                node = left->nodes.constEnd() - 1;
            else
                node = left->nodes.constBegin();
            if (node != left->nodes.constEnd()) { // can a left page be empty and point to another left??
                if (keyOut)
                    *keyOut = node.key().data;
                if (valueOut)
                    *valueOut = getDataFromNode(node.value());
                return true;
            } else {
                // This should never happen if rebalancing is working properly
                Q_ASSERT(0);
            }
        }
    } else {
        --node;
        if (keyOut)
            *keyOut = node.key().data;
        if (valueOut)
            *valueOut = getDataFromNode(node.value());
        return true;
    }
    cursor->eof_ = true;
    return false;
}

bool HBtreePrivate::cursorSet(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut, const QByteArray &matchKey, bool exact)
{
    HBTREE_DEBUG("searching for" << (exact? "exactly" : "") << matchKey);
    QByteArray keyData, valueData;
    NodeKey nkey(compareFunction_, matchKey);
    NodePage *page = 0;
    if (!searchPage(cursor, cursor->transaction_, nkey, SearchKey, false, &page))
        return false;

    bool ok = false;

    if (page->nodes.contains(nkey)) {
        keyData = nkey.data;
        valueData = getDataFromNode(page->nodes.value(nkey));
        ok = true;
    } else if (!exact) {
        Node node = page->nodes.lowerBound(nkey);
        if (node != page->nodes.constEnd()) { // if found key equal or greater than, return
            keyData = node.key().data;
            valueData = getDataFromNode(node.value());
            ok = true;
        } else { // check sibling
            HBTREE_DEBUG("Checking sibling of" << page->meta);
            if (page->rightPageNumber != PageInfo::INVALID_PAGE) {
                NodePage *right = static_cast<NodePage *>(getPage(page->rightPageNumber));
                node = right->nodes.lowerBound(nkey);
                if (node != right->nodes.constEnd()) {
                    keyData = node.key().data;
                    valueData = getDataFromNode(node.value());
                    ok = true;
                }
            }
        }
    }

    if (ok) {
        if (keyOut)
            *keyOut = keyData;
        if (valueOut)
            *valueOut = valueData;
    }

    return ok;
}

void HBtreePrivate::copy(const HBtreePrivate::MarkerPage &src, HBtreePrivate::MarkerPage *dst)
{
    quint32 pgno = dst->info.number;
    *dst = src;
    dst->info.number = pgno;
}

QByteArray HBtreePrivate::qInitializedByteArray() const
{
    Q_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE);
    return QByteArray(spec_.pageSize, (char)0);
}

QByteArray HBtreePrivate::qUninitializedByteArray() const
{
    return QByteArray(spec_.pageSize, Qt::Uninitialized);
}

#define CHECK_TRUE(x) do {if (!(x)) {qDebug() << "INTEGRITY: condition false:" << #x; return false;}} while (0)
bool HBtreePrivate::verifyIntegrity(const HBtreePrivate::Page *page) const
{
    CHECK_TRUE(page);
    HBTREE_DEBUG("verifying" << page->info);
    CHECK_TRUE(page->info.isTypeValid());
    CHECK_TRUE(page->info.isValid());
    CHECK_TRUE(capacity(page) >= (page->info.upperOffset + page->info.lowerOffset));
    if (page->info.type == PageInfo::Marker) {
        const MarkerPage *mp = static_cast<const MarkerPage *>(page);
        CHECK_TRUE(mp->info.number == currentMarker().info.number); // only verify current marker
        CHECK_TRUE(mp->info.lowerOffset == mp->collectiblePages.size());
        CHECK_TRUE(mp->info.upperOffset == 0);
        CHECK_TRUE(mp->meta.size <= size_);
        CHECK_TRUE(mp->meta.syncedRevision == lastSyncedRevision_ || mp->meta.syncedRevision == (lastSyncedRevision_ + 1));
        //if (mp->meta.syncedRevision == lastSyncedRevision_) // we just synced
        //    ;
        //else // we've had a number of revisions since last sync
        //    ;
        CHECK_TRUE(mp->meta.revision >= lastSyncedRevision_);
        if (mp->meta.rootPage != PageInfo::INVALID_PAGE)
            CHECK_TRUE(mp->meta.rootPage <= (size_ / spec_.pageSize));
    } else if (page->info.type == PageInfo::Leaf || page->info.type == PageInfo::Branch) {
        const NodePage *np = static_cast<const NodePage *>(page);
        CHECK_TRUE(np->history.size() == np->meta.historySize);
        CHECK_TRUE((np->info.lowerOffset / 2) == np->nodes.size());
        if (np->commited) {
            CHECK_TRUE(np->meta.commitId <= currentMarker().meta.revision);
            CHECK_TRUE(np->meta.syncId <= currentMarker().meta.syncedRevision);
        } else {
            CHECK_TRUE(np->meta.commitId > currentMarker().meta.revision);
            CHECK_TRUE(np->meta.syncId == (lastSyncedRevision_ + 1));
        }
        CHECK_TRUE(nodeSpaceLeft(np) <= capacity(np));
        CHECK_TRUE(nodeSpaceUsed(np) <= capacity(np));
        CHECK_TRUE((nodeSpaceUsed(np) + nodeSpaceLeft(np)) == capacity(np));

        Node it = np->nodes.constBegin();

        if (np->parent) {
//            Node node = np->parent->nodes.find(np->parentKey);
//            CHECK_TRUE(node != np->parent->nodes.constEnd());
//            CHECK_TRUE(np->nodes.constBegin().key() >= node.key());
        }
        while (it != np->nodes.constEnd()) {
            CHECK_TRUE(it.key().compareFunction == compareFunction_);
            if (np->parent)
                CHECK_TRUE(it.key() >= np->parentKey);
            if (page->info.type == PageInfo::Leaf) {
                CHECK_TRUE(it.key().data.size() > 0);
                if (it.value().flags & NodeHeader::Overflow) {
                    CHECK_TRUE(it.value().data.size() == 0);
                    CHECK_TRUE(it.value().overflowPage != PageInfo::INVALID_PAGE);
                } else {
                    CHECK_TRUE(it.value().overflowPage == PageInfo::INVALID_PAGE);
                }
            } else {
                // branch page key data size may be 0 (empty key)
                CHECK_TRUE(!(it.value().flags & NodeHeader::Overflow));
                CHECK_TRUE(it.value().overflowPage != PageInfo::INVALID_PAGE);
                CHECK_TRUE(it.value().data.size() == 0);
            }
            ++it;
        }
    } else if (page->info.type == PageInfo::Overflow) {
        const OverflowPage *op = static_cast<const OverflowPage *>(page);
        if (op->nextPage != PageInfo::INVALID_PAGE)
            CHECK_TRUE(op->data.size() == capacity(page));
        else
            CHECK_TRUE(op->data.size() <= capacity(page));
    } else
        CHECK_TRUE(false);
    return true;
}

// ######################################################################
// ### Public implementation
// ######################################################################

HBtree::HBtree()
    : d_ptr(new HBtreePrivate(this)), autoSyncRate_(0)
{
}


HBtree::HBtree(const QString &fileName)
    : d_ptr(new HBtreePrivate(this, fileName)), autoSyncRate_(0)
{
}

HBtree::~HBtree()
{
    close();
}

void HBtree::setFileName(const QString &fileName)
{
    Q_D(HBtree);
    d->fileName_ = fileName;
}

void HBtree::setOpenMode(OpenMode mode)
{
    Q_D(HBtree);
    d->openMode_ = mode;
}

void HBtree::setCompareFunction(HBtree::CompareFunction compareFunction)
{
    Q_D(HBtree);
    d->compareFunction_ = compareFunction;
}

void HBtree::setCacheSize(int size)
{
    Q_UNUSED(size);
    // TODO...
}

HBtree::OpenMode HBtree::openMode() const
{
    Q_D(const HBtree);
    return d->openMode_;
}

QString HBtree::fileName() const
{
    Q_D(const HBtree);
    return d->fileName_;
}

bool HBtree::open()
{
    close();
    Q_D(HBtree);

    if (d->fileName_.isEmpty())
        return false;

    int oflags = d->openMode_ == ReadOnly ? O_RDONLY : O_RDWR | O_CREAT;
    int fd = ::open(d->fileName_.toAscii(), oflags, 0644);

    if (fd == -1)
        return false;
    return d->open(fd);
}

void HBtree::close()
{
    Q_D(HBtree);
    d->close();
}

size_t HBtree::size() const
{
    Q_D(const HBtree);
    return d->size_;
}

quint16 HBtree::pageSize() const
{
    Q_D(const HBtree);
    Q_ASSERT(d->fd_ != -1);
    return d->spec_.pageSize;
}

bool HBtree::sync()
{
    Q_D(HBtree);
    return d->sync(d->currentMarker());
}

bool HBtree::rollback()
{
    Q_D(HBtree);
    return d->rollback();
}

HBtreeTransaction *HBtree::beginTransaction(HBtreeTransaction::Type type)
{
    Q_D(HBtree);
    return d->beginTransaction(type);
}

quint32 HBtree::tag() const
{
    const Q_D(HBtree);
    return (quint32)d->currentMarker().meta.tag;
}

bool HBtree::isWriting() const
{
    const Q_D(HBtree);
    return d->writeTransaction_ != NULL;
}

HBtreeTransaction *HBtree::writeTransaction() const
{
    const Q_D(HBtree);
    return d->writeTransaction_;
}

QString HBtree::errorMessage() const
{
    return QString("huzzah wazzah!");
}

bool HBtree::commit(HBtreeTransaction *transaction, quint64 tag)
{
    Q_D(HBtree);
    bool ok = d->commit(transaction, tag);
    if (ok && autoSyncRate_ && (stats_.numCommits % autoSyncRate_) == 0)
        ok = sync();
    return ok;
}

void HBtree::abort(HBtreeTransaction *transaction)
{
    Q_D(HBtree);
    d->abort(transaction);
}

bool HBtree::put(HBtreeTransaction *transaction, const QByteArray &key, const QByteArray &value)
{
    Q_D(HBtree);
    return d->put(transaction, key, value);
}

QByteArray HBtree::get(HBtreeTransaction *transaction, const QByteArray &key)
{
    Q_D(HBtree);
    return d->get(transaction, key);
}

bool HBtree::del(HBtreeTransaction *transaction, const QByteArray &key)
{
    Q_D(HBtree);
    return d->del(transaction, key);
}

bool HBtree::doCursorOp(HBtreeCursor *cursor, HBtreeCursor::Op op, const QByteArray &key)
{
    Q_D(HBtree);
    bool ok = false;
    QByteArray keyOut, valueOut;
    switch (op) {
    case HBtreeCursor::ExactMatch:
    case HBtreeCursor::FuzzyMatch:
        cursor->initialized_ = false;
        if (op == HBtreeCursor::ExactMatch)
            ok = d->cursorSet(cursor, &keyOut, &valueOut, key, true);
        else
            ok = d->cursorSet(cursor, &keyOut, &valueOut, key, false);
        break;
    case HBtreeCursor::Next:
        if (!cursor->initialized_)
            ok = d->cursorFirst(cursor, &keyOut, &valueOut);
        else
            ok = d->cursorNext(cursor, &keyOut, &valueOut);
        break;
    case HBtreeCursor::Previous:
        if (!cursor->initialized_)
            ok = d->cursorLast(cursor, &keyOut, &valueOut);
        else
            ok = d->cursorPrev(cursor, &keyOut, &valueOut);
        break;
    case HBtreeCursor::First:
        cursor->initialized_ = false;
        ok = d->cursorFirst(cursor, &keyOut, &valueOut);
        break;
    case HBtreeCursor::Last:
        cursor->initialized_ = false;
        ok = d->cursorLast(cursor, &keyOut, &valueOut);
        break;
    default:
        HBTREE_ERROR("cursor op unknown");
        ok = false;
    }

    cursor->valid_ = ok;

    if (!ok) {
        cursor->key_ = QByteArray();
        cursor->value_ = QByteArray();
        cursor->initialized_ = false;
    } else {
        cursor->key_ = keyOut;
        cursor->value_ = valueOut;
    }

    return ok;
}

// ######################################################################
// ### Output streams
// ######################################################################

QDebug operator << (QDebug dbg, const HBtreePrivate::PageInfo &pi)
{
    QString pageStr;
    switch (pi.type) {
    case HBtreePrivate::PageInfo::Branch:
        pageStr = QLatin1Literal("Branch");
        break;
    case HBtreePrivate::PageInfo::Marker:
        pageStr = QLatin1Literal("Marker");
        break;
    case HBtreePrivate::PageInfo::Leaf:
        pageStr = QLatin1Literal("Leaf");
        break;
    case HBtreePrivate::PageInfo::Spec:
        pageStr = QLatin1Literal("Spec");
        break;
    case HBtreePrivate::PageInfo::Overflow:
        pageStr = QLatin1Literal("Overflow");
        break;
    default:
        pageStr = QLatin1Literal("Unknown");
        break;
    }

    dbg.nospace() << pageStr << " # " << pi.number << " ["
                  << "checksum:" << pi.checksum
                  << ","
                  << "lower:" << pi.lowerOffset
                  << ","
                  << "upper:" << pi.upperOffset
                  << "]";
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::MarkerPage &p)
{
    dbg.nospace() << p.info;
    dbg.nospace() << "\n\tmeta => ["
                  << "root:" << (p.meta.rootPage == HBtreePrivate::PageInfo::INVALID_PAGE
                                 ? "Invalid" : QString::number(p.meta.rootPage))
                  << ", "
                  << "commitId:" << p.meta.revision
                  << ", "
                  << "syncId:" << p.meta.syncedRevision
                  << ", "
                  << "tag:" << p.meta.tag
                  << ", "
                  << "size:" << p.meta.size
                  << "]";

    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodeKey &k)
{
    dbg.nospace() << k.data;
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodeValue &value)
{
    if (value.flags & HBtreePrivate::NodeHeader::Overflow)
        dbg.nospace() << "Overflow -> " << value.overflowPage;
    else if (value.overflowPage != HBtreePrivate::PageInfo::INVALID_PAGE)
        dbg.nospace() << "Child page -> " << value.overflowPage;
    else
        dbg.nospace() << value.data;
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodePage::Meta &m)
{
     dbg.nospace() << "["
                   << "commitNumber:" << m.commitId
                   << ", "
                   << "syncNumber:" << m.syncId
                   << ", "
                   << "historySize:" << m.historySize
                   << ", "
                   << "flags:" << m.flags
                   << "]";
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodePage &p)
{
    dbg.nospace() << p.info;
    if (p.parent)
        dbg.nospace() << " parent => " << p.parent->info  << ":" << p.parentKey;
    dbg.nospace() << " meta => " << p.meta;
#if HBTREE_VERBOSE_OUTPUT
    dbg.nospace() << " history => " << p.history;
#endif
#if HBTREE_VERBOSE_OUTPUT
    dbg.nospace() << " nodes => " << p.nodes;
#else
    if (p.info.type == HBtreePrivate::PageInfo::Branch)
        dbg.nospace() << " nodes => " << p.nodes;
    else
        dbg.nospace() << " nodes => " << p.nodes.keys();
#endif
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::OverflowPage &p)
{
    dbg.nospace() << p.info;
    dbg.nospace() << "\n\tnextPage => " << (p.nextPage == HBtreePrivate::PageInfo::INVALID_PAGE ? "None" : QString::number(p.nextPage).toAscii());
    dbg.nospace() << "\n\tdata size => " << p.data.size();
#if HBTREE_VERBOSE_OUTPUT
    dbg.nospace() << "\n\tdata => " << p.data;
#endif
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodeHeader &n)
{
    dbg.nospace() << "(keySize -> " << n.keySize;
    if (n.flags & HBtreePrivate::NodeHeader::Overflow)
        dbg.nospace() << ", overflow -> " << n.context.overflowPage;
    else
        dbg.nospace() << ", dataSize -> " << n.context.valueSize;
    dbg.nospace() << ")";
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::HistoryNode &hn)
{
    dbg.nospace() << "(Page #" << hn.pageNumber << ", Commit #" << hn.commitNumber << ", Sync #" << hn.syncNumber << ")";
    return dbg.space();
}


quint16 HBtreePrivate::PageInfo::headerSize() const
{
    switch (type) {
    case HBtreePrivate::PageInfo::Marker:
        return sizeof(HBtreePrivate::PageInfo)
                + sizeof(HBtreePrivate::MarkerPage::Meta);
    case HBtreePrivate::PageInfo::Leaf:
    case HBtreePrivate::PageInfo::Branch:
        return sizeof(HBtreePrivate::PageInfo)
                + sizeof(HBtreePrivate::NodePage::Meta);
    case HBtreePrivate::PageInfo::Overflow:
        return sizeof(HBtreePrivate::PageInfo) + sizeof(HBtreePrivate::NodeHeader);
    default:
        Q_ASSERT(0);
    }
    return 0;
}

HBtreePrivate::HistoryNode::HistoryNode(HBtreePrivate::NodePage *np)
    : pageNumber(np->info.number), commitNumber(np->meta.commitId), syncNumber(np->meta.syncId)
{
}
