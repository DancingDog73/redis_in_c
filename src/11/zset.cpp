#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "zset.h"
#include "common.h"

static ZNode *znode_new(const char *name, size_t len, double score){
    ZNode *node = (ZNode *)malloc(size_of(ZNode) + len);
    avl_init(&node->tree);
    node->hmap.next = NULL;
    node->hmap.hcode = str_hash((uint8_t *)name, len);
    node->score = score;
    node->len = len;
    memcpy(&node->name[0], name, len);
    return node;
}

static void znode_del(ZNode *node){
    free(node);
}