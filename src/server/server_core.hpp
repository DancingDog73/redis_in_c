#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <string>

#include "common.h"
#include "zset.h"
#include "hashtable.h"
#include "list.h"
#include "heap.h"
#include "thread_pool.h"
#include "Server.hpp"

typedef std::vector<uint8_t> Buffer;

extern const size_t k_max_msg;
extern const size_t k_max_args;

struct Conn {
    int fd = -1;

    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;

    uint64_t last_active_ms = 0;
    DList idle_node;
};


struct GlobalData{
    HMap db;

    std::vector<Conn *> fd2conn;

    DList idle_list;

    std::vector<HeapItem> heap;

    TheadPool thread_pool;

};


enum {
    ERR_UNKNOWN = 1,
    ERR_TOO_BIG = 2,
    ERR_BAD_TYP = 3,
    ERR_BAD_ARG = 4,
};

enum {
    TAG_NIL = 0,
    TAG_ERR = 1,
    TAG_STR = 2,
    TAG_INT = 3,
    TAG_DBL = 4,
    TAG_ARR = 5,
};

enum {
    T_INIT = 0,
    T_STR = 1,
    T_ZSET = 2,
};



struct Entry {
    struct HNode node;  
    std::string key;

    size_t heap_idx = -1;
    uint32_t type = 0;
    
    std::string str;
    ZSet zset;
};

struct LookupKey {
    struct HNode node;
    std::string key;
};

void msg(const char *msg);
void msg_errno(const char *msg);
void die(const char *msg);
void fd_set_nb(int fd);

void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len);
void buf_consume(std::vector<uint8_t> &buf, size_t n);
void buf_append_u8(Buffer &buf, uint8_t data);
void buf_append_u32(Buffer &buf, uint32_t data);
void buf_append_i64(Buffer &buf, int64_t data);
void buf_append_dbl(Buffer &buf, double data);

int32_t handle_accept(int fd, Server& server);
void conn_destroy(Conn *conn, Server& server);

Entry *entry_new(uint32_t type);
void entry_set_ttl(Entry *ent, int64_t ttl_ms, Server& server);
void entry_del_sync(Entry *ent);
void entry_del_func(void *arg);
void entry_del(Entry *ent, Server& server);
bool entry_eq(HNode *node, HNode *key);

void out_nil(Buffer &out);
void out_str(Buffer &out, const char *s, size_t size);
void out_int(Buffer &out, int64_t val);
void out_dbl(Buffer &out, double val);
void out_err(Buffer &out, uint32_t code, const std::string &msg);
void out_arr(Buffer &out, uint32_t n);
size_t out_begin_arr(Buffer &out);
void out_end_arr(Buffer &out, size_t ctx, uint32_t n);

void do_get(std::vector<std::string> &cmd, Buffer &out, Server& server);
void do_set(std::vector<std::string> &cmd, Buffer &out, Server& server);
void do_del(std::vector<std::string> &cmd, Buffer &out, Server& server);
bool cb_keys(HNode *node, void *arg);
void do_keys(std::vector<std::string> &, Buffer &out, Server& server);

void heap_delete(std::vector<HeapItem> &a, size_t pos);
void heap_upsert(std::vector<HeapItem> &a, size_t pos, HeapItem t);

uint32_t next_timer_ms(Server& server);
void process_timers(Server& server);

void handle_read(Conn *conn, Server& server);
void handle_write(Conn *conn);


void do_request(std::vector<std::string> &cmd, Buffer &out, Server& server);
bool try_one_request(Conn *conn, Server& server);
int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out);