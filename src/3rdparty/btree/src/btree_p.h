/*      $OpenBSD: btree.c,v 1.30 2010/09/01 12:13:21 martinh Exp $ */

/*
 * Copyright (c) 2009, 2010 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef BTREE_P_H
#define BTREE_P_H

#include <sys/types.h>
#include <sys/tree.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <sys/uio.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#if defined(__APPLE__)
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#  define SHA1 CC_SHA1
#else
#  include <openssl/sha.h>
#endif

#include "btree.h"

#define ATTR_PACKED __attribute__((packed))


#define PAGESIZE         4096
#define BT_MINKEYS       4
#define BT_MAGIC         0xB3DBB3DB
#define BT_VERSION       4
#define BT_COMMIT_PAGES  64     /* max number of pages to write in one commit */
#define BT_MAXCACHE_DEF  1024   /* max number of pages to keep in cache  */

#define P_INVALID        0xFFFFFFFF

#define F_ISSET(w, f)    (((w) & (f)) == (f))

typedef uint32_t         pgno_t;
typedef uint16_t         indx_t;

/* There are four page types: meta, index, leaf and overflow.
 * They all share the same page header.
 */
struct page {                           /* represents an on-disk page */
        uint32_t         checksum;
        pgno_t           pgno;          /* page number */
#define P_BRANCH         0x01           /* branch page */
#define P_LEAF           0x02           /* leaf page */
#define P_OVERFLOW       0x04           /* overflow page */
#define P_META           0x08           /* meta page */
#define P_HEAD           0x10           /* header page */
        uint32_t         flags;
#define lower            b.fb.fb_lower
#define upper            b.fb.fb_upper
#define p_next_pgno      b.pb_next_pgno
        union page_bounds {
                struct {
                        indx_t   fb_lower;      /* lower bound of free space */
                        indx_t   fb_upper;      /* upper bound of free space */
                } fb;
                pgno_t           pb_next_pgno;  /* overflow page linked list */
        } b;
        indx_t           ptrs[1];               /* dynamic size */
} ATTR_PACKED;

#define PAGEHDRSZ        offsetof(struct page, ptrs)

#define NUMKEYSP(p)      (((p)->lower - PAGEHDRSZ) >> 1)
#define NUMKEYS(mp)      (((mp)->page->lower - PAGEHDRSZ) >> 1)
#define SIZELEFT(mp)     (indx_t)((mp)->page->upper - (mp)->page->lower)
#define PAGEFILL(bt, mp) (1000 * ((bt)->head.psize - PAGEHDRSZ - SIZELEFT(mp)) / \
                                ((bt)->head.psize - PAGEHDRSZ))
#define IS_LEAF(mp)      F_ISSET((mp)->page->flags, P_LEAF)
#define IS_BRANCH(mp)    F_ISSET((mp)->page->flags, P_BRANCH)
#define IS_OVERFLOW(mp)  F_ISSET((mp)->page->flags, P_OVERFLOW)

struct node {
#define n_pgno           p.np_pgno
#define n_dsize          p.np_dsize
        union {
                pgno_t           np_pgno;       /* child page number */
                uint32_t         np_dsize;      /* leaf data size */
        }                p;
        uint16_t         ksize;                 /* key size */
#define F_BIGDATA        0x01                   /* data put on overflow page */
        uint8_t          flags;
        char             data[1];
} ATTR_PACKED;

#define NODESIZE         offsetof(struct node, data)

#ifdef ENABLE_BIG_KEYS
/*  page header (minus the ptrs field)
    + the size to store an index into the page to the node
    + the node header (minus data field)
    + the the min data size required for internal data keeping (ie overflow page)
    + one index_t and nodesize to make room for at least one other branch node
    + divide by 2, we need at least two keys for splitting to work */
#define MAXKEYSIZE       ((PAGESIZE - (PAGEHDRSZ + (sizeof(indx_t) + NODESIZE + sizeof(pgno_t)) * 2)) / 2)
#else
#define MAXKEYSIZE       255
#endif

#define MAXPFXSIZE       255

#define INDXSIZE(k)      (NODESIZE + ((k) == NULL ? 0 : (k)->size))
#define LEAFSIZE(k, d)   (NODESIZE + (k)->size + (d)->size)
#define NODEPTRP(p, i)   ((struct node *)((char *)(p) + (p)->ptrs[i]))
#define NODEPTR(mp, i)   NODEPTRP((mp)->page, i)
#define NODEKEY(node)    (char *)((node)->data)
#define NODEDATA(node)   (char *)((char *)(node)->data + (node)->ksize)
#define NODEPGNO(node)   ((node)->p.np_pgno)
#define NODEDSZ(node)    ((node)->p.np_dsize)

struct bt_head {                                /* header page content */
        uint32_t         magic;
        uint32_t         version;
        uint32_t         flags;
        uint32_t         psize;                 /* page size */
        uint16_t         ksize;
} ATTR_PACKED;

