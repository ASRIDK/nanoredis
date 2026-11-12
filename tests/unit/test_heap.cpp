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

TEST(heap_single_item_insert_and_delete) {
    std::vector<HeapItem> heap;
    Owner a;
    heap_upsert(heap, &a, 7);
    CHECK(heap.size() == 1);
    CHECK(a.heap_idx == 0);
    heap_delete(heap, 0);
    CHECK(heap.empty());
    CHECK(a.heap_idx == kNotInHeap);

    // A removed slot can be inserted again.
    heap_upsert(heap, &a, 3);
    CHECK(heap.size() == 1);
    CHECK(a.heap_idx == 0);
}

TEST(heap_root_is_minimum_after_ascending_and_descending_inserts) {
    for (int descending = 0; descending < 2; ++descending) {
        std::vector<HeapItem> heap;
        std::vector<Owner> owners(64);
        for (size_t i = 0; i < owners.size(); ++i) {
            uint64_t v = descending ? owners.size() - i : i + 1;
            heap_upsert(heap, &owners[i], v);
        }
        verify(heap);
        CHECK(heap[0].val == 1);
    }
}

TEST(heap_upsert_moves_item_up_and_down) {
    std::vector<HeapItem> heap;
    std::vector<Owner> owners(16);
    for (size_t i = 0; i < owners.size(); ++i) {
        heap_upsert(heap, &owners[i], (i + 1) * 10);
    }
    // Lowering the largest value makes it the new root.
    heap_upsert(heap, &owners[15], 1);
    CHECK(heap[0].ref == &owners[15]);
    verify(heap);
    // Raising the root sinks it below its former children.
    heap_upsert(heap, &owners[15], 1000);
    CHECK(heap[0].ref == &owners[0]);
    CHECK(owners[15].heap_idx != 0);
    verify(heap);
    CHECK(heap.size() == owners.size());  // upsert never duplicates
}

TEST(heap_handles_duplicate_values) {
    std::vector<HeapItem> heap;
    std::vector<Owner> owners(20);
    for (auto &o : owners) {
        heap_upsert(heap, &o, 5);
    }
    verify(heap);
    while (!heap.empty()) {
        CHECK(heap[0].val == 5);
        heap_delete(heap, heap.size() - 1);  // deleting the last slot is a special case
        verify(heap);
    }
}
