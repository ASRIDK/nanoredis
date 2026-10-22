#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nanoredis {

constexpr size_t kNotInHeap = static_cast<size_t>(-1);

// Base class for anything stored in the heap. The heap keeps `heap_idx` up to
// date as items move, so owners can update or remove their item in O(log n),
// and the heap can hand back the owner (via static_cast) when it pops.
struct HeapSlot {
    size_t heap_idx = kNotInHeap;
};

struct HeapItem {
    uint64_t val = 0;
    HeapSlot *ref = nullptr;
};

// Restores the heap property after the item at `pos` was added or changed.
void heap_update(std::vector<HeapItem> &heap, size_t pos);

// Inserts `slot` with value `val`, or updates its value if already present.
void heap_upsert(std::vector<HeapItem> &heap, HeapSlot *slot, uint64_t val);

// Removes the item at `pos`.
void heap_delete(std::vector<HeapItem> &heap, size_t pos);

}  // namespace nanoredis
