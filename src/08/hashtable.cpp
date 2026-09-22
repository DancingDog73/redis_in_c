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

static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)){
    if(!htab->tab){
        return NULL;
    }

    size_t pos = key->hcode & htab->mask;
    HNode **from = &htab->tab[pos];
    for(HNode *cur; (cur = *from) != NULL; from = &cur->next){
        if(cur->hcode == key->hcode && eq(cur, key)){
            return from;
        }
    }
    return NULL;
}

static HNode *h_detach(HTab *htab, HNode **from){
    HNode *node = *from;
    *from = node->next;
    htab->size--;
    return node;
}

const size_t k_max_load_factor = 8;

static void hm_trigger_rehashing(HMap *hmap){
    hmap->older = hmap->newer;
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_rehashing(hmap);
    HNode **from = h_lookup(&hmap->newer, key, eq);
    if(!from){
        from = h_lookup(&hmap->older, key, eq);
    }

    return from ? *from : NULL;
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)){
    hm_help_rehashing(hmap);
    if(HNode **from = h_lookup(&hmap->newer, key, eq)){
        return h_detach(&hmap->newer, from);
    }
    if(HNode **from = h_lookup(&hmap->older, key, eq)){
        return h_detach(&hmap->older, from);
    }
    return NULL;
}


void hm_insert(HMap *hmap, HNode *node) {
    if(!hmap->newer.tab){
        h_init(&hmap->newer, 4);
    }
    h_insert(&hmap->newer, node);
    if(!hmap->older.tab){
        size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if(hmap->newer.size >= shreshold){
            hm_trigger_rehashing(hmap);
        }
    }
    hm_help_rehashing(hmap);
}


int main(){
    return 0;
}