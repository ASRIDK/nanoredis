#pragma once

namespace nanoredis {

// Intrusive circular doubly linked list. A standalone DList acts as the
// sentinel head; elements embed one (by inheritance).
struct DList {
    DList *prev = this;
    DList *next = this;

    DList() = default;
    DList(const DList &) = delete;
    DList &operator=(const DList &) = delete;
};

inline bool dlist_empty(const DList *node) {
    return node->next == node;
}

inline void dlist_detach(DList *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = node->next = node;
}

// Inserts `rookie` immediately before `target` (i.e. at the tail when
// `target` is the sentinel).
inline void dlist_insert_before(DList *target, DList *rookie) {
    DList *prev = target->prev;
    prev->next = rookie;
    rookie->prev = prev;
    rookie->next = target;
    target->prev = rookie;
}

}  // namespace nanoredis
