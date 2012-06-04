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
#include <errno.h>

#include <QDebug>

#include "hbtree.h"
#include "hbtree_p.h"

#include "crc32.h"

#define HBTREE_DEBUG_OUTPUT 0
#define HBTREE_VERBOSE_OUTPUT 0

#define HBTREE_VERSION 0xdeadc0de
#define HBTREE_DEFAULT_PAGE_SIZE 4096

// How many previous commits to leave intact (not including the synced commit)
#define HBTREE_COMMIT_CHAIN 1

#include "hbtreeassert_p.h"

#if HBTREE_VERBOSE_OUTPUT && !HBTREE_DEBUG_OUTPUT
#   undef HBTREE_VERBOSE_OUTPUT
#   define HBTREE_VERBOSE_OUTPUT 0
#endif

#define HBTREE_DEBUG(qDebugStatement) if (HBTREE_DEBUG_OUTPUT) (qDebug().nospace() << "[HBtree:" << fileName_ << "] " << __FUNCTION__ << " =>").space() << qDebugStatement
#define HBTREE_VERBOSE(qDebugStatement) if (HBTREE_VERBOSE_OUTPUT) HBTREE_DEBUG(qDebugStatement)
#define HBTREE_ERROR(qDebugStatement) (qCritical().nospace() << "ERROR! HBtree(" << fileName_ << ") " << __FUNCTION__ << " =>").space() << qDebugStatement
#define HBTREE_ERROR_LAST(msg) do {lastErrorMessage_ = QLatin1String(msg); (qCritical().nospace() << "ERROR! HBtree(" << fileName_ << ") " << __FUNCTION__ << " =>").space() << msg;} while (0)


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
    : q_ptr(q), fileName_(name), fd_(-1), openMode_(HBtree::ReadOnly), size_(0), lastSyncedId_(0), cacheSize_(20),
      compareFunction_(0),
      writeTransaction_(0), readTransaction_(0), lastPage_(PageInfo::INVALID_PAGE), cursorDisrupted_(false),
#ifdef QT_TESTLIB_LIB
      forceCommitFail_(0),
#endif
      lastWriteError_(0), lastReadError_(0)
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

    if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        lastErrorMessage_ = QLatin1String("failed to take a lock - ") + QLatin1String(strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    pageBuffer_.resize(HBTREE_DEFAULT_PAGE_SIZE);

    // Read spec page
    int rc = pread(fd_, (void *)pageBuffer_.data(), HBTREE_DEFAULT_PAGE_SIZE, 0);
    q->stats_.reads++;

    if (rc != HBTREE_DEFAULT_PAGE_SIZE) {
        // Write spec
        if (rc == 0) {

            HBTREE_DEBUG("New file:" << "[" << "fd:" << fd_ << "]");

            if (!writeSpec()) {
                HBTREE_ERROR_LAST("failed to write spec");
                return false;
            }

            const quint32 initSize = spec_.pageSize * 3;

            // Write sync markers
            MarkerPage synced0(1);
            synced0.meta.size = initSize;

            QByteArray buffer(spec_.pageSize, (char)0);
            memcpy(buffer.data(), &synced0.info, sizeof(PageInfo));
            memcpy(buffer.data() + sizeof(PageInfo), &synced0.meta, sizeof(MarkerPage::Meta));

            if (!writePage(&buffer)) {
                HBTREE_ERROR_LAST("failed to write sync marker 0");
                return false;
            }

            serializePageNumber(2, &buffer);
            if (!writePage(&buffer)) {
                HBTREE_ERROR_LAST("failed to write sync marker 1");
                return false;
            }

            marker_ = synced0;
            synced_ = synced0;

            lastSyncedId_ = 0;
            size_ = initSize;
        } else {
            lastReadError_ = errno;
            lastErrorMessage_ = QLatin1String("failed to read spec page - ") + QLatin1String(strerror(errno));
            HBTREE_ERROR("failed to read spec page - rc:" << rc);
            return false;
        }
    } else {
        HBTREE_DEBUG("Opening existing file:" << "[" << "fd:" << fd_ << "]");

        if (!readSpec(pageBuffer_)) {
            HBTREE_ERROR_LAST("failed to read spec information");
            return false;
        }

        // Get synced marker
        QList<quint32> overflowPages;
        if (!readSyncedMarker(&marker_, &overflowPages)) {
            HBTREE_ERROR_LAST("sync markers invalid.");
            return false;
        }

        synced_ = marker_;

        if (openMode_ == HBtree::ReadWrite) {
            off_t currentSize = lseek(fd_, 0, SEEK_END);
            if (static_cast<off_t>(marker_.meta.size) < currentSize) {
                if (ftruncate(fd_, marker_.meta.size) != 0) {
                    lastErrorMessage_ = QLatin1String("failed to truncate file - ") + QLatin1String(strerror(errno));
                    HBTREE_ERROR("failed to truncate from" << currentSize << "for" << marker_);
                    return false;
                }
            }

            size_ = marker_.meta.size;
            lastSyncedId_ = marker_.meta.syncId;
            collectiblePages_.unite(marker_.residueHistory);
            marker_.residueHistory.clear();
            marker_.info.upperOffset = 0;

            // This has to be done after syncing as well
            foreach (quint32 pgno, overflowPages)
                collectiblePages_.remove(pgno);

        } else {
            size_ = marker_.meta.size;
        }
    }

    lastPage_ = size_ / spec_.pageSize;
    HBTREE_ASSERT(verifyIntegrity(&marker_))(marker_);

    HBTREE_DEBUG("opened btree with"
                 << "[spec:" << spec_
                 << ", marker:" << marker_
                 << ", lastSyncedId:" << lastSyncedId_
                 << ", collectiblePages_:" << collectiblePages_
                 << ", residueHistory_:" << residueHistory_
                 << ", size_:" << size_
                 << ", lastPage_:" << lastPage_
                 << "]");
    lastReadError_ = 0;
    return true;
}

void HBtreePrivate::close(bool doSync)
{
    HBTREE_ASSERT(!readTransaction_ && !writeTransaction_);

    if (fd_ != -1) {
        HBTREE_DEBUG("closing btree with fd:" << fd_);
        if (doSync)
            sync();
        if (::flock(fd_, LOCK_UN) != 0) {
            lastErrorMessage_ = QLatin1String("failed to unlock file - ") + QLatin1String(strerror(errno));
            HBTREE_ERROR("failed to unlock file");
        }
        ::close(fd_);
        fd_ = -1;
        if (dirtyPages_.size()) {
            HBTREE_DEBUG("aborting" << dirtyPages_.size() << "dirty pages");
            dirtyPages_.clear();
        }
        cacheClear();
        collectiblePages_.clear();
        spec_ = Spec();
        lastSyncedId_ = 0;
        residueHistory_.clear();
        collectiblePages_.clear();
        marker_ = MarkerPage(0);
        synced_ = MarkerPage(0);
        cursorDisrupted_ = false;
        spec_ = Spec();
    }
}

bool HBtreePrivate::readSpec(const QByteArray &binaryData)
{
    QByteArray buffer = binaryData;
    PageInfo info;
    Spec spec;
    memcpy(&info, buffer.constData(), sizeof(PageInfo));
    memcpy(&spec, buffer.constData() + sizeof(PageInfo), sizeof(Spec));

    if (info.type != PageInfo::Spec) {
        HBTREE_DEBUG("spec type mismatch. Expected" << PageInfo::Spec << "got" << info);
        return false;
    }

    if (info.number != 0) {
        HBTREE_DEBUG("spec number mismatch. Expected 0 got" << info);
        return false;
    }

    if (spec.version != HBTREE_VERSION) {
        HBTREE_DEBUG("spec version mismatch. Expected" << HBTREE_VERSION << "got" << spec.version);
        return false;
    }

    memcpy(&spec_, &spec, sizeof(Spec));

    // If page size is not equal to default page size, then we need to read in the
    // real page size at this time for the checksum to work properly
    if (buffer.size() < spec.pageSize) {
        buffer = QByteArray(spec.pageSize, Qt::Uninitialized);
        int rc = pread(fd_, (void *)buffer.data(), spec.pageSize, 0);

        if (rc != spec.pageSize) {
            lastReadError_ = errno;
            HBTREE_DEBUG("failed to read" << spec.pageSize << "bytes for spec page. Spec:" << spec);
            return false;
        }
        lastReadError_ = 0;
    }

    quint32 crc = calculateChecksum(buffer);
    if (crc != info.checksum) {
        HBTREE_DEBUG("spec checksum mismatch. Expected" << info.checksum << "got" << crc);
        spec_ = Spec();
        return false;
    }

    return true;
}

bool HBtreePrivate::writeSpec()
{
    struct stat sb;
    int rc = 0;
    if ((rc = fstat(fd_, &sb)) != 0) {
        HBTREE_DEBUG("fstat fail - rc:" << rc);
        return false;
    }

    Spec spec;
    spec.version = HBTREE_VERSION;
    spec.keySize = 255;
    spec.pageSize = spec_.pageSize ? spec_.pageSize : (sb.st_blksize > HBTREE_DEFAULT_PAGE_SIZE ? sb.st_blksize : HBTREE_DEFAULT_PAGE_SIZE);

    pageBuffer_.fill((char)0, spec.pageSize);

    PageInfo info(PageInfo::Spec, 0);
    memcpy(pageBuffer_.data(), &info, sizeof(PageInfo));
    memcpy(pageBuffer_.data() + sizeof(PageInfo), &spec, sizeof(Spec));

    memcpy(&spec_, &spec, sizeof(Spec));

    if (!writePage(&pageBuffer_)) {
        spec_ = Spec();
        return false;
    }
    return true;
}

// ######################################################################
// ### Serialization and deserialization
// ######################################################################

QByteArray HBtreePrivate::serializePage(const HBtreePrivate::Page &page) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);

    switch (page.info.type) {
    case PageInfo::Branch:
    case PageInfo::Leaf:
        return serializeNodePage(static_cast<const NodePage &>(page));
    case PageInfo::Overflow:
        return serializeOverflowPage(static_cast<const OverflowPage &>(page));
    default:
        HBTREE_ASSERT(0);
    }
    return QByteArray();
}

HBtreePrivate::Page *HBtreePrivate::deserializePage(const QByteArray &buffer, Page *page) const
{
    HBTREE_ASSERT(page);
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(buffer.size())(spec_);

    quint32 pageType = deserializePageType(buffer);
    switch (pageType) {
    case PageInfo::Leaf:
    case PageInfo::Branch:
        static_cast<NodePage &>(*page) = deserializeNodePage(buffer);
        break;
    case PageInfo::Overflow:
        static_cast<OverflowPage &>(*page) = deserializeOverflowPage(buffer);
        break;
    default:
        HBTREE_ASSERT(0)(pageType).message(QStringLiteral("unknown page type"));
        return 0;
    }
    return page;
}

HBtreePrivate::PageInfo HBtreePrivate::deserializePageInfo(const QByteArray &buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(buffer.size())(spec_);

    PageInfo info;
    memcpy(&info, buffer.constData(), sizeof(PageInfo));
    return info;
}

void HBtreePrivate::serializePageInfo(const HBtreePrivate::PageInfo &info, QByteArray *buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer->size() == (int)spec_.pageSize)(buffer->size())(spec_);
    HBTREE_ASSERT(buffer->isDetached());
    memcpy(buffer->data(), &info, sizeof(PageInfo));
}

