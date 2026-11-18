#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <vector>
#include <string>


#include "server_core.hpp"

GlobalData g_data; 

void msg(const char *msg){
    fprintf(stderr, "%s\n", msg);
}

void msg_errno(const char *msg){
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

void die(const char *msg){
    fprintf(stderr, "[%d], %s\n", errno, msg);
    abort();
}

uint64_t get_monotonic_msec() {
    struct timespec tv = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return uint64_t(tv.tv_sec) * 1000 + tv.tv_nsec / 1000 / 1000;
}

void fd_set_nb(int fd){
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if(errno){
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;
    errno = 0;

    (void)fcntl(fd, F_SETFL, flags);
    if(errno){
        die("fcntl error");
    }
}

const size_t k_max_msg = 32 << 20;




void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len){
    buf.insert(buf.end(), data, data + len);
}

void buf_consume(std::vector<uint8_t> &buf, size_t n){
    buf.erase(buf.begin(), buf.begin() + n);
}

int32_t handle_accept(int fd, Server& server){

    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if(connfd < 0){
        msg_errno("accept() error");
        return -1;
    }

    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
        ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24, 
        ntohs(client_addr.sin_port)     
    );

    fd_set_nb(connfd);

    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;
    conn->last_active_ms = get_monotonic_msec();
    dlist_insert_before(server.getIdleList(), &conn->idle_node);
    
    server.ensureConnCapacity((size_t) conn->fd);
    assert(!server.getConnection(conn->fd));
    
    server.setConnection(conn->fd, conn);
    return 0;
}

void conn_destroy(Conn *conn, Server& server){
    (void)close(conn->fd);
    server.setConnection(conn->fd, NULL);
    dlist_detach(&conn->idle_node);
    delete conn;
}

const size_t k_max_args = 200 * 1000;


Entry *entry_new(uint32_t type){
    Entry *ent = new Entry();
    ent->type = type;
    return ent;
}


void entry_del_sync(Entry *ent){
    if(ent->type == T_ZSET){
        zset_clear(&ent->zset);
    }
    delete ent;
}

void entry_del_func(void *arg){
    entry_del_sync((Entry *)arg);
}

void entry_del(Entry *ent, Server& server){
    entry_set_ttl(ent, -1, server);

    size_t set_size = (ent->type == T_ZSET) ? hm_size(&ent->zset.hmap) : 0;
    const size_t k_large_container_size = 1000;
    if(set_size > k_large_container_size){
        thread_pool_queue(server.getThreadPool(), &entry_del_func, ent);
    } else {
        entry_del_sync(ent);
    }
}


bool entry_eq(HNode *node, HNode *key){
    struct Entry *ent = container_of(node, struct Entry, node);
    struct LookupKey *keydata = container_of(key, struct LookupKey, node);
    return ent->key == keydata->key;
}


typedef std::vector<uint8_t> Buffer;

void buf_append_u8(Buffer &buf, uint8_t data){
    buf.push_back(data);
}

void buf_append_u32(Buffer &buf, uint32_t data){
    buf_append(buf, (const uint8_t *)&data, 4);
}

void buf_append_i64(Buffer &buf, int64_t data){
    buf_append(buf, (const uint8_t *)&data, 8);
}

void buf_append_dbl(Buffer &buf, double data){
    buf_append(buf, (const uint8_t *)&data, 8);
}

void out_nil(Buffer &out){
    buf_append_u8(out, TAG_NIL);
}

void out_str(Buffer &out, const char *s, size_t size){
    buf_append_u8(out, TAG_STR);
    buf_append_u32(out, (uint32_t) size);
    buf_append(out, (const uint8_t *)s, size);
}

void out_int(Buffer &out, int64_t val){
    buf_append_u8(out, TAG_INT);
    buf_append_i64(out, val);
}

void out_dbl(Buffer &out, double val){
    buf_append_u8(out, TAG_DBL);
    buf_append_dbl(out, val);
}

void out_err(Buffer &out, uint32_t code, const std::string &msg){
    buf_append_u8(out, TAG_ERR);
    buf_append_u32(out, code);
    buf_append_u32(out, (uint32_t)msg.size());
    buf_append(out, (const uint8_t *)msg.data(), msg.size());
}

void out_arr(Buffer &out, uint32_t n){
    buf_append_u8(out, TAG_ARR);
    buf_append_u32(out, n);
}

size_t out_begin_arr(Buffer &out){
    out.push_back(TAG_ARR);
    buf_append_u32(out, 0);
    return out.size() - 4;
}

void out_end_arr(Buffer &out, size_t ctx, uint32_t n){
    assert(out[ctx - 1] == TAG_ARR);
    memcpy(&out[ctx], &n, 4);
}

