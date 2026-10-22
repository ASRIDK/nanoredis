#include "protocol.h"

#include <cstring>

namespace nanoredis {

namespace {

bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
    if (cur + 4 > end) {
        return false;
    }
    std::memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out) {
    if (cur + n > end) {
        return false;
    }
    out.assign(reinterpret_cast<const char *>(cur), n);
    cur += n;
    return true;
}

void put_u32(std::vector<uint8_t> &buf, uint32_t v) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&v);
    buf.insert(buf.end(), bytes, bytes + 4);
}

}  // namespace

bool parse_request(const uint8_t *data, size_t size, std::vector<std::string> &out) {
    const uint8_t *cur = data;
    const uint8_t *end = data + size;

    uint32_t nstr = 0;
    if (!read_u32(cur, end, nstr) || nstr > kMaxArgs) {
        return false;
    }
    out.clear();
    out.reserve(nstr);
    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(cur, end, len)) {
            return false;
        }
        out.emplace_back();
        if (!read_str(cur, end, len, out.back())) {
            return false;
        }
    }
    return cur == end;  // reject trailing garbage
}

std::vector<uint8_t> encode_request(const std::vector<std::string> &args) {
    std::vector<uint8_t> buf;
    put_u32(buf, 0);  // body length, patched below
    put_u32(buf, static_cast<uint32_t>(args.size()));
    for (const auto &arg : args) {
        put_u32(buf, static_cast<uint32_t>(arg.size()));
        buf.insert(buf.end(), arg.begin(), arg.end());
    }
    uint32_t body_len = static_cast<uint32_t>(buf.size() - 4);
    std::memcpy(buf.data(), &body_len, 4);
    return buf;
}

}  // namespace nanoredis
