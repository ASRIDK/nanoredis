#pragma once

// A deliberately tiny unit-test harness: TEST(name) registers a function and
// CHECK(cond) records a failure without aborting the remaining checks.

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace test {

struct Case {
    const char *name;
    std::function<void()> fn;
};

inline std::vector<Case> &registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int &failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char *name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

}  // namespace test

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST(name)                                                                                 \
    static void name();                                                                            \
    static test::Registrar TEST_CONCAT(registrar_, name)(#name, name);                             \
    static void name()

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::fprintf(stderr, "  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond);        \
            ++test::failures();                                                                    \
        }                                                                                          \
    } while (0)