void do_get(std::vector<std::string> &cmd, Buffer &out, Server& server){
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(server.getDb(), &key.node, &entry_eq);
    if(!node ){
        return out_nil(out);
    }

    Entry *ent = container_of(node, Entry, node);
    if(ent->type != T_STR){
        return out_err(out, ERR_BAD_TYP, "not a string value");
    }
    
    return out_str(out, ent->str.data(), ent->str.size());
}

void do_set(std::vector<std::string> &cmd, Buffer &out, Server& server){
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(server.getDb(), &key.node, &entry_eq);
    if(node){
        Entry *ent = container_of(node, Entry, node);
        if(ent->type != T_STR){
            return out_err(out, ERR_BAD_TYP, "a non-string value exists");
        }
        ent->str.swap(cmd[2]);
    }  else {
        Entry *ent = entry_new(T_STR);
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->str.swap(cmd[2]);
        hm_insert(server.getDb(), &ent->node);
    }

    return out_nil(out);

}

void do_del(std::vector<std::string> &cmd, Buffer &out, Server& server){
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_delete(server.getDb(), &key.node, &entry_eq);
    if(node){
        entry_del(container_of(node, Entry, node), server);
    }
    return out_int(out, node ? 1 : 0);
}

bool cb_keys(HNode *node, void *arg){
    Buffer &out = *(Buffer *)arg;
    const std::string &key = container_of(node, Entry, node)->key;
    out_str(out, key.data(), key.size());
    return true;
}

void do_keys(std::vector<std::string> &, Buffer &out, Server& server){
    out_arr(out, (uint32_t)hm_size(server.getDb()));
    hm_foreach(server.getDb(), &cb_keys, (void *)&out);
}

void heap_delete(std::vector<HeapItem> &a, size_t pos) {
    
    a[pos] = a.back();
    a.pop_back();
    
    if (pos < a.size()) {
        heap_update(a.data(), pos, a.size());
    }
}

void heap_upsert(std::vector<HeapItem> &a, size_t pos, HeapItem t) {
    if (pos < a.size()) {
        a[pos] = t;         
    } else {
        pos = a.size();
        a.push_back(t);     
    }
    heap_update(a.data(), pos, a.size());
}

void entry_set_ttl(Entry *ent, int64_t ttl_ms, Server& server) {
    if (ttl_ms < 0 && ent->heap_idx != (size_t)-1) {
        
        heap_delete(server.getHeap(), ent->heap_idx);
        ent->heap_idx = -1;
    } else if (ttl_ms >= 0) {
        
        uint64_t expire_at = get_monotonic_msec() + (uint64_t)ttl_ms;
        HeapItem item = {expire_at, &ent->heap_idx};
        heap_upsert(server.getHeap(), ent->heap_idx, item);
    }
}

bool str2dbl(const std::string &s, double &out){
    char *endp = NULL;
    out = strtod(s.c_str(), &endp);
    return endp == s.c_str() + s.size() && !isnan(out);
}

bool str2int(const std::string &s, int64_t &out){
    char *endp = NULL;
    out = strtoll(s.c_str(), &endp, 10);
    return endp == s.c_str() + s.size();
}

void do_zadd(std::vector<std::string> &cmd, Buffer &out, Server& server){
    double score = 0;
    if(!str2dbl(cmd[2], score)){
        return out_err(out, ERR_BAD_ARG, "expect float");
    }

    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *hnode = hm_lookup(server.getDb(), &key.node, &entry_eq);

    Entry *ent = NULL;
    if(!hnode){
        ent = entry_new(T_ZSET);
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        hm_insert(server.getDb(), &ent->node);
    } else {
        ent = container_of(hnode, Entry, node);
        if(ent->type != T_ZSET){
            return out_err(out, ERR_BAD_TYP, "expect zset");
        }
    }

    const std::string &name = cmd[3];
    bool added = zset_insert(&ent->zset, name.data(), name.size(), score);
    return out_int(out, (int64_t)added);
}

const ZSet k_empty_zset;

ZSet *expect_zset(std::string &s, Server& server){
    LookupKey key;
    key.key.swap(s);
    key.node.hcode = str_hash((uint8_t *) key.key.data(), key.key.size());
    HNode *hnode = hm_lookup(server.getDb(), &key.node, &entry_eq);
    if(!hnode){
        return (ZSet *)&k_empty_zset;
    }
    Entry *ent = container_of(hnode, Entry, node);
    return ent->type == T_ZSET ? &ent->zset : NULL;
}

