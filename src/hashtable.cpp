#include "hashtable.h"

#include <cassert>
#include <cstdlib>

namespace nanoredis {

namespace {

constexpr size_t kRehashingWork = 128;  // keys migrated per operation
constexpr size_t kMaxLoadFactor = 8;
constexpr size_t kInitialCapacity = 4;

void h_init(HTab *htab, size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0);
    htab->tab = static_cast<HNode **>(std::calloc(n, sizeof(HNode *)));
    if (htab->tab == nullptr) {
        std::abort();
    }
    htab->mask = n - 1;
    htab->size = 0;
}

void h_insert(HTab *htab, HNode *node) {
    size_t pos = node->hcode & htab->mask;
    node->next = htab->tab[pos];
    htab->tab[pos] = node;
    htab->size++;
}

// Returns the address of the parent pointer that owns the matching node, so the
// caller can unlink it without a separate "previous" pointer.
HNode **h_lookup(HTab *htab, const HNode *key, HNodeEq eq) {
    if (htab->tab == nullptr) {
        return nullptr;
    }
    size_t pos = key->hcode & htab->mask;
    HNode **from = &htab->tab[pos];
    for (HNode *cur; (cur = *from) != nullptr; from = &cur->next) {
        if (cur->hcode == key->hcode && eq(cur, key)) {
            return from;
        }
    }
    return nullptr;
}

HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from;
    *from = node->next;
    htab->size--;
    return node;
}

void hm_help_rehashing(HMap *hmap) {
    size_t nwork = 0;
    while (nwork < kRehashingWork && hmap->older.size > 0) {
        HNode **from = &hmap->older.tab[hmap->migrate_pos];
        if (*from == nullptr) {
            hmap->migrate_pos++;
            continue;
        }
        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        nwork++;
    }
    if (hmap->older.size == 0 && hmap->older.tab != nullptr) {
        std::free(hmap->older.tab);
        hmap->older = HTab{};
    }
}

void hm_trigger_rehashing(HMap *hmap) {
    assert(hmap->older.tab == nullptr);
    hmap->older = hmap->newer;
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;
}

bool h_foreach(HTab *htab, bool (*f)(HNode *, void *), void *arg) {
    for (size_t i = 0; htab->tab != nullptr && i <= htab->mask; ++i) {
        for (HNode *node = htab->tab[i]; node != nullptr; node = node->next) {
            if (!f(node, arg)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

HNode *hm_lookup(HMap *hmap, const HNode *key, HNodeEq eq) {
    hm_help_rehashing(hmap);
    HNode **from = h_lookup(&hmap->newer, key, eq);
    if (from == nullptr) {
        from = h_lookup(&hmap->older, key, eq);
    }
    return from != nullptr ? *from : nullptr;
}

void hm_insert(HMap *hmap, HNode *node) {
    if (hmap->newer.tab == nullptr) {
        h_init(&hmap->newer, kInitialCapacity);
    }
    h_insert(&hmap->newer, node);

    if (hmap->older.tab == nullptr) {
        size_t threshold = (hmap->newer.mask + 1) * kMaxLoadFactor;
        if (hmap->newer.size >= threshold) {
            hm_trigger_rehashing(hmap);
        }
    }
    hm_help_rehashing(hmap);
}

HNode *hm_delete(HMap *hmap, const HNode *key, HNodeEq eq) {
    hm_help_rehashing(hmap);
    if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
        return h_detach(&hmap->newer, from);
    }
    if (HNode **from = h_lookup(&hmap->older, key, eq)) {
        return h_detach(&hmap->older, from);
    }
    return nullptr;
}

void hm_clear(HMap *hmap) {
    std::free(hmap->newer.tab);
    std::free(hmap->older.tab);
    *hmap = HMap{};
}

size_t hm_size(const HMap *hmap) {
    return hmap->newer.size + hmap->older.size;
}

void hm_foreach(HMap *hmap, bool (*f)(HNode *, void *), void *arg) {
    if (h_foreach(&hmap->newer, f, arg)) {
        h_foreach(&hmap->older, f, arg);
    }
}

uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

}  // namespace nanoredis
