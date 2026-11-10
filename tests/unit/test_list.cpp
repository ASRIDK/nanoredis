#include "../../src/list.h"
#include "test.h"

using namespace nanoredis;

TEST(dlist_starts_empty_and_self_linked) {
    DList head;
    CHECK(dlist_empty(&head));
    CHECK(head.next == &head);
    CHECK(head.prev == &head);
}

TEST(dlist_insert_before_sentinel_appends_in_order) {
    DList head, a, b, c;
    dlist_insert_before(&head, &a);
    dlist_insert_before(&head, &b);
    dlist_insert_before(&head, &c);
    CHECK(!dlist_empty(&head));
    CHECK(head.next == &a);
    CHECK(a.next == &b);
    CHECK(b.next == &c);
    CHECK(c.next == &head);
    CHECK(head.prev == &c);
    CHECK(c.prev == &b);
    CHECK(b.prev == &a);
    CHECK(a.prev == &head);
}

TEST(dlist_detach_middle_relinks_neighbours) {
    DList head, a, b, c;
    dlist_insert_before(&head, &a);
    dlist_insert_before(&head, &b);
    dlist_insert_before(&head, &c);
    dlist_detach(&b);
    CHECK(a.next == &c);
    CHECK(c.prev == &a);
    // The detached node is self-linked, so detaching again is harmless.
    CHECK(b.next == &b && b.prev == &b);
    dlist_detach(&b);
    CHECK(a.next == &c);
}

TEST(dlist_detach_all_leaves_empty_list) {
    DList head, a, b;
    dlist_insert_before(&head, &a);
    dlist_insert_before(&head, &b);
    dlist_detach(&a);
    dlist_detach(&b);
    CHECK(dlist_empty(&head));
}

TEST(dlist_move_to_tail_models_lru_touch) {
    DList head, a, b, c;
    dlist_insert_before(&head, &a);
    dlist_insert_before(&head, &b);
    dlist_insert_before(&head, &c);
    dlist_detach(&a);
    dlist_insert_before(&head, &a);
    CHECK(head.next == &b);
    CHECK(head.prev == &a);
    CHECK(c.next == &a);
}
