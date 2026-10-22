#include "net.h"

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

}  // namespace nanoredis
