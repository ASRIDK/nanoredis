#include "avl.h"

#include <algorithm>
#include <cassert>

namespace nanoredis {

namespace {

void avl_update(AVLNode *node) {
    node->height = 1 + std::max(avl_height(node->left), avl_height(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

/*
 *     node              new_node
 *    /    \            /        \
 *   a   new_node  ->  node       c
 *       /     \      /    \
 *    inner     c    a    inner
 */
AVLNode *rot_left(AVLNode *node) {
    AVLNode *parent = node->parent;
    AVLNode *new_node = node->right;
    AVLNode *inner = new_node->left;

    node->right = inner;
    if (inner != nullptr) {
        inner->parent = node;
    }
    new_node->parent = parent;
    new_node->left = node;
    node->parent = new_node;

    avl_update(node);
    avl_update(new_node);
    return new_node;
}

AVLNode *rot_right(AVLNode *node) {
    AVLNode *parent = node->parent;
    AVLNode *new_node = node->left;
    AVLNode *inner = new_node->right;

    node->left = inner;
    if (inner != nullptr) {
        inner->parent = node;
    }
    new_node->parent = parent;
    new_node->right = node;
    node->parent = new_node;

    avl_update(node);
    avl_update(new_node);
    return new_node;
}

// The left subtree is two levels taller than the right one.
AVLNode *avl_fix_left(AVLNode *node) {
    if (avl_height(node->left->left) < avl_height(node->left->right)) {
        node->left = rot_left(node->left);  // left-right case
    }
    return rot_right(node);
}

// The right subtree is two levels taller than the left one.
AVLNode *avl_fix_right(AVLNode *node) {
    if (avl_height(node->right->right) < avl_height(node->right->left)) {
        node->right = rot_right(node->right);  // right-left case
    }
    return rot_left(node);
}

// The slot in the parent (or the caller's root variable) that points at `node`.
AVLNode **parent_link(AVLNode *node, AVLNode **root) {
    AVLNode *parent = node->parent;
    if (parent == nullptr) {
        return root;
    }
    return parent->left == node ? &parent->left : &parent->right;
}

// Removes a node that has at most one child.
AVLNode *avl_del_easy(AVLNode *node) {
    assert(node->left == nullptr || node->right == nullptr);
    AVLNode *child = node->left != nullptr ? node->left : node->right;
    AVLNode *parent = node->parent;
    if (child != nullptr) {
        child->parent = parent;
    }
    if (parent == nullptr) {
        return child;
    }
    AVLNode *unused_root = nullptr;
    *parent_link(node, &unused_root) = child;
    return avl_fix(parent);
}

}  // namespace

AVLNode *avl_fix(AVLNode *node) {
    for (;;) {
        AVLNode *root = node;
        AVLNode **from = parent_link(node, &root);
        AVLNode *parent = node->parent;

        avl_update(node);
        uint32_t l = avl_height(node->left);
        uint32_t r = avl_height(node->right);
        if (l == r + 2) {
            *from = avl_fix_left(node);
        } else if (l + 2 == r) {
            *from = avl_fix_right(node);
        }

        if (parent == nullptr) {
            return *from;
        }
        node = parent;
    }
}

AVLNode *avl_del(AVLNode *node) {
    if (node->left == nullptr || node->right == nullptr) {
        return avl_del_easy(node);
    }

    // Two children: detach the in-order successor, then put it in node's place.
    AVLNode *victim = node->right;
    while (victim->left != nullptr) {
        victim = victim->left;
    }
    AVLNode *root = avl_del_easy(victim);

    *victim = *node;
    if (victim->left != nullptr) {
        victim->left->parent = victim;
    }
    if (victim->right != nullptr) {
        victim->right->parent = victim;
    }
    *parent_link(node, &root) = victim;
    return root;
}

AVLNode *avl_offset(AVLNode *node, int64_t offset) {
    int64_t pos = 0;  // position of `node` relative to the starting node
    while (offset != pos) {
        if (pos < offset && pos + avl_cnt(node->right) >= offset) {
            // The target is inside the right subtree.
            node = node->right;
            pos += avl_cnt(node->left) + 1;
        } else if (pos > offset && pos - avl_cnt(node->left) <= offset) {
            // The target is inside the left subtree.
            node = node->left;
            pos -= avl_cnt(node->right) + 1;
        } else {
            // Go up to the parent.
            AVLNode *parent = node->parent;
            if (parent == nullptr) {
                return nullptr;
            }
            if (parent->right == node) {
                pos -= avl_cnt(node->left) + 1;
            } else {
                pos += avl_cnt(node->right) + 1;
            }
            node = parent;
        }
    }
    return node;
}

int64_t avl_rank(const AVLNode *node) {
    int64_t rank = avl_cnt(node->left);
    for (; node->parent != nullptr; node = node->parent) {
        if (node->parent->right == node) {
            rank += avl_cnt(node->parent->left) + 1;
        }
    }
    return rank;
}

}  // namespace nanoredis
