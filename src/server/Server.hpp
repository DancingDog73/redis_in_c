#ifndef _SERVEUR
#define _SERVEUR 

#include <vector>
#include "common.h"
#include "zset.h"
#include "hashtable.h"
#include "list.h"
#include "heap.h"
#include "thread_pool.h"
#include "Clock.hpp"

struct Conn;


class Server{
    private:
        HMap db;
        std::vector<Conn *> fd2conn;
        DList idle_list;
        std::vector<HeapItem> heap;
        TheadPool thread_pool;
        int idleTimeOutMs;
        Clock& clock;

    public:
        Server(uint64_t t, Clock& c);
        virtual ~Server();
        HMap* getDb();
        std::vector<Conn *>& getConnections();
        DList* getIdleList();
        std::vector<HeapItem>& getHeap();
        TheadPool* getThreadPool();
        Conn* getConnection(int num);
        void ensureConnCapacity(size_t capacity);
        void setConnection(int num, Conn* conn);
        uint64_t getIdleTimeOut();
        uint64_t nowMs();




};
#endif