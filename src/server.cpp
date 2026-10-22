#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include "net.h"

using namespace nanoredis;

// Handles one length-prefixed request: | len (4) | body (len) |, and echoes "world".
static int one_request(int connfd) {
    uint8_t rbuf[4 + kMaxMsg];
    if (read_full(connfd, rbuf, 4) != 0) {
        return -1;
    }

    uint32_t len = 0;
    std::memcpy(&len, rbuf, 4);
    if (len > kMaxMsg) {
        std::fprintf(stderr, "message too long\n");
        return -1;
    }
    if (read_full(connfd, rbuf + 4, len) != 0) {
        return -1;
    }
    std::printf("client says: %.*s\n", static_cast<int>(len), rbuf + 4);

    const char reply[] = "world";
    uint8_t wbuf[4 + sizeof(reply)];
    len = static_cast<uint32_t>(std::strlen(reply));
    std::memcpy(wbuf, &len, 4);
    std::memcpy(wbuf + 4, reply, len);
    return write_all(connfd, wbuf, 4 + len);
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
        // Serve requests on this connection until the client disconnects.
        while (one_request(connfd) == 0) {
        }
        close(connfd);
    }
}
