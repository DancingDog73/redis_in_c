#include <iostream>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <set>
#include "avl.h"

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})



struct Data {
    AVLNode node;
    uint32_t val = 0;

};

struct Container {
    AVLNode *root = NULL;
};

static void  add(Container &c, uint32_t val){
    Data *data = new Data();
    avl_init(&data->node);
    data->val = val;

    AVLNode *cur = NULL;
    AVLNode **from = &c.root;
    while(*from){
        cur = *from;
        uint32_t node_val = container_of(cur, Data, node)->val;
        from = (val < node_val) ? &cur->left : &cur->right;
    }
    *from = &data->node;
    data->node.parent = cur;
    c.root = avl_fix(&data->node);
}

static bool del(Container &c, uint32_t val){
    AVLNode *cur = c.root;
    while(cur){
        uint32_t node_val = container_of(cur, Data, node)->val;
        if(val == node_val){
            break;
        }
        cur = val < node_val ? cur->left : cur->right;
    }

    if(!cur){
        return false;
    }

    c.root = avl_del(cur);
    delete container_of(cur, Data, node);
    return true;
}

static void avl_verify(AVLNode *parent, AVLNode *node){
    if(!node){
        return;
    }

    assert(node->parent == parent);
    avl_verify(node, node->left);
    avl_verify(node, node->right);

    assert(node->cnt == 1 + avl_cnt(node->left) + avl_cnt(node->right));

    uint32_t l = avl_height(node->left);
    uint32_t r = avl_height(node->right);
    assert(l == r || l + 1 == r || l == r + 1);
    assert(node->height == 1 + std::max(l, r));

    uint32_t val = container_of(node, Data, node)->val;
    if(node->left){
        assert(node->left->parent == node);
        assert(container_of(node->left, Data, node)->val >= val);
    }
    if(node->right){
        assert(node->right->parent == node);
        assert(container_of(node->right, Data, node)->val >= val);
    }
    
}


int main(){

    std::cout << "My name is Zero sir !\n";
    return 0;
}