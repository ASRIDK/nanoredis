#include <cstring>
#include <string>
#include <vector>

#include "../../src/buffer.h"
#include "../../src/net.h"
#include "../../src/protocol.h"
#include "test.h"

using namespace nanoredis;

namespace {

// Returns the body of a framed request (everything after the length header).
std::vector<uint8_t> body_of(const std::vector<uint8_t> &frame) {
    return std::vector<uint8_t>(frame.begin() + 4, frame.end());
}

uint32_t u32_at(const uint8_t *p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

}  // namespace

TEST(protocol_request_roundtrip) {
    std::vector<std::string> args = {"set", "key", std::string("bin\0val", 7), ""};
    auto frame = encode_request(args);
    CHECK(u32_at(frame.data()) == frame.size() - 4);

    auto body = body_of(frame);
    std::vector<std::string> out;
    CHECK(parse_request(body.data(), body.size(), out));
    CHECK(out == args);
}

TEST(protocol_empty_request_roundtrip) {
    auto body = body_of(encode_request({}));
    std::vector<std::string> out = {"stale"};
    CHECK(parse_request(body.data(), body.size(), out));
    CHECK(out.empty());
}

TEST(protocol_rejects_truncated_and_trailing_bytes) {
    auto body = body_of(encode_request({"get", "k"}));
    std::vector<std::string> out;

    // Every strict prefix is malformed.
    for (size_t n = 0; n < body.size(); ++n) {
        CHECK(!parse_request(body.data(), n, out));
    }
    // Trailing garbage is rejected.
    auto extra = body;
    extra.push_back(0);
    CHECK(!parse_request(extra.data(), extra.size(), out));
}

TEST(protocol_rejects_too_many_args_and_oversized_string) {
    std::vector<std::string> out;

    uint32_t huge = static_cast<uint32_t>(kMaxArgs + 1);
    uint8_t buf[4];
    std::memcpy(buf, &huge, 4);
    CHECK(!parse_request(buf, sizeof(buf), out));

    // One argument claiming far more bytes than are present.
    uint8_t body[8];
    uint32_t one = 1, len = 1000;
    std::memcpy(body, &one, 4);
    std::memcpy(body + 4, &len, 4);
    CHECK(!parse_request(body, sizeof(body), out));
}

TEST(protocol_response_encoding) {
    Buffer out;
    size_t header = response_begin(out);
    out_int(out, 42);
    response_end(out, header);

    CHECK(out.size() == 4 + 1 + 8);
    CHECK(u32_at(out.data()) == 9);
    CHECK(out.data()[4] == TAG_INT);
    int64_t v;
    std::memcpy(&v, out.data() + 5, 8);
    CHECK(v == 42);
}

TEST(protocol_streamed_array_count_is_patched) {
    Buffer out;
    size_t ctx = out_begin_arr(out);
    out_str(out, "a");
    out_nil(out);
    out_end_arr(out, ctx, 2);

    CHECK(out.data()[0] == TAG_ARR);
    CHECK(u32_at(out.data() + 1) == 2);
}

TEST(protocol_error_encoding) {
    Buffer out;
    out_err(out, ERR_BAD_ARG, "oops");
    CHECK(out.data()[0] == TAG_ERR);
    CHECK(u32_at(out.data() + 1) == ERR_BAD_ARG);
    CHECK(u32_at(out.data() + 5) == 4);
    CHECK(std::memcmp(out.data() + 9, "oops", 4) == 0);
}

TEST(protocol_oversized_response_is_replaced_by_error) {
    Buffer out;
    size_t header = response_begin(out);
    std::string big(kMaxMsg + 1, 'x');
    out_str(out, big);
    response_end(out, header);

    CHECK(out.data()[4] == TAG_ERR);
    CHECK(u32_at(out.data() + 5) == ERR_TOO_BIG);
    CHECK(u32_at(out.data()) == out.size() - 4);
}
