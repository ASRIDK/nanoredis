#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "buffer.h"

namespace nanoredis {

// Upper bound on the number of strings in a single request.
constexpr size_t kMaxArgs = 200 * 1000;

// ---- Requests --------------------------------------------------------------
//
// | body_len (4) | nstr (4) | len (4) | str | len (4) | str | ... |

// Decodes a request body. Returns false if the body is malformed.
bool parse_request(const uint8_t *data, size_t size, std::vector<std::string> &out);

// Encodes `args` as a full framed request.
std::vector<uint8_t> encode_request(const std::vector<std::string> &args);

// ---- Responses -------------------------------------------------------------
//
// | body_len (4) | tagged value |
// A tagged value starts with a one-byte tag followed by its payload:
//   NIL                     -
//   ERR   | code (4) | len (4) | message |
//   STR   | len (4) | bytes |
//   INT   | int64 (8) |
//   DBL   | double (8) |
//   ARR   | n (4) | n tagged values |

enum Tag : uint8_t {
    TAG_NIL = 0,
    TAG_ERR = 1,
    TAG_STR = 2,
    TAG_INT = 3,
    TAG_DBL = 4,
    TAG_ARR = 5,
};

enum ErrorCode : uint32_t {
    ERR_UNKNOWN = 1,  // unknown command
    ERR_TOO_BIG = 2,  // response exceeds kMaxMsg
    ERR_BAD_TYP = 3,  // operation against a key holding the wrong type
    ERR_BAD_ARG = 4,  // malformed argument
};

void out_nil(Buffer &out);
void out_str(Buffer &out, const char *s, size_t size);
void out_str(Buffer &out, const std::string &s);
void out_int(Buffer &out, int64_t value);
void out_dbl(Buffer &out, double value);
void out_err(Buffer &out, ErrorCode code, const std::string &msg);
void out_arr(Buffer &out, uint32_t n);

// For arrays whose length is not known up front: reserve the count, then patch it.
size_t out_begin_arr(Buffer &out);
void out_end_arr(Buffer &out, size_t ctx, uint32_t n);

// Reserves the 4-byte length header for a response and returns its position.
size_t response_begin(Buffer &out);
// Patches the length header; replaces oversized responses with ERR_TOO_BIG.
void response_end(Buffer &out, size_t header);

}  // namespace nanoredis
