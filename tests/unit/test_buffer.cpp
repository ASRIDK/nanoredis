#include <cstring>
#include <vector>

#include "../../src/buffer.h"
#include "test.h"

using namespace nanoredis;

TEST(buffer_starts_empty) {
    Buffer buf;
    CHECK(buf.empty());
    CHECK(buf.size() == 0);
}

TEST(buffer_append_typed_values_in_order) {
    Buffer buf;
    buf.append_u8(7);
    buf.append_u32(0xdeadbeef);
    buf.append_i64(-5);
    buf.append_f64(2.5);
    CHECK(buf.size() == 1 + 4 + 8 + 8);

    const uint8_t *p = buf.data();
    CHECK(p[0] == 7);
    uint32_t u32;
    std::memcpy(&u32, p + 1, 4);
    CHECK(u32 == 0xdeadbeef);
    int64_t i64;
    std::memcpy(&i64, p + 5, 8);
    CHECK(i64 == -5);
    double f64;
    std::memcpy(&f64, p + 13, 8);
    CHECK(f64 == 2.5);
}

TEST(buffer_consume_advances_front) {
    Buffer buf;
    const uint8_t bytes[] = {1, 2, 3, 4, 5};
    buf.append(bytes, sizeof bytes);
    buf.consume(2);
    CHECK(buf.size() == 3);
    CHECK(buf.data()[0] == 3);
    buf.consume(3);
    CHECK(buf.empty());
}

TEST(buffer_reusable_after_full_consume) {
    Buffer buf;
    buf.append_u32(1);
    buf.consume(4);
    buf.append_u8(9);
    CHECK(buf.size() == 1);
    CHECK(buf.data()[0] == 9);
}

TEST(buffer_truncate_keeps_prefix_of_readable_region) {
    Buffer buf;
    const uint8_t bytes[] = {1, 2, 3, 4, 5};
    buf.append(bytes, sizeof bytes);
    buf.consume(1);
    buf.truncate(2);
    CHECK(buf.size() == 2);
    CHECK(buf.data()[0] == 2 && buf.data()[1] == 3);
}

TEST(buffer_compaction_preserves_remaining_bytes) {
    Buffer buf;
    std::vector<uint8_t> big(10000);
    for (size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<uint8_t>(i % 251);
    }
    buf.append(big.data(), big.size());
    // Consuming more than half (and >4096) triggers compaction.
    buf.consume(6000);
    CHECK(buf.size() == 4000);
    bool same = true;
    for (size_t i = 0; i < buf.size(); ++i) {
        same = same && buf.data()[i] == big[6000 + i];
    }
    CHECK(same);
    buf.append_u8(42);
    CHECK(buf.data()[buf.size() - 1] == 42);
}
