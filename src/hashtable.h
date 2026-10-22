#pragma once

#include <cstddef>
#include <cstdint>

namespace nanoredis {

// Intrusive hash node: embed it (by inheritance) in the value type.
struct HNode {
    HNode *next = nullptr;
    uint64_t hcode = 0;
};

// A fixed-size chained hashtable whose capacity is a power of two.
struct HTab {
    HNode **tab = nullptr;
    size_t mask = 0;
    size_t size = 0;
};

// A resizable hashtable built from two HTabs. When the load factor is
// exceeded, `newer` is reallocated and keys are migrated from `older` a few at
// a time on every operation, so no single call pays for the full rehash.
struct HMap {
    HTab newer;
    HTab older;
    size_t migrate_pos = 0;
};

using HNodeEq = bool (*)(const HNode *, const HNode *);

HNode *hm_lookup(HMap *hmap, const HNode *key, HNodeEq eq);
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, const HNode *key, HNodeEq eq);
void hm_clear(HMap *hmap);
size_t hm_size(const HMap *hmap);

// Calls `f(node, arg)` for every node until `f` returns false.
void hm_foreach(HMap *hmap, bool (*f)(HNode *, void *), void *arg);

// FNV-1a style hash used for string keys.
uint64_t str_hash(const uint8_t *data, size_t len);

}  // namespace nanoredis
