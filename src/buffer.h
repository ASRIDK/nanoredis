#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nanoredis {

// A growable FIFO byte buffer. Data is appended at the back and consumed from
// the front; consumed space is reclaimed lazily so consume() is O(1).
class Buffer {
public:
    const uint8_t *data() const { return storage_.data() + begin_; }
    uint8_t *data() { return storage_.data() + begin_; }
    size_t size() const { return storage_.size() - begin_; }
    bool empty() const { return size() == 0; }

    void append(const void *src, size_t n) {
        const auto *bytes = static_cast<const uint8_t *>(src);
        storage_.insert(storage_.end(), bytes, bytes + n);
    }

    void append_u8(uint8_t v) { append(&v, 1); }
    void append_u32(uint32_t v) { append(&v, 4); }
    void append_i64(int64_t v) { append(&v, 8); }
    void append_f64(double v) { append(&v, 8); }

    void consume(size_t n) {
        begin_ += n;
        if (begin_ == storage_.size()) {
            storage_.clear();
            begin_ = 0;
        } else if (begin_ > 4096 && begin_ * 2 > storage_.size()) {
            // Compact once the dead prefix dominates the buffer.
            storage_.erase(storage_.begin(), storage_.begin() + static_cast<ptrdiff_t>(begin_));
            begin_ = 0;
        }
    }

    // Shrinks the readable region to its first `n` bytes.
    void truncate(size_t n) { storage_.resize(begin_ + n); }

private:
    std::vector<uint8_t> storage_;
    size_t begin_ = 0;
};

}  // namespace nanoredis
