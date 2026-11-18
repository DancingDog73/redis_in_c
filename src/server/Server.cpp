#include "Server.hpp"
#include "server_core.hpp"



Server::Server(){
    dlist_init(&idle_list);
    thread_pool_init(&thread_pool, 4);
}

Server::~Server(){

}

HMap* Server::getDb(){
    return &db;
}


std::vector<Conn *>& Server::getConnections(){
    return fd2conn;
}

DList* Server::getIdleList(){
    return &idle_list;
}


std::vector<HeapItem>& Server::getHeap(){
    return heap;
}


TheadPool* Server::getThreadPool(){
    return &thread_pool;
}

Conn* Server::getConnection(int num){
    if(num >= 0 && static_cast<size_t>(num) < fd2conn.size()){
        return fd2conn[num];
    }
    return nullptr;
}

void Server::setConnection(int num, Conn* conn){
    fd2conn[num] = conn;
}

void Server::ensureConnCapacity(size_t capacity){
    if(fd2conn.size() <= capacity){
        fd2conn.resize(capacity + 1);
    }
}