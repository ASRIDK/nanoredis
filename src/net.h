#pragma once

#include <cstddef>
#include <cstdint>

namespace nanoredis {

constexpr uint16_t kDefaultPort = 1234;

// Prints `msg` with errno and aborts. Used for unrecoverable setup errors.
[[noreturn]] void die(const char *msg);

// Logs a message with errno to stderr without exiting.
void log_errno(const char *msg);

}  // namespace nanoredis
