#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "net.h"

using namespace nanoredis;

static int query(int fd, const std::string &text) {
    uint32_t len = static_cast<uint32_t>(text.size());
    if (len > kMaxMsg) {
        return -1;
    }
    std::vector<uint8_t> wbuf(4 + len);
    std::memcpy(wbuf.data(), &len, 4);
    std::memcpy(wbuf.data() + 4, text.data(), len);
    if (write_all(fd, wbuf.data(), wbuf.size()) != 0) {
        return -1;
    }

    uint8_t rbuf[4 + kMaxMsg];
    if (read_full(fd, rbuf, 4) != 0) {
        return -1;
    }
    std::memcpy(&len, rbuf, 4);
    if (len > kMaxMsg || read_full(fd, rbuf + 4, len) != 0) {
        return -1;
    }
    std::printf("server says: %.*s\n", static_cast<int>(len), rbuf + 4);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDefaultPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        die("connect()");
    }

    for (const char *text : {"hello1", "hello2", "hello3"}) {
        if (query(fd, text) != 0) {
            break;
        }
    }
    close(fd);
    return 0;
}
