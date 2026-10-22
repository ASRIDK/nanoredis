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
