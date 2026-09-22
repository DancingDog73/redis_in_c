#include <assert.h>
#include <stdlib.h>     // calloc(), free()
#include "hashtable.h"


static void h_init(HTab *htab, size_t n){
    assert(n > 0 && ((n - 1) & n) == 0);
    htab->tab = (HNode **)calloc(n, sizeof(HNode *));
    htab->mask = n-1;
    htab->size = 0;
}

static void h_insert(HTab *htab, HNode *node){
    size_t pos = node->hcode & htab->mask;
    HNode *next = htab->tab[pos];
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

int main(){
    return 0;
}