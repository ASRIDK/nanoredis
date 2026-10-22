#include "protocol.h"

#include <cstring>

#include "net.h"

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

void out_nil(Buffer &out) {
    out.append_u8(TAG_NIL);
}

void out_str(Buffer &out, const char *s, size_t size) {
    out.append_u8(TAG_STR);
    out.append_u32(static_cast<uint32_t>(size));
    out.append(s, size);
}

void out_str(Buffer &out, const std::string &s) {
    out_str(out, s.data(), s.size());
}

void out_int(Buffer &out, int64_t value) {
    out.append_u8(TAG_INT);
    out.append_i64(value);
}

void out_dbl(Buffer &out, double value) {
    out.append_u8(TAG_DBL);
    out.append_f64(value);
}

void out_err(Buffer &out, ErrorCode code, const std::string &msg) {
    out.append_u8(TAG_ERR);
    out.append_u32(code);
    out.append_u32(static_cast<uint32_t>(msg.size()));
    out.append(msg.data(), msg.size());
}

void out_arr(Buffer &out, uint32_t n) {
    out.append_u8(TAG_ARR);
    out.append_u32(n);
}

size_t out_begin_arr(Buffer &out) {
    out.append_u8(TAG_ARR);
    out.append_u32(0);  // patched by out_end_arr
    return out.size() - 4;
}

void out_end_arr(Buffer &out, size_t ctx, uint32_t n) {
    std::memcpy(out.data() + ctx, &n, 4);
}

size_t response_begin(Buffer &out) {
    size_t header = out.size();
    out.append_u32(0);  // patched by response_end
    return header;
}

void response_end(Buffer &out, size_t header) {
    size_t body_size = out.size() - header - 4;
    if (body_size > kMaxMsg) {
        out.truncate(header + 4);
        out_err(out, ERR_TOO_BIG, "response is too big");
        body_size = out.size() - header - 4;
    }
    uint32_t len = static_cast<uint32_t>(body_size);
    std::memcpy(out.data() + header, &len, 4);
}

}  // namespace nanoredis
