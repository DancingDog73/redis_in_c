#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "zset.h"
#include "common.h"


static ZNode *znode_new(const char *name, size_t len, double score){
    ZNode *node = (ZNode *)malloc(sizeof(ZNode) + len);
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

struct HKey {
    HNode node;
    const char *name = NULL;
    size_t len = 0;
};

static bool hcmp(HNode *node, HNode *key){
    ZNode *znode = container_of(node, ZNode, hmap);
    HKey *hkey = container_of(key, HKey, node);
    if(znode->len != hkey->len){
        return false;
    }

    return 0 == memcmp(znode->name, hkey->name, znode->len);
}

ZNode *zset_lookup(ZSet *zset, const char *name, size_t len){
    if(!zset->root){
        return NULL;
    }

    HKey key;
    key.node.hcode = str_hash((uint8_t *)name, len);
    key.name = name;
    key.len = len;
    HNode *found = hm_lookup(&zset->hmap, &key.node, &hcmp);
    return found ? container_of(found, ZNode, hmap) : NULL;
}

static size_t min(size_t lhs, size_t rhs) {
    return lhs < rhs ? lhs : rhs;
}



static bool zless(AVLNode *lhs, AVLNode *rhs){
    ZNode *zl = container_of(lhs, ZNode, tree);
    ZNode *zr = container_of(rhs, ZNode, tree);
    if(zl->score != zr->score){
        return zl->score < zr->score;
    }
    int rv = memcmp(zl->name, zr->name, min(zl->len, zr->len));
    return (rv != 0) ? (rv < 0) : (zl->len < zr->len);
}

static void tree_insert(ZSet *zset, ZNode *node){
    AVLNode *parent = NULL;
    AVLNode **from = &zset->root;
    while(*from){
        parent = *from;
        from = zless(&node->tree, parent) ? &parent->left : &parent->right;
    }
    *from = &node->tree;
    node->tree.parent = parent;
    zset->root = avl_fix(&node->tree);
}


int main(){}

