#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include "net.h"

using namespace nanoredis;

static void handle_connection(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        log_errno("read() error");
        return;
    }
    std::printf("client says: %s\n", rbuf);

    const char wbuf[] = "world";
    if (write(connfd, wbuf, std::strlen(wbuf)) < 0) {
        log_errno("write() error");
    }
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDefaultPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        die("bind()");
    }
    if (listen(fd, SOMAXCONN) != 0) {
        die("listen()");
    }

    for (;;) {
        sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd, reinterpret_cast<sockaddr *>(&client_addr), &addrlen);
        if (connfd < 0) {
            continue;
        }
        handle_connection(connfd);
        close(connfd);
    }
}
