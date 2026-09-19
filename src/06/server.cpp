#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <vector>

static void msg(const char *msg){
    fprintf(stderr, "%s\n", msg);
}

static void msg_errno(const char *msg){
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void die(const char *msg){
    fprintf(stderr, "[%d], %s\n", errno, msg);
    abort();
}

struct Conn {
    int fd = -1;

    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};


int main(){    
    printf("My name is Zero sir !\n");

    return 0;
}