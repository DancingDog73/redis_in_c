#include <iostream>
#include <assert.h>
#include "avl.h"

static uint32_t max(uint32_t lhs, uint32_t rhs){
    return lhs < rhs ? rhs : lhs;
}

static void avl_update(AVLNode *node){
    node->height = 1 + max(avl_height(node->left), avl_height(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

static AVLNode *rot_left(AVLNode *node){
    AVLNode *parent = node->parent;
    AVLNode *new_node = node->right;
    AVLNode *inner = node->left;

    node->right = inner;
    if(inner){
        inner->parent = node;
    }

    new_node->parent = parent;
    new_node->left = node;
    node->parent = new_node;

    avl_update(node);
    avl_update(new_node);
    return new_node;
}

int main(){

    std::cout << "My name is Zero sir !\n";
    return 0;
}