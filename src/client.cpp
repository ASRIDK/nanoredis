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

static int send_request(int fd, const std::string &text) {
    uint32_t len = static_cast<uint32_t>(text.size());
    if (len > kMaxMsg) {
        return -1;
    }
    std::vector<uint8_t> wbuf(4 + len);
    std::memcpy(wbuf.data(), &len, 4);
    std::memcpy(wbuf.data() + 4, text.data(), len);
    return write_all(fd, wbuf.data(), wbuf.size());
}

static int read_response(int fd) {
    uint8_t rbuf[4 + kMaxMsg];
    if (read_full(fd, rbuf, 4) != 0) {
        return -1;
    }
    uint32_t len = 0;
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

    // Pipeline several requests before reading any response.
    const std::vector<std::string> requests = {"hello1", "hello2", "hello3"};
    for (const auto &text : requests) {
        if (send_request(fd, text) != 0) {
            die("write()");
        }
    }
    for (size_t i = 0; i < requests.size(); ++i) {
        if (read_response(fd) != 0) {
            die("read()");
        }
    }
    close(fd);
    return 0;
}
