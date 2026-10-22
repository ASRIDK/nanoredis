#include "heap.h"

#include <cassert>

namespace nanoredis {

namespace {

size_t heap_parent(size_t i) {
    return (i + 1) / 2 - 1;
}

size_t heap_left(size_t i) {
    return i * 2 + 1;
}

void heap_up(std::vector<HeapItem> &heap, size_t pos) {
    HeapItem item = heap[pos];
    while (pos > 0 && heap[heap_parent(pos)].val > item.val) {
        heap[pos] = heap[heap_parent(pos)];
        heap[pos].ref->heap_idx = pos;
        pos = heap_parent(pos);
    }
    heap[pos] = item;
    heap[pos].ref->heap_idx = pos;
}

void heap_down(std::vector<HeapItem> &heap, size_t pos) {
    HeapItem item = heap[pos];
    const size_t len = heap.size();
    for (;;) {
        size_t left = heap_left(pos);
        size_t right = left + 1;
        size_t min_pos = pos;
        uint64_t min_val = item.val;
        if (left < len && heap[left].val < min_val) {
            min_pos = left;
            min_val = heap[left].val;
        }
        if (right < len && heap[right].val < min_val) {
            min_pos = right;
        }
        if (min_pos == pos) {
            break;
        }
        heap[pos] = heap[min_pos];
        heap[pos].ref->heap_idx = pos;
        pos = min_pos;
    }
    heap[pos] = item;
    heap[pos].ref->heap_idx = pos;
}

}  // namespace

void heap_update(std::vector<HeapItem> &heap, size_t pos) {
    if (pos > 0 && heap[heap_parent(pos)].val > heap[pos].val) {
        heap_up(heap, pos);
    } else {
        heap_down(heap, pos);
    }
}

void heap_upsert(std::vector<HeapItem> &heap, HeapSlot *slot, uint64_t val) {
    size_t pos = slot->heap_idx;
    if (pos < heap.size()) {
        heap[pos].val = val;
    } else {
        pos = heap.size();
        heap.push_back({val, slot});
    }
    heap_update(heap, pos);
}

void heap_delete(std::vector<HeapItem> &heap, size_t pos) {
    assert(pos < heap.size());
    heap[pos].ref->heap_idx = kNotInHeap;
    heap[pos] = heap.back();
    heap.pop_back();
    if (pos < heap.size()) {
        heap_update(heap, pos);
    }
}

}  // namespace nanoredis