void do_zrem(std::vector<std::string> &cmd, Buffer &out, Server& server){
    ZSet *zset = expect_zset(cmd[1], server);
    if(!zset){
        return out_err(out, ERR_BAD_TYP, "expect zset");
    }

    const std::string &name = cmd[2];
    ZNode *znode = zset_lookup(zset, name.data(), name.size());
    if(znode){
        zset_delete(zset, znode);
    }
    return out_int(out, znode ? 1 : 0);
}

void do_zscore(std::vector<std::string> &cmd, Buffer &out, Server& server){
    ZSet *zset = expect_zset(cmd[1], server);
    if(!zset){
        return out_err(out, ERR_BAD_TYP, "expect zset");
    }

    const std::string &name = cmd[2];
    ZNode *znode = zset_lookup(zset, name.data(), name.size());
    return znode ? out_dbl(out, znode->score) : out_nil(out); 
}

void do_zquery(std::vector<std::string> &cmd, Buffer &out, Server& server){
    double score = 0;
    if(!str2dbl(cmd[2], score)) {
        return out_err(out, ERR_BAD_ARG, "expect fp nummber");
    }

    const std::string &name = cmd[3];
    int64_t offset = 0, limit = 0;
    if(!str2int(cmd[4], offset) || !str2int(cmd[5], limit)){
        return out_err(out, ERR_BAD_ARG, "expect int");
    }

    ZSet *zset = expect_zset(cmd[1], server);
    if(!zset){
        return out_err(out, ERR_BAD_TYP, "expect zset");
    }

    if(limit <= 0){
        return out_arr(out, 0);
    }
    ZNode *znode = zset_seekge(zset, score, name.data(), name.size());
    znode = znode_offset(znode, offset);


    size_t ctx = out_begin_arr(out);
    int64_t n = 0;
    while(znode && n < limit){
        out_str(out, znode->name, znode->len);
        out_dbl(out, znode->score);
        znode = znode_offset(znode, +1);
        n += 2;
    }
    out_end_arr(out, ctx, (uint32_t)n);
}

void do_expire(std::vector<std::string> &cmd, Buffer &out, Server& server) {
    
    int64_t ttl_ms = 0;
    if (!str2int(cmd[2], ttl_ms)) {
        return out_err(out, ERR_BAD_ARG, "expect int64");
    }
    
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *node = hm_lookup(server.getDb(), &key.node, &entry_eq);
    
    if (node) {
        Entry *ent = container_of(node, Entry, node);
        entry_set_ttl(ent, ttl_ms, server);
    }
    return out_int(out, node ? 1: 0);
}

void do_ttl(std::vector<std::string> &cmd, Buffer &out, Server& server) {
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(server.getDb(), &key.node, &entry_eq);
    if (!node) {
        return out_int(out, -2);    // not found
    }

    Entry *ent = container_of(node, Entry, node);
    if (ent->heap_idx == (size_t)-1) {
        return out_int(out, -1);    // no TTL
    }

    uint64_t expire_at = server.getHeap()[ent->heap_idx].val;
    uint64_t now_ms = get_monotonic_msec();
    return out_int(out, expire_at > now_ms ? (expire_at - now_ms) : 0);
}

bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out){
    if(cur + 4 > end){
        return false;
    }

    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out){
    if(cur + n > end){
        return false;
    }

    out.assign(cur, cur + n);
    cur += n;
    return true;
}

int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out){
    const uint8_t *end = data + size;
    uint32_t nstr = 0;
    if(!read_u32(data, end, nstr)){
        return -1;
    }
    if(nstr > k_max_args){
        return -1;
    }

    while(out.size() < nstr){
        uint32_t len = 0;
        if(!read_u32(data, end, len)){
            return -1;
        }
        out.push_back(std::string());
        if(!read_str(data, end, len, out.back())){
            return -1;
        }
    }

    if(data != end){
        return -1;
    }

    return 0;
}


void do_request(std::vector<std::string> &cmd, Buffer &out, Server& server){
    if(cmd.size() == 2 && cmd[0] == "get"){
       return  do_get(cmd, out, server);
    } else if(cmd.size() == 3 && cmd[0] == "set"){
        return  do_set(cmd, out, server);
    }  else if (cmd.size() == 3 && cmd[0] == "pexpire") {
        return do_expire(cmd, out, server);
    } else if (cmd.size() == 2 && cmd[0] == "pttl") {
        return do_ttl(cmd, out, server);
    } else if(cmd.size() == 2 && cmd[0] == "del"){
        return  do_del(cmd, out, server);
    } else if(cmd.size() == 1 && cmd[0] == "keys"){
        return do_keys(cmd, out, server);
    } else if(cmd.size() == 4 && cmd[0] == "zadd"){
        return do_zadd(cmd, out, server);
    }else if(cmd.size() == 3 && cmd[0] == "zrem"){
        return do_zrem(cmd, out, server);
    }else if(cmd.size() == 3 && cmd[0] == "zscore"){
        return do_zscore(cmd, out, server);
    }else if(cmd.size() == 6 && cmd[0] == "zquery"){
        return do_zquery(cmd, out, server);
    } else {
        return out_err(out, ERR_UNKNOWN, "unknown command.");
    }
}