HBtreePrivate::Page *HBtreePrivate::newDeserializePage(const QByteArray &buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(buffer.size())(spec_);

    PageInfo pi = deserializePageInfo(buffer);
    Page *page = 0;
    switch (pi.type) {
        case PageInfo::Leaf:
        case PageInfo::Branch:
            page = new NodePage;
            break;
        case PageInfo::Overflow:
            page = new OverflowPage;
            break;
        default:
            HBTREE_ASSERT(0)(pi).message(QStringLiteral("unknown type"));
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
    HBTREE_ASSERT(page.info.type == PageInfo::Branch ||
                  page.info.type == PageInfo::Leaf ||
                  page.info.type == PageInfo::Overflow)(page);
    QByteArray ba = serializePage(page);
    if (ba.isEmpty()) {
        HBTREE_DEBUG("failed to serialize" << page.info);
        return false;
    }

    if (!writePage(&ba)) {
        HBTREE_DEBUG("failed to write" << page.info);
        return false;
    }

    return true;
}

void HBtreePrivate::serializeChecksum(quint32 checksum, QByteArray *buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer->size() == (int)spec_.pageSize)(buffer->size())(spec_);
    HBTREE_ASSERT(checksum != 0)(checksum);
    HBTREE_ASSERT(buffer->isDetached());
    memcpy(buffer->data() + PageInfo::OFFSETOF_CHECKSUM, &checksum, sizeof(quint32));
}

void HBtreePrivate::serializePageNumber(quint32 pgno, QByteArray *buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer->size() == (int)spec_.pageSize)(buffer->size())(spec_);
    HBTREE_ASSERT(buffer->isDetached());
    memcpy(buffer->data() + PageInfo::OFFSETOF_NUMBER, &pgno, sizeof(quint32));
}

quint32 HBtreePrivate::deserializePageNumber(const QByteArray &buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(buffer.size())(spec_);
    quint32 pageNumber;
    memcpy(&pageNumber, buffer.constData() + PageInfo::OFFSETOF_NUMBER, sizeof(quint32));
    return pageNumber;
}

quint32 HBtreePrivate::deserializePageType(const QByteArray &buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(buffer.size())(spec_);
    quint32 pageType;
    memcpy(&pageType, buffer.constData() + PageInfo::OFFSETOF_TYPE, sizeof(quint32));
    return pageType;
}

bool HBtreePrivate::readMarker(quint32 pageNumber, HBtreePrivate::MarkerPage *markerOut, QList<quint32> *overflowPages)
{
    HBTREE_ASSERT(markerOut)(pageNumber);
    HBTREE_ASSERT(overflowPages)(pageNumber);
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE)(pageNumber);
    HBTREE_ASSERT(pageNumber == 1 || pageNumber == 2)(pageNumber);

    pageBuffer_ = readPage(pageNumber);

    if (pageBuffer_.isEmpty()) {
        HBTREE_DEBUG("failed to read marker" << pageNumber);
        return false;
    }

    MarkerPage &mp = *markerOut;
    memcpy(&mp.info, pageBuffer_.constData(), sizeof(PageInfo));
    memcpy(&mp.meta, pageBuffer_.constData() + sizeof(PageInfo), sizeof(MarkerPage::Meta));

    const char *ptr = pageBuffer_.constData() + sizeof(PageInfo) + sizeof(MarkerPage::Meta);
    QByteArray overflowData;
    if (mp.meta.flags & MarkerPage::DataOnOverflow) {
        HBTREE_ASSERT(mp.info.hasPayload())(mp);
        NodeHeader node;
        memcpy(&node, pageBuffer_.constData() + sizeof(PageInfo) + sizeof(MarkerPage::Meta), sizeof(NodeHeader));
        mp.overflowPage = node.context.overflowPage;
        walkOverflowPages(node.context.overflowPage, &overflowData, overflowPages);
        ptr = overflowData.constData();
    }

    if (mp.info.hasPayload()) {
        for (int i = 0; i < mp.info.upperOffset; i += sizeof(quint32)) {
            quint32 pgno;
            memcpy(&pgno, ptr, sizeof(quint32));
            mp.residueHistory.insert(pgno);
            ptr += sizeof(quint32);
        }
    }

    HBTREE_DEBUG("read" << mp);
    return true;
}


HBtreePrivate::NodePage HBtreePrivate::deserializeNodePage(const QByteArray &buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(buffer.size())(spec_);

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

            HBTREE_VERBOSE("deserialized node" << i << "from" << node << "to [" << key << "," << value << "]");
            page.nodes.uncheckedAppend(key, value);
        }
    }
    return page;
}

QByteArray HBtreePrivate::serializeNodePage(const HBtreePrivate::NodePage &page) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(page.history.size() == page.meta.historySize)(page);

    HBTREE_DEBUG("serializing" << page.info);

    pageBuffer_.fill((char)0, spec_.pageSize);

    serializePageInfo(page.info, &pageBuffer_);
    memcpy(pageBuffer_.data() + sizeof(PageInfo), &page.meta, sizeof(NodePage::Meta));

    size_t offset = sizeof(PageInfo) + sizeof(NodePage::Meta);
    foreach (const HistoryNode &hn, page.history) {
        memcpy(pageBuffer_.data() + offset, &hn, sizeof(HistoryNode));
        offset += sizeof(HistoryNode);
    }

    HBTREE_VERBOSE("serializing" << page.info.lowerOffset / sizeof(quint16) << "page nodes");

    if (page.info.hasPayload()) {
        int i = 0;
        quint16 *indices = (quint16 *)(pageBuffer_.data() + offset);
        char *upperPtr = pageBuffer_.data() + pageBuffer_.size();
        Node it = page.nodes.constBegin();
        while (it != page.nodes.constEnd()) {
            const NodeKey &key = it.key();
            const NodeValue &value = it.value();
            quint16 nodeSize = value.data.size() + key.data.size() + sizeof(NodeHeader);
            upperPtr -= nodeSize;
            NodeHeader node;
            node.flags = value.flags;
            node.keySize = key.data.size();
            if (value.flags & NodeHeader::Overflow || page.info.type == PageInfo::Branch) {
                HBTREE_ASSERT(value.data.size() == 0)(value)(page);
                node.context.overflowPage = value.overflowPage;
            } else {
                HBTREE_ASSERT(page.info.type == PageInfo::Leaf)(page);
                node.context.valueSize = value.data.size();
            }
            memcpy(upperPtr, &node, sizeof(NodeHeader));
            memcpy(upperPtr + sizeof(NodeHeader), key.data.constData(), key.data.size());
            memcpy(upperPtr + sizeof(NodeHeader) + key.data.size(), value.data.constData(), value.data.size());
            quint16 upperOffset = (quint16)((pageBuffer_.data() + pageBuffer_.size()) - upperPtr);
            indices[i++] = upperOffset;
            HBTREE_VERBOSE("serialized node" << i << "from [" << key<< "," << value << "]"
                           << "@offset" << upperOffset << "to" << node);
            ++it;
        }
    }

    return pageBuffer_;
}

HBtreePrivate::NodePage::Meta HBtreePrivate::deserializeNodePageMeta(const QByteArray &buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(buffer.size())(spec_);
    NodePage::Meta meta;
    memcpy(&meta, buffer.constData() + sizeof(PageInfo), sizeof(NodePage::Meta));
    return meta;
}

HBtreePrivate::OverflowPage HBtreePrivate::deserializeOverflowPage(const QByteArray &buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(buffer.size())(spec_);

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
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT((size_t)page.data.size() <= capacity(&page))(capacity(&page))(page)(page.data.size());

    HBTREE_DEBUG("serializing" << page.info);

    pageBuffer_.fill((char)0, spec_.pageSize);
    serializePageInfo(page.info, &pageBuffer_);
    NodeHeader node;
    node.flags = 0;
    node.keySize = page.data.size();
    node.context.overflowPage = page.nextPage;
    memcpy(pageBuffer_.data() + sizeof(PageInfo), &node, sizeof(NodeHeader));
    memcpy(pageBuffer_.data() + sizeof(PageInfo) + sizeof(NodeHeader), page.data.constData(), page.data.size());

    HBTREE_VERBOSE("serialized" << page);

    return pageBuffer_;
}

// ######################################################################
// ### Page reading/writing/commiting/syncing
// ######################################################################

QByteArray HBtreePrivate::readPage(quint32 pageNumber)
{
    pageBuffer_.resize(spec_.pageSize);

    const off_t offset = pageNumber * spec_.pageSize;
    if (lseek(fd_, offset, SEEK_SET) != offset) {
        HBTREE_DEBUG("failed to see to offset" << offset << "for page" << pageNumber);
        return QByteArray();
    }

    ssize_t rc = read(fd_, (void *)pageBuffer_.data(), spec_.pageSize);
    if (rc != spec_.pageSize) {
        lastReadError_ = errno;
        HBTREE_DEBUG("failed to read @" << offset << "for page" << pageNumber << "- rc:" << rc);
        return QByteArray();
    }
    lastReadError_ = 0;

    PageInfo pageInfo = deserializePageInfo(pageBuffer_);

    if (pageInfo.number != pageNumber) {
        HBTREE_DEBUG("page number does not match. expected" << pageNumber << "got" << pageInfo.number);
        marker_.meta.flags |= MarkerPage::Corrupted;
        return QByteArray();
    }

    quint32 crc = calculateChecksum(pageBuffer_);
    if (pageInfo.checksum != crc) {
        HBTREE_DEBUG("checksum does not match. expected" << pageInfo.checksum << "got" << crc);
        marker_.meta.flags |= MarkerPage::Corrupted;
        return QByteArray();
    }

    HBTREE_DEBUG("read page:" << pageInfo);

    const Q_Q(HBtree);
    const_cast<HBtree*>(q)->stats_.reads++;

    return pageBuffer_;
}

bool HBtreePrivate::writePage(QByteArray *buffer) const
{
    HBTREE_ASSERT(buffer);
    HBTREE_ASSERT(buffer->isDetached());
    HBTREE_ASSERT(spec_.pageSize > 0)(spec_);

    if (buffer->size() != spec_.pageSize) {
        HBTREE_DEBUG("can't write buffer with size" << buffer->size());
        return false;
    }

    quint32 checksum = calculateChecksum(*buffer);
    serializeChecksum(checksum, buffer);

    quint32 pageNumber = deserializePageNumber(*buffer);

    if (pageNumber == PageInfo::INVALID_PAGE) {
        HBTREE_DEBUG("can't write. Innvalid page number detcted in buffer");
        return false;
    }

    const off_t offset = pageNumber * spec_.pageSize;
    ssize_t rc = pwrite(fd_, (const void *)buffer->constData(), spec_.pageSize, offset);
    if (rc != spec_.pageSize) {
        lastWriteError_ = errno;
        HBTREE_DEBUG("failed pwrite. Expected page size" << spec_.pageSize << "- rc:" << rc);
        return false;
    }
    lastWriteError_ = 0;
    HBTREE_DEBUG("wrote page" << deserializePageInfo(*buffer));

    const Q_Q(HBtree);
    const_cast<HBtree *>(q)->stats_.writes++;

    return true;
}

