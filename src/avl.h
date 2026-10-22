#pragma once

#include <cstdint>

namespace nanoredis {

// Intrusive AVL tree node, augmented with the subtree size so that rank and
// offset queries run in O(log n). Embed it (by inheritance) in the value type.
struct AVLNode {
    AVLNode *parent = nullptr;
    AVLNode *left = nullptr;
    AVLNode *right = nullptr;
    uint32_t height = 1;  // height of the subtree rooted here
    uint32_t cnt = 1;     // number of nodes in the subtree rooted here
};

inline uint32_t avl_height(const AVLNode *node) {
    return node != nullptr ? node->height : 0;
}

inline uint32_t avl_cnt(const AVLNode *node) {
    return node != nullptr ? node->cnt : 0;
}

// Rebalances the tree after `node` was attached or modified; returns the new root.
AVLNode *avl_fix(AVLNode *node);

// Detaches `node` from its tree; returns the new root (possibly null).
AVLNode *avl_del(AVLNode *node);

// Walks `offset` positions in sorted order from `node` (negative = backwards).
// Returns null when the target is out of range.
AVLNode *avl_offset(AVLNode *node, int64_t offset);

// Zero-based position of `node` in sorted order.
int64_t avl_rank(const AVLNode *node);

}  // namespace nanoredis
