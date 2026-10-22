#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nanoredis {

// Upper bound on the number of strings in a single request.
constexpr size_t kMaxArgs = 200 * 1000;

// Decodes a request body: | nstr (4) | len (4) | str | len (4) | str | ... |
// Returns false if the body is malformed.
bool parse_request(const uint8_t *data, size_t size, std::vector<std::string> &out);

// Encodes `args` as a full framed request: | body_len (4) | body |.
std::vector<uint8_t> encode_request(const std::vector<std::string> &args);

}  // namespace nanoredis