bool HBtreePrivate::sync()
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(spec_)(HBTREE_DEFAULT_PAGE_SIZE);
    HBTREE_ASSERT(verifyIntegrity(&marker_))(marker_);
    HBTREE_DEBUG("syncing" << marker_);

    if (openMode_ == HBtree::ReadOnly)
        return true;

    if (marker_.meta.syncId == lastSyncedId_) {
        HBTREE_DEBUG("already sunced at" << lastSyncedId_);
        return true;
    }

    MarkerPage synced0(1);

    if (fsync(fd_) != 0) {
        HBTREE_ERROR_LAST("failed to sync data");
        return false;
    }

    // Write marker 1
    copy(marker_, &synced0);

    synced0.residueHistory.unite(collectiblePages_);
    synced0.info.lowerOffset = 0;
    synced0.info.upperOffset = synced0.residueHistory.size() * sizeof(quint32);

    QByteArray buffer(spec_.pageSize, (char)0);
    QList<quint32> overflowPages;
    bool useOverflow = synced0.info.upperOffset > capacity(&synced0);

    if (useOverflow)
        synced0.meta.flags |= MarkerPage::DataOnOverflow;

    if (synced0.info.hasPayload()) {
        QByteArray extra;
        char *ptr = buffer.data() + sizeof(PageInfo) + sizeof(MarkerPage::Meta);
        if (useOverflow) {
            extra.fill((char)0, synced0.info.upperOffset);
            ptr = extra.data();
        }
        foreach (quint32 pgno, synced0.residueHistory) {
            memcpy(ptr, &pgno, sizeof(quint32));
            ptr += sizeof(quint32);
        }
        if (useOverflow) {
            HBTREE_ASSERT(dirtyPages_.isEmpty())(dirtyPages_)(synced0);
            NodeHeader node;
            node.context.overflowPage = putDataOnOverflow(extra, &overflowPages);
            memcpy(buffer.data() + sizeof(PageInfo) + sizeof(MarkerPage::Meta), &node, sizeof(NodeHeader));
            PageMap::const_iterator it = dirtyPages_.constBegin();
            while (it != dirtyPages_.constEnd()) {
                HBTREE_ASSERT(it.value()->info.type == PageInfo::Overflow)(*it.value())(synced0);
                pageBuffer_ = serializePage(*it.value());
                if (pageBuffer_.isEmpty()) {
                    HBTREE_DEBUG("failed to serialize" << synced0.info);
                    return false;
                }
                if (!writePage(&pageBuffer_)) {
                    HBTREE_DEBUG("failed to write" << synced0.info);
                    return false;
                }
                it.value()->dirty = false;
                cacheDelete(it.value()->info.number);
                ++it;
            }
            dirtyPages_.clear();
            synced0.overflowPage = node.context.overflowPage;
        }
    }

    synced0.meta.size = lseek(fd_, 0, SEEK_END);

    memcpy(buffer.data(), &synced0.info, sizeof(PageInfo));
    memcpy(buffer.data() + sizeof(PageInfo), &synced0.meta, sizeof(MarkerPage::Meta));

    if (!writePage(&buffer)) {
        HBTREE_ERROR_LAST("failed to write sync marker 0");
        return false;
    }

    // Add previous synced marker overflow pages to collectible list
    if (synced_.meta.flags & MarkerPage::DataOnOverflow) {
        QList<quint32> pages;
        if (!getOverflowPageNumbers(synced_.overflowPage, &pages)) {
            HBTREE_DEBUG("failed to get overflow pages for" << synced_);
            return false;
        }
        foreach (quint32 pgno, pages)
            collectiblePages_.insert(pgno);
    }

    copy(synced0, &synced_);

    lastSyncedId_++;
    collectiblePages_.unite(marker_.residueHistory);
    marker_.residueHistory.clear();
    marker_.info.upperOffset = 0;
    residueHistory_.clear();
    size_ = synced0.meta.size;
    // Remove the overflow pages from the collectible list. They may have been used
    // if we had to put the residue pages on overflow pages.
    // This has to be done on open as well.
    foreach (quint32 pgno, overflowPages)
        collectiblePages_.remove(pgno);

    if (fsync(fd_) != 0) {
        HBTREE_DEBUG("wrote but failed to sync marker 0");
        return false;
    }

    HBTREE_DEBUG("synced marker 0 and upped sync id to" << lastSyncedId_);

    Q_Q(HBtree);
    q->stats_.numSyncs++;

    // Just change page number and write second marker
    serializePageNumber(2, &buffer);
    if (!writePage(&buffer)) {
        HBTREE_ERROR_LAST("failed to write sync marker 1");
        return false;
    }

    HBTREE_VERBOSE("synced marker 1");

    return true;
}

