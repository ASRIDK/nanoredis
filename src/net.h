#pragma once

#include <sys/types.h>

#include <cstddef>
#include <cstdint>

namespace nanoredis {

constexpr uint16_t kDefaultPort = 1234;
constexpr size_t kMaxMsg = 32 << 20;  // largest message body accepted on the wire (32 MiB)

// Prints `msg` with errno and aborts. Used for unrecoverable setup errors.
[[noreturn]] void die(const char *msg);

// Logs a message with errno to stderr without exiting.
void log_errno(const char *msg);

// Blocking helpers that loop until exactly `n` bytes are transferred.
// Return 0 on success, -1 on error or unexpected EOF.
int read_full(int fd, uint8_t *buf, size_t n);
int write_all(int fd, const uint8_t *buf, size_t n);

}  // namespace nanoredis
