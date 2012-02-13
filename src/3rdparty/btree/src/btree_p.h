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

#include "btree.h"

#include <QCryptographicHash>
#define SHA_DIGEST_LENGTH 20 /* 160 bits */

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
        QCryptographicHash       *hasher;
};

void                     btree_dump_tree(struct btree *bt, pgno_t pgno, int depth);
void                     btree_dump_page_from_memory(struct page *p);
void                     btree_close_nosync(struct btree *bt);

#endif // BTREE_P_H