void response_begin(Buffer &out, size_t *header){
    *header = out.size();
    buf_append_u32(out, 0);
}

size_t response_size(Buffer &out, size_t header){
    return out.size() - header - 4;
}

void response_end(Buffer &out, size_t header){
    size_t msg_size = response_size(out, header);
    if(msg_size > k_max_msg){
        out.resize(header + 4);
        out_err(out, ERR_TOO_BIG, "response is too big.");
        msg_size = response_size(out, header);
    }

    uint32_t len = (uint32_t)msg_size;
    memcpy(&out[header], &len, 4);
}

bool try_one_request(Conn *conn, Server& server){
    if(conn->incoming.size() < 4){
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if(len > k_max_msg){
        msg("too long");
        conn->want_close = true;
        return false;
    }

    if(4 + len > conn->incoming.size()){
        return false;
    }
    const uint8_t *request = &conn->incoming[4];
    std::vector<std::string> cmd;
    if(parse_req(request, len, cmd)){
        conn->want_close = true;
        return false;
    }

    size_t header_pos = 0;
    response_begin(conn->outgoing, &header_pos);
    do_request(cmd, conn->outgoing, server);
    response_end(conn->outgoing, header_pos);

    buf_consume(conn->incoming, 4 + len);

    return true;
}

void handle_write(Conn *conn){
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());

    if(rv < 0 && errno == EAGAIN){
        return;
    }

    if(rv < 0){
        msg_errno("write() error");
        conn->want_close = true;
        return;
    }

    buf_consume(conn->outgoing, (size_t)rv);

    if(conn->outgoing.size() == 0){
        conn->want_read = true;
        conn->want_write = false;
    }
}

void handle_read(Conn *conn, Server& server){
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if(rv < 0 && errno == EAGAIN){
        return;
    }

    if(rv < 0){
        msg_errno("read() error");
        conn->want_close = true;
        return;
    }

    if(rv == 0){
        if(conn->incoming.size() == 0){
            msg("client closed");
        } else {
            msg("unexpected EOF");
        }
        conn->want_close = true;
        return;
    }

    buf_append(conn->incoming, buf, (size_t)rv);
    while(try_one_request(conn, server)){}

    if(conn->outgoing.size() > 0){
        conn->want_read = false;
        conn->want_write = true;

        return handle_write(conn);
    }
}


uint32_t next_timer_ms(Server& server) {
    uint64_t now_ms = get_monotonic_msec();
    uint64_t next_ms = (uint64_t)-1;    
    if (!dlist_empty(server.getIdleList())) {
        Conn *conn = container_of(server.getIdleList()->next, Conn, idle_node);
        next_ms = conn->last_active_ms + server.getIdleTimeOut();
    }
    
    if (!server.getHeap().empty() && server.getHeap()[0].val < next_ms) {
        next_ms = server.getHeap()[0].val;
    }
    
    if (next_ms == (uint64_t)-1) {
        return -1;  
    }
    if (next_ms <= now_ms) {
        return 0;   
    }
    return (int32_t)(next_ms - now_ms);
}

bool hnode_same(HNode *node, HNode *key) {
    return node == key;
}

void process_timers(Server& server) {
    uint64_t now_ms = get_monotonic_msec();
    
    while (!dlist_empty(server.getIdleList())) {
        Conn *conn = container_of(server.getIdleList()->next, Conn, idle_node);
        uint64_t next_ms = conn->last_active_ms + server.getIdleTimeOut();
        if (next_ms >= now_ms) {
            break;  
        }

        fprintf(stderr, "removing idle connection: %d\n", conn->fd);
        conn_destroy(conn, server);
    }
    
    const size_t k_max_works = 2000;
    size_t nworks = 0;
    const std::vector<HeapItem> &heap = server.getHeap();
    while (!heap.empty() && heap[0].val < now_ms) {
        Entry *ent = container_of(heap[0].ref, Entry, heap_idx);
        HNode *node = hm_delete(server.getDb(), &ent->node, &hnode_same);
        assert(node == &ent->node);
        
        entry_del(ent, server);
        if (nworks++ >= k_max_works) {
           
            break;
        }
    }
}