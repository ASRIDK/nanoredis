#include "net.h"

#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nanoredis {

void die(const char *msg) {
    std::fprintf(stderr, "[%d] %s: %s\n", errno, msg, std::strerror(errno));
    std::abort();
}

void log_errno(const char *msg) {
    std::fprintf(stderr, "%s: %s\n", msg, std::strerror(errno));
}

bool parse_port(const char *text, uint16_t &out) {
    char *end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0' || value < 1 || value > 65535) {
        return false;
    }
    out = static_cast<uint16_t>(value);
    return true;
}

int read_full(int fd, uint8_t *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv < 0 && errno == EINTR) {
            continue;
        }
        if (rv <= 0) {
            return -1;
        }
        n -= static_cast<size_t>(rv);
        buf += rv;
    }
    return 0;
}

int write_all(int fd, const uint8_t *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv < 0 && errno == EINTR) {
            continue;
        }
        if (rv <= 0) {
            return -1;
        }
        n -= static_cast<size_t>(rv);
        buf += rv;
    }
    return 0;
}

}  // namespace nanoredis
