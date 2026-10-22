#include <string>
#include <vector>

#include "../../src/zset.h"
#include "test.h"

using namespace nanoredis;

namespace {

bool add(ZSet &z, const std::string &name, double score) {
    return zset_insert(&z, name.data(), name.size(), score);
}

std::vector<std::string> names_from(ZNode *node, size_t limit) {
    std::vector<std::string> out;
    for (; node != nullptr && out.size() < limit; node = znode_offset(node, 1)) {
        out.push_back(node->name);
    }
    return out;
}

}  // namespace

TEST(zset_orders_by_score_then_name) {
    ZSet z;
    CHECK(add(z, "c", 2.0));
    CHECK(add(z, "a", 1.0));
    CHECK(add(z, "b", 2.0));
    CHECK(add(z, "d", -5.5));
    CHECK(zset_size(&z) == 4);

    ZNode *first = zset_seekge(&z, -1e300, "", 0);
    CHECK((names_from(first, 10) == std::vector<std::string>{"d", "a", "b", "c"}));
    zset_clear(&z);
}

TEST(zset_update_moves_member) {
    ZSet z;
    add(z, "x", 1.0);
    add(z, "y", 2.0);
    CHECK(!add(z, "x", 3.0));  // update, not insert
    CHECK(zset_size(&z) == 2);

    ZNode *x = zset_lookup(&z, "x", 1);
    CHECK(x != nullptr && x->score == 3.0);
    CHECK(znode_rank(x) == 1);
    CHECK((names_from(zset_seekge(&z, 0, "", 0), 10) == std::vector<std::string>{"y", "x"}));
    zset_clear(&z);
}

TEST(zset_seekge_and_offset_support_range_queries) {
    ZSet z;
    for (int i = 0; i < 100; ++i) {
        std::string name = "m" + std::to_string(i);
        add(z, name, i * 10.0);
    }
    ZNode *node = zset_seekge(&z, 255.0, "", 0);
    CHECK(node != nullptr && node->score == 260.0);
    CHECK(znode_offset(node, 2) != nullptr && znode_offset(node, 2)->score == 280.0);
    CHECK(znode_offset(node, -26) != nullptr && znode_offset(node, -26)->score == 0.0);
    CHECK(znode_offset(node, -27) == nullptr);
    CHECK(zset_seekge(&z, 991.0, "", 0) == nullptr);
    zset_clear(&z);
}

TEST(zset_delete_removes_from_both_indexes) {
    ZSet z;
    add(z, "a", 1.0);
    add(z, "b", 2.0);
    ZNode *a = zset_lookup(&z, "a", 1);
    zset_delete(&z, a);
    CHECK(zset_lookup(&z, "a", 1) == nullptr);
    CHECK(zset_size(&z) == 1);
    CHECK(hm_size(&z.hmap) == 1);
    zset_clear(&z);
}
