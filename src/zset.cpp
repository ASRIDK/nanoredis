#include "zset.h"

#include <algorithm>
#include <cstring>

namespace nanoredis {

namespace {

// A name-only key used to probe the hashtable.
struct HKey : HNode {
    const char *name = nullptr;
    size_t len = 0;
};

bool hcmp(const HNode *node, const HNode *key) {
    const auto *znode = static_cast<const ZNode *>(node);
    const auto *hkey = static_cast<const HKey *>(key);
    return znode->name.size() == hkey->len &&
           std::memcmp(znode->name.data(), hkey->name, hkey->len) == 0;
}

ZNode *from_avl(AVLNode *node) {
    return node != nullptr ? static_cast<ZNode *>(node) : nullptr;
}

// Orders members by (score, name).
bool zless(const AVLNode *lhs, double score, const char *name, size_t len) {
    const auto *zl = static_cast<const ZNode *>(lhs);
    if (zl->score != score) {
        return zl->score < score;
    }
    int rv = std::memcmp(zl->name.data(), name, std::min(zl->name.size(), len));
    if (rv != 0) {
        return rv < 0;
    }
    return zl->name.size() < len;
}

bool zless(const AVLNode *lhs, const AVLNode *rhs) {
    const auto *zr = static_cast<const ZNode *>(rhs);
    return zless(lhs, zr->score, zr->name.data(), zr->name.size());
}

void tree_insert(ZSet *zset, ZNode *node) {
    AVLNode *parent = nullptr;
    AVLNode **from = &zset->root;
    while (*from != nullptr) {
        parent = *from;
        from = zless(node, parent) ? &parent->left : &parent->right;
    }
    *from = node;
    node->parent = parent;
    zset->root = avl_fix(node);
}

void zset_update(ZSet *zset, ZNode *node, double score) {
    if (node->score == score) {
        return;
    }
    // Re-position the node in the tree under its new score.
    zset->root = avl_del(node);
    *static_cast<AVLNode *>(node) = AVLNode{};
    node->score = score;
    tree_insert(zset, node);
}

}  // namespace

ZNode *zset_lookup(ZSet *zset, const char *name, size_t len) {
    if (zset->root == nullptr) {
        return nullptr;
    }
    HKey key;
    key.hcode = str_hash(reinterpret_cast<const uint8_t *>(name), len);
    key.name = name;
    key.len = len;
    return static_cast<ZNode *>(hm_lookup(&zset->hmap, &key, hcmp));
}

bool zset_insert(ZSet *zset, const char *name, size_t len, double score) {
    if (ZNode *node = zset_lookup(zset, name, len)) {
        zset_update(zset, node, score);
        return false;
    }
    auto *node = new ZNode(name, len, score);
    node->hcode = str_hash(reinterpret_cast<const uint8_t *>(name), len);
    hm_insert(&zset->hmap, node);
    tree_insert(zset, node);
    return true;
}

void zset_delete(ZSet *zset, ZNode *node) {
    HKey key;
    key.hcode = node->hcode;
    key.name = node->name.data();
    key.len = node->name.size();
    hm_delete(&zset->hmap, &key, hcmp);
    zset->root = avl_del(node);
    delete node;
}

ZNode *zset_seekge(ZSet *zset, double score, const char *name, size_t len) {
    AVLNode *found = nullptr;
    for (AVLNode *node = zset->root; node != nullptr;) {
        if (zless(node, score, name, len)) {
            node = node->right;
        } else {
            found = node;  // candidate; keep looking for a smaller one
            node = node->left;
        }
    }
    return from_avl(found);
}

ZNode *znode_offset(ZNode *node, int64_t offset) {
    return node != nullptr ? from_avl(avl_offset(node, offset)) : nullptr;
}

int64_t znode_rank(const ZNode *node) {
    return avl_rank(node);
}

size_t zset_size(const ZSet *zset) {
    return avl_cnt(zset->root);
}

void zset_clear(ZSet *zset) {
    // Post-order deletion so children are freed before their parent.
    struct Walker {
        static void dispose(AVLNode *node) {
            if (node == nullptr) {
                return;
            }
            dispose(node->left);
            dispose(node->right);
            delete static_cast<ZNode *>(node);
        }
    };
    Walker::dispose(zset->root);
    hm_clear(&zset->hmap);
    zset->root = nullptr;
}

}  // namespace nanoredis
