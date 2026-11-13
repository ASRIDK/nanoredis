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

TEST(zset_reinserting_a_member_updates_score_and_reports_not_new) {
    ZSet z;
    CHECK(add(z, "a", 1.0));
    CHECK(!add(z, "a", 5.0));
    CHECK(zset_size(&z) == 1);
    ZNode *node = zset_lookup(&z, "a", 1);
    CHECK(node != nullptr);
    CHECK(node->score == 5.0);
    zset_clear(&z);
}

TEST(zset_lookup_of_missing_member_and_empty_set_is_null) {
    ZSet z;
    CHECK(zset_lookup(&z, "nope", 4) == nullptr);
    CHECK(zset_seekge(&z, 0.0, "", 0) == nullptr);
    CHECK(zset_size(&z) == 0);
    add(z, "a", 1.0);
    CHECK(zset_lookup(&z, "b", 1) == nullptr);
    zset_clear(&z);
}

TEST(zset_seekge_past_the_last_member_is_null) {
    ZSet z;
    add(z, "a", 1.0);
    add(z, "b", 2.0);
    CHECK(zset_seekge(&z, 3.0, "", 0) == nullptr);
    ZNode *first = zset_seekge(&z, -100.0, "", 0);
    CHECK(first != nullptr && first->name == "a");
    zset_clear(&z);
}

TEST(zset_rank_and_offset_are_consistent_and_bounded) {
    ZSet z;
    for (int i = 0; i < 50; ++i) {
        add(z, "m" + std::to_string(1000 + i), static_cast<double>(i));
    }
    ZNode *first = zset_seekge(&z, 0.0, "", 0);
    CHECK(first != nullptr);
    CHECK(znode_rank(first) == 0);
    CHECK(znode_offset(first, -1) == nullptr);
    CHECK(znode_offset(first, 50) == nullptr);
    ZNode *last = znode_offset(first, 49);
    CHECK(last != nullptr && znode_rank(last) == 49);
    CHECK(znode_offset(last, 1) == nullptr);
    CHECK(znode_offset(last, -49) == first);
    zset_clear(&z);
}

TEST(zset_clear_empties_the_set_and_allows_reuse) {
    ZSet z;
    for (int i = 0; i < 100; ++i) {
        add(z, std::to_string(i), static_cast<double>(i));
    }
    CHECK(zset_size(&z) == 100);
    zset_clear(&z);
    CHECK(zset_size(&z) == 0);
    CHECK(zset_lookup(&z, "5", 1) == nullptr);
    CHECK(add(z, "5", 1.0));
    CHECK(zset_size(&z) == 1);
    zset_clear(&z);
}
