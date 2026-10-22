#include <set>
#include <string>
#include <vector>

#include "../../src/hashtable.h"
#include "test.h"

using namespace nanoredis;

namespace {

struct Item : HNode {
    explicit Item(int v) : value(v) {
        hcode = str_hash(reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    }
    int value;
};

bool item_eq(const HNode *a, const HNode *b) {
    return static_cast<const Item *>(a)->value == static_cast<const Item *>(b)->value;
}

Item *find(HMap &map, int v) {
    Item key(v);
    return static_cast<Item *>(hm_lookup(&map, &key, item_eq));
}

}  // namespace

TEST(hashtable_insert_lookup_delete) {
    HMap map;
    std::vector<Item *> items;
    for (int i = 0; i < 10000; ++i) {
        items.push_back(new Item(i));
        hm_insert(&map, items.back());
    }
    CHECK(hm_size(&map) == 10000);

    for (int i = 0; i < 10000; ++i) {
        Item *found = find(map, i);
        CHECK(found != nullptr && found->value == i);
    }
    CHECK(find(map, -1) == nullptr);

    for (int i = 0; i < 10000; i += 2) {
        Item key(i);
        HNode *removed = hm_delete(&map, &key, item_eq);
        CHECK(removed != nullptr);
    }
    CHECK(hm_size(&map) == 5000);
    for (int i = 0; i < 10000; ++i) {
        CHECK((find(map, i) != nullptr) == (i % 2 == 1));
    }

    hm_clear(&map);
    CHECK(hm_size(&map) == 0);
    for (Item *item : items) {
        delete item;
    }
}

TEST(hashtable_lookup_during_progressive_rehash) {
    HMap map;
    std::vector<Item *> items;
    // The 256th insert grows the table from 32 to 64 slots; only 128 of the 256
    // keys are migrated by that call, so the old table is still live.
    for (int i = 0; i < 256; ++i) {
        items.push_back(new Item(i));
        hm_insert(&map, items.back());
    }
    CHECK(map.older.tab != nullptr);
    CHECK(map.older.size > 0 && map.newer.size > 0);
    for (int i = 0; i < 256; ++i) {
        CHECK(find(map, i) != nullptr);
    }
    CHECK(map.older.tab == nullptr);  // lookups finish the migration
    CHECK(hm_size(&map) == 256);
    hm_clear(&map);
    for (Item *item : items) {
        delete item;
    }
}

TEST(hashtable_foreach_visits_every_node_once) {
    HMap map;
    std::vector<Item *> items;
    for (int i = 0; i < 1000; ++i) {
        items.push_back(new Item(i));
        hm_insert(&map, items.back());
    }
    std::set<int> seen;
    hm_foreach(
        &map,
        [](HNode *node, void *arg) {
            static_cast<std::set<int> *>(arg)->insert(static_cast<Item *>(node)->value);
            return true;
        },
        &seen);
    CHECK(seen.size() == 1000);
    hm_clear(&map);
    for (Item *item : items) {
        delete item;
    }
}
