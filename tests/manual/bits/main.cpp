/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btree.h"

int main(int, char **)
{
    struct btree *bt = btree_open("database.db", BT_NOSYNC, 0644);
    if (!bt) {
        fprintf(stderr, "Couldn't open database\n");
        return 0;
    }
    const struct btree_stat *s = btree_stat(bt);
    fprintf(stderr, "\tentries: %llu\n", s->entries);
    fprintf(stderr, "\tpsize: %d\n", s->psize);
    fprintf(stderr, "\ttag: %d\n", s->tag);

    btval key, value;
    key.free_data = 0;
    if (sizeof(long) == 8)
        key.data = (void *)"foo64";
    else
        key.data = (void *)"foo32";
    key.size = 5;
    key.mp = 0;
    value.free_data = 0;
    if (sizeof(long) == 8)
        value.data = (void *)"bar64";
    else
        value.data = (void *)"bar32";
    value.size = 5;
    value.mp = 0;
    int ret = btree_put(bt, &key, &value, 0);
    if (ret != BT_SUCCESS) {
        fprintf(stderr, "failed to put %s\n", (const char *)key.data);
        return 0;
    }
    btval_reset(&key);
    btval_reset(&value);

    struct cursor *c = btree_cursor_open(bt);
    ret = btree_cursor_get(c, &key, &value, BT_FIRST);
    if (ret != BT_SUCCESS) {
        fprintf(stderr, "failed to seek to first entry\n");
        return 0;
    }
    char *k = strndup((const char *)key.data, key.size);
    char *v = strndup((const char *)value.data, value.size);
    fprintf(stderr, "%s : %s\n", k, v);
    btval_reset(&key);
    btval_reset(&value);
    free(k);
    free(v);

    while (true) {
        int ret = btree_cursor_get(c, &key, &value, BT_NEXT);
        if (ret != BT_SUCCESS)
            break;
        char *k = strndup((const char *)key.data, key.size);
        char *v = strndup((const char *)value.data, value.size);
        fprintf(stderr, "%s : %s\n", k, v);
        btval_reset(&key);
        btval_reset(&value);
        free(k);
        free(v);
    }
    btree_close(bt);
    return 1;
}
