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
#include <iostream>

#include "common.h"
#include "zset.h"
#include "hashtable.h"
#include "list.h"
#include "heap.h"
#include "thread_pool.h"

#include "server_core.hpp"
#include "Server.hpp"

int main(int argc, char* argv[]){    

    int port = 1234;
    if(argc > 1){
        int dm_port = std::stoi(argv[1]);
        if(dm_port > 1023 && dm_port < 49152){
            port = dm_port;
        }
    } else {
        std::cout << "You can specify the port of the app : like this : " << argv[0] << " <port> !\n";
    }
    Server server{};

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(port);
    addr.sin_addr.s_addr = ntohl(0);
    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) { die("bind()"); }

    fd_set_nb(fd);

    rv = listen(fd, SOMAXCONN);
    if(rv){die("listen()");}


    std::vector<struct pollfd> poll_args;
    while(true){
        poll_args.clear();
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);
        for(Conn *conn : server.getConnections()){
            if(!conn){
                continue;
            }

            struct pollfd pfd = {conn->fd, POLLERR, 0};
            if(conn->want_read){
                pfd.events |= POLLIN;
            }
            if(conn->want_write){
                pfd.events |= POLLOUT;
            }
            poll_args.push_back(pfd);
        }

        int32_t timeout_ms = next_timer_ms(server);
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), timeout_ms);
        if(rv < 0 && errno == EINTR){
            continue;
        }
        if(rv < 0){
            die("poll");
        }

        if(poll_args[0].revents){
            handle_accept(fd, server);
        }

        for(size_t i = 1; i < poll_args.size(); i++){
            uint32_t ready = poll_args[i].revents;
            if(ready == 0){
                continue;
            }

            Conn *conn = server.getConnections()[poll_args[i].fd];
            conn->last_active_ms = get_monotonic_msec();
            dlist_detach(&conn->idle_node);
            dlist_insert_before(server.getIdleList(), &conn->idle_node);

            if(ready & POLLIN){
                assert(conn->want_read);
                handle_read(conn, server);
            }
            if(ready & POLLOUT){
                assert(conn->want_write);
                handle_write(conn);
            }

            if((ready & POLLERR) || conn->want_close){
                conn_destroy(conn, server);
            }
        }
        process_timers(server);
    }
    

    return 0;
}