struct bt_meta {                                /* meta (footer) page content */
#define BT_TOMBSTONE     0x01                   /* file is replaced */
#define BT_MARKER        0x02                   /* flushed with fsync */
        uint32_t         flags;
        pgno_t           pgno;                  /* this metapage page number */
        pgno_t           root;                  /* page number of root page */
        pgno_t           prev_meta;             /* previous meta page number */
        uint64_t         created_at;            /* time_t type */
        uint32_t         branch_pages;
        uint32_t         leaf_pages;
        uint32_t         overflow_pages;
        uint32_t         revisions;
        uint32_t         depth;
        uint64_t         entries;
        uint32_t         tag;
        unsigned char    hash[SHA_DIGEST_LENGTH];
} ATTR_PACKED;

struct btkey {
        size_t           len;
        char             str[MAXPFXSIZE];
};

struct mpage {                                  /* an in-memory cached page */
        RB_ENTRY(mpage)          entry;         /* page cache entry */
        SIMPLEQ_ENTRY(mpage)     next;          /* queue of dirty pages */
        TAILQ_ENTRY(mpage)       lru_next;      /* LRU queue */
        struct mpage            *parent;        /* NULL if root */
        unsigned int             parent_index;  /* keep track of node index */
        struct btkey             prefix;
        struct page             *page;
        pgno_t                   pgno;          /* copy of page->pgno */
        short                    ref;           /* increased by cursors */
        short                    dirty;         /* 1 if on dirty queue */
};
RB_HEAD(page_cache, mpage);
SIMPLEQ_HEAD(dirty_queue, mpage);
TAILQ_HEAD(lru_queue, mpage);

static int               mpage_cmp(struct mpage *a, struct mpage *b);
static struct mpage     *mpage_lookup(struct btree *bt, pgno_t pgno);
static void              mpage_add(struct btree *bt, struct mpage *mp);
static void              mpage_free(struct mpage *mp);
static void              mpage_del(struct btree *bt, struct mpage *mp);
static void              mpage_flush(struct btree *bt);
static struct mpage     *mpage_copy(struct btree *bt, struct mpage *mp);
static void              mpage_prune(struct btree *bt);
static void              mpage_dirty(struct btree *bt, struct mpage *mp);
static struct mpage     *mpage_touch(struct btree *bt, struct mpage *mp);

RB_PROTOTYPE(page_cache, mpage, entry, mpage_cmp);
RB_GENERATE(page_cache, mpage, entry, mpage_cmp);

struct ppage {                                  /* ordered list of pages */
        SLIST_ENTRY(ppage)       entry;
        struct mpage            *mpage;
        unsigned int             ki;            /* cursor index on page */
};
SLIST_HEAD(page_stack, ppage);

#define CURSOR_EMPTY(c)          SLIST_EMPTY(&(c)->stack)
#define CURSOR_TOP(c)            SLIST_FIRST(&(c)->stack)
#define CURSOR_POP(c)            SLIST_REMOVE_HEAD(&(c)->stack, entry)
#define CURSOR_PUSH(c,p)         SLIST_INSERT_HEAD(&(c)->stack, p, entry)

struct cursor {
        struct btree            *bt;
        struct btree_txn        *txn;
        struct page_stack        stack;         /* stack of parent pages */
        short                    initialized;   /* 1 if initialized */
        short                    eof;           /* 1 if end is reached */
};

#define METAHASHLEN      offsetof(struct bt_meta, hash)
#define METADATA(p)      ((bt_meta *)((char *)p + PAGEHDRSZ))

struct btree_txn {
        pgno_t                   root;          /* current / new root page */
        pgno_t                   next_pgno;     /* next unallocated page */
        struct btree            *bt;            /* btree is ref'd */
        struct dirty_queue      *dirty_queue;   /* modified pages */
#define BT_TXN_RDONLY            0x01           /* read-only transaction */
#define BT_TXN_ERROR             0x02           /* an error has occurred */
        unsigned int             flags;
        unsigned int             tag;           /* a tag on which the transaction was initiated */
};

struct btree {
        int                      fd;
        char                    *path;
#define BT_FIXPADDING            0x01           /* internal */
        unsigned int             flags;
        bt_cmp_func              cmp;           /* user compare function */
        struct bt_head           head;
        struct bt_meta           meta;
        struct page_cache       *page_cache;
        struct lru_queue        *lru_queue;
        struct btree_txn        *txn;           /* current write transaction */
        int                      ref;           /* increased by cursors & txn */
        struct btree_stat        stat;
        off_t                    size;          /* current file size */
};

void                     btree_dump_tree(struct btree *bt, pgno_t pgno, int depth);
void                     btree_dump_page_from_memory(struct page *p);
void                     btree_dump(struct btree *bt);

static int               btree_read_page(struct btree *bt, pgno_t pgno,
                            struct page *page);
