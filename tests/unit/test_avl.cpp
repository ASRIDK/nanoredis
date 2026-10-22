#include <algorithm>
#include <random>
#include <set>
#include <vector>

#include "../../src/avl.h"
#include "test.h"

using namespace nanoredis;

namespace {

struct Data : AVLNode {
    explicit Data(uint32_t v) : val(v) {}
    uint32_t val;
};

struct Container {
    AVLNode *root = nullptr;

    void insert(uint32_t val) {
        auto *data = new Data(val);
        AVLNode *cur = nullptr;
        AVLNode **from = &root;
        while (*from != nullptr) {
            cur = *from;
            from = val < static_cast<Data *>(cur)->val ? &cur->left : &cur->right;
        }
        *from = data;
        data->parent = cur;
        root = avl_fix(data);
    }

    bool erase(uint32_t val) {
        AVLNode *cur = root;
        while (cur != nullptr && static_cast<Data *>(cur)->val != val) {
            cur = val < static_cast<Data *>(cur)->val ? cur->left : cur->right;
        }
        if (cur == nullptr) {
            return false;
        }
        root = avl_del(cur);
        delete static_cast<Data *>(cur);
        return true;
    }

    void clear() {
        while (root != nullptr) {
            AVLNode *node = root;
            root = avl_del(node);
            delete static_cast<Data *>(node);
        }
    }
};

// Checks parent links, balance, cached height/count and ordering of the subtree.
void verify(const AVLNode *parent, const AVLNode *node) {
    if (node == nullptr) {
        return;
    }
    CHECK(node->parent == parent);
    verify(node, node->left);
    verify(node, node->right);

    CHECK(node->cnt == 1 + avl_cnt(node->left) + avl_cnt(node->right));
    uint32_t l = avl_height(node->left);
    uint32_t r = avl_height(node->right);
    CHECK(l == r || l + 1 == r || l == r + 1);
    CHECK(node->height == 1 + std::max(l, r));

    uint32_t val = static_cast<const Data *>(node)->val;
    if (node->left != nullptr) {
        CHECK(static_cast<const Data *>(node->left)->val <= val);
    }
    if (node->right != nullptr) {
        CHECK(static_cast<const Data *>(node->right)->val >= val);
    }
}

void collect(const AVLNode *node, std::vector<uint32_t> &out) {
    if (node == nullptr) {
        return;
    }
    collect(node->left, out);
    out.push_back(static_cast<const Data *>(node)->val);
    collect(node->right, out);
}

void check_matches(const Container &c, const std::multiset<uint32_t> &ref) {
    verify(nullptr, c.root);
    CHECK(avl_cnt(c.root) == ref.size());
    std::vector<uint32_t> values;
    collect(c.root, values);
    CHECK(std::equal(values.begin(), values.end(), ref.begin(), ref.end()));
}

}  // namespace

TEST(avl_random_insert_and_delete_keep_invariants) {
    std::mt19937 rng(12345);
    Container c;
    std::multiset<uint32_t> ref;
    for (int i = 0; i < 2000; ++i) {
        uint32_t v = rng() % 1000;
        c.insert(v);
        ref.insert(v);
        if (i % 97 == 0) {
            check_matches(c, ref);
        }
    }
    check_matches(c, ref);

    for (int i = 0; i < 2000; ++i) {
        uint32_t v = rng() % 1000;
        auto it = ref.find(v);
        CHECK(c.erase(v) == (it != ref.end()));
        if (it != ref.end()) {
            ref.erase(it);
        }
    }
    check_matches(c, ref);
    c.clear();
}

TEST(avl_sequential_insert_stays_balanced) {
    Container c;
    for (uint32_t i = 0; i < 1023; ++i) {
        c.insert(i);
    }
    verify(nullptr, c.root);
    CHECK(avl_height(c.root) <= 11);  // a perfect tree of 1023 nodes has height 10
    c.clear();
}

TEST(avl_offset_and_rank_agree_with_sorted_order) {
    Container c;
    for (uint32_t i = 0; i < 200; ++i) {
        c.insert(i * 2);  // even numbers 0..398
    }
    AVLNode *min = c.root;
    while (min->left != nullptr) {
        min = min->left;
    }
    for (int64_t offset = 0; offset < 200; ++offset) {
        AVLNode *node = avl_offset(min, offset);
        CHECK(node != nullptr && static_cast<Data *>(node)->val == offset * 2);
        CHECK(avl_rank(node) == offset);
        for (int64_t back = 0; back <= offset; back += 37) {
            AVLNode *prev = avl_offset(node, -back);
            CHECK(prev != nullptr && static_cast<Data *>(prev)->val == (offset - back) * 2);
        }
    }
    CHECK(avl_offset(min, 200) == nullptr);
    CHECK(avl_offset(min, -1) == nullptr);
    c.clear();
}
