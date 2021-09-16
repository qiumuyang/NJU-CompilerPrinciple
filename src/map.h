/***************************************************************************
 *
 * Copyright (c) 2016 zkdnfcf, Inc. All Rights Reserved
 * $Id$
 *
 **************************************************************************/

/**
 * @file hash.h
 * @author zk(zkdnfc@163.com)
 * @date 2016/05/31 18:26:01
 * @version $Revision$
 * @brief
 *
 **/

#ifndef _MAP_H
#define _MAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rbtree.h"

struct map {
    struct rb_node node;
    char *key;
    void *val;
};

typedef struct map map_t;
typedef struct rb_root root_t;
typedef struct rb_node rb_node_t;

map_t *get(root_t *root, const char *str);
int put(root_t *root, const char *key, void *val);
map_t *map_first(root_t *tree);
map_t *map_next(rb_node_t *node);
void freeMap(root_t *tree, void (*delval)(void *));

#endif  //_MAP_H