static struct mpage     *btree_get_mpage(struct btree *bt, pgno_t pgno);
enum SearchType {
        SearchKey=0,
        SearchFirst=1,
        SearchLast=2,
};
static int               btree_search_page_root(struct btree *bt,
                            struct mpage *root, struct btval *key,
                            struct cursor *cursor, enum SearchType searchType, int modify,
                            struct mpage **mpp);
static int               btree_search_page(struct btree *bt,
                            struct btree_txn *txn, struct btval *key,
                            struct cursor *cursor, enum SearchType searchType, int modify,
                            struct mpage **mpp);

static int               btree_write_header(struct btree *bt, int fd);
static int               btree_read_header(struct btree *bt);
static int               btree_is_meta_page(struct btree *bt, struct page *p);
static int               btree_read_meta(struct btree *bt, pgno_t *p_next);
static int               btree_read_meta_with_tag(struct btree *bt, unsigned int tag, pgno_t *p_root);
static int               btree_write_meta(struct btree *bt, pgno_t root,
                                          unsigned int flags, uint32_t tag);
static void              btree_ref(struct btree *bt);

static struct node      *btree_search_node(struct btree *bt, struct mpage *mp,
                            struct btval *key, int *exactp, unsigned int *kip);
static int               btree_add_node(struct btree *bt, struct mpage *mp,
                            indx_t indx, struct btval *key, struct btval *data,
                            pgno_t pgno, uint8_t flags);
static void              btree_del_node(struct btree *bt, struct mpage *mp,
                            indx_t indx);
static int               btree_read_data(struct btree *bt, struct mpage *mp,
                            struct node *leaf, struct btval *data);

static int               btree_rebalance(struct btree *bt, struct mpage *mp);
static int               btree_update_key(struct btree *bt, struct mpage *mp,
                            indx_t indx, struct btval *key);
static int               btree_adjust_prefix(struct btree *bt,
                            struct mpage *src, int delta);
static int               btree_move_node(struct btree *bt, struct mpage *src,
                            indx_t srcindx, struct mpage *dst, indx_t dstindx);
static int               btree_merge(struct btree *bt, struct mpage *src,
                            struct mpage *dst);
static int               btree_split(struct btree *bt, struct mpage **mpp,
                            unsigned int *newindxp, struct btval *newkey,
                            struct btval *newdata, pgno_t newpgno);
static struct mpage     *btree_new_page(struct btree *bt, uint32_t flags);
static int               btree_write_overflow_data(struct btree *bt,
                            struct page *p, struct btval *data);

static void              cursor_pop_page(struct cursor *cursor);
static struct ppage     *cursor_push_page(struct cursor *cursor,
                            struct mpage *mp);

static int               bt_set_key(struct btree *bt, struct mpage *mp,
                            struct node *node, struct btval *key);
static int               btree_sibling(struct cursor *cursor, int move_right, int rightmost);
static int               btree_cursor_next(struct cursor *cursor,
                            struct btval *key, struct btval *data);
static int               btree_cursor_prev(struct cursor *cursor,
                            struct btval *key, struct btval *data);
static int               btree_cursor_set(struct cursor *cursor,
                            struct btval *key, struct btval *data, int *exactp);
static int               btree_cursor_first(struct cursor *cursor,
                            struct btval *key, struct btval *data);

static void              bt_reduce_separator(struct btree *bt, struct node *min,
                            struct btval *sep);
static void              remove_prefix(struct btree *bt, struct btval *key,
                            size_t pfxlen);
static void              expand_prefix(struct btree *bt, struct mpage *mp,
                            indx_t indx, struct btkey *expkey);
static void              concat_prefix(struct btree *bt, char *pfxstr, size_t pfxlen,
                            char *keystr, size_t keylen,char *dest, size_t *size);
static void              common_prefix(struct btree *bt, struct btkey *min,
                            struct btkey *max, struct btkey *pfx);
static void              find_common_prefix(struct btree *bt, struct mpage *mp);

static size_t            bt_leaf_size(struct btree *bt, struct mpage *mp, struct btval *key,
                            struct btval *data);
static int               bt_is_overflow(struct btree *bt, struct mpage *mp, size_t ksize,
                            size_t dsize);
static size_t            bt_branch_size(struct btree *bt, struct btval *key);

static pgno_t            btree_compact_tree(struct btree *bt, pgno_t pgno,
                            struct btree *btc);

static int               memncmp(const void *s1, size_t n1,
                            const void *s2, size_t n2, void *);
static int               memnrcmp(const void *s1, size_t n1,
                            const void *s2, size_t n2, void *);

static uint32_t          calculate_crc32(const char *begin, const char *end);
static uint32_t          calculate_checksum(struct btree *bt, const struct page *p);
static int               verify_checksum(struct btree *bt, const struct page *page);

#endif // BTREE_P_H
