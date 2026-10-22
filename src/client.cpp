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

namespace {

// Pretty-prints one tagged value. Returns the number of bytes consumed, or -1.
int64_t print_value(const uint8_t *data, size_t size, int depth) {
    if (size < 1) {
        return -1;
    }
    auto indent = [depth] {
        for (int i = 0; i < depth; ++i) {
            std::printf("  ");
        }
    };
    uint32_t u32 = 0;
    switch (data[0]) {
    case TAG_NIL:
        indent();
        std::printf("(nil)\n");
        return 1;
    case TAG_ERR: {
        if (size < 9) {
            return -1;
        }
        uint32_t code = 0;
        std::memcpy(&code, data + 1, 4);
        std::memcpy(&u32, data + 5, 4);
        if (size < 9 + static_cast<size_t>(u32)) {
            return -1;
        }
        indent();
        std::printf("(err) %u %.*s\n", code, static_cast<int>(u32), data + 9);
        return 9 + u32;
    }
    case TAG_STR:
        if (size < 5) {
            return -1;
        }
        std::memcpy(&u32, data + 1, 4);
        if (size < 5 + static_cast<size_t>(u32)) {
            return -1;
        }
        indent();
        std::printf("(str) %.*s\n", static_cast<int>(u32), data + 5);
        return 5 + u32;
    case TAG_INT: {
        if (size < 9) {
            return -1;
        }
        int64_t value = 0;
        std::memcpy(&value, data + 1, 8);
        indent();
        std::printf("(int) %lld\n", static_cast<long long>(value));
        return 9;
    }
    case TAG_DBL: {
        if (size < 9) {
            return -1;
        }
        double value = 0;
        std::memcpy(&value, data + 1, 8);
        indent();
        std::printf("(dbl) %g\n", value);
        return 9;
    }
    case TAG_ARR: {
        if (size < 5) {
            return -1;
        }
        std::memcpy(&u32, data + 1, 4);
        indent();
        std::printf("(arr) len=%u\n", u32);
        size_t consumed = 5;
        for (uint32_t i = 0; i < u32; ++i) {
            int64_t rv = print_value(data + consumed, size - consumed, depth + 1);
            if (rv < 0) {
                return -1;
            }
            consumed += static_cast<size_t>(rv);
        }
        indent();
        std::printf("(arr) end\n");
        return static_cast<int64_t>(consumed);
    }
    default:
        return -1;
    }
}

int read_response(int fd) {
    std::vector<uint8_t> rbuf(4);
    if (read_full(fd, rbuf.data(), 4) != 0) {
        return -1;
    }
    uint32_t len = 0;
    std::memcpy(&len, rbuf.data(), 4);
    if (len > kMaxMsg) {
        std::fprintf(stderr, "response too long\n");
        return -1;
    }
    rbuf.resize(4 + len);
    if (read_full(fd, rbuf.data() + 4, len) != 0) {
        return -1;
    }
    if (print_value(rbuf.data() + 4, len, 0) != static_cast<int64_t>(len)) {
        std::fprintf(stderr, "bad response\n");
        return -1;
    }
    return 0;
}

}  // namespace

int main(int argc, char **argv) {
    uint16_t port = kDefaultPort;
    int first_arg = 1;
    if (argc >= 3 && std::strcmp(argv[1], "-p") == 0) {
        if (!parse_port(argv[2], port)) {
            std::fprintf(stderr, "invalid port: %s\n", argv[2]);
            return 1;
        }
        first_arg = 3;
    }
    if (first_arg >= argc) {
        std::fprintf(stderr, "usage: %s [-p port] command [args...]\n", argv[0]);
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        die("connect()");
    }

    std::vector<std::string> args(argv + first_arg, argv + argc);
    std::vector<uint8_t> request = encode_request(args);
    int rc = write_all(fd, request.data(), request.size()) == 0 && read_response(fd) == 0 ? 0 : 1;
    close(fd);
    return rc;
}
