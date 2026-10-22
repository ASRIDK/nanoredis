#include <map>
#include <memory>
#include <random>
#include <vector>

#include "../../src/heap.h"
#include "test.h"

using namespace nanoredis;

namespace {

struct Owner : HeapSlot {
    uint64_t val = 0;
};

void verify(const std::vector<HeapItem> &heap) {
    for (size_t i = 0; i < heap.size(); ++i) {
        CHECK(heap[i].ref->heap_idx == i);
        size_t left = i * 2 + 1;
        size_t right = left + 1;
        if (left < heap.size()) {
            CHECK(heap[i].val <= heap[left].val);
        }
        if (right < heap.size()) {
            CHECK(heap[i].val <= heap[right].val);
        }
    }
}

}  // namespace

TEST(heap_random_upsert_and_delete_keep_invariants) {
    std::mt19937 rng(2024);
    std::vector<HeapItem> heap;
    std::vector<std::unique_ptr<Owner>> owners;
    for (int i = 0; i < 500; ++i) {
        owners.push_back(std::make_unique<Owner>());
    }

    for (int step = 0; step < 20000; ++step) {
        Owner &o = *owners[rng() % owners.size()];
        if (rng() % 3 == 0 && o.heap_idx != kNotInHeap) {
            heap_delete(heap, o.heap_idx);
            CHECK(o.heap_idx == kNotInHeap);
        } else {
            o.val = rng() % 10000;
            heap_upsert(heap, &o, o.val);
            CHECK(heap[o.heap_idx].val == o.val);
        }
        if (step % 997 == 0) {
            verify(heap);
        }
    }
    verify(heap);

    // Draining the heap yields values in non-decreasing order.
    uint64_t last = 0;
    while (!heap.empty()) {
        CHECK(heap[0].val >= last);
        last = heap[0].val;
        heap_delete(heap, 0);
    }
}