bool HBtreePrivate::readSyncedMarker(HBtreePrivate::MarkerPage *markerOut, QList<quint32> *overflowPages)
{
    HBTREE_ASSERT(markerOut);
    if (!readMarker(1, markerOut, overflowPages)) {
        HBTREE_DEBUG("synced marker 1 invalid. Checking synced marker 2.");
        if (!readMarker(2, markerOut, overflowPages)) {
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
    HBTREE_ASSERT(transaction)(tag);
    HBTREE_DEBUG("commiting" << dirtyPages_.size() << "pages");

    off_t sizeBefore = lseek(fd_, 0, SEEK_END);
    PageMap::iterator it = dirtyPages_.begin();
    QVector<Page *> commitedPages;
    QSet<quint32> collectedPages;
#ifdef QT_TESTLIB_LIB
    int numCommited = 0;
#endif
    bool ok = true;
    while (it != dirtyPages_.constEnd()) {
        HBTREE_ASSERT(verifyIntegrity(it.value()))(*it.value());

        if (it.value()->info.type == PageInfo::Branch || it.value()->info.type == PageInfo::Leaf)
            collectedPages.unite(collectHistory(static_cast<NodePage *>(it.value())));

        pageBuffer_ = serializePage(*it.value());

#ifdef QT_TESTLIB_LIB
        ok = !(forceCommitFail_ && numCommited++ >= forceCommitFail_);
        if (ok)
#endif
            ok = writePage(&pageBuffer_);
        if (!ok) {
            HBTREE_DEBUG("failed to commit page" << *it.value());
            if (lseek(fd_, 0, SEEK_END) != sizeBefore) {
                HBTREE_DEBUG("size increased to" << lseek(fd_, 0, SEEK_END) << "- truncating to" << sizeBefore);
                if (ftruncate(fd_, sizeBefore) != 0)
                    HBTREE_DEBUG("failed to truncate");
            }
            break;
        }

        it.value()->dirty = false;

        if (it.value()->info.type == PageInfo::Overflow)
            cacheDelete(it.value()->info.number);
        else
            commitedPages.append(it.value());

        it = dirtyPages_.erase(it);
    }

    if (!ok) {
        HBTREE_DEBUG("Failed to commit pages. Successfully commited" << commitedPages.size() << "pages");
        foreach (Page *p, commitedPages)
            cacheDelete(p->info.number);
        return false;
    }

    HBTREE_DEBUG("adding" << collectedPages << "to collectible list");
    collectiblePages_.unite(collectedPages);

    size_ = lseek(fd_, 0, SEEK_END);
    dirtyPages_.clear();

    // Write marker
    MarkerPage mp = marker_;
    mp.meta.revision++;
    mp.meta.syncId = lastSyncedId_ + 1;
    mp.meta.root = transaction->rootPage_;
    mp.meta.tag = tag;
    mp.meta.size = 0;
    mp.residueHistory = residueHistory_;
    mp.info.upperOffset = residueHistory_.size() * sizeof(quint32);

    copy(mp, &marker_);
    HBTREE_ASSERT(verifyIntegrity(&marker_))(marker_);

    abort(transaction);

    Q_Q(HBtree);
    q->stats_.numCommits++;

    return true;
}

void HBtreePrivate::abort(HBtreeTransaction *transaction)
{
    HBTREE_ASSERT(transaction);
    HBTREE_DEBUG("aborting transaction with" << dirtyPages_.size() << "dirty pages");
    foreach (Page *page, dirtyPages_) {
        HBTREE_ASSERT(cacheFind(page->info.number))(page->info);
        cacheDelete(page->info.number);
    }
    dirtyPages_.clear();
    if (transaction->isReadWrite()) {
        HBTREE_ASSERT(transaction == writeTransaction_);
        writeTransaction_ = 0;
    } else {
        HBTREE_ASSERT(transaction == readTransaction_);
        readTransaction_ = 0;
    }
    delete transaction;
    cachePrune();
    cursorDisrupted_ = false;
}

quint32 HBtreePrivate::calculateChecksum(quint32 crc, const char *begin, const char *end) const
{
    Q_ASSERT(begin <= end);
    return crc32_little(crc, (const unsigned char *)begin, end-begin);
}

quint32 HBtreePrivate::calculateChecksum(const QByteArray &buffer) const
{
    HBTREE_ASSERT(spec_.pageSize >= HBTREE_DEFAULT_PAGE_SIZE)(HBTREE_DEFAULT_PAGE_SIZE)(spec_);
    HBTREE_ASSERT(buffer.size() == (int)spec_.pageSize)(spec_)(buffer.size());

    quint32 crcOffset = sizeof(quint32);
    const char *begin = buffer.constData();
    const char *end = buffer.constData() + spec_.pageSize;
    PageInfo info = deserializePageInfo(buffer);
    quint32 crc = 0;
    HBTREE_VERBOSE("calculating checksum for" << info);

    if (info.type == PageInfo::Spec) {
        crc = calculateChecksum(crc, begin + crcOffset, begin + (sizeof(PageInfo) + sizeof(Spec)));
    } else if (info.type == PageInfo::Branch || info.type == PageInfo::Leaf) {
        NodePage::Meta meta = deserializeNodePageMeta(buffer);
        crc = calculateChecksum(crc, begin + crcOffset, begin
                    + (sizeof(HBtreePrivate::PageInfo)
                       + sizeof(HBtreePrivate::NodePage::Meta)
                       + meta.historySize * sizeof(HistoryNode)
                       + info.lowerOffset));
        crc = calculateChecksum(crc, end - info.upperOffset, end);
    } else if (info.type == PageInfo::Marker) {
        crc = calculateChecksum(crc, begin + crcOffset, begin
                    + (sizeof(HBtreePrivate::PageInfo)
                       + sizeof(HBtreePrivate::MarkerPage::Meta)));
    } else if (info.type == PageInfo::Overflow) {
        HBTREE_ASSERT(info.hasPayload())(info); // lower offset represents size of overflow
        crc = calculateChecksum(crc, begin + crcOffset, begin
                    + (sizeof(HBtreePrivate::PageInfo)
                       + sizeof(HBtreePrivate::NodeHeader)
                       + info.lowerOffset));
    } else {
        HBTREE_ASSERT(0).message(QStringLiteral("unknown page type"));
        HBTREE_ERROR_LAST("unknown page type");
        return 0;
    }

    HBTREE_VERBOSE("checksum:" <<  crc);

    return crc;
}

// ######################################################################
// ### btree operations
// ######################################################################


HBtreeTransaction *HBtreePrivate::beginTransaction(HBtreeTransaction::Type type)
{
    Q_Q(HBtree);

    HBTREE_ASSERT(!writeTransaction_ && !readTransaction_);

    if (writeTransaction_ || readTransaction_) {
        HBTREE_ERROR_LAST("Only one transaction type supported at a time");
        return 0;
    }

    if (type == HBtreeTransaction::ReadWrite && writeTransaction_) {
        HBTREE_ERROR_LAST("cannot open write transaction when one in progress");
        return 0;
    }

    if (type == HBtreeTransaction::ReadWrite && openMode_ == HBtree::ReadOnly) {
        HBTREE_ERROR_LAST("cannot open write transaction on read only btree");
        return 0;
    }

//    if (type == HBtreeTransaction::ReadOnly && currentMarker().meta.rootPage == PageInfo::INVALID_PAGE) {
//        HBTREE_ERROR("nothing to read");
//        return 0;
//    }

    if (type == HBtreeTransaction::ReadWrite) {
        HBTREE_ASSERT(dirtyPages_.isEmpty())(dirtyPages_);
    }

    // Do we check here if a write process wrote some more data?
    // The readAndWrite auto test will not pass without some kind of
    // marker update check.

    HBtreeTransaction *transaction = new HBtreeTransaction(q, type);
    transaction->rootPage_ = marker_.meta.root;
    transaction->tag_ = marker_.meta.tag;
    transaction->revision_ = marker_.meta.revision;
    if (type == HBtreeTransaction::ReadWrite)
        writeTransaction_ = transaction;
    else
        readTransaction_ = transaction;
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
    HBTREE_ASSERT(transaction)(keyData)(valueData);
    HBTREE_DEBUG( "put => [" << keyData
              #if HBTREE_VERBOSE_OUTPUT
                  << "," << valueData
              #endif
                  << "] @" << transaction);

    if (transaction->isReadOnly()) {
        HBTREE_ERROR_LAST("can't write with read only transaction");
        return false;
    }

    if (keyData.size() > 512) {
        lastErrorMessage_ = QLatin1String("cannot insert keys larger than 512 bytes");
        HBTREE_ERROR("cannot insert keys larger than 512 bytes. Key size:" << keyData.size());
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
        HBTREE_ASSERT(verifyIntegrity(page))(*page);
    }

    Q_Q(HBtree);
    if (!page && searchPage(NULL, transaction, nkey, SearchKey, true, &page)) {
        if (page->nodes.contains(nkey)) {
            HBTREE_DEBUG("already contains key. Removing");
            if (!removeNode(page, nkey)) {
                HBTREE_ERROR_LAST("failed to remove previous value of key");
                return false;
            }
            q->stats_.numEntries--;
        }
    }

    bool ok = false;
    if (spaceNeededForNode(keyData, valueData) <= spaceLeft(page))
        ok = insertNode(page, nkey, nval);
    else
        ok = split(page, nkey, nval);

    HBTREE_ASSERT(verifyIntegrity(page))(*page);

    if (closeTransaction) {
        ok = transaction->commit(0);
    }

    q->stats_.numEntries++;
    cachePrune();

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
    QByteArray ret = getDataFromNode(nval);
    cachePrune();
    return ret;
}

bool HBtreePrivate::del(HBtreeTransaction *transaction, const QByteArray &keyData)
{
    HBTREE_ASSERT(transaction)(keyData);
    HBTREE_DEBUG( "del => [" << keyData << "] @" << transaction);

    if (transaction->isReadOnly()) {
        HBTREE_ERROR_LAST("can't delete with read only transaction");
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
        lastErrorMessage_ = QLatin1String("failed to rebalance tree");
        HBTREE_ERROR("failed to rebalance" << *page);
        return false;
    }

    if (closeTransaction) {
        ok |= transaction->commit(0);
    }

    Q_Q(HBtree);
    q->stats_.numEntries--;

    cachePrune();
    cursorDisrupted_ = true;
    return ok;
}

HBtreePrivate::Page *HBtreePrivate::newPage(HBtreePrivate::PageInfo::Type type)
{
    int pageNumber = PageInfo::INVALID_PAGE;

    bool collected = false;
    if (collectiblePages_.size()) {
        quint32 n = *collectiblePages_.constBegin();
        collectiblePages_.erase(collectiblePages_.begin());
        pageNumber = n;
        collected = true;
    } else {
        pageNumber = lastPage_++;
    }

    Page *page = cacheRemove(pageNumber);

    if (page) {
        HBTREE_ASSERT(page->dirty == false)(*page);
        if (page->info.type == type) {
            destructPage(page);
        } else {
            deletePage(page);
            page = 0;
        }
    }

    switch (type) {
        case PageInfo::Leaf:
        case PageInfo::Branch: {
            NodePage *np = page ? new (page) NodePage(type, pageNumber) : new NodePage(type, pageNumber);
            np->meta.syncId = lastSyncedId_ + 1;
            np->meta.commitId = marker_.meta.revision + 1;
            np->collected = collected;
            page = np;
            break;
        }
        case PageInfo::Overflow: {
            OverflowPage *ofp = page ? new (page) OverflowPage(type, pageNumber) : new OverflowPage(type, pageNumber);
            page = ofp;
            break;
        }
        case PageInfo::Marker:
        case PageInfo::Spec:
        case PageInfo::Unknown:
        default:
            HBTREE_ASSERT(0).message(QStringLiteral("unknown type"));
            return 0;
    }

    HBTREE_DEBUG("created new page" << page->info);

    cacheInsert(pageNumber, page);
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
    HBTREE_ASSERT(page);
    HBTREE_ASSERT(page->info.type == PageInfo::Branch || page->info.type == PageInfo::Leaf)(page->info);

    if (page->dirty) {
        HBTREE_DEBUG(page->info << "is dirty, no need to touch");
        return page;
    }

    HBTREE_ASSERT(cacheFind(page->info.number))(page->info);
    HBTREE_ASSERT(!dirtyPages_.contains(page->info.number))(page->info);

    HBTREE_DEBUG("touching page" << page->info);
    NodePage *touched = static_cast<NodePage *>(newPage(PageInfo::Type(page->info.type)));
    HBTREE_ASSERT(touched->info.number != page->info.number)(page->info);
    copy(*page, touched);
    touched->meta.syncId = lastSyncedId_ + 1;
    touched->meta.commitId = marker_.meta.revision + 1;

    HBTREE_DEBUG("touched page" << page->info.number << "to" << touched->info.number);

    // Set parent's child page number to new one
    if (touched->parent) {
        HBTREE_ASSERT(touched->parent->info.type == PageInfo::Branch)(touched->parent->info);
        HBTREE_ASSERT(touched->parent->nodes.contains(touched->parentKey))(touched->parentKey)(*touched->parent);

        NodeValue &val = touched->parent->nodes[touched->parentKey];
        val.overflowPage = touched->info.number;
    }

    if (touched->rightPageNumber != PageInfo::INVALID_PAGE) {
        NodePage *right = static_cast<NodePage *>(cacheFind(touched->rightPageNumber));
        if (right)
            right->leftPageNumber = touched->info.number;
    }

    if (touched->leftPageNumber != PageInfo::INVALID_PAGE) {
        NodePage *left = static_cast<NodePage *>(cacheFind(touched->leftPageNumber));
        if (left)
            left->rightPageNumber = touched->info.number;
    }

    touched->dirty = true;

    addHistoryNode(touched, HistoryNode(page));

    HBTREE_ASSERT(verifyIntegrity(touched))(*touched);

    return touched;
}

quint32 HBtreePrivate::putDataOnOverflow(const QByteArray &value, QList<quint32> *pagesUsed)
{
    HBTREE_DEBUG("putting data on overflow page");
    int sizePut = 0;
    quint32 overflowPageNumber = PageInfo::INVALID_PAGE;
    OverflowPage *prevPage = 0;
    if (pagesUsed)
        pagesUsed->clear();
    while (sizePut < value.size()) {
        OverflowPage *overflowPage = static_cast<OverflowPage *>(newPage(PageInfo::Overflow));
        if (overflowPageNumber == PageInfo::INVALID_PAGE)
            overflowPageNumber = overflowPage->info.number;
        int sizeToPut = qMin((value.size() - sizePut), (int)capacity(overflowPage));
        HBTREE_DEBUG("putting" << sizeToPut << "bytes @ offset" << sizePut);
        overflowPage->data.resize(sizeToPut);
        memcpy(overflowPage->data.data(), value.constData() + sizePut, sizeToPut);
        if (pagesUsed)
            pagesUsed->append(overflowPage->info.number);
        if (prevPage)
            prevPage->nextPage = overflowPage->info.number;
        overflowPage->info.lowerOffset = (quint16)sizeToPut; // put it here too for quicker checksum checking
        sizePut += sizeToPut;
        prevPage = overflowPage;

        HBTREE_ASSERT(verifyIntegrity(overflowPage))(*overflowPage);
    }
    return overflowPageNumber;
}

QByteArray HBtreePrivate::getDataFromNode(const HBtreePrivate::NodeValue &nval)
{
    if (nval.flags & NodeHeader::Overflow) {
        QByteArray data;
        getOverflowData(nval.overflowPage, &data);
        return data;
    } else {
        return nval.data;
    }
}

bool HBtreePrivate::walkOverflowPages(quint32 startPage, QByteArray *data, QList<quint32> *pages)
{
    HBTREE_ASSERT(data || pages);

    if (data)
        data->clear();
    if (pages)
        pages->clear();

    while (startPage != PageInfo::INVALID_PAGE) {
        OverflowPage *page = static_cast<OverflowPage *>(getPage(startPage));
        if (page) {
            if (data)
                data->append(page->data);
            if (pages)
                pages->append(startPage);
            startPage = page->nextPage;
        } else {
            if (data)
                data->clear();
            if (pages)
                pages->clear();
            return false;
        }
    }

    return true;
}

bool HBtreePrivate::getOverflowData(quint32 startPage, QByteArray *data)
{
    return walkOverflowPages(startPage, data, 0);
}

bool HBtreePrivate::getOverflowPageNumbers(quint32 startPage, QList<quint32> *pages)
{
    return walkOverflowPages(startPage, 0, pages);
}

QSet<quint32> HBtreePrivate::collectHistory(NodePage *page)
{
    QSet<quint32> pages;
    int numBeforeSync = 0;
    int numBetweenSyncAndCommit = 0;
    quint16 numRemoved = 0;
    QList<HistoryNode>::iterator it = page->history.begin();
    while (it != page->history.end()) {
        bool canCollect = false;
        if (it->syncId <= lastSyncedId_) {
            if (numBeforeSync++)
                canCollect = true;
        }

        if (it->syncId > lastSyncedId_ && it->commitId <= marker_.meta.revision) {
            if (numBetweenSyncAndCommit++ >= HBTREE_COMMIT_CHAIN)
                canCollect = true;
        }

        if (canCollect) {
            HBTREE_DEBUG("marking" << *it << "as collectible. Last sync =" << lastSyncedId_);
            pages.insert(it->pageNumber);
            numRemoved++;
            it = page->history.erase(it);
            continue;
        }
        ++it;
    }
    page->meta.historySize -= numRemoved;
    return pages;
}

HBtreePrivate::Page *HBtreePrivate::cacheFind(quint32 pgno) const
{
    PageMap::const_iterator it = cache_.find(pgno);
    if (it != cache_.constEnd())
        return it.value();
    return 0;
}

HBtreePrivate::Page *HBtreePrivate::cacheRemove(quint32 pgno)
{
    Page *page = cache_.take(pgno);
    if (page)
        lru_.removeOne(page);
    return page;
}

void HBtreePrivate::cacheDelete(quint32 pgno)
{
    Page *page = cacheRemove(pgno);
    if (page)
        deletePage(page);
}

void HBtreePrivate::cacheClear()
{
    PageMap::const_iterator it = cache_.constBegin();
    while (it != cache_.constEnd()) {
        deletePage(it.value());
        ++it;
    }
    cache_.clear();
    lru_.clear();
}

void HBtreePrivate::cacheInsert(quint32 pgno, HBtreePrivate::Page *page)
{
    HBTREE_ASSERT(pgno > 2)(pgno);
    HBTREE_ASSERT(pgno != PageInfo::INVALID_PAGE);
    HBTREE_ASSERT(pgno < lastPage_)(pgno)(lastPage_);
    cache_.insert(pgno, page);
    lru_.removeOne(page);
    lru_.append(page);
}

void HBtreePrivate::cachePrune()
{
    if (lru_.size() > (int)cacheSize_) {
        QList<Page *>::iterator it = lru_.begin();
        while (it != lru_.end()) {
            if (lru_.size() <= (int)cacheSize_)
                break;
            Page *page = *it;
            if (!page->dirty) {
                HBTREE_ASSERT(page);
                HBTREE_DEBUG("pruning page" << page->info);
                cache_.remove(page->info.number);
                it = lru_.erase(it);
                deletePage(page);
            } else {
                ++it;
            }
        }
    }
}

void HBtreePrivate::removeFromTree(HBtreePrivate::NodePage *page)
{
    HBTREE_ASSERT(writeTransaction_);

    if (!page->dirty) {
        HBTREE_DEBUG(page->info << "not dirty. No need to remove. Root =" << writeTransaction_->rootPage_);
        return;
    }

    HBTREE_DEBUG("removing" << page->info << "from tree with root" << writeTransaction_->rootPage_);

    // Don't collect this page if it was new'ed (i.e. not on disk as a collectible)
    if (page->collected) {
        addHistoryNode(NULL, HistoryNode(page));
    } else {
        HBTREE_DEBUG("page is not on disk. Will not collect history:" << page->history);
    }

    // Same for history nodes
    foreach (const HistoryNode &hn, page->history)
        addHistoryNode(NULL, hn);

    // Don't need to commit since it's not part of our tree
    dirtyPages_.remove(page->info.number);
    cacheDelete(page->info.number);
}

bool HBtreePrivate::searchPage(HBtreeCursor *cursor, HBtreeTransaction *transaction, const NodeKey &key, SearchType searchType,
                               bool modify, HBtreePrivate::NodePage **pageOut)
{
    quint32 root = 0;

    if (!transaction) {
        HBTREE_ASSERT(modify == false)(key)(searchType);
        root = marker_.meta.root;
    } else {
        root = transaction->rootPage_;
    }

    if (root == PageInfo::INVALID_PAGE) {
        HBTREE_DEBUG("btree is empty");
        *pageOut = 0;
        return false;
    }

    NodePage *page = static_cast<NodePage *>(getPage(root));

    if (page && modify) {
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
        HBTREE_ASSERT(child->nodes.size() > 1)(*child);

        if (searchType == SearchLast) {
            parentIter = (child->nodes.constEnd() - 1);
        } else if (searchType == SearchFirst) {
            parentIter = child->nodes.constBegin();
        } else {
            HBTREE_DEBUG("searching upper bound for" << key);
            parentIter = child->nodes.upperBound(key) - 1;
            HBTREE_DEBUG("found key" << parentIter.key() << "to page" << parentIter.value());
        }

        if (cursor) {
            if (parentIter == child->nodes.constBegin())
                leftQ.push(HBtreePrivate::PageInfo::INVALID_PAGE);
            else
                leftQ.push((parentIter - 1).value().overflowPage);

            if (parentIter == (child->nodes.constEnd() - 1))
                rightQ.push(HBtreePrivate::PageInfo::INVALID_PAGE);
            else
                rightQ.push((parentIter + 1).value().overflowPage);
        }

        parent = child;
        child = static_cast<NodePage *>(getPage(parentIter.value().overflowPage));

        HBTREE_ASSERT(child)(parentIter)(*parent);

        if (child->info.type == PageInfo::Branch) {
            child->leftPageNumber = PageInfo::INVALID_PAGE;
            child->rightPageNumber = PageInfo::INVALID_PAGE;
        }

        child->parent = parent;
        child->parentKey = parentIter.key();

        if (modify && (child = touchNodePage(child)) == NULL) {
            lastErrorMessage_ = QLatin1String("failed to touch page");
            HBTREE_ERROR("failed to touch page" << child->info);
            return false;
        }

        HBTREE_ASSERT(verifyIntegrity(parent))(*parent);
    }

    HBTREE_ASSERT(verifyIntegrity(child))(*child);
    HBTREE_ASSERT(child->info.type == PageInfo::Leaf)(child->info.type);

    if (cursor) {
        child->rightPageNumber = getRightSibling(rightQ);
        child->leftPageNumber = getLeftSibling(leftQ);
        cursor->lastLeaf_ = child->info.number;

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

    HBTREE_VERBOSE("must go up" << idx << "levels");

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

    HBTREE_VERBOSE("must go up" << idx << "levels");

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

HBtreePrivate::Page *HBtreePrivate::getPage(quint32 pageNumber)
{
    HBTREE_ASSERT(pageNumber > 2 && pageNumber != PageInfo::INVALID_PAGE)(pageNumber);
    Page *page = cacheFind(pageNumber);
    if (page) {
        Q_Q(HBtree);
        q->stats_.hits++;
        HBTREE_DEBUG("got" << page->info << "from cache");
        lru_.removeOne(page);
        lru_.append(page);
        return page;
    }

    HBTREE_DEBUG("reading page #" << pageNumber);

    QByteArray buffer;
    buffer = readPage(pageNumber);

    if (buffer.isEmpty()) {
        HBTREE_DEBUG("failed to read page" << pageNumber);
        return 0;
    }

    page = newDeserializePage(buffer);
    if (!page) {
        HBTREE_DEBUG("failed to deserialize page" << pageNumber);
        return 0;
    }

    page->dirty = false;
    cacheInsert(pageNumber, page);

    return page;
}

void HBtreePrivate::deletePage(HBtreePrivate::Page *page) const
{
    HBTREE_ASSERT(page);
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
        HBTREE_ASSERT(0)(*page).message(QStringLiteral("unknown page type"));
    }
}

void HBtreePrivate::destructPage(HBtreePrivate::Page *page) const
{
    HBTREE_ASSERT(page);
    HBTREE_DEBUG("destructing page" << page->info);
    switch (page->info.type) {
    case PageInfo::Overflow:
        static_cast<OverflowPage *>(page)->~OverflowPage();
        break;
    case PageInfo::Leaf:
    case PageInfo::Branch:
        static_cast<NodePage *>(page)->~NodePage();
        break;
    default:
        HBTREE_ASSERT(0)(*page).message(QStringLiteral("unknown page type"));
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
        HBTREE_ASSERT(static_cast<const NodePage *>(page)->meta.historySize == static_cast<const NodePage *>(page)->history.size())(*static_cast<const NodePage *>(page));
        return sizeof(HBtreePrivate::PageInfo)
                + sizeof(HBtreePrivate::NodePage::Meta)
                + static_cast<const NodePage *>(page)->meta.historySize * sizeof(HistoryNode);
    case HBtreePrivate::PageInfo::Overflow:
        return sizeof(HBtreePrivate::PageInfo) + sizeof(HBtreePrivate::NodeHeader);
    default:
        HBTREE_ASSERT(0)(*page).message(QStringLiteral("unhandled page type"));
    }
    return 0;
}

quint16 HBtreePrivate::spaceLeft(const Page *page) const
{
    HBTREE_ASSERT(spaceUsed(page) <= capacity(page))(capacity(page))(spaceUsed(page))(*page);
    return capacity(page) - spaceUsed(page);
}

quint16 HBtreePrivate::spaceUsed(const Page *page) const
{
    return page->info.upperOffset + page->info.lowerOffset;
}

quint16 HBtreePrivate::capacity(const Page *page) const
{
    return spec_.pageSize - headerSize(page);
}

double HBtreePrivate::pageFill(HBtreePrivate::NodePage *page) const
{
    double pageFill = 1.0 - (float)spaceLeft(page) / (float)capacity(page);
    pageFill *= 100.0f;
    return pageFill;
}

bool HBtreePrivate::hasSpaceFor(HBtreePrivate::NodePage *page, const HBtreePrivate::NodeKey &key, const HBtreePrivate::NodeValue &value) const
{
    quint16 left = spaceLeft(page);
    quint16 spaceRequired = spaceNeededForNode(key.data, value.data);
    return left >= spaceRequired;
}

bool HBtreePrivate::insertNode(NodePage *page, const NodeKey &key, const NodeValue &value)
{
    HBTREE_ASSERT(page)(key)(value);
    HBTREE_ASSERT(page->dirty)(*page)(key)(value);

    HBTREE_DEBUG("inserting" << key << "," << value << "in" << page->info);
    NodeValue valueCopy;

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
            page->meta.flags |= NodeHeader::Overflow;
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

bool HBtreePrivate::removeNode(HBtreePrivate::NodePage *page, const HBtreePrivate::NodeKey &key, bool isTransfer)
{
    HBTREE_ASSERT(page)(key);
    HBTREE_ASSERT(page->dirty)(*page)(key);
    HBTREE_DEBUG("removing" << key << "from" << page->info);

    if (!page->nodes.contains(key)) {
        HBTREE_DEBUG("nothing to remove for key" << key);
        return true;
    }

    NodeValue value = page->nodes.value(key);

    if (value.flags & NodeHeader::Overflow && !isTransfer) {
        HBTREE_ASSERT(page->info.type == PageInfo::Leaf)(*page)(key);
        QList<quint32> overflowPages;
        if (!getOverflowPageNumbers(value.overflowPage, &overflowPages)) {
            HBTREE_ERROR_LAST("failed to get overflow page numbers");
            return false;
        }

        foreach (quint32 pageNumber, overflowPages) {
            HistoryNode hn;
            hn.pageNumber = pageNumber;
            hn.syncId = lastSyncedId_ + 1;
            hn.commitId = marker_.meta.revision + 1;
            addHistoryNode(NULL, hn);
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

    HBTREE_DEBUG("inserting key/value in copy");
    insertNode(&copy, key, value); // no need to insert full value here??
    int splitIndex = 0;
    if (copy.info.type == PageInfo::Branch) {
        // Branch page should be fine to split in the middle since there's
        // no data and just keys...
        splitIndex = copy.nodes.size() / 2; // bias for left page
    } else if (copy.info.type == PageInfo::Leaf) {
        // Find node at which number of bytes exceeds half
        // the capacity of the left page
        quint16 threshold = capacity(left) / 2;
        threshold -= (spec_.overflowThreshold / 2);
        // Note: subtracting (spec_.overflowThreshold / 2) from the threshold seems to increase file size
        // when inserting contigious data. Adding the same decreases the file size.
        // Decreases file size with random data.
        quint16 current = 0;
        Node it = copy.nodes.constBegin();
        while (current < threshold && it != copy.nodes.constEnd()) {
            current += spaceNeededForNode(it.key().data, it.value().data);
            ++it;
            splitIndex++;
        }
    } else {
        HBTREE_ASSERT(0)(copy).message(QStringLiteral("what are you splitting??"));
        HBTREE_DEBUG("splitting unknown page type" << copy);
        return false;
    }
    HBTREE_DEBUG("splitIndex =" << splitIndex << "from" << copy.nodes.size() << "nodes in copy");
    Node splitIter = copy.nodes.constBegin() + splitIndex;
    NodeKey splitKey = splitIter.key();
    NodeValue splitValue(right->info.number);

    right->parent = left->parent;

    // split branch if no space
    if (spaceNeededForNode(splitKey.data, splitValue.data) >= spaceLeft(right->parent)) {
        HBTREE_DEBUG("not enough space in right parent - splitting:");
        if (!split(right->parent, splitKey, splitValue)) {
            HBTREE_DEBUG("failed to split parent" << *right->parent);
            return false;
        } else {
            if (right->parent != left->parent) {
                HBTREE_ASSERT(0).message(QStringLiteral("parents not the same. What happened?"));
                return false;
                // TODO: Original btree does something here...
                // WHAAAAAT ISSSSS IIIITTTTTTT?????
                // Never seem to hit this assert though...
            }
        }
    } else {
        right->parentKey = splitKey;
        HBTREE_DEBUG("set right parentKey" << splitKey << "and inserting split node in parent");
        if (!insertNode(right->parent, splitKey, splitValue)) {
            HBTREE_DEBUG("failed to insert split keys in right parent");
            return false;
        }
    }

    int index = 0;
    Node node = copy.nodes.constBegin();
    while (node != copy.nodes.constEnd()) {
        if (index++ < splitIndex) {
//            // There's a corner case where a 3-way split becomes necessary, when the new node
//            // is too big for the left page. If this is true then key should be <= than node.key
//            // (since it may have been already inserted) and value should not be an overflow.
//            if (spaceNeededForNode(node.key().data, node.value().data) > spaceLeft(left)) {
//                HBTREE_ASSERT(left->info.type != PageInfo::Branch); // This should never happen with a branch page
//                HBTREE_ASSERT(key <= node.key());
//                HBTREE_ASSERT(willCauseOverflow(key.data, value.data) == false);
//                if (!split(left, node.key(), node.value(), &left)) {
//                    HBTREE_ERROR("3-way split fail");
//                    return false;
//                }
//                ++node;
//                continue;
//            }
            HBTREE_ASSERT(spaceNeededForNode(node.key().data, node.value().data) <= spaceLeft(left))(node)(spaceLeft(left));

            if (!insertNode(left, node.key(), node.value())) {
                HBTREE_DEBUG("failed to insert key in to left");
                return false;
            }
            HBTREE_VERBOSE("inserted" << node.key() << "in left");
        }
        else {
            HBTREE_ASSERT(spaceNeededForNode(node.key().data, node.value().data) <= spaceLeft(right))(node)(spaceLeft(right));

            if (!insertNode(right, node.key(), node.value())) {
                HBTREE_DEBUG("failed to insert key in to right");
                return false;
            }
            HBTREE_VERBOSE("inserted" << node.key() << "in right");
        }
        ++node;
    }

    // adjust right/left siblings
    HBTREE_ASSERT(left->info.type == right->info.type)(*left)(*right);
    if (left->info.type == PageInfo::Leaf) {
        // since we have a new right, the left's right sibling has a new left.
        if (left->rightPageNumber != PageInfo::INVALID_PAGE) {
            NodePage *outerRight = static_cast<NodePage *>(cacheFind(left->rightPageNumber));
            if (outerRight)
                outerRight->leftPageNumber = right->info.number;
        }

        left->rightPageNumber = right->info.number;
        right->leftPageNumber = left->info.number;
    }

    HBTREE_ASSERT(verifyIntegrity(right))(*right);
    HBTREE_ASSERT(verifyIntegrity(left))(*left);

    if (rightOut)
        *rightOut = right;

    return true;
}

bool HBtreePrivate::rebalance(HBtreePrivate::NodePage *page)
{
    NodePage *parent = page->parent;

    if (pageFill(page) > spec_.pageFillThreshold)
        return true;

    HBTREE_DEBUG("rebalancing" << *page << "with page fill" << pageFill(page) << "and fill threshold" << spec_.pageFillThreshold);

    if (parent == NULL) { // root
        HBTREE_ASSERT(writeTransaction_->rootPage_ != PageInfo::INVALID_PAGE)(*page);
        HBTREE_ASSERT(page->info.number == writeTransaction_->rootPage_)(*page)(writeTransaction_->rootPage_);
        Q_Q(HBtree);
        if (page->nodes.size() == 0) {
            HBTREE_DEBUG("making root invalid, btree empty.");
            writeTransaction_->rootPage_ = PageInfo::INVALID_PAGE;
            removeFromTree(page);
        } else if (page->info.type == PageInfo::Branch && page->nodes.size() == 1) {
            NodePage *root = static_cast<NodePage *>(getPage(page->nodes.constBegin().value().overflowPage));
            root->parent = NULL;
            writeTransaction_->rootPage_ = page->nodes.constBegin().value().overflowPage;
            HBTREE_DEBUG("collapsing one node in root branch" << page->info << "setting root to page" << writeTransaction_->rootPage_);
            q->stats_.depth--;
            removeFromTree(page);
        } else {
            HBTREE_DEBUG("no need to rebalance root page");
        }
        return true;
    }

    HBTREE_ASSERT(parent->nodes.size() >= 2)(*parent);
    HBTREE_ASSERT(parent->info.type == PageInfo::Branch)(*parent);

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
        destNode = page->nodes.size() ? page->nodes.constEnd() - 1 : page->nodes.constBegin();
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

    if (pageFill(neighbour) > spec_.pageFillThreshold && neighbour->nodes.size() > 2
            && hasSpaceFor(page, sourceNode.key(), sourceNode.value())) {

        HBTREE_DEBUG("moving" << sourceNode << "from" << neighbour->info << "with page fill" << pageFill(neighbour) << "and fill threshold" << spec_.pageFillThreshold);

        bool canUpdate = true;
        if (neighbourBranchNode != neighbour->parent->nodes.constBegin()
                && sourceNode == neighbour->nodes.constBegin()) {
            // key in neighbour's parent can change
            // make sure sourceNode.key can fit in neighbour parent
            if (sourceNode.key().data.size() >
                    neighbour->parentKey.data.size()) {
                quint16 diff = sourceNode.key().data.size() - neighbour->parentKey.data.size();
                if (diff > spaceLeft(neighbour->parent)) {
                    HBTREE_DEBUG("not enough space in neighbour parent for new key - diff:" << diff << ", space:" << spaceLeft(neighbour->parent));
                    canUpdate = false;
                }
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
                if (diff > spaceLeft(page->parent)) {
                    HBTREE_DEBUG("not enough space in page parent for new key - diff:" << diff << ", space:" << spaceLeft(page->parent));
                    canUpdate = false;
                }
            }
        }

        if (canUpdate && !moveNode(neighbour, page, sourceNode)) {
            return false;
        }
    } else {
        // Account for transfer of history
        // Account for extra history node in dst page in the case of touch page.
        HBTREE_DEBUG("merging" << neighbour->info << "and" << page->info);
        if (pageBranchNode == parent->nodes.constBegin() && spaceLeft(page) >= (spaceUsed(neighbour) + sizeof(HistoryNode) * (neighbour->history.size() + 1))) {
            if (!mergePages(neighbour, page)) {
                HBTREE_DEBUG("failed to merge");
                return false;
            }
        } else if (spaceLeft(neighbour) >= (spaceUsed(page) + sizeof(HistoryNode) * (page->history.size() + 1))) {
            if (!mergePages(page, neighbour)) {
                HBTREE_DEBUG("failed to merge");
                return false;
            }
        } else {
            HBTREE_DEBUG("can't merge - page space:" << spaceLeft(page) << ", neighbour space:" << spaceUsed(neighbour));
        }
    }

    return true;
}

// TODO: turn in to transferNode(src, dst) // only with same parents.
bool HBtreePrivate::moveNode(HBtreePrivate::NodePage *src, HBtreePrivate::NodePage *dst, HBtreePrivate::Node node)
{
    HBTREE_ASSERT(src->parent)(node);
    HBTREE_ASSERT(dst->parent)(node);
    HBTREE_ASSERT(dst->parent == src->parent)(*dst->parent)(*src->parent)(node);
    HBTREE_ASSERT(src->info.type == dst->info.type)(*dst->parent)(*src->parent)(node);
    HBTREE_ASSERT(src->parentKey <= node.key())(*dst->parent)(*src->parent)(node)(src->parentKey);

    HBTREE_DEBUG("moving" << node << "from" << src->info << "to" << dst->info);

    src = touchNodePage(src);
    dst = touchNodePage(dst);

    if (!src || !dst)
        return false;

    bool decending = src->parentKey > dst->parentKey;
    HBTREE_DEBUG("moving from" << (decending ? "higher to lower" : "lower to higher"));

    NodeKey nkey = node.key();

    insertNode(dst, node.key(), node.value());
    removeNode(src, node.key(), true);

    if (dst->parentKey > nkey) {
        // must change destination parent key
        HBTREE_DEBUG("changing dst->parent key in" << dst->parent->info << "to" << nkey);
        removeNode(dst->parent, dst->parentKey);
        insertNode(dst->parent, nkey, NodeValue(dst->info.number));
        dst->parentKey = nkey;
    }

    if (src->parentKey <= nkey && decending) {
        HBTREE_ASSERT(!src->parentKey.data.isEmpty());
        HBTREE_DEBUG("changing src->parent key in" << src->parent->info << "to" << nkey);
        // must change source parent key
        removeNode(src->parent, src->parentKey);
        insertNode(src->parent, src->nodes.constBegin().key(), NodeValue(src->info.number));
        src->parentKey = src->nodes.constBegin().key();
    }

    return true;
}

bool HBtreePrivate::mergePages(HBtreePrivate::NodePage *page, HBtreePrivate::NodePage *dst)
{
    HBTREE_ASSERT(dst->parent);
    HBTREE_ASSERT(page->parent);
    HBTREE_ASSERT(page->parent == dst->parent)(*dst->parent)(*page->parent);

    HBTREE_DEBUG("merging" << page->info << "in to" << dst->info);

    // No need to touch page since only dst is changing.
    dst = touchNodePage(dst);

    Node it = page->nodes.constBegin();
    while (it != page->nodes.constEnd()) {
        insertNode(dst, it.key(), it.value());
        ++it;
    }

    if (page->parentKey > dst->parentKey) {
        // Merging a page with larger key values in to a page
        // with smaller key values. Just delete the key to
        // greater page form parent
        HBTREE_DEBUG("merging from higher to lower. Parent key unchanged");
        removeNode(dst->parent, page->parentKey);

        // right page becomes insignificant, change left/right siblings
        if (page->rightPageNumber != PageInfo::INVALID_PAGE) {
            NodePage *right = static_cast<NodePage *>(cacheFind(page->rightPageNumber));
            if (right) {
                right->leftPageNumber = dst->info.number;
                dst->rightPageNumber = right->info.number;
            }
        }
    } else {
        // Merging page with smaller keys in to page
        // with bigger keys. Change dst parent key.
        HBTREE_DEBUG("merging from lower to higher. Changing parent key in" << dst->parent->info << "to" << page->parentKey << "->" << dst->info.number);
        removeNode(dst->parent, dst->parentKey, true);
        removeNode(page->parent, page->parentKey);
        NodeKey nkey = page->parentKey;
        NodeValue nval;
        nval.overflowPage = dst->info.number;
        insertNode(dst->parent, nkey, nval);
        dst->parentKey = page->parentKey;


        // left page becomes insignificant, change left/right siblings
        if (page->leftPageNumber != PageInfo::INVALID_PAGE) {
            NodePage *left = static_cast<NodePage *>(cacheFind(page->leftPageNumber));
            if (left) {
                left->rightPageNumber = dst->info.number;
                dst->leftPageNumber = left->info.number;
            }
        }
    }

    // Overflow pages may have been transfered over
    if (page->meta.flags & NodeHeader::Overflow)
        dst->meta.flags |= NodeHeader::Overflow;

    removeFromTree(page);
    return rebalance(dst->parent);
}

bool HBtreePrivate::addHistoryNode(HBtreePrivate::NodePage *src, const HBtreePrivate::HistoryNode &hn)
{
    if (src) {
        HBTREE_DEBUG("adding history" << hn << "to" << src->info);
        if (spaceLeft(src) >= sizeof(HistoryNode)) {
            src->history.prepend(hn);
            src->meta.historySize++;
            return true;
        } else {
            HBTREE_DEBUG("no space. Removing history from" << src->info << "and adding to residue");
            foreach (const HistoryNode &h, src->history)
                residueHistory_.insert(h.pageNumber);
            residueHistory_.insert(hn.pageNumber);
            src->clearHistory();
            return true;
        }
    } else {
        HBTREE_DEBUG("adding history" << hn << "to residue");
        residueHistory_.insert(hn.pageNumber);
    }
    return true;
}

void HBtreePrivate::dump()
{
    qDebug() << "Dumping tree from marker" << marker_;
    if (marker_.meta.root == PageInfo::INVALID_PAGE) {
        qDebug() << "This be empty laddy";
        return;
    }
    NodePage *root = static_cast<NodePage *>(getPage(marker_.meta.root));
    dumpPage(root, 0);
}

void HBtreePrivate::dump(HBtreeTransaction *transaction)
{
    qDebug() << "Dumping tree from transaction @" << transaction;
    if (transaction->rootPage_ == PageInfo::INVALID_PAGE) {
        qDebug() << "This be empty laddy";
        return;
    }
    NodePage *root = static_cast<NodePage *>(getPage(transaction->rootPage_));
    dumpPage(root, 0);
}

void HBtreePrivate::dumpPage(HBtreePrivate::NodePage *page, int depth)
{
    QByteArray tabs(depth, '\t');
    qDebug() << tabs << page->info;
    qDebug() << tabs << page->meta;
    qDebug() << tabs << page->history;
    qDebug() << tabs << "right =>" << (page->rightPageNumber == PageInfo::INVALID_PAGE ? "Unavailable" : QString::number(page->rightPageNumber).toLatin1());
    qDebug() << tabs << "left =>" << (page->leftPageNumber == PageInfo::INVALID_PAGE ? "Unavailable" : QString::number(page->leftPageNumber).toLatin1());
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
        HBTREE_ASSERT(0)(page->info).message(QStringLiteral("unknown type"));
    }
}

// ######################################################################
// ### btree cursors
// ######################################################################

bool HBtreePrivate::cursorLast(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut)
{
    HBTREE_ASSERT(cursor);
    HBTREE_DEBUG("getting last node with cursor @" << cursor);
    NodePage *page = 0;
    searchPage(cursor, cursor->transaction_, NodeKey(), SearchLast, false, &page);
    if (page) {
        Node node = page->nodes.constEnd() - 1;
        if (keyOut)
            *keyOut = node.key().data;
        if (valueOut)
            *valueOut = getDataFromNode(node.value());
        return true;
    }
    return false;

}

bool HBtreePrivate::cursorFirst(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut)
{
    HBTREE_ASSERT(cursor);
    HBTREE_DEBUG("getting first node with cursor @" << cursor);
    NodePage *page = 0;
    searchPage(cursor, cursor->transaction_, NodeKey(), SearchFirst, false, &page);
    if (page) {
        Node node = page->nodes.constBegin();
        if (keyOut)
            *keyOut = node.key().data;
        if (valueOut)
            *valueOut = getDataFromNode(node.value());
        return true;
    }
    return false;
}

bool HBtreePrivate::cursorNext(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut)
{
    HBTREE_ASSERT(cursor);
    HBTREE_DEBUG("getting next node with cursor @" << cursor);

    if (!cursor->valid_)
        return cursorFirst(cursor, keyOut, valueOut);

    HBTREE_DEBUG("last key/value - " << NodeKey(compareFunction_, cursor->key_) << NodeValue(cursor->value_) << ". last leaf -" << cursor->lastLeaf_);

    NodeKey nkey(compareFunction_, cursor->key_);
    NodePage *page = 0;
    Node node;
    bool ok = false;
    bool checkRight = false;

    if (cursor->lastLeaf_ != PageInfo::INVALID_PAGE && !cursorDisrupted_) {
        page = static_cast<NodePage *>(getPage(cursor->lastLeaf_));
        if (page) {
            node = page->nodes.lowerBound(nkey);
            if (node != page->nodes.constEnd()) {
                if (node.key() > nkey) {
                    ok = true;
                } else if (++node != page->nodes.constEnd()) {
                    ok = true;
                } else {
                    page = 0;
                    checkRight = true;
                }
            } else {
                page = 0;
            }
        }
    }

    if (!page && !searchPage(cursor, cursor->transaction_, nkey, SearchKey, false, &page))
        return false;

    if (!checkRight && !ok) {
        node = page->nodes.lowerBound(nkey);
        checkRight = (node == page->nodes.constEnd() || node.key() == nkey) &&
                (node == page->nodes.constEnd() || ++node == page->nodes.constEnd());
        ok = !checkRight;
    }

    // Could've been deleted so check if node == end.
    if (checkRight && !ok) {
        cursor->lastLeaf_ = PageInfo::INVALID_PAGE;
        HBTREE_DEBUG("moving right from" << page->info << "to" << page->rightPageNumber);
        if (page->rightPageNumber != PageInfo::INVALID_PAGE) {
            NodePage *right = static_cast<NodePage *>(getPage(page->rightPageNumber));
            node = right->nodes.constBegin();
            if (node != right->nodes.constEnd()) {
                ok = true;
            } else {
                // This should never happen if rebalancing is working properly
                HBTREE_ASSERT(0)(*right)(node)(*page).message(QStringLiteral("what up?"));
            }
        }
    }

    if (ok) {
        if (keyOut)
            *keyOut = node.key().data;
        if (valueOut)
            *valueOut = getDataFromNode(node.value());
    }

    return ok;
}

bool HBtreePrivate::cursorPrev(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut)
{
    HBTREE_ASSERT(cursor);
    HBTREE_DEBUG("getting previous node with cursor @" << cursor);

    if (!cursor->valid_)
        return cursorLast(cursor, keyOut, valueOut);

    HBTREE_DEBUG("last key/value was - " << cursor->key_ << cursor->value_);

    NodeKey nkey(compareFunction_, cursor->key_);
    NodePage *page = 0;
    Node node;
    bool ok = false;
    bool checkLeft = false;

    if (cursor->lastLeaf_ != PageInfo::INVALID_PAGE && !cursorDisrupted_) {
        page = static_cast<NodePage *>(getPage(cursor->lastLeaf_));
        if (page) {
            node = page->nodes.lowerBound(nkey);
            if (node != page->nodes.constBegin()) {
                if (node.key() < nkey) {
                    ok = true;
                } else {
                    --node;
                    ok = true;
                }
            } else {
                if (node.key() < nkey) {
                    ok = true;
                } else {
                    page = 0;
                    checkLeft = true;
                }
            }
        }
    }

    if (!page && !searchPage(cursor, cursor->transaction_, nkey, SearchKey, false, &page))
        return false;

    if (!ok && !checkLeft) {
        node = page->nodes.lowerBound(nkey);
        checkLeft = node == page->nodes.constBegin() && node.key() >= nkey;
        ok = !checkLeft;
        if (!checkLeft)
            --node;
    }

    if (checkLeft) {
        HBTREE_DEBUG("moving left from" << page->info << "to" << page->leftPageNumber);
        cursor->lastLeaf_ = PageInfo::INVALID_PAGE;
        if (page->leftPageNumber != PageInfo::INVALID_PAGE) {
            NodePage *left = static_cast<NodePage *>(getPage(page->leftPageNumber));
            if (left->nodes.size() > 1)
                node = left->nodes.constEnd() - 1;
            else
                node = left->nodes.constBegin();
            if (node != left->nodes.constEnd()) {
                ok = true;
            } else {
                // This should never happen if rebalancing is working properly
                HBTREE_ASSERT(0)(*left)(node)(*page).message(QStringLiteral("what up?"));
                ok = false;
            }
        }
    }

    if (ok) {
        if (keyOut)
            *keyOut = node.key().data;
        if (valueOut)
            *valueOut = getDataFromNode(node.value());
        return true;
    }

    return false;
}

bool HBtreePrivate::cursorSet(HBtreeCursor *cursor, QByteArray *keyOut, QByteArray *valueOut, const QByteArray &matchKey, bool exact, HBtreeCursor::RangePolicy policy)
{
    HBTREE_ASSERT(cursor)(matchKey)(exact);
    HBTREE_DEBUG("searching for" << (exact ? "exactly" : "") << matchKey);

    QByteArray keyData, valueData;
    NodeKey nkey(compareFunction_, matchKey);

    NodePage *page = 0;
    if (!searchPage(exact ? NULL : cursor, cursor->transaction_, nkey, SearchKey, false, &page))
        return false;

    bool ok = false;
    if (page->nodes.contains(nkey)) {
        keyData = nkey.data;
        valueData = getDataFromNode(page->nodes.value(nkey));
        ok = true;
    } else if (!exact && policy == HBtreeCursor::EqualOrGreater) {
        Node node = page->nodes.lowerBound(nkey);
        if (node != page->nodes.constEnd()) { // if found key equal or greater than, return
            keyData = node.key().data;
            valueData = getDataFromNode(node.value());
            ok = true;
        } else { // check sibling
            HBTREE_DEBUG("reached end. Moving right from" << page->info << "to" << page->rightPageNumber);
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
    } else if (!exact && policy == HBtreeCursor::EqualOrLess) {
        Node node = page->nodes.lowerBound(nkey);
        HBTREE_ASSERT(node == page->nodes.constEnd() || node.key() > nkey);
        if (node == page->nodes.constBegin()) {
            if (page->leftPageNumber != PageInfo::INVALID_PAGE) {
                NodePage *left = static_cast<NodePage *>(getPage(page->leftPageNumber));
                if (left->nodes.size() > 1)
                    node = left->nodes.constEnd() - 1;
                else
                    node = left->nodes.constBegin();
                if (node != left->nodes.constEnd()) {
                    keyData = node.key().data;
                    valueData = getDataFromNode(node.value());
                    ok = true;
                } else {
                    // This should never happen if rebalancing is working properly
                    HBTREE_ASSERT(0)(*left)(node)(*page).message(QStringLiteral("what up?"));
                    ok = false;
                }
            }
        } else {
            --node;
            keyData = node.key().data;
            valueData = getDataFromNode(node.value());
            ok = true;
        }
    }

    if (keyOut)
        *keyOut = keyData;
    if (valueOut)
        *valueOut = valueData;

    return ok;
}

bool HBtreePrivate::doCursorOp(HBtreeCursor *cursor, HBtreeCursor::Op op, const QByteArray &key, HBtreeCursor::RangePolicy policy)
{
    bool ok = false;
    QByteArray keyOut, valueOut;

    if (cursor->valid_ == false)
        cursor->lastLeaf_ = PageInfo::INVALID_PAGE;

    switch (op) {
    case HBtreeCursor::ExactMatch:
        ok = cursorSet(cursor, &keyOut, &valueOut, key, true, policy);
        break;
    case HBtreeCursor::FuzzyMatch:
        ok = cursorSet(cursor, &keyOut, &valueOut, key, false, policy);
        break;
    case HBtreeCursor::Next:
        ok = cursorNext(cursor, &keyOut, &valueOut);
        break;
    case HBtreeCursor::Previous:
        ok = cursorPrev(cursor, &keyOut, &valueOut);
        break;
    case HBtreeCursor::First:
        ok = cursorFirst(cursor, &keyOut, &valueOut);
        break;
    case HBtreeCursor::Last:
        ok = cursorLast(cursor, &keyOut, &valueOut);
        break;
    default:
        HBTREE_ASSERT(0)(op)(key).message(QStringLiteral("Not a valid cursor op"));
        ok = false;
    }

    cursor->valid_ = ok;

    if (!ok) {
        cursor->key_ = QByteArray();
        cursor->value_ = QByteArray();
    } else {
        cursor->key_ = keyOut;
        cursor->value_ = valueOut;
    }

    cachePrune();
    return ok;
}

void HBtreePrivate::copy(const Page &src, Page *dst)
{
    HBTREE_ASSERT(dst);
    HBTREE_ASSERT(src.info.type == dst->info.type)(dst->info)(src.info);
    quint32 pgno = dst->info.number;
    switch (src.info.type) {
    case PageInfo::Branch:
    case PageInfo::Leaf: {
        NodePage *npDst = static_cast<NodePage *>(dst);
        const NodePage &npSrc = static_cast<const NodePage &>(src);
        bool collected = npDst->collected;
        *npDst = npSrc;
        npDst->collected = collected;
        }
        break;
    case PageInfo::Marker:
        *static_cast<MarkerPage *>(dst) = static_cast<const MarkerPage &>(src);
        break;
    default:
        HBTREE_ASSERT(0).message(QStringLiteral("what are you doing bub?"));
        return;
    }
    dst->info.number = pgno;
}

#define CHECK_TRUE_X(expr, vars) do {if (!(expr)) {HBTREE_ASSERT((expr)) vars.ignore(); return false;}} while (0)
bool HBtreePrivate::verifyIntegrity(const HBtreePrivate::Page *pPage) const
{
    CHECK_TRUE_X(pPage, (pPage));
    const HBtreePrivate::Page &page = *pPage;
    HBTREE_VERBOSE("verifying" << page);
    CHECK_TRUE_X(page.info.type > 0 && page.info.type < PageInfo::Unknown, (page.info.type));
    CHECK_TRUE_X(page.info.number != PageInfo::INVALID_PAGE, (page.info.number));
    if (page.info.type != PageInfo::Marker || marker_.meta.syncId == lastSyncedId_) {
        // These checks are only valid for a marker on sync.
        CHECK_TRUE_X(capacity(&page) >= (page.info.upperOffset + page.info.lowerOffset), (capacity(&page)));
        CHECK_TRUE_X(spaceLeft(&page) <= capacity(&page), (spaceLeft(&page))(capacity(&page)));
        CHECK_TRUE_X(spaceUsed(&page) <= capacity(&page), (spaceUsed(&page))(capacity(&page)));
        CHECK_TRUE_X(((spaceUsed(&page) + spaceLeft(&page)) == capacity(&page)), (spaceUsed(&page))(spaceLeft(&page))(capacity(&page)));
    }
    if (page.info.type == PageInfo::Marker) {
        const MarkerPage &mp = static_cast<const MarkerPage &>(page);
        CHECK_TRUE_X(mp.info.number == marker_.info.number, (marker_.info.number)(mp.info.number));
        CHECK_TRUE_X(mp.info.upperOffset == mp.residueHistory.size() * sizeof(quint32), (mp.residueHistory.size())(mp.info.upperOffset / sizeof(quint32)));
        CHECK_TRUE_X(mp.meta.size <= size_, (size_));
        CHECK_TRUE_X(mp.meta.syncId == lastSyncedId_ || mp.meta.syncId == (lastSyncedId_ + 1), (lastSyncedId_));
        //if (mp->meta.syncedRevision == lastSyncedRevision_) // we just synced
        //    ;
        //else // we've had a number of revisions since last sync
        //    ;
        CHECK_TRUE_X(mp.meta.revision >= lastSyncedId_, (lastSyncedId_));
        if (mp.meta.root != PageInfo::INVALID_PAGE)
            CHECK_TRUE_X(mp.meta.root <= (size_ / spec_.pageSize), (spec_)(size_));
    } else if (page.info.type == PageInfo::Leaf || page.info.type == PageInfo::Branch) {
        const NodePage &np = static_cast<const NodePage &>(page);
        CHECK_TRUE_X(np.history.size() == np.meta.historySize, (np.meta.historySize)(np.history.size()));
        CHECK_TRUE_X((np.info.lowerOffset / 2) == np.nodes.size(), (np.info.lowerOffset)(np.nodes.size()));
        if (np.dirty) {
            CHECK_TRUE_X(np.meta.syncId == (lastSyncedId_ + 1), (lastSyncedId_)(np.meta.syncId));
        } else {
            CHECK_TRUE_X(np.meta.syncId <= marker_.meta.syncId, (marker_)(np.meta.syncId));
        }

        Node it = np.nodes.constBegin();

        if (np.parent) {
//            Node node = np->parent->nodes.find(np->parentKey);
//            CHECK_TRUE(node != np->parent->nodes.constEnd());
//            CHECK_TRUE(np->nodes.constBegin().key() >= node.key());
        }
        while (it != np.nodes.constEnd()) {
            CHECK_TRUE_X(it.key().compareFunction == compareFunction_, (it.key().compareFunction)(compareFunction_));
            if (np.parent)
                CHECK_TRUE_X(it.key() >= np.parentKey, (it)(*np.parent));
            if (page.info.type == PageInfo::Leaf) {
                CHECK_TRUE_X(it.key().data.size() > 0, (it));
                if (it.value().flags & NodeHeader::Overflow) {
                    CHECK_TRUE_X(it.value().data.size() == 0, (it)(np));
                    CHECK_TRUE_X(it.value().overflowPage != PageInfo::INVALID_PAGE, (it));
                } else {
                    CHECK_TRUE_X(it.value().overflowPage == PageInfo::INVALID_PAGE, (it));
                }
            } else {
                // branch page key data size may be 0 (empty key)
                CHECK_TRUE_X(!(it.value().flags & NodeHeader::Overflow), (it));
                CHECK_TRUE_X(it.value().overflowPage != PageInfo::INVALID_PAGE, (it));
                CHECK_TRUE_X(it.value().data.size() == 0, (it));
            }
            ++it;
        }
    } else if (page.info.type == PageInfo::Overflow) {
        const OverflowPage &op = static_cast<const OverflowPage &>(page);
        if (op.nextPage != PageInfo::INVALID_PAGE)
            CHECK_TRUE_X(op.data.size() == capacity(&page), (op.data.size())(capacity(&page)));
        else
            CHECK_TRUE_X(op.data.size() <= capacity(&page), (op.data.size())(capacity(&page)));
    } else
        CHECK_TRUE_X(false, (false));
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
    Q_D(HBtree);
    d->cacheSize_ = size;
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
    int fd = ::open(d->fileName_.toLatin1(), oflags, 0644);

    if (fd == -1) {
        d->lastErrorMessage_ = QString(QLatin1String("failed to open file. Error = %1. Filename = %2"))
                                       .arg(QLatin1String(strerror(errno))).arg(d->fileName_);
        return false;
    }
    return d->open(fd);
}

void HBtree::close()
{
    Q_D(HBtree);
    d->close();
}

bool HBtree::isOpen() const
{
    Q_D(const HBtree);
    return d->fd_ != -1;
}

int HBtree::lastWriteError() const
{
    Q_D(const HBtree);
    return d->lastWriteError_;
}

int HBtree::lastReadError() const
{
    Q_D(const HBtree);
    return d->lastReadError_;
}

size_t HBtree::size() const
{
    Q_D(const HBtree);
    return d->size_;
}

bool HBtree::sync()
{
    Q_D(HBtree);
    return d->sync();
}

bool HBtree::rollback()
{
    Q_D(HBtree);
    return d->rollback();
}

bool HBtree::clearData()
{
    Q_D(HBtree);
    d->close(false);
    if (QFile::exists(d->fileName_))
        QFile::remove(d->fileName_);
    return open();
}

HBtreeTransaction *HBtree::beginTransaction(HBtreeTransaction::Type type)
{
    Q_D(HBtree);
    return d->beginTransaction(type);
}

quint32 HBtree::tag() const
{
    const Q_D(HBtree);
    return (quint32)d->marker_.meta.tag;
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
    const Q_D(HBtree);
    return d->lastErrorMessage_;
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

bool HBtree::doCursorOp(HBtreeCursor *cursor, HBtreeCursor::Op op, const QByteArray &key, HBtreeCursor::RangePolicy policy)
{
    Q_D(HBtree);
    return d->doCursorOp(cursor, op, key, policy);
}

// ######################################################################
// ### Output streams
// ######################################################################


QDebug operator << (QDebug dbg, const HBtreePrivate::Spec &p)
{
    dbg.nospace() << "["
                  << "keySize:" << p.keySize
                  << ", "
                  << "overflowThreshold:" << p.overflowThreshold
                  << ", "
                  << "pageFillThreshold:" << p.pageFillThreshold
                  << ", "
                  << "pageSize:" << p.pageSize
                  << ", "
                  << "version:" << p.version
                  << "]";
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::PageInfo &pi)
{
    QString pageStr;
    switch (pi.type) {
    case HBtreePrivate::PageInfo::Branch:
        pageStr = QStringLiteral("Branch");
        break;
    case HBtreePrivate::PageInfo::Marker:
        pageStr = QStringLiteral("Marker");
        break;
    case HBtreePrivate::PageInfo::Leaf:
        pageStr = QStringLiteral("Leaf");
        break;
    case HBtreePrivate::PageInfo::Spec:
        pageStr = QStringLiteral("Spec");
        break;
    case HBtreePrivate::PageInfo::Overflow:
        pageStr = QStringLiteral("Overflow");
        break;
    default:
        pageStr = QStringLiteral("Unknown");
        break;
    }

    dbg.nospace() << pageStr << " " << pi.number << " ["
              << "crc:" << pi.checksum
              << ", "
              << "offsets:[" << pi.lowerOffset
              << ", "
              << pi.upperOffset
              << "]]";
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::Page &page)
{
    dbg.nospace() << page.info << " - dirty:" << page.dirty;
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::MarkerPage &p)
{
    dbg.nospace() << p.info;
    dbg.nospace() << " meta => ["
                  << "root:" << (p.meta.root == HBtreePrivate::PageInfo::INVALID_PAGE
                                 ? QStringLiteral("Invalid") : QString::number(p.meta.root))
                  << ", "
                  << "commitId:" << p.meta.revision
                  << ", "
                  << "syncId:" << p.meta.syncId
                  << ", "
                  << "tag:" << p.meta.tag
                  << ", "
                  << "size:" << p.meta.size
                  << ", "
                  << "flags:" << p.meta.flags
                  << "]";
    dbg.nospace() << " residue => " << p.residueHistory;

    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodeKey &k)
{
    QByteArray data = k.data;
#if !HBTREE_VERBOSE_OUTPUT
    if (data.size() > 10)
        data = data.left(6).append("...").append(data.right(4));
#endif
    if (data.size())
        dbg.nospace() << data;
    else
        dbg.nospace() << "{empty}";
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodeValue &value)
{
    if (value.flags & HBtreePrivate::NodeHeader::Overflow)
        dbg.nospace() << "overflow:" << value.overflowPage;
    else if (value.overflowPage != HBtreePrivate::PageInfo::INVALID_PAGE)
        dbg.nospace() << "page:" << value.overflowPage;
    else {
        QByteArray data = value.data;
#if !HBTREE_VERBOSE_OUTPUT
        if (data.size() > 10)
            data = data.left(6).append("...").append(data.right(4));
#endif
        if (data.size())
            dbg.nospace() << data;
        else
            dbg.nospace() << "{empty}";
    }
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodePage::Meta &m)
{
     dbg.nospace() << "["
                   << "syncNumber:" << m.syncId
                   << ", "
                   << "comitNumber:" << m.commitId
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
        dbg.nospace() << " parent => [info:" << p.parent->info  << ", key:" << p.parentKey << "]";
    dbg.nospace() << " meta => " << p.meta;
#if HBTREE_VERBOSE_OUTPUT
    dbg.nospace() << " history => " << p.history;
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
    dbg.nospace() << " with [nextPage:" << (p.nextPage == HBtreePrivate::PageInfo::INVALID_PAGE ? "None" : QByteArray::number(p.nextPage));
    dbg.nospace() << ", dataSize:" << p.data.size() << "]";
#if HBTREE_VERBOSE_OUTPUT
    dbg.nospace() << "\n\tdata => " << p.data;
#endif
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::NodeHeader &n)
{
    dbg.nospace() << "(keySize:" << n.keySize;
    if (n.flags & HBtreePrivate::NodeHeader::Overflow)
        dbg.nospace() << ", overflow:" << n.context.overflowPage;
    else
        dbg.nospace() << ", dataSize:" << n.context.valueSize;
    dbg.nospace() << ")";
    return dbg.space();
}

QDebug operator << (QDebug dbg, const HBtreePrivate::HistoryNode &hn)
{
    dbg.nospace() << "(page:" << hn.pageNumber << ", syncId:" << hn.syncId << ", commitId:" << hn.commitId << ")";
    return dbg.space();
}

HBtreePrivate::HistoryNode::HistoryNode(HBtreePrivate::NodePage *np)
    : pageNumber(np->info.number), syncId(np->meta.syncId), commitId(np->meta.commitId)
{
}


void HBtreePrivate::NodePage::clearHistory()
{
    history.clear();
    meta.historySize = 0;
}
