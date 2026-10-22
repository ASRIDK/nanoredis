#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "net.h"
#include "protocol.h"

using namespace nanoredis;

static int read_response(int fd) {
    std::vector<uint8_t> rbuf(4);
    if (read_full(fd, rbuf.data(), 4) != 0) {
        return -1;
    }
    uint32_t len = 0;
    std::memcpy(&len, rbuf.data(), 4);
    if (len > kMaxMsg || len < 4) {
        std::fprintf(stderr, "bad response length\n");
        return -1;
    }
    rbuf.resize(4 + len);
    if (read_full(fd, rbuf.data() + 4, len) != 0) {
        return -1;
    }

    uint32_t status = 0;
    std::memcpy(&status, rbuf.data() + 4, 4);
    std::printf("[%u] %.*s\n", status, static_cast<int>(len - 4), rbuf.data() + 8);
    return 0;
}

int main(int argc, char **argv) {
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

    std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<uint8_t> request = encode_request(args);
    if (write_all(fd, request.data(), request.size()) != 0 || read_response(fd) != 0) {
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}
