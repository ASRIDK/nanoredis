#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "avl.h"
#include "hashtable.h"

namespace nanoredis {

// A sorted set member. It lives in two indexes at once:
//   - the AVL tree, ordered by (score, name), for range and rank queries;
//   - the hashtable, keyed by name, for O(1) point lookups.
struct ZNode : AVLNode, HNode {
    ZNode(const char *name_data, size_t name_len, double s) : score(s), name(name_data, name_len) {}
    double score;
    std::string name;
};

struct ZSet {
    AVLNode *root = nullptr;
    HMap hmap;
};

// Adds a member or updates its score. Returns true if a new member was added.
bool zset_insert(ZSet *zset, const char *name, size_t len, double score);

// Finds a member by name, or returns null.
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len);

// Removes and frees a member.
void zset_delete(ZSet *zset, ZNode *node);

// First member with (score, name) >= the given pair, or null.
ZNode *zset_seekge(ZSet *zset, double score, const char *name, size_t len);

// Member `offset` positions away in sorted order, or null.
ZNode *znode_offset(ZNode *node, int64_t offset);

// Zero-based rank of a member in sorted order.
int64_t znode_rank(const ZNode *node);

size_t zset_size(const ZSet *zset);

// Frees every member.
void zset_clear(ZSet *zset);

}  // namespace nanoredis
