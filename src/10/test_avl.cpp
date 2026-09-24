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


int main(){

    std::cout << "My name is Zero sir !\n";
    return 0;